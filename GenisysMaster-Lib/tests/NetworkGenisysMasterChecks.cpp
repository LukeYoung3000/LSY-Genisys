#include "NetworkGenisysMaster.h"
#include "ProtocolGenisysSlave.h"

#include <Ws2tcpip.h>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

using Bytes = std::vector<uint8_t>;
void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }

#include "UdpTestPeer.h"

int main()
{
	WSADATA data{};
	if (WSAStartup(MAKEWORD(2, 2), &data)) return 1;
	int result = 0;
	try
	{
		Peer shared, other;
		shared.Add(3, 0x31); shared.Add(4, 0x42); other.Add(5, 0x53);
		auto protocol = std::make_shared<LSY::ProtocolGenisysMaster>();
		std::map<uint8_t, std::shared_ptr<LSY::MasterDataFrame>> frames;
		int callbacks = 0;
		for (uint8_t id : Bytes{3, 4, 5})
		{
			auto frame = std::make_shared<LSY::MasterDataFrame>("Master indications", 8);
			std::function<void(std::string, LSY::MasterDataFrame::CALLBACKEVENT)> callback =
				[&](std::string, LSY::MasterDataFrame::CALLBACKEVENT event) { if (event == LSY::MasterDataFrame::CALLBACKEVENT::INDICATION_CHANGE) ++callbacks; };
			frame->AddEventCallback(callback);
			Check(protocol->AddDataFrame(id, false, frame), "Register master frame"); frames[id] = frame;
		}
		LSY::NetworkGenisysMaster::Config config; config.local_ip_ = "127.0.0.1"; config.receive_port_ = 0; config.automatic_polling_ = false;
		LSY::NetworkGenisysMaster master(config);
		Check(master.AddProtocol(protocol), "Map protocol to UDP socket");
		Check(master.AddSlave(3, {"127.0.0.1", shared.port}) && master.AddSlave(4, {"127.0.0.1", shared.port}) &&
			master.AddSlave(5, {"127.0.0.1", other.port}), "Map several IDs including shared endpoint");
		Check(!master.AddSlave(3, {"127.0.0.1", other.port}), "Reject duplicate ID");
		Check(master.Start() && master.GetLocalPort() != 0 && master.Process(), "Start and idle without blocking");
		const auto master_port = master.GetLocalPort();
		Check(!master.AddSlave(6, {"127.0.0.1", other.port}), "Freeze mappings while started");
		LSY::NetworkGenisysMaster::Config occupied = config; occupied.receive_port_ = master_port;
		LSY::NetworkGenisysMaster second(occupied);
		auto second_protocol = std::make_shared<LSY::ProtocolGenisysMaster>();
		auto second_frame = std::make_shared<LSY::MasterDataFrame>("Other", 8);
		second_protocol->AddDataFrame(3, false, second_frame);
		Check(second.AddProtocol(second_protocol) && second.AddSlave(3, {"127.0.0.1", shared.port}) && !second.Start(), "Reject occupied receive port");

		Check(master.RequestRecall(3) && master.RequestRecall(4) && master.RequestRecall(5), "Concurrent manual recalls");
		Check(!master.RequestRecall(3) && !master.RequestRecall(6), "Reject busy/unknown recalls");
		// Valid GE full-indication response from the wrong configured endpoint.
		const Bytes response{0xF2, 3, 0, 0x16, 1, 0x3E, 2, 8, 3, 0x50, 0xE0, 1, 0x25, 0x9B, 0xF6};
		other.Send(response, master_port);
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		Check(!master.Process() && (*frames[3])[0] == 0 && callbacks == 0, "Wrong endpoint cannot update another slave ID");
		// UDP fragments must not combine, even when a different endpoint intervenes.
		shared.Send(Bytes(response.begin(), response.begin() + 6), master_port);
		shared.Send(Bytes(response.begin() + 6, response.end()), master_port);
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		Check(!master.Process() && (*frames[3])[0] == 0, "Reject partial UDP datagrams without changing frames");

		std::map<uint8_t, bool> completed;
		const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
		while (completed.size() < 3 && std::chrono::steady_clock::now() < deadline)
		{
			shared.Process(master_port); other.Process(master_port);
			Check(master.Process(), "Process valid UDP responses");
			for (const auto& event : master.GetResponses()) if (event.transaction_complete) completed[event.slave_id] = true;
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		Check(completed.size() == 3 && callbacks == 3 && (*frames[3])[0] == 0x31 && (*frames[4])[0] == 0x42 &&
			(*frames[5])[0] == 0x53, "All shared/separate endpoint recalls and FA/F1 exchanges complete");
		Check(master.RequestPoll(4), "Manual poll after recall completion");
		bool poll_complete = false;
		const auto poll_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
		while (!poll_complete && std::chrono::steady_clock::now() < poll_deadline)
		{
			shared.Process(master_port); Check(master.Process(), "Manual poll receive");
			for (const auto& event : master.GetResponses()) if (event.slave_id == 4 && event.transaction_complete) poll_complete = true;
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		Check(poll_complete && callbacks == 3, "Unchanged poll has no duplicate indication callbacks");
		Check(master.RequestRecall(3) && master.Shutdown() && master.Shutdown(), "Shutdown is idempotent with a pending request");
		Check(master.Start() && master.RequestRecall(3), "Restart clears abandoned request state");
		master.Shutdown();
		std::cout << "PASS: UDP recalls against existing slave protocol; multiple IDs share one endpoint\n"
			"PASS: endpoint routing, source checks, datagram isolation and socket lifecycle\n"
			"All NetworkGenisysMaster checks passed.\n";
	}
	catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; result = 1; }
	WSACleanup();
	return result;
}
