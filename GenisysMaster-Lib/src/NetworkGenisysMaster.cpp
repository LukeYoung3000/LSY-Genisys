#include "NetworkGenisysMaster.h"
#include "GenisysMaster-Lib-Logging.h"
#include <Ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

namespace LSY
{
	NetworkGenisysMaster::NetworkGenisysMaster(Config configuration) : configuration_(configuration) {}
	NetworkGenisysMaster::~NetworkGenisysMaster() { Shutdown(); }

	bool NetworkGenisysMaster::AddProtocol(std::shared_ptr<ProtocolGenisysMaster>& protocol)
	{
		if (socket_ != INVALID_SOCKET || !protocol) return false;
		protocol_ = protocol;
		return true;
	}

	bool NetworkGenisysMaster::AddSlave(uint8_t slave_id, Endpoint endpoint)
	{
		if (socket_ != INVALID_SOCKET || slave_id == 0 || endpoint.port_ == 0) return false;
		sockaddr_in address{};
		address.sin_family = AF_INET;
		address.sin_port = htons(endpoint.port_);
		if (InetPtonA(AF_INET, endpoint.ip_address_.c_str(), &address.sin_addr) != 1) return false;
		return endpoints_.emplace(slave_id, address).second;
	}

	bool NetworkGenisysMaster::Start()
	{
		if (socket_ != INVALID_SOCKET || !protocol_ || endpoints_.empty() ||
			configuration_.poll_interval_ms_ == 0 || configuration_.response_timeout_ms_ == 0 ||
			configuration_.recovery_interval_ms_ == 0) return false;
		for (const auto& endpoint : endpoints_)
			if (!protocol_->HasSlave(endpoint.first)) return false;
		sockaddr_in local{};
		local.sin_family = AF_INET;
		local.sin_port = htons(configuration_.receive_port_);
		if (InetPtonA(AF_INET, configuration_.local_ip_.c_str(), &local.sin_addr) != 1) return false;
		WSADATA data{};
		if (WSAStartup(MAKEWORD(2, 2), &data) != 0) return false;
		wsa_started_ = true;
		socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		BOOL exclusive = TRUE;
		u_long nonblocking = 1;
		if (socket_ == INVALID_SOCKET ||
			setsockopt(socket_, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char*>(&exclusive), sizeof(exclusive)) == SOCKET_ERROR ||
			bind(socket_, reinterpret_cast<sockaddr*>(&local), sizeof(local)) == SOCKET_ERROR ||
			ioctlsocket(socket_, FIONBIO, &nonblocking) == SOCKET_ERROR)
		{
			MasterLogging::LogErrorF("NetworkGenisysMaster::Start: Socket error [%d]", WSAGetLastError());
			Shutdown();
			return false;
		}
		int length = sizeof(local);
		if (getsockname(socket_, reinterpret_cast<sockaddr*>(&local), &length) == SOCKET_ERROR)
		{
			Shutdown();
			return false;
		}
		local_port_ = ntohs(local.sin_port);
		states_.clear();
		for (const auto& endpoint : endpoints_)
		{
			SlaveState state;
			state.next_request = Clock::now();
			states_.emplace(endpoint.first, state);
		}
		return true;
	}

	bool NetworkGenisysMaster::Shutdown()
	{
		if (socket_ != INVALID_SOCKET)
		{
			closesocket(socket_);
			socket_ = INVALID_SOCKET;
		}
		if (wsa_started_) { WSACleanup(); wsa_started_ = false; }
		local_port_ = 0;
		responses_.clear();
		transaction_events_.clear();
		states_.clear();
		// No controls are acknowledged by stopping the transport.
		if (protocol_)
			for (const auto& endpoint : endpoints_) protocol_->CancelRequest(endpoint.first);
		return true;
	}

	uint16_t NetworkGenisysMaster::GetLocalPort() const { return local_port_; }

