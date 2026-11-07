#include "ConsoleControls.h"
#include "ProtocolGenisysSlave.h"
#include <Ws2tcpip.h>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

using Bytes = std::vector<uint8_t>;
using Network = LSY::NetworkGenisysMaster;
void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
#include "UdpTestPeer.h"

struct Fixture
{
	Peer peer;
	ConsoleControls controls;
	std::shared_ptr<LSY::ProtocolGenisysMaster> protocol = std::make_shared<LSY::ProtocolGenisysMaster>();
	std::unique_ptr<Network> master;
	Fixture()
	{
		peer.Add(3, 0x31); controls.Initialize(3, 8);
		auto indications = std::make_shared<LSY::MasterDataFrame>("Indications", 8);
		Check(protocol->AddDataFrame(3, false, indications) && protocol->AddDataFrame(3, true, controls.frame), "Register console frames");
		Network::Config config; config.local_ip_ = "127.0.0.1"; config.receive_port_ = 0;
		config.automatic_polling_ = false; config.response_timeout_ms_ = 20; config.retry_count_ = 2;
		master = std::make_unique<Network>(config);
		Check(master->AddProtocol(protocol) && master->AddSlave(3, {"127.0.0.1", peer.port}) && master->Start(), "Start console control fixture");
	}
	void Until(const std::function<bool()>& condition)
	{
		const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
		while (!condition() && std::chrono::steady_clock::now() < deadline)
		{
			peer.Process(master->GetLocalPort()); Check(master->Process(), "Process control exchange");
			for (const auto& response : master->GetResponses()) controls.OnResponse(response);
			for (const auto& event : master->GetTransactionEvents()) controls.OnTransaction(event);
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		Check(condition(), "Console control outcome not reached");
	}
};

int main()
{
	WSADATA data{}; if (WSAStartup(MAKEWORD(2, 2), &data)) return 1;
	int result = 0;
	try
	{
		{
			Fixture f;
			f.controls.frame->WriteByte(0, 0x81); f.controls.frame->WriteBit(1, 3, true);
			Check(f.controls.Dirty() && f.peer.requests[0xFC] == 0, "Staging edits never transmits controls");
			Check(f.controls.Send(*f.master, 3, false), "Send changed controls");
			f.controls.frame->WriteByte(0, 0x82); // Edit while the previous snapshot is in flight.
			f.Until([&] { return f.controls.Result() == "ACKNOWLEDGED"; });
			Check((*f.peer.control_frames[3])[0] == 0x81 && (*f.peer.control_frames[3])[1] == 8 &&
				f.peer.control_callbacks[3] == 1 && f.controls.Dirty(), "Slave callback sees sent snapshot while newer draft remains modified");
			Check(f.controls.Send(*f.master, 3, false), "Explicitly send newer control draft");
			f.Until([&] { return f.controls.Result() == "ACKNOWLEDGED"; });
			Check((*f.peer.control_frames[3])[0] == 0x82 && f.peer.control_callbacks[3] == 2 && !f.controls.Dirty(), "Second explicit send applies the newer draft");
			Check(!f.controls.Send(*f.master, 3, false), "No-change draft does not transmit");
		}
		{
			Fixture f;
			Check(f.controls.Send(*f.master, 3, true), "Full frame can explicitly send initial zeros");
			f.Until([&] { return f.controls.Result() == "ACKNOWLEDGED"; });
			Check(f.peer.last_controls[3].size() >= 21 && f.peer.control_callbacks[3] == 1, "Full zero frame reaches slave callback");
			f.controls.include_mode = true; f.controls.mode = 5;
			Check(f.controls.Dirty() && f.controls.Send(*f.master, 3, false), "Mode-only control is explicit");
			f.Until([&] { return f.controls.Result() == "ACKNOWLEDGED"; });
			Check(f.peer.last_controls[3][2] == 0xE0 && f.peer.last_controls[3][3] == 5 && !f.controls.Dirty(), "Caller mode is sent and tracked separately");
		}
		{
			Fixture f;
			f.peer.drop_indication_replies = 1;
			f.controls.frame->WriteByte(0, 0x45);
			Check(f.controls.Send(*f.master, 3, false), "Send control whose response will be lost");
			f.Until([&] { return f.controls.Result() == "TIMEOUT - outcome unknown"; });
			Check(f.peer.requests[0xFC] == 1 && (*f.peer.control_frames[3])[0] == 0x45 && f.controls.Dirty(), "Applied but unanswered control is uncertain, never automatically resent");
			f.controls.frame->WriteByte(0, 0x46);
			Check(f.controls.Send(*f.master, 3, false), "Explicit send after timeout uses edited draft");
			f.Until([&] { return f.controls.Result() == "ACKNOWLEDGED"; });
			Check((*f.peer.control_frames[3])[0] == 0x46 && f.peer.requests[0xFC] == 2, "Canceled snapshot cannot replace the latest draft");
		}
		{
			Fixture f;
			f.peer.drop_ack_replies = 10;
			f.controls.frame->WriteByte(0, 0x33);
			Check(f.controls.Send(*f.master, 3, false), "Send control with missing final F1");
			f.Until([&] { return f.controls.Result() == "ACKNOWLEDGED; exchange failed"; });
			Check(!f.controls.Dirty() && f.peer.requests[0xFC] == 1 && f.peer.requests[0xFA] == 3,
				"Control acknowledgement survives later FA timeout; only FA is retried");
		}
		std::cout << "PASS: staged controls, explicit byte/bit/full/mode sends and slave callbacks\n"
			"PASS: newer drafts, uncertain outcomes, post-timeout edits and acknowledgement tracking\n"
			"All ConsoleControls checks passed.\n";
	}
	catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; result = 1; }
	WSACleanup(); return result;
}
