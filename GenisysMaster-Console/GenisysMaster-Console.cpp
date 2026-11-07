#include "ConsoleDashboard.h"
#include "ConsoleConfiguration.h"
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
		ConsoleConfiguration settings;
		std::string config_path;
		bool help_requested = false;
		// Load XML first, so argument order never changes CLI precedence.
		for (int i = 1; i < argc; ++i)
		{
			const std::string arg = argv[i];
			if (arg == "--help") { help_requested = true; continue; }
			if (arg == "--config")
			{
				if (!config_path.empty()) throw std::runtime_error("Use --config only once");
				if (i + 1 >= argc) throw std::runtime_error("Missing XML file after --config");
				config_path = argv[++i];
				if (config_path.empty()) throw std::runtime_error("Empty --config path");
			}
			else if (arg == "--slave") i += 4;
			else if (arg != "--plain" && arg != "--manual" && arg != "--recall-all" && i + 1 < argc) ++i;
		}
		if (!config_path.empty() && !help_requested) LoadConsoleConfiguration(config_path, settings);
		auto& config = settings.network;
		std::vector<Slave> slaves;
		for (const auto& item : settings.slaves)
			slaves.push_back({item.id, item.endpoint, std::make_shared<LSY::MasterDataFrame>("Indications " + std::to_string(item.id), item.frame_bytes)});
		bool& recall_all = settings.recall_all;
		bool& plain = settings.plain;
		unsigned& wait_ms = settings.wait_ms;
		unsigned& run_ms = settings.run_ms;
		bool cli_slaves = false;
		for (int i = 1; i < argc; ++i)
		{
			const std::string arg = argv[i];
			if (arg == "--help")
			{
				std::cout << "GenisysMaster-Console [--config file.xml] [--local-ip IPv4] [--local-port port]\n"
					"  [--slave id IPv4 port frame-bytes]... [--recall-all] [--wait-ms milliseconds]\n"
					"  [--plain] [--manual] [--poll-ms n] [--timeout-ms n] [--retries n] [--recovery-ms n] [--run-ms n]\n"
					"XML is loaded first; CLI flags override it; --slave replaces the XML slave list.\n"
					"Defaults: local 0.0.0.0:7170, slave 34 at 127.0.0.1:7169, 125 bytes.\n"
					"IDs/ports/size accept decimal or 0x hex. --recall-all runs once then exits.\n"
					"Default: fixed dashboard. --plain enables scrolling text (also used for redirected I/O).\n"
					"Dashboard: C controls, J/K byte, Left/Right bit, Space toggle, E byte, M mode, S send, F full.\n"
					"Up/Down select slave, PgUp/PgDn bytes, R recall, A recall all, H help, Q quit.\n"
					"Plain: r <id>, r all, show <id>, controls <id>, setbyte <id> <offset> <value>\n"
					"  setbit <id> <offset> <bit> <0|1>, mode <id> <value|off>, send <id>, sendall <id>, status, q.\n"
					"Edits are local until explicitly sent. Controls are not automatically retried.\n";
				return 0;
			}
			if (arg == "--config") { if (i + 1 >= argc) throw std::runtime_error("Missing XML file after --config"); ++i; continue; }
			if (arg == "--plain") { plain = true; continue; }
			if (arg == "--recall-all") { recall_all = true; continue; }
			if (arg == "--manual") { config.automatic_polling_ = false; continue; }
			if (arg == "--slave" && i + 4 < argc)
			{
				if (!cli_slaves) { slaves.clear(); cli_slaves = true; }
				Slave slave{};
				slave.id = static_cast<uint8_t>(Number(argv[++i], 1, 255));
				slave.endpoint.ip_address_ = argv[++i];
				slave.endpoint.port_ = static_cast<uint16_t>(Number(argv[++i], 1, 65535));
				const unsigned bytes = Number(argv[++i], 1, 224);
				slave.indications = std::make_shared<LSY::MasterDataFrame>("Indications " + std::to_string(slave.id), bytes);
				slaves.push_back(slave);
				continue;
			}
			if (i + 1 >= argc) throw std::runtime_error("Missing argument for " + arg);
			if (arg == "--local-ip") config.local_ip_ = argv[++i];
			else if (arg == "--local-port") config.receive_port_ = static_cast<uint16_t>(Number(argv[++i], 0, 65535));
			else if (arg == "--wait-ms") wait_ms = Number(argv[++i], 1, 600000);
			else if (arg == "--poll-ms") config.poll_interval_ms_ = Number(argv[++i], 1, 600000);
			else if (arg == "--timeout-ms") config.response_timeout_ms_ = Number(argv[++i], 1, 600000);
			else if (arg == "--retries") config.retry_count_ = Number(argv[++i], 0, 100);
			else if (arg == "--recovery-ms") config.recovery_interval_ms_ = Number(argv[++i], 1, 600000);
			else if (arg == "--run-ms") run_ms = Number(argv[++i], 0, 600000);
			else throw std::runtime_error("Unknown argument: " + arg);
		}
		if (slaves.empty())
			slaves.push_back({34, {"127.0.0.1", 7169}, std::make_shared<LSY::MasterDataFrame>("Indications 34", 125)});

		if (recall_all) config.automatic_polling_ = false;
		if (recall_all && run_ms) throw std::runtime_error("Use either --recall-all or --run-ms");
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
				!protocol->AddDataFrame(slave.id, true, slave.controls.frame) || !network.AddSlave(slave.id, slave.endpoint))
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
		if (!network.Start()) throw std::runtime_error("Cannot bind UDP socket; check local IP/port");
		if (!config_path.empty()) emit("Config loaded: " + config_path);
		emit("UDP bound to " + config.local_ip_ + ":" + std::to_string(network.GetLocalPort()));
		for (const auto& slave : slaves)
			emit("Slave " + std::to_string(slave.id) + " -> " + slave.endpoint.ip_address_ + ":" + std::to_string(slave.endpoint.port_));
		std::map<uint8_t, bool> completed;
		auto recall = [&](uint8_t id)
		{
			if (network.RequestRecall(id)) { completed[id] = false; emit("Recall sent: " + std::to_string(id)); return true; }
			emit("Recall refused: slave " + std::to_string(id) + " is unknown, busy, or send failed", true);
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
			if (!network.Process()) emit("UDP processing reported an error or rejected datagram", true);
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