	bool NetworkGenisysMaster::Send(uint8_t slave_id, const std::vector<uint8_t>& message)
	{
		const auto endpoint = endpoints_.find(slave_id);
		if (socket_ == INVALID_SOCKET || endpoint == endpoints_.end() || message.empty()) return false;
		const auto& address = endpoint->second;
		if (sendto(socket_, reinterpret_cast<const char*>(message.data()), static_cast<int>(message.size()), 0,
			reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != static_cast<int>(message.size()))
		{
			MasterLogging::LogErrorF("NetworkGenisysMaster::Send: Socket error [%d]", WSAGetLastError());
			return false;
		}
		return true;
	}

	bool NetworkGenisysMaster::RequestRecall(uint8_t slave_id)
	{
		if (socket_ == INVALID_SOCKET || !endpoints_.count(slave_id)) return false;
		std::vector<uint8_t> message;
		if (!protocol_->BuildRecall(slave_id, message)) return false;
		return StartRequest(slave_id, REQUEST::RECALL, message);
	}

	bool NetworkGenisysMaster::RequestPoll(uint8_t slave_id, bool secure)
	{
		if (socket_ == INVALID_SOCKET || !endpoints_.count(slave_id)) return false;
		std::vector<uint8_t> message;
		if (!protocol_->BuildPoll(slave_id, message, secure)) return false;
		return StartRequest(slave_id, REQUEST::POLL, message);
	}

	bool NetworkGenisysMaster::RequestControl(uint8_t slave_id, bool all_data_flag, bool include_mode, uint8_t mode)
	{
		if (socket_ == INVALID_SOCKET || !endpoints_.count(slave_id)) return false;
		std::vector<uint8_t> message;
		if (!protocol_->BuildControl(slave_id, message, all_data_flag, include_mode, mode)) return false;
		return StartRequest(slave_id, REQUEST::CONTROL, message);
	}

	bool NetworkGenisysMaster::StartRequest(uint8_t slave_id, REQUEST request, const std::vector<uint8_t>& message)
	{
		auto& state = states_.at(slave_id);
		state.status.busy = true;
		state.status.retries = 0;
		state.request = request;
		state.saw_indications = false;
		state.retryable = request != REQUEST::CONTROL;
		state.wire = message;
		state.deadline = Clock::now() + std::chrono::milliseconds(configuration_.response_timeout_ms_);
		if (Send(slave_id, message)) return true;
		FailRequest(slave_id, TRANSACTIONEVENT::SEND_FAILED);
		return false;
	}

	void NetworkGenisysMaster::SetStatus(uint8_t slave_id, COMMUNICATIONSTATUS status)
	{
		auto& state = states_.at(slave_id);
		if (state.status.communication == status) return;
		state.status.communication = status;
		// Registration can be replaced from the callback without invalidating it.
		auto callback = status_callback_;
		if (callback) callback(slave_id, status);
	}

	void NetworkGenisysMaster::FailRequest(uint8_t slave_id, TRANSACTIONEVENT reason)
	{
		auto& state = states_.at(slave_id);
		transaction_events_.push_back({slave_id, reason, state.request == REQUEST::CONTROL, state.status.retries});
		if (reason == TRANSACTIONEVENT::TIMEOUT) ++state.status.timeout_count;
		state.status.busy = false;
		state.status.indications_current = false;
		state.wire.clear();
		protocol_->CancelRequest(slave_id);
		state.next_request = Clock::now() + std::chrono::milliseconds(configuration_.recovery_interval_ms_);
		SetStatus(slave_id, COMMUNICATIONSTATUS::OFFLINE);
	}

	bool NetworkGenisysMaster::ProcessTimers()
	{
		bool valid = true;
		const auto now = Clock::now();
		for (auto& entry : states_)
		{
			const uint8_t id = entry.first;
			auto& state = entry.second;
			if (state.status.busy)
			{
				if (now < state.deadline) continue;
				if (state.retryable && state.status.retries < configuration_.retry_count_)
				{
					++state.status.retries;
					transaction_events_.push_back({id, TRANSACTIONEVENT::RETRY, state.request == REQUEST::CONTROL, state.status.retries});
					state.deadline = now + std::chrono::milliseconds(configuration_.response_timeout_ms_);
					if (!Send(id, state.wire)) { FailRequest(id, TRANSACTIONEVENT::SEND_FAILED); valid = false; }
				}
				else FailRequest(id, TRANSACTIONEVENT::TIMEOUT);
				continue;
			}
			if (!configuration_.automatic_polling_ || now < state.next_request) continue;
			if (state.status.indications_current)
			{
				if (!RequestPoll(id, configuration_.secure_poll_)) valid = false;
			}
			else if (!RequestRecall(id)) valid = false;
		}
		return valid;
	}

	bool NetworkGenisysMaster::GetSlaveStatus(uint8_t slave_id, SlaveStatus& status) const
	{
		const auto state = states_.find(slave_id);
		if (state == states_.end()) return false;
		status = state->second.status;
		return true;
	}

	bool NetworkGenisysMaster::AddStatusCallback(std::function<void(uint8_t, COMMUNICATIONSTATUS)>& callback)
	{
		status_callback_ = callback;
		return true;
	}

	const std::vector<NetworkGenisysMaster::TransactionEvent>& NetworkGenisysMaster::GetTransactionEvents() const
	{
		return transaction_events_;
	}

	bool NetworkGenisysMaster::SourceMatches(const std::vector<uint8_t>& message, const sockaddr_in& source) const
	{
		// Preflight every complete frame in the datagram before changing protocol
		// state. Each ID must be mapped to this sender, even for shared endpoints.
		size_t start = 0;
		while (start < message.size())
		{
			if (message[start] != 0xF1 && message[start] != 0xF2) return false;
			size_t address = start + 1;
			if (address >= message.size()) return false;
			uint8_t id = message[address];
			if (id == 0xF0)
			{
				if (++address >= message.size() || message[address] > 15) return false;
				id = 0xF0 | message[address];
			}
			else if (id >= 0xF0) return false;
			const auto endpoint = endpoints_.find(id);
			if (endpoint == endpoints_.end() || endpoint->second.sin_addr.s_addr != source.sin_addr.s_addr ||
				endpoint->second.sin_port != source.sin_port) return false;
			size_t end = address + 1;
			while (end < message.size() && message[end] != 0xF6) ++end;
			if (end == message.size()) return false;
			start = end + 1;
		}
		return !message.empty();
	}

	bool NetworkGenisysMaster::Process()
	{
		responses_.clear();
		transaction_events_.clear();
		if (socket_ == INVALID_SOCKET) return false;
		bool valid = true;
		for (int count = 0; count < 64; ++count)
		{
			char buffer[65536];
			sockaddr_in source{};
			int length = sizeof(source);
			const int received = recvfrom(socket_, buffer, sizeof(buffer), 0,
				reinterpret_cast<sockaddr*>(&source), &length);
			if (received == SOCKET_ERROR)
			{
				const int error = WSAGetLastError();
				if (error == WSAEWOULDBLOCK) break;
				MasterLogging::LogErrorF("NetworkGenisysMaster::Process: Socket error [%d]", error);
				// ICMP errors must not prevent timeout/recovery processing.
				valid = false;
				break;
			}
			std::vector<uint8_t> message(buffer, buffer + received);
			if (!SourceMatches(message, source))
			{
				MasterLogging::LogWarning("NetworkGenisysMaster: Dropped incomplete or unexpected-source datagram");
				valid = false;
				continue;
			}
			if (!protocol_->ProcessDatagram(message)) valid = false;
			const auto& events = protocol_->GetResponses();
			responses_.insert(responses_.end(), events.begin(), events.end());
			// ProcessDatagram emits one FA per accepted F2, in event order. Send
			// individual acknowledgements to the configured endpoint for each ID.
			for (const auto& event : events)
			{
				auto& state = states_.at(event.slave_id);
				if (event.transaction_complete)
				{
					state.status.busy = false;
					++state.status.completed_count;
					if (state.request == REQUEST::RECALL && state.saw_indications) state.status.indications_current = true;
					state.wire.clear();
					state.next_request = Clock::now() + std::chrono::milliseconds(configuration_.poll_interval_ms_);
					transaction_events_.push_back({event.slave_id, TRANSACTIONEVENT::COMPLETE,
						state.request == REQUEST::CONTROL, state.status.retries});
					SetStatus(event.slave_id, COMMUNICATIONSTATUS::ONLINE);
					continue;
				}
				state.saw_indications = true;
				SetStatus(event.slave_id, COMMUNICATIONSTATUS::ONLINE);
				// Split the corresponding generated FA at its unescaped terminator.
				auto end = message.begin();
				while (end != message.end() && *end != 0xF6) ++end;
				if (end == message.end()) { valid = false; break; }
				++end;
				const std::vector<uint8_t> ack(message.begin(), end);
				state.wire = ack;
				state.retryable = true; // Only FA may be retried, never the original FC.
				state.status.retries = 0;
				state.deadline = Clock::now() + std::chrono::milliseconds(configuration_.response_timeout_ms_);
				if (!Send(event.slave_id, ack)) valid = false;
				message.erase(message.begin(), end);
			}
		}
		if (!ProcessTimers()) valid = false;
		return valid;
	}

	const std::vector<ProtocolGenisysMaster::Response>& NetworkGenisysMaster::GetResponses() const
	{
		return responses_;
	}
}
