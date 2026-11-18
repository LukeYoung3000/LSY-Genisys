#include "NetworkGenisysMaster.h"
#include "ProtocolGenisysSlave.h"
#include <Ws2tcpip.h>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

using Bytes = std::vector<uint8_t>;
using Network = LSY::NetworkGenisysMaster;
using Comm = Network::COMMUNICATIONSTATUS;
using Event = Network::TRANSACTIONEVENT;
void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
#include "UdpTestPeer.h"

struct Fixture
{
	Peer peer;
	std::shared_ptr<LSY::ProtocolGenisysMaster> protocol = std::make_shared<LSY::ProtocolGenisysMaster>();
	std::shared_ptr<LSY::MasterDataFrame> indications = std::make_shared<LSY::MasterDataFrame>("Indications", 8);
	std::shared_ptr<LSY::MasterDataFrame> controls = std::make_shared<LSY::MasterDataFrame>("Controls", 8);
	std::unique_ptr<Network> master;
	std::vector<Network::TransactionEvent> events;
	std::vector<Comm> statuses;
	unsigned indication_callbacks = 0;
	unsigned rejected_calls = 0;

	Fixture(bool automatic = true, bool second_slave = false, uint32_t retries = 1)
	{
		peer.Add(3, 0x31);
		Check(protocol->AddDataFrame(3, false, indications) && protocol->AddDataFrame(3, true, controls), "Register polling frames");
		std::function<void(std::string, LSY::MasterDataFrame::CALLBACKEVENT)> callback =
			[this](std::string, LSY::MasterDataFrame::CALLBACKEVENT event)
			{ if (event == LSY::MasterDataFrame::CALLBACKEVENT::INDICATION_CHANGE) ++indication_callbacks; };
		indications->AddEventCallback(callback);
		Network::Config config; config.local_ip_ = "127.0.0.1"; config.receive_port_ = 0;
		config.automatic_polling_ = automatic; config.poll_interval_ms_ = 25;
		config.response_timeout_ms_ = 20; config.retry_count_ = retries; config.recovery_interval_ms_ = 40;
		master = std::make_unique<Network>(config);
		Check(master->AddProtocol(protocol) && master->AddSlave(3, {"127.0.0.1", peer.port}), "Map polling endpoint");
		if (second_slave)
		{
			peer.Add(4, 0x42);
			auto frame = std::make_shared<LSY::MasterDataFrame>("Second", 8);
			Check(protocol->AddDataFrame(4, false, frame) && master->AddSlave(4, {"127.0.0.1", peer.port}), "Add shared endpoint polling ID");
		}
		std::function<void(uint8_t, Comm)> status_callback = [this](uint8_t id, Comm status)
		{ if (id == 3) statuses.push_back(status); };
		master->AddStatusCallback(status_callback);
		Check(master->Start(), "Start polling master");
	}

