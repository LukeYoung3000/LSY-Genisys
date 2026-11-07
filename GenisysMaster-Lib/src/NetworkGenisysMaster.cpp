#include "NetworkGenisysMaster.h"
#include "MasterTransport.h"
#include "GenisysMaster-Lib-Logging.h"
#include <Ws2tcpip.h>

namespace LSY
{
	NetworkGenisysMaster::NetworkGenisysMaster(Config configuration)
		: NetworkGenisysMaster(configuration, CreateMasterTransport(configuration)) {}
	NetworkGenisysMaster::NetworkGenisysMaster(Config configuration, std::unique_ptr<MasterTransport> transport)
		: configuration_(configuration), transport_(std::move(transport)) {}
	NetworkGenisysMaster::~NetworkGenisysMaster() { Shutdown(); }
	bool NetworkGenisysMaster::IsSerial() const { return configuration_.connection_type_ == Config::CONNECTIONTYPE::SERIAL; }
	bool NetworkGenisysMaster::IsRunning() const { return transport_ && transport_->IsOpen(); }
	bool NetworkGenisysMaster::SerialBusy() const
	{
		if (!IsSerial()) return false;
		if (transport_->WritePending()) return true;
		for (const auto& state : states_) if (state.second.status.busy) return true;
		return false;
	}
	bool NetworkGenisysMaster::AddProtocol(std::shared_ptr<ProtocolGenisysMaster>& protocol)
	{
		if (IsRunning() || !protocol) return false;
		protocol_ = protocol; return true;
	}
	bool NetworkGenisysMaster::AddSlave(uint8_t slave_id) { return IsSerial() && AddSlave(slave_id, {}); }
	bool NetworkGenisysMaster::AddSlave(uint8_t slave_id, Endpoint endpoint)
	{
		if (IsRunning() || !slave_id) return false;
		sockaddr_in address{};
		if (!IsSerial())
		{
			if (!endpoint.port_) return false;
			address.sin_family = AF_INET; address.sin_port = htons(endpoint.port_);
			if (InetPtonA(AF_INET, endpoint.ip_address_.c_str(), &address.sin_addr) != 1) return false;
		}
		return endpoints_.emplace(slave_id, address).second;
	}
	bool NetworkGenisysMaster::Start()
	{
		if (IsRunning() || !transport_ || !protocol_ || endpoints_.empty() || !configuration_.poll_interval_ms_ ||
			!configuration_.response_timeout_ms_ || !configuration_.recovery_interval_ms_) return false;
		for (const auto& endpoint : endpoints_) if (!protocol_->HasSlave(endpoint.first)) return false;
		if (!transport_->Start()) return false;
		states_.clear(); serial_frame_.clear(); last_serial_slave_ = 0;
		for (const auto& endpoint : endpoints_)
		{
			SlaveState state; state.next_request = Clock::now(); states_.emplace(endpoint.first, state);
		}
		return true;
	}
	bool NetworkGenisysMaster::Shutdown()
	{
		if (transport_) transport_->Stop();
		responses_.clear(); transaction_events_.clear(); states_.clear(); serial_frame_.clear(); last_serial_slave_ = 0;
		if (protocol_) for (const auto& endpoint : endpoints_) protocol_->CancelRequest(endpoint.first);
		return true;
	}
	uint16_t NetworkGenisysMaster::GetLocalPort() const { return transport_ ? transport_->LocalPort() : 0; }
	bool NetworkGenisysMaster::Send(uint8_t slave_id, const std::vector<uint8_t>& message)
	{
		const auto endpoint = endpoints_.find(slave_id);
		return IsRunning() && endpoint != endpoints_.end() && !message.empty() && transport_->Send(endpoint->second, message);
	}
	bool NetworkGenisysMaster::RequestRecall(uint8_t slave_id)
	{
		if (!IsRunning() || SerialBusy() || !endpoints_.count(slave_id)) return false;
		std::vector<uint8_t> message;
		return protocol_->BuildRecall(slave_id, message) && StartRequest(slave_id, REQUEST::RECALL, message);
	}
	bool NetworkGenisysMaster::RequestPoll(uint8_t slave_id, bool secure)
	{
		if (!IsRunning() || SerialBusy() || !endpoints_.count(slave_id)) return false;
		std::vector<uint8_t> message;
		return protocol_->BuildPoll(slave_id, message, secure) && StartRequest(slave_id, REQUEST::POLL, message);
	}
	bool NetworkGenisysMaster::RequestControl(uint8_t slave_id, bool all_data_flag, bool include_mode, uint8_t mode)
	{
		if (!IsRunning() || SerialBusy() || !endpoints_.count(slave_id)) return false;
		std::vector<uint8_t> message;
		return protocol_->BuildControl(slave_id, message, all_data_flag, include_mode, mode) && StartRequest(slave_id, REQUEST::CONTROL, message);
	}
	bool NetworkGenisysMaster::StartRequest(uint8_t slave_id, REQUEST request, const std::vector<uint8_t>& message)
	{
		auto& state = states_.at(slave_id);
		state.status.busy = true; state.status.retries = 0; state.request = request;
		state.saw_indications = false; state.retryable = request != REQUEST::CONTROL; state.wire = message;
		state.deadline = Clock::now() + std::chrono::milliseconds(configuration_.response_timeout_ms_);
		bool inputReady = true;
		if (IsSerial()) { serial_frame_.clear(); last_serial_slave_ = slave_id; inputReady = transport_->DiscardInput(); }
		if (inputReady && Send(slave_id, message)) { state.waiting_write = transport_->WritePending(); return true; }
		FailRequest(slave_id, TRANSACTIONEVENT::SEND_FAILED);
		if (IsSerial() && !IsRunning()) FailSerialTransport();
		return false;
	}
	void NetworkGenisysMaster::SetStatus(uint8_t slave_id, COMMUNICATIONSTATUS status)
	{
		auto& state = states_.at(slave_id);
		if (state.status.communication == status) return;
		state.status.communication = status;
		auto callback = status_callback_; if (callback) callback(slave_id, status);
	}
	void NetworkGenisysMaster::FailRequest(uint8_t slave_id, TRANSACTIONEVENT reason)
	{
		auto& state = states_.at(slave_id);
		transaction_events_.push_back({slave_id, reason, state.request == REQUEST::CONTROL, state.status.retries});
		if (reason == TRANSACTIONEVENT::TIMEOUT) ++state.status.timeout_count;
		state.status.busy = false; state.waiting_write = false; state.status.indications_current = false; state.wire.clear();
		if (IsSerial()) serial_frame_.clear();
		protocol_->CancelRequest(slave_id);
		state.next_request = Clock::now() + std::chrono::milliseconds(configuration_.recovery_interval_ms_);
		SetStatus(slave_id, COMMUNICATIONSTATUS::OFFLINE);
	}
	void NetworkGenisysMaster::FailSerialTransport()
	{
		transport_->Stop(); serial_frame_.clear();
		for (auto& entry : states_)
		{
			if (entry.second.status.busy) FailRequest(entry.first, TRANSACTIONEVENT::SEND_FAILED);
			entry.second.status.indications_current = false;
			SetStatus(entry.first, COMMUNICATIONSTATUS::OFFLINE);
		}
	}
	bool NetworkGenisysMaster::ProcessTimers(bool allow_automatic_requests)
	{
		bool valid = true; const auto now = Clock::now();
		for (auto& entry : states_)
		{
			const uint8_t id = entry.first; auto& state = entry.second;
			if (state.status.busy)
			{
				if (state.waiting_write)
				{
					if (transport_->WritePending()) continue;
					state.waiting_write = false;
					state.deadline = now + std::chrono::milliseconds(configuration_.response_timeout_ms_);
				}
				if (now < state.deadline) continue;
				if (state.retryable && state.status.retries < configuration_.retry_count_)
				{
					++state.status.retries;
					transaction_events_.push_back({id, TRANSACTIONEVENT::RETRY, state.request == REQUEST::CONTROL, state.status.retries});
					state.deadline = now + std::chrono::milliseconds(configuration_.response_timeout_ms_);
					if (!Send(id, state.wire)) { FailRequest(id, TRANSACTIONEVENT::SEND_FAILED); valid = false; }
					else state.waiting_write = transport_->WritePending();
					if (IsSerial() && !IsRunning()) { FailSerialTransport(); return false; }
				}
				else FailRequest(id, TRANSACTIONEVENT::TIMEOUT);
				continue;
			}
			if (IsSerial() || !allow_automatic_requests || !configuration_.automatic_polling_ || now < state.next_request) continue;
			if (!(state.status.indications_current ? RequestPoll(id, configuration_.secure_poll_) : RequestRecall(id))) valid = false;
		}
		if (IsSerial() && allow_automatic_requests && configuration_.automatic_polling_ && !SerialBusy())
		{
			// Round-robin across due IDs so a slow bus cannot starve later IDs.
			auto entry = states_.upper_bound(last_serial_slave_);
			for (size_t count = 0; count < states_.size(); ++count)
			{
				if (entry == states_.end()) entry = states_.begin();
				const auto id = entry->first; const auto& state = entry->second;
				if (now >= state.next_request)
					return (state.status.indications_current ? RequestPoll(id, configuration_.secure_poll_) : RequestRecall(id)) && valid;
				++entry;
			}
		}
		return valid;
	}
	bool NetworkGenisysMaster::GetSlaveStatus(uint8_t slave_id, SlaveStatus& status) const
	{
		const auto state = states_.find(slave_id); if (state == states_.end()) return false;
		status = state->second.status; return true;
	}
	bool NetworkGenisysMaster::AddStatusCallback(std::function<void(uint8_t, COMMUNICATIONSTATUS)>& callback)
	{ status_callback_ = callback; return true; }
	const std::vector<NetworkGenisysMaster::TransactionEvent>& NetworkGenisysMaster::GetTransactionEvents() const { return transaction_events_; }
	bool NetworkGenisysMaster::SourceMatches(const std::vector<uint8_t>& message, const sockaddr_in& source) const
	{
		size_t start = 0;
		while (start < message.size())
		{
			if (message[start] != 0xF1 && message[start] != 0xF2) return false;
			size_t address = start + 1; if (address >= message.size()) return false;
			uint8_t id = message[address];
			if (id == 0xF0)
			{
				if (++address >= message.size() || message[address] > 15) return false;
				id = 0xF0 | message[address];
			}
			else if (id >= 0xF0) return false;
			const auto endpoint = endpoints_.find(id);
			if (endpoint == endpoints_.end()) return false;
			if (IsSerial()) { const auto state = states_.find(id); if (state == states_.end() || !state->second.status.busy) return false; }
			else if (endpoint->second.sin_addr.s_addr != source.sin_addr.s_addr || endpoint->second.sin_port != source.sin_port) return false;
			size_t end = address + 1; while (end < message.size() && message[end] != 0xF6) ++end;
			if (end == message.size()) return false; start = end + 1;
		}
		return !message.empty();
	}
	bool NetworkGenisysMaster::AcceptResponse(std::vector<uint8_t>& message)
	{
		bool valid = protocol_->ProcessDatagram(message);
		const auto& events = protocol_->GetResponses(); responses_.insert(responses_.end(), events.begin(), events.end());
		for (const auto& event : events)
		{
			auto& state = states_.at(event.slave_id);
			if (event.transaction_complete)
			{
				state.status.busy = false; state.waiting_write = false; ++state.status.completed_count;
				if (state.request == REQUEST::RECALL && state.saw_indications) state.status.indications_current = true;
				state.wire.clear(); state.next_request = Clock::now() + std::chrono::milliseconds(configuration_.poll_interval_ms_);
				transaction_events_.push_back({event.slave_id, TRANSACTIONEVENT::COMPLETE, state.request == REQUEST::CONTROL, state.status.retries});
				SetStatus(event.slave_id, COMMUNICATIONSTATUS::ONLINE); continue;
			}
			state.saw_indications = true; SetStatus(event.slave_id, COMMUNICATIONSTATUS::ONLINE);
			auto end = message.begin(); while (end != message.end() && *end != 0xF6) ++end;
			if (end == message.end()) { valid = false; break; } ++end;
			const std::vector<uint8_t> ack(message.begin(), end); state.wire = ack;
			state.retryable = true; state.status.retries = 0;
			state.deadline = Clock::now() + std::chrono::milliseconds(configuration_.response_timeout_ms_);
			if (!Send(event.slave_id, ack)) valid = false;
			state.waiting_write = transport_->WritePending();
			message.erase(message.begin(), end);
			if (IsSerial() && !IsRunning()) { FailSerialTransport(); return false; }
		}
		return valid;
	}
	bool NetworkGenisysMaster::ProcessSerialBytes(const std::vector<uint8_t>& bytes)
	{
		bool valid = true; sockaddr_in unused{};
		for (auto byte : bytes)
		{
			if (byte == 0xF1 || byte == 0xF2)
			{
				if (!serial_frame_.empty()) valid = false;
				serial_frame_.assign(1, byte); continue;
			}
			if (serial_frame_.empty()) { valid = false; continue; }
			serial_frame_.push_back(byte);
			if (byte == 0xF6)
			{
				std::vector<uint8_t> frame; frame.swap(serial_frame_);
				if (!SourceMatches(frame, unused)) valid = false;
				else if (!AcceptResponse(frame)) valid = false;
				if (!IsRunning()) break;
			}
			else if (serial_frame_.size() >= 1024) { serial_frame_.clear(); valid = false; }
		}
		if (!valid) MasterLogging::LogWarning("NetworkGenisysMaster: Rejected serial noise, malformed or unexpected-slave response");
		return valid;
	}
	bool NetworkGenisysMaster::Process()
	{
		return Process(true);
	}
	bool NetworkGenisysMaster::Process(bool allow_automatic_requests)
	{
		responses_.clear(); transaction_events_.clear();
		if (!IsRunning()) return false;
		if (!transport_->PollWrite()) { if (IsSerial()) FailSerialTransport(); return false; }
		bool valid = true;
		for (int count = 0; count < 64; ++count)
		{
			std::vector<uint8_t> message; sockaddr_in source{};
			const auto result = transport_->Receive(message, source);
			if (result == MasterTransport::READ::EMPTY) break;
			if (result == MasterTransport::READ::FAILED)
			{
				valid = false; if (IsSerial()) { FailSerialTransport(); return false; } break;
			}
			if (IsSerial()) { if (!ProcessSerialBytes(message)) valid = false; if (transport_->WritePending()) break; }
			else if (!SourceMatches(message, source))
			{
				MasterLogging::LogWarning("NetworkGenisysMaster: Dropped incomplete or unexpected-source datagram"); valid = false;
			}
			else if (!AcceptResponse(message)) valid = false;
			if (!IsRunning()) return false;
		}
		if (!ProcessTimers(allow_automatic_requests)) valid = false;
		return valid;
	}
	const std::vector<ProtocolGenisysMaster::Response>& NetworkGenisysMaster::GetResponses() const { return responses_; }
}
