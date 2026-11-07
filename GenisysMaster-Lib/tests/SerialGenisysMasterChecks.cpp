#include "NetworkGenisysMaster.h"
#include "../src/MasterTransport.h"
#include "ProtocolGenisysSlave.h"
#include <Windows.h>
#include <deque>
#include <iostream>
#include <thread>
#include <stdexcept>
#include <functional>
#include "../../GenisysMaster-Console/ConsoleRecalls.h"
using Network = LSY::NetworkGenisysMaster;
using Bytes = std::vector<uint8_t>;
void Check(bool value, const char* text) { if (!value) throw std::runtime_error(text); }
namespace LSY
{
	struct NetworkGenisysMasterTestAccess
	{
		static std::unique_ptr<Network> Create(Network::Config config, std::unique_ptr<MasterTransport> io)
		{ return std::unique_ptr<Network>(new Network(config, std::move(io))); }
	};
}
#include "SerialTestPeer.h"
struct Fixture
{
	StreamPeer* peer = nullptr;
	std::unique_ptr<Network> master;
	std::shared_ptr<LSY::ProtocolGenisysMaster> protocol = std::make_shared<LSY::ProtocolGenisysMaster>();
	std::map<uint8_t, std::shared_ptr<LSY::MasterDataFrame>> frames, controls;
	std::vector<Network::TransactionEvent> events;
	explicit Fixture(bool automatic = false, unsigned retries = 1)
	{
		auto stream = std::make_unique<StreamPeer>(); peer = stream.get();
		Network::Config config; config.connection_type_ = Network::Config::CONNECTIONTYPE::SERIAL;
		config.automatic_polling_ = automatic; config.poll_interval_ms_ = 1; config.response_timeout_ms_ = 15;
		config.recovery_interval_ms_ = 20; config.retry_count_ = retries;
		master = LSY::NetworkGenisysMasterTestAccess::Create(config, std::move(stream));
		for (uint8_t id : std::vector<uint8_t>{3, 4, 242})
		{
			peer->Add(id, id);
			frames[id] = std::make_shared<LSY::MasterDataFrame>("Master indications", 8);
			controls[id] = std::make_shared<LSY::MasterDataFrame>("Master controls", 8);
			Check(protocol->AddDataFrame(id, false, frames[id]) && protocol->AddDataFrame(id, true, controls[id]), "Register master frames");
			Check(master->AddSlave(id), "Register ID without endpoint");
		}
		Check(master->AddProtocol(protocol) && master->Start(), "Start serial fixture");
		Check(master->GetLocalPort() == 0, "Serial has no UDP port");
	}
	Network::SlaveStatus Status(uint8_t id)
	{ Network::SlaveStatus status; Check(master->GetSlaveStatus(id, status), "Read slave status"); return status; }
	void Step()
	{ master->Process(); const auto& batch = master->GetTransactionEvents(); events.insert(events.end(), batch.begin(), batch.end()); }
	void Until(const std::function<bool()>& condition)
	{
		const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(3);
		while (!condition() && std::chrono::steady_clock::now() < end) { Step(); std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
		Check(condition(), "Serial exchange did not reach expected outcome");
	}
};
void CheckNativePair(const std::string& masterPort, const std::string& slavePort);
int main(int argc, char** argv)
{
	try
	{
		{
			Fixture f;
			Check(f.master->RequestRecall(3), "Manual serial recall");
			Check(!f.master->RequestRecall(4) && !f.master->RequestControl(4, true), "Shared serial wire rejects overlapping exchanges before building protocol requests");
			f.Until([&] { return f.Status(3).indications_current; });
			Check((*f.frames[3])[0] == 3 && f.peer->requests[0xFA] == 1, "Byte-at-a-time F2/FA/F1 updates unmodified data frame");
			Check(f.master->RequestRecall(4), "Busy rejection leaves other slave usable");
			f.Until([&] { return f.Status(4).indications_current; });
			Check(f.master->RequestRecall(242), "Escaped slave address recall");
			f.Until([&] { return f.Status(242).indications_current; });
			Check((*f.frames[242])[0] == 242, "Escaped address and value across single-byte chunks");
		}
		std::cout << "PASS: serial registration, fragmented replies, escaped IDs, unchanged frames and exclusive bus\n";
		{
			Fixture f(true);
			f.peer->offline[4] = true;
			f.Until([&] { return f.Status(3).completed_count >= 4 && f.Status(242).completed_count >= 4 && f.Status(4).timeout_count >= 1; });
			Check(f.peer->requestOrder.size() >= 3 && f.peer->requestOrder[0] == 3 && f.peer->requestOrder[1] == 4,
				"Round-robin begins at first ID then gives the next ID a turn");
			Check(f.Status(4).communication == Network::COMMUNICATIONSTATUS::OFFLINE && !f.Status(4).indications_current, "Offline slave is stale without starving other IDs");
			f.peer->offline[4] = false;
			f.Until([&] { return f.Status(4).indications_current; });
			Check(f.peer->byId[4][0xFD] >= 3, "Recovery uses a full recall");
		}
		std::cout << "PASS: round-robin polling, independent offline status and recovery on one bus\n";
		{
			Fixture f;
			f.peer->holdWrite = true;
			Check(f.master->RequestRecall(3), "Queue asynchronous serial write");
			const auto start = std::chrono::steady_clock::now();
			for (int i = 0; i < 50; ++i) { f.Step(); std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
			Check(f.Status(3).busy && f.Status(3).timeout_count == 0 && f.peer->requests[0xFD] == 1, "Response deadline does not expire during pending write");
			Check(std::chrono::steady_clock::now() - start < std::chrono::seconds(1), "Process never waits for a pending write");
			f.peer->holdWrite = false; f.Until([&] { return f.Status(3).indications_current; });
			f.peer->corrupt = true; Check(f.master->RequestRecall(3), "Request with corrupt serial reply");
			f.Until([&] { return !f.Status(3).busy; });
			Check(f.peer->requests[0xFD] == 3 && f.Status(3).indications_current, "Corrupt CRC does not complete request; retry recovers");
		}
		std::cout << "PASS: asynchronous write deadlines, nonblocking processing and CRC/retry recovery\n";
		{
			Fixture f;
			f.controls[3]->WriteByte(0, 0x81); f.peer->offline[3] = true;
			Check(f.master->RequestControl(3), "Explicit serial control");
			f.Until([&] { return f.Status(3).timeout_count == 1; });
			Check(f.peer->requests[0xFC] == 1 && !f.events.empty() && f.events.back().control_request && f.events.back().type == Network::TRANSACTIONEVENT::TIMEOUT,
				"Unanswered FC is never automatically retransmitted");
			f.peer->offline[3] = false; f.peer->dropF1 = true; f.controls[3]->WriteByte(0, 0x82);
			Check(f.master->RequestControl(3), "Explicit new control after timeout");
			bool acknowledged = false;
			f.Until([&] {
				for (const auto& response : f.master->GetResponses()) acknowledged = acknowledged || response.control_acknowledged;
				return f.Status(3).timeout_count == 2;
			});
			Check(acknowledged && (*f.peer->controls[3])[0] == 0x82 && f.peer->requests[0xFC] == 2 && f.peer->requests[0xFA] == 2,
				"Only FA retries after F2 acknowledges a control");
		}
		std::cout << "PASS: explicit controls, acknowledgement semantics, FC sent once and bounded FA retries\n";
		{
			Fixture f(false, 0);
			f.peer->offline[3] = true; Check(f.master->RequestRecall(3), "Recall before partial-response timeout");
			f.Step(); f.peer->incoming.push_back({0xF2, 3, 0}); f.Step();
			f.Until([&] { return f.Status(3).timeout_count == 1; });
			f.peer->offline[3] = false;
			Check(f.master->RequestRecall(4), "Next slave after partial response");
			// Garbage and an unsolicited F1 for a different ID must not satisfy the exchange.
			f.peer->incoming.push_back({0x11, 0x22, 0xF1, 3, 0xF6});
			f.Until([&] { return f.Status(4).indications_current; });
			Check((*f.frames[3])[0] == 0 && (*f.frames[4])[0] == 4, "Partial/noisy/wrong-ID input cannot contaminate the next slave");
			f.peer->fragment = 4096;
			Check(f.master->RequestRecall(242), "Recall before oversized noise"); f.Step();
			f.peer->incoming.push_front(Bytes(2000, 0x01)); f.peer->incoming.front()[0] = 0xF2;
			f.Until([&] { return f.Status(242).indications_current; });
			f.peer->failRead = true;
			Check(!f.master->Process() && f.Status(4).communication == Network::COMMUNICATIONSTATUS::OFFLINE && !f.Status(4).indications_current,
				"Port errors close transport and mark cached data stale for every ID");
			f.peer->failRead = false; Check(f.master->Start(), "Explicit restart after port failure");
			Check(f.master->RequestControl(3, true), "Control before shutdown");
			Check(f.master->Shutdown() && f.master->Shutdown(), "Idempotent shutdown with asynchronous write pending");
			Check(f.master->Start() && f.master->RequestRecall(3), "Shutdown cancels protocol state for restart");
			f.Until([&] { return f.Status(3).indications_current; });
		}
		std::cout << "PASS: stale fragments, noise, wrong IDs, bounded framing, port failure and restart\n";
		{
			Network::Config config; config.connection_type_ = Network::Config::CONNECTIONTYPE::SERIAL;
			config.com_port_ = "NUL";
			Check(!LSY::CreateMasterTransport(config)->Start(), "Reject non-COM device path");
			config.com_port_ = "COM0"; Check(!LSY::CreateMasterTransport(config)->Start(), "Reject invalid COM number");
			config.com_port_ = "COM999999999"; config.data_bits_ = 7;
			Check(!LSY::CreateMasterTransport(config)->Start(), "Reject lossy 7-bit serial configuration");
			config.data_bits_ = 8;
			auto io = LSY::CreateMasterTransport(config);
			Check(!io->Start() && !io->IsOpen(), "Unavailable COM port fails cleanly"); io->Stop(); io->Stop();
			config.connection_type_ = static_cast<Network::Config::CONNECTIONTYPE>(99);
			Network invalid(config); Check(!invalid.Start(), "Unknown transport kind fails safely");
		}
		{
			Fixture f(true);
			f.Step(); // Already-running automatic exchange must finish first.
			ConsoleRecalls recalls({3, 4, 242}, true);
			recalls.QueueAll(); Check(recalls.Queue(4) && !recalls.Queue(9), "Queued recalls deduplicate and reject unknown IDs");
			std::vector<uint8_t> sent;
			const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
			while ((recalls.Pending() || !f.Status(242).indications_current) && std::chrono::steady_clock::now() < deadline)
			{
				f.master->Process(!recalls.Pending());
				for (const auto& request : recalls.Process(*f.master)) { Check(request.sent, "Queued serial recall starts when bus is idle"); sent.push_back(request.id); }
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
			Check(sent == std::vector<uint8_t>({3, 4, 242}) && f.Status(242).indications_current && f.peer->requestOrder.size() >= 4 &&
				std::vector<uint8_t>(f.peer->requestOrder.begin(), f.peer->requestOrder.begin() + 4) == std::vector<uint8_t>({3, 3, 4, 242}),
				"Recall-all serializes every ID and defers new automatic polls without starving manual recalls");
		}
		std::cout << "PASS: native COM validation and console recall-all priority/serialization\nAll SerialGenisysMaster checks passed.\n";
		if (argc == 3) CheckNativePair(argv[1], argv[2]);
		else if (argc != 1) throw std::runtime_error("Usage: SerialGenisysMasterChecks.exe [master-COM slave-COM]");
		else std::cout << "SKIP: native serial loopback (supply two connected COM ports to exercise the driver)\n";
		return 0;
	}
	catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}


// Optional native test: use a preconfigured virtual null-modem pair or two
// physically connected ports. No drivers are installed and no ports are guessed.
void CheckNativePair(const std::string& masterPort, const std::string& slavePort)
{
	Network::Config config; config.connection_type_ = Network::Config::CONNECTIONTYPE::SERIAL;
	config.com_port_ = slavePort; config.response_timeout_ms_ = 1000; config.write_timeout_ms_ = 2000;
	config.automatic_polling_ = false;
	auto peer = LSY::CreateMasterTransport(config);
	Check(peer->Start(), "Open the supplied native slave COM port");
	auto duplicate = LSY::CreateMasterTransport(config); Check(!duplicate->Start(), "Native COM port is exclusive");
	config.com_port_ = masterPort; Network master(config);
	auto protocol = std::make_shared<LSY::ProtocolGenisysMaster>(); LSY::ProtocolGenisysSlave slave;
	std::map<uint8_t, std::shared_ptr<LSY::MasterDataFrame>> indications, controls;
	std::map<uint8_t, std::shared_ptr<LSY::DataFrame>> peerControls;
	for (uint8_t id : std::vector<uint8_t>{3, 242})
	{
		auto input = std::make_shared<LSY::DataFrame>("Native slave", 8);
		auto output = std::make_shared<LSY::DataFrame>("Native slave controls", 8);
		input->WriteByte(0, 0xF6); peerControls[id] = output;
		Check(slave.AddDataFrame(id, false, input) && slave.AddDataFrame(id, true, output), "Native slave frames");
		indications[id] = std::make_shared<LSY::MasterDataFrame>("Native indications", 8);
		controls[id] = std::make_shared<LSY::MasterDataFrame>("Native controls", 8);
		Check(protocol->AddDataFrame(id, false, indications[id]) && protocol->AddDataFrame(id, true, controls[id]) && master.AddSlave(id), "Native master frames");
	}
	Check(master.AddProtocol(protocol) && master.Start(), "Open the supplied native master COM port");
	std::vector<uint8_t> inputFrame;
	std::deque<Bytes> outputChunks;
	auto status = [&](uint8_t id) { Network::SlaveStatus state; Check(master.GetSlaveStatus(id, state), "Native status"); return state; };
	auto until = [&](const std::function<bool()>& ready)
	{
		const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(8);
		while (!ready() && std::chrono::steady_clock::now() < deadline)
		{
			Check(master.Process(), "Native master processing"); Check(peer->PollWrite(), "Native peer write completion");
			Bytes chunk; sockaddr_in unused{};
			const auto result = peer->Receive(chunk, unused);
			Check(result != LSY::MasterTransport::READ::FAILED, "Native peer receive");
			for (auto byte : chunk)
			{
				inputFrame.push_back(byte);
				if (byte != 0xF6) continue;
				Check(slave.ProcessMessages(inputFrame, "SERIAL:NATIVE"), "Unchanged slave protocol over native serial");
				// Send every response one byte at a time through the real driver.
				for (auto reply : inputFrame) outputChunks.push_back({reply});
				inputFrame.clear();
			}
			if (!peer->WritePending() && !outputChunks.empty())
			{
				Check(peer->Send(unused, outputChunks.front()), "Native fragmented peer write"); outputChunks.pop_front();
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		Check(ready(), "Native serial exchange did not complete; verify COM pairing and wiring");
	};
	for (uint8_t id : std::vector<uint8_t>{3, 242})
	{
		Check(master.RequestRecall(id), "Native recall"); until([&] { return status(id).indications_current; });
		Check((*indications[id])[0] == 0xF6, "Native escaped indication");
	}
	controls[242]->WriteByte(0, 0xF1); controls[242]->WriteByte(1, 0xF2); controls[242]->WriteByte(2, 0xF6);
	const auto completed = status(242).completed_count;
	Check(master.RequestControl(242, true, true, 5), "Native full control and mode");
	until([&] { return status(242).completed_count > completed; });
	Check((*peerControls[242])[0] == 0xF1 && (*peerControls[242])[1] == 0xF2 && (*peerControls[242])[2] == 0xF6, "Native controls reach the slave unchanged");
	Check(master.RequestRecall(3), "Native pending write before shutdown");
	Check(master.Shutdown() && master.Start(), "Native cancellation, resource release and reopen");
	master.Shutdown(); peer->Stop();
	std::cout << "PASS: native connected COM ports, exclusive access, byte-fragmented indications, controls and pending-I/O shutdown\n";
}