	Network::SlaveStatus Status(uint8_t id = 3)
	{
		Network::SlaveStatus status;
		Check(master->GetSlaveStatus(id, status), "Get slave status");
		return status;
	}
	void Tick()
	{
		peer.Process(master->GetLocalPort());
		if (!master->Process()) ++rejected_calls;
		const auto& batch = master->GetTransactionEvents();
		events.insert(events.end(), batch.begin(), batch.end());
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	void Until(const std::function<bool()>& condition, const char* message)
	{
		const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
		while (!condition() && std::chrono::steady_clock::now() < deadline) Tick();
		Check(condition(), message);
	}
	unsigned Count(Event type)
	{
		unsigned count = 0; for (const auto& event : events) if (event.slave_id == 3 && event.type == type) ++count;
		return count;
	}
};

void AutomaticChecks()
{
	Fixture f(true, true);
	Check(f.Status().communication == Comm::UNKNOWN && !f.Status().indications_current, "Initial data is unknown");
	f.Until([&] { return f.Status().completed_count >= 3 && f.Status(4).completed_count >= 3; }, "Initial recalls followed by automatic polls");
	Check(f.peer.station_requests[3][0xFD] == 1 && f.peer.station_requests[3][0xFB] >= 2 &&
		f.Status().communication == Comm::ONLINE && f.Status().indications_current && f.indication_callbacks == 1,
		"Recall once then poll without duplicate indication callbacks");
	f.peer.frames[3]->WriteByte(1, 0x88);
	f.Until([&] { return (*f.indications)[1] == 0x88 && f.indication_callbacks == 2; }, "Changed indications arrive by polling");
	const auto other_completed = f.Status(4).completed_count;
	f.peer.offline_ids[3] = true;
	f.Until([&] { return f.Status().communication == Comm::OFFLINE; }, "Silent slave goes offline");
	Check(!f.Status().indications_current && (*f.indications)[1] == 0x88 && f.Status().timeout_count == 1,
		"Offline data is retained but marked stale");
	Check(f.Count(Event::RETRY) >= 1 && f.Count(Event::TIMEOUT) == 1, "Report retry and timeout");
	f.Until([&] { return f.Status(4).completed_count > other_completed + 1; }, "Other ID keeps polling on the shared socket");
	f.peer.frames[3]->WriteByte(1, 0x99); // Updated while the master was offline.
	const auto recall_count = f.peer.station_requests[3][0xFD];
	f.peer.offline_ids[3] = false;
	f.Until([&] { return f.Status().communication == Comm::ONLINE && f.Status().indications_current && (*f.indications)[1] == 0x99; }, "Recovery recalls a full frame before resuming polls");
	Check(f.peer.station_requests[3][0xFD] > recall_count && f.indication_callbacks == 3, "Recovered slave receives full recall");
	Check(f.statuses == std::vector<Comm>({Comm::ONLINE, Comm::OFFLINE, Comm::ONLINE}), "Status callback fires only on transitions");
	std::cout << "PASS: scheduled polling, callbacks, independent IDs, offline status and full-recall recovery\n";
}

void RetryChecks()
{
	{
		Fixture f(false);
		f.peer.drop_requests = 1;
		Check(f.master->RequestRecall(3), "Request recall with first request lost");
		f.Until([&] { return f.Status().completed_count == 1; }, "Retry lost recall");
		Check(f.peer.requests[0xFD] == 2 && f.Count(Event::RETRY) == 1 && f.indication_callbacks == 1, "Exactly one recall retransmission");
	}
	{
		Fixture f(false);
		f.peer.drop_ack_replies = 1;
		Check(f.master->RequestRecall(3), "Start lost FA response check");
		f.Until([&] { return f.Status().completed_count == 1; }, "Retry lost acknowledgement reply");
		Check(f.peer.requests[0xFD] == 1 && f.peer.requests[0xFA] == 2 && f.indication_callbacks == 1,
			"Retry FA rather than original recall after F2");
	}
	{
		Fixture f(false);
		f.peer.corrupt_next_reply = true;
		Check(f.master->RequestRecall(3), "Start corrupted reply check");
		f.Until([&] { return f.Status().completed_count == 1; }, "Corrupt packet does not prevent retry recovery");
		Check(f.rejected_calls >= 1 && f.peer.requests[0xFD] == 2 && f.indication_callbacks == 1,
			"Bad CRC neither updates indications nor satisfies response timeout");
	}
	{
		Fixture f(false, false, 0);
		f.peer.online = false;
		Check(f.master->RequestPoll(3), "Start zero-retry poll");
		f.Until([&] { return f.Status().communication == Comm::OFFLINE; }, "Zero retries still times out");
		Check(f.peer.requests[0xFB] == 1 && f.Count(Event::RETRY) == 0, "Retry count zero sends once");
		f.peer.online = true;
		Check(f.master->RequestRecall(3), "Manual recovery allowed after timeout");
		f.Until([&] { return f.Status().completed_count == 1; }, "Manual recovery succeeds");
	}
	{
		Fixture f(false, false, 2);
		f.peer.online = false;
		Check(f.master->RequestRecall(3), "Start retry exhaustion check");
		f.Until([&] { return f.Status().communication == Comm::OFFLINE; }, "Exhaust two retries");
		Check(f.peer.requests[0xFD] == 3 && f.Count(Event::RETRY) == 2 && f.Count(Event::TIMEOUT) == 1 &&
			!f.Status().busy && f.indication_callbacks == 0, "Initial request plus exactly two retries, then release busy state");
	}
	{
		Fixture f(false);
		f.peer.drop_ack_replies = 10;
		Check(f.master->RequestRecall(3), "Start final acknowledgement timeout check");
		f.Until([&] { return f.Status().communication == Comm::OFFLINE; }, "Missing final F1 also exhausts retries");
		Check(f.peer.requests[0xFD] == 1 && f.peer.requests[0xFA] == 2 && !f.Status().indications_current &&
			f.Status().completed_count == 0 && f.indication_callbacks == 1, "F2 alone does not complete exchange; FA retry budget is bounded");
	}
	std::cout << "PASS: bounded request/FA retries, invalid-packet recovery, and zero-retry configuration\n";
}

void ControlTimeoutChecks()
{
	Fixture f(false);
	f.peer.online = false;
	f.controls->WriteByte(0, 1);
	Check(f.master->RequestControl(3), "Send control once");
	f.Until([&] { return f.Status().communication == Comm::OFFLINE; }, "Control timeout is reported");
	Check(f.peer.requests[0xFC] == 1 && f.Count(Event::RETRY) == 0 && f.Count(Event::TIMEOUT) == 1,
		"Uncertain control must not automatically retransmit");
	std::vector<uint64_t> offsets; Bytes values;
	Check(f.controls->GetUpdates(offsets, values) && values == Bytes{1}, "Timed-out control remains unacknowledged");
	Check(f.events.back().control_request, "Timeout identifies control request");
	{
		Fixture acknowledged(false);
		acknowledged.peer.drop_ack_replies = 1;
		acknowledged.controls->WriteByte(0, 1);
		Check(acknowledged.master->RequestControl(3), "Send control with lost final acknowledgement");
		acknowledged.Until([&] { return acknowledged.Status().completed_count == 1; }, "Retry FA after valid control response");
		Check(acknowledged.peer.requests[0xFC] == 1 && acknowledged.peer.requests[0xFA] == 2,
			"Control exchange retries only FA, never FC");
		Check(acknowledged.controls->GetUpdates(offsets, values) && values.empty(), "Valid control response acknowledges the prepared snapshot");
	}
	std::cout << "PASS: control timeout without command retransmission or false acknowledgement\n";
}

int main()
{
	WSADATA data{}; if (WSAStartup(MAKEWORD(2, 2), &data)) return 1;
	int result = 0;
	try { AutomaticChecks(); RetryChecks(); ControlTimeoutChecks(); std::cout << "All PollingGenisysMaster checks passed.\n"; }
	catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; result = 1; }
	WSACleanup(); return result;
}
