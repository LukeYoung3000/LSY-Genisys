#include "ConsoleDashboard.h"
#include "ConsoleConfiguration.h"
#include "ConsoleRecalls.h"
#include "GenisysMaster-Lib-Logging.h"

#include <conio.h>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <thread>


unsigned Number(const std::string& text, unsigned low, unsigned high)
{
	size_t used = 0;
	if (text.empty() || text[0] == '-') throw std::runtime_error("Invalid numeric argument: " + text);
	const auto value = std::stoul(text, &used, text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X') ? 16 : 10);
	if (used != text.size() || value < low || value > high) throw std::runtime_error("Invalid numeric argument: " + text);
	return value;
}

void Show(const Slave& slave, bool controls = false)
{
	const auto frame = controls ? slave.controls.frame : slave.indications;
	std::cout << "Slave " << unsigned(slave.id) << (controls ? " desired controls (hex, local draft):" : " indications (hex):");
	for (uint64_t i = 0; i < frame->GetNumBytes(); ++i)
	{
		if (i % 16 == 0) std::cout << '\n' << std::setw(3) << std::setfill(' ') << i << ": ";
		std::cout << std::hex << std::setw(2) << std::setfill('0') << unsigned((*frame)[i]) << ' ' << std::dec;
	}
	std::cout << std::setfill(' ');
	if (controls)
	{
		std::cout << "\nLast control: " << slave.controls.Result() << " | mode ";
		if (slave.controls.include_mode) std::cout << "0x" << std::hex << unsigned(slave.controls.mode) << std::dec;
		else std::cout << "omitted";
		std::cout << " | modified=" << slave.controls.Dirty();
	}
	else if (slave.indications->HasModeByte()) std::cout << "\nMode: 0x" << std::hex << unsigned(slave.indications->GetModeByte()) << std::dec;
	std::cout << '\n';
}

int main(int argc, char** argv)
{
	try
	{
		const auto arguments = ParseConsoleArguments(std::vector<std::string>(argv + 1, argv + argc));
		if (arguments.help)
		{
			std::cout << "GenisysMaster-Console [--config file.xml] [--transport udp|serial]\n"
				"UDP: [--local-ip IPv4] [--local-port port] [--slave id IPv4 port frame-bytes]...\n"
				"Serial: [--com-port COM10] [--baud 9600] [--parity N|E|O] [--data-bits 8]\n"
				"  [--stop-bits 1|2] [--write-timeout-ms n] [--dtr true|false] [--rts true|false]\n"
				"  [--serial-slave id frame-bytes]... (or existing --slave syntax; endpoints ignored)\n"
				"Both: [--plain] [--manual] [--recall-all] [--wait-ms n] [--run-ms n]\n"
				"  [--poll-ms n] [--timeout-ms n] [--retries n] [--recovery-ms n]\n"
				"XML loads first; CLI flags override it; CLI slave entries replace the XML list.\n"
				"Defaults: UDP 0.0.0.0:7170, slave 34 at 127.0.0.1:7169, 125 bytes.\n"
				"Serial defaults: COM1, 9600 8N1, DTR/RTS off, write timeout 1000 ms.\n"
				"IDs/ports/size accept decimal or 0x hex. --recall-all runs once then exits.\n"
				"Serial recalls are queued; one exchange uses the bus at a time. Controls are not queued.\n"
				"Default: fixed dashboard. --plain selects scrolling text (also used for redirected I/O).\n"
				"Dashboard: C controls, J/K byte, Left/Right bit, Space toggle, E byte, M mode, S send, F full.\n"
				"Up/Down select slave, PgUp/PgDn bytes, R recall, A recall all, H help, Q quit.\n"
				"Plain: r <id>, r all, show <id>, controls <id>, setbyte <id> <offset> <value>\n"
				"  setbit <id> <offset> <bit> <0|1>, mode <id> <value|off>, send <id>, sendall <id>, status, q.\n"
				"Edits stay local until explicitly sent. Controls are never automatically retried.\n";
			return 0;
		}
		const auto& settings = arguments.settings;
		const auto& config = settings.network;
		const auto& config_path = arguments.config_path;
		const bool serial = config.connection_type_ == LSY::NetworkGenisysMaster::Config::CONNECTIONTYPE::SERIAL;
		const bool recall_all = settings.recall_all;
		bool plain = settings.plain;
		const unsigned wait_ms = settings.wait_ms, run_ms = settings.run_ms;
		std::vector<Slave> slaves;
		std::vector<uint8_t> slave_ids;
		for (const auto& item : settings.slaves)
		{
			slaves.push_back({item.id, item.endpoint, std::make_shared<LSY::MasterDataFrame>("Indications " + std::to_string(item.id), item.frame_bytes)});
			slave_ids.push_back(item.id);
		}
		ConsoleRecalls recalls(slave_ids, serial);
		ConsoleDashboard dashboard;
		if (!plain && !dashboard.Start()) plain = true;
		auto emit = [&](const std::string& text, bool error = false)
		{
			if (dashboard.Active()) dashboard.AddEvent(text, error);
			else (error ? std::cerr : std::cout) << text << '\n';
		};
		LSY::MasterLogging::AddLogErrorFunc([&](std::string text) { emit("ERROR: " + text, true); return true; });
		LSY::MasterLogging::AddLogWarningFunc([&](std::string text) { emit("WARNING: " + text, true); return true; });

		auto protocol = std::make_shared<LSY::ProtocolGenisysMaster>();
		LSY::NetworkGenisysMaster network(config);
		if (!network.AddProtocol(protocol)) throw std::runtime_error("Cannot attach protocol");
		for (auto& slave : slaves)
		{
			slave.controls.Initialize(slave.id, slave.indications->GetNumBytes());
			if (!protocol->AddDataFrame(slave.id, false, slave.indications) ||
				!protocol->AddDataFrame(slave.id, true, slave.controls.frame) || !(serial ? network.AddSlave(slave.id) : network.AddSlave(slave.id, slave.endpoint)))
				throw std::runtime_error("Invalid or duplicate slave/endpoint configuration");
			const uint8_t id = slave.id;
			const auto frame = slave.indications;
			std::function<void(std::string, LSY::MasterDataFrame::CALLBACKEVENT)> callback =
				[&, id](std::string, LSY::MasterDataFrame::CALLBACKEVENT event)
				{
					if (dashboard.Active() && event == LSY::MasterDataFrame::CALLBACKEVENT::INDICATION_CHANGE)
						for (const auto& item : slaves) if (item.id == id) dashboard.ObserveIndications(item);
					emit("Slave " + std::to_string(id) +
						(event == LSY::MasterDataFrame::CALLBACKEVENT::INDICATION_CHANGE ? " indication callback" : " mode callback"));
				};
			frame->AddEventCallback(callback);
		}
		std::function<void(uint8_t, LSY::NetworkGenisysMaster::COMMUNICATIONSTATUS)> status_callback =
			[&](uint8_t id, LSY::NetworkGenisysMaster::COMMUNICATIONSTATUS status)
			{
				emit("Slave " + std::to_string(id) + " status: " +
					(status == LSY::NetworkGenisysMaster::COMMUNICATIONSTATUS::ONLINE ? "ONLINE" :
					status == LSY::NetworkGenisysMaster::COMMUNICATIONSTATUS::OFFLINE ? "OFFLINE" : "UNKNOWN"));
			};
		network.AddStatusCallback(status_callback);
		if (!network.Start()) throw std::runtime_error(serial ? "Cannot open serial port; check COM name, availability and line settings" : "Cannot bind UDP socket; check local IP/port");
		if (!config_path.empty()) emit("Config loaded: " + config_path);
		emit(ConsoleTransportDescription(config, network.GetLocalPort()));
		for (const auto& slave : slaves)
			emit("Slave " + std::to_string(slave.id) + " -> " + ConsoleSlaveDestination(config, slave.endpoint));
		std::map<uint8_t, bool> completed;
		auto recall = [&](uint8_t id)
		{
			if (recalls.Queue(id)) { completed[id] = false; emit("Recall queued: " + std::to_string(id)); return true; }
			emit("Recall refused: slave " + std::to_string(id) + " is unknown", true);
			return false;
		};
		auto find_slave = [&](uint8_t id) -> Slave&
		{
			for (auto& slave : slaves) if (slave.id == id) return slave;
			throw std::runtime_error("Unknown slave ID " + std::to_string(id));
		};
		auto send_control = [&](Slave& slave, bool full)
		{
			if (slave.controls.Send(network, slave.id, full))
			{
				emit("Control sent: slave " + std::to_string(slave.id) + (full ? " (FULL frame)" : " (changed bytes/mode)"));
				return true;
			}
			emit("Control NOT sent: slave " + std::to_string(slave.id) + " (busy, no changes/mode, or send failed)", true);
			return false;
		};
		if (recall_all)
		{
			for (const auto& slave : slaves) if (!recall(slave.id)) return 1;
		}
		else if (!run_ms && plain) std::cout << "Commands: r/show/controls <id>, setbyte, setbit, mode, send/sendall <id>, status, help, q\n> ";

		auto show_status = [&]()
		{
			for (const auto& slave : slaves)
			{
				LSY::NetworkGenisysMaster::SlaveStatus status;
				if (!network.GetSlaveStatus(slave.id, status)) continue;
				std::cout << "Slave " << unsigned(slave.id) << " " <<
					(status.communication == LSY::NetworkGenisysMaster::COMMUNICATIONSTATUS::ONLINE ? "ONLINE" :
					status.communication == LSY::NetworkGenisysMaster::COMMUNICATIONSTATUS::OFFLINE ? "OFFLINE" : "UNKNOWN") <<
					" busy=" << status.busy << " indications-current=" << status.indications_current <<
					" retries=" << status.retries << " completed=" << status.completed_count << " timeouts=" << status.timeout_count << '\n';
			}
		};
		const auto started = std::chrono::steady_clock::now();
		std::string command;
		auto next_render = std::chrono::steady_clock::now();
		for (;;)
		{
			bool transport_failed = false;
			if (!network.Process(!recalls.Pending()))
			{
				emit(serial ? "Serial processing reported an error or rejected input" : "UDP processing reported an error or rejected datagram", true);
				transport_failed = serial && !network.IsRunning();
			}
			for (const auto& request : transport_failed ? std::vector<ConsoleRecalls::Result>{} : recalls.Process(network))
			{
				emit(std::string(request.sent ? "Recall sent: " : "Recall send failed: ") + std::to_string(request.id), !request.sent);
				if (!request.sent && recall_all) return 1;
			}
			for (const auto& response : network.GetResponses()) find_slave(response.slave_id).controls.OnResponse(response);
			for (const auto& event : network.GetTransactionEvents())
			{
				using Event = LSY::NetworkGenisysMaster::TRANSACTIONEVENT;
				auto& controls = find_slave(event.slave_id).controls;
				controls.OnTransaction(event);
				if (event.control_request && event.type != Event::RETRY)
					emit("Slave " + std::to_string(event.slave_id) + " control: " + controls.Result(), event.type != Event::COMPLETE);
				if (event.type == Event::COMPLETE) continue;
				emit("Slave " + std::to_string(event.slave_id) + " " +
					(event.type == Event::RETRY ? "RETRY" : event.type == Event::TIMEOUT ? "TIMEOUT" : "SEND FAILED") +
					" retries=" + std::to_string(event.retries), event.type != Event::RETRY);
			}
			for (const auto& response : network.GetResponses())
			{
				if (plain) std::cout << "Slave " << unsigned(response.slave_id) << (response.type == LSY::ProtocolGenisysMaster::RESPONSETYPE::INDICATION_DATA ? " F2 received; FA generated\n" : " F1 received; exchange complete\n");
				if (response.transaction_complete) completed[response.slave_id] = true;
				if (plain && response.type == LSY::ProtocolGenisysMaster::RESPONSETYPE::INDICATION_DATA)
					for (const auto& slave : slaves) if (slave.id == response.slave_id) Show(slave);
			}
			// Report pending control outcomes before leaving a failed serial session.
			if (transport_failed) { emit("Serial port closed after an I/O error; restart the console to reconnect", true); return 1; }
			if (dashboard.Active())
			{
				for (const auto& action : dashboard.ReadActions(slaves))
				{
					using Action = ConsoleDashboard::Action;
					if (action.action == Action::QUIT) return 0;
					auto& slave = find_slave(action.slave_id);
					if (action.action == Action::RECALL_SELECTED) recall(action.slave_id);
					if (action.action == Action::RECALL_ALL) for (const auto& item : slaves) recall(item.id);
					if (action.action == Action::TOGGLE_BIT)
					{
						bool value = false; slave.controls.frame->ReadBit(action.offset, action.bit, value);
						slave.controls.frame->WriteBit(action.offset, action.bit, !value);
					}
					if (action.action == Action::SET_BYTE) slave.controls.frame->WriteByte(action.offset, action.value);
					if (action.action == Action::SET_MODE) { slave.controls.include_mode = action.include_mode; slave.controls.mode = action.value; }
					if (action.action == Action::SEND_CONTROL) send_control(slave, false);
					if (action.action == Action::SEND_FULL) send_control(slave, true);
				}
				const auto now = std::chrono::steady_clock::now();
				if (now >= next_render)
				{
					if (!dashboard.Render(slaves, network, config)) throw std::runtime_error("Cannot refresh dashboard console screen");
					next_render = now + std::chrono::milliseconds(200);
				}
			}
			if (recall_all)
			{
				bool all_complete = true;
				for (const auto& slave : slaves) if (!completed[slave.id]) all_complete = false;
				if (all_complete) return 0;
				if (std::chrono::steady_clock::now() - started >= std::chrono::milliseconds(wait_ms))
				{
					emit("Recall wait expired before all exchanges completed", true);
					return 1;
				}
			}
			else if (run_ms)
			{
				if (std::chrono::steady_clock::now() - started >= std::chrono::milliseconds(run_ms))
				{
					if (plain) show_status();
					for (const auto& slave : slaves)
					{
						LSY::NetworkGenisysMaster::SlaveStatus status;
						if (!network.GetSlaveStatus(slave.id, status) || !status.indications_current ||
							status.communication != LSY::NetworkGenisysMaster::COMMUNICATIONSTATUS::ONLINE) return 1;
					}
					return 0;
				}
			}
			else if (plain) while (_kbhit())
			{
				const int ch = _getch();
				if (ch == 0 || ch == 224) { _getch(); continue; }
				if (ch == 3) return 0;
				if (ch == '\b') { if (!command.empty()) { command.pop_back(); std::cout << "\b \b"; } continue; }
				if (ch != '\r') { if (ch >= 32 && ch < 127) { command += static_cast<char>(ch); std::cout << static_cast<char>(ch); } continue; }
				std::cout << '\n';
				if (command == "q") return 0;
				try
				{
					std::istringstream input(command); std::vector<std::string> words;
					for (std::string word; input >> word;) words.push_back(word);
					if (command == "status") show_status();
					else if (command == "help") std::cout <<
						"r <id> / r all | show <id> | controls <id> | setbyte <id> <offset> <value>\n"
						"setbit <id> <offset> <bit> <0|1> | mode <id> <value|off> | send <id> | sendall <id> | q\n"
						"Values are decimal or 0x hex. Edits are local; send transmits changed bytes, sendall transmits the entire frame.\n";
					else if (command == "r all") { for (const auto& slave : slaves) recall(slave.id); }
					else if (!words.empty())
					{
						if (words.size() < 2) throw std::runtime_error("Unknown command; type help");
						auto& slave = find_slave(static_cast<uint8_t>(Number(words[1], 1, 255)));
						const auto& action = words[0];
						if (words.size() == 2 && action == "r") recall(slave.id);
						else if (words.size() == 2 && action == "show") Show(slave);
						else if (words.size() == 2 && action == "controls") Show(slave, true);
						else if (words.size() == 2 && (action == "send" || action == "sendall")) send_control(slave, action == "sendall");
						else if (words.size() == 3 && action == "mode")
						{
							const bool include = words[2] != "off";
							const auto value = include ? static_cast<uint8_t>(Number(words[2], 0, 255)) : uint8_t(0);
							slave.controls.include_mode = include; slave.controls.mode = value;
							emit("Mode staged locally for slave " + std::to_string(slave.id));
						}
						else if ((words.size() == 4 && action == "setbyte") || (words.size() == 5 && action == "setbit"))
						{
							const unsigned offset = Number(words[2], 0, static_cast<unsigned>(slave.controls.frame->GetNumBytes() - 1));
							if (action == "setbyte") slave.controls.frame->WriteByte(offset, static_cast<uint8_t>(Number(words[3], 0, 255)));
							else
							{
								const auto bit = static_cast<uint8_t>(Number(words[3], 0, 7));
								const bool value = Number(words[4], 0, 1) != 0;
								slave.controls.frame->WriteBit(offset, bit, value);
							}
							emit("Control staged locally: slave " + std::to_string(slave.id) + " byte " + std::to_string(offset));
						}
						else throw std::runtime_error("Unknown command or argument count; type help");
					}
				}
				catch (const std::exception& error) { std::cerr << error.what() << '\n'; }
				command.clear(); std::cout << "> ";
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
	}
	catch (const std::exception& error) { std::cerr << error.what() << "\nUse --help for usage.\n"; return 1; }
}
