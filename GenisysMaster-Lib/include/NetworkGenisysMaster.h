#pragma once

#include <WinSock2.h>
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ProtocolGenisysMaster.h"

namespace LSY
{
	class NetworkGenisysMaster
	{
	public:
		struct Config
		{
			std::string local_ip_ = "0.0.0.0";
			uint16_t receive_port_ = 7170; // Zero requests an OS-assigned port.
			bool automatic_polling_ = true;
			bool secure_poll_ = true;
			uint32_t poll_interval_ms_ = 500;
			uint32_t response_timeout_ms_ = 500;
			uint32_t retry_count_ = 2; // Retransmissions after the initial send.
			uint32_t recovery_interval_ms_ = 1000;
		};
		struct Endpoint
		{
			std::string ip_address_ = "127.0.0.1"; // Numeric IPv4 address.
			uint16_t port_ = 7169;
		};

		enum class COMMUNICATIONSTATUS { UNKNOWN, ONLINE, OFFLINE };
		enum class TRANSACTIONEVENT { RETRY, COMPLETE, TIMEOUT, SEND_FAILED };
		struct SlaveStatus
		{
			COMMUNICATIONSTATUS communication = COMMUNICATIONSTATUS::UNKNOWN;
			bool busy = false;
			bool indications_current = false;
			uint32_t retries = 0;
			uint64_t completed_count = 0;
			uint64_t timeout_count = 0;
		};
		struct TransactionEvent
		{
			uint8_t slave_id;
			TRANSACTIONEVENT type;
			bool control_request; // The original request was a control.
			uint32_t retries;
		};

		NetworkGenisysMaster(Config configuration);
		~NetworkGenisysMaster();
		NetworkGenisysMaster(const NetworkGenisysMaster&) = delete;
		NetworkGenisysMaster& operator=(const NetworkGenisysMaster&) = delete;
		bool AddProtocol(std::shared_ptr<ProtocolGenisysMaster> & protocol);
		bool AddSlave(uint8_t slave_id, Endpoint endpoint);
		bool Start();
		bool Process(); // Nonblocking; handles up to 64 datagrams per call.
		bool Shutdown();
		uint16_t GetLocalPort() const;
		bool RequestRecall(uint8_t slave_id);
		bool RequestPoll(uint8_t slave_id, bool secure = true);
		bool RequestControl(uint8_t slave_id, bool all_data_flag = false,
			bool include_mode = false, uint8_t mode = 0);
		const std::vector<ProtocolGenisysMaster::Response>& GetResponses() const;
		const std::vector<TransactionEvent>& GetTransactionEvents() const;
		bool GetSlaveStatus(uint8_t slave_id, SlaveStatus& status) const;
		bool AddStatusCallback(std::function<void(uint8_t, COMMUNICATIONSTATUS)>& callback);

	private:
		using Clock = std::chrono::steady_clock;
		enum class REQUEST { POLL, RECALL, CONTROL };
		struct SlaveState
		{
			SlaveStatus status;
			REQUEST request = REQUEST::POLL;
			bool saw_indications = false;
			bool retryable = true;
			std::vector<uint8_t> wire;
			Clock::time_point deadline;
			Clock::time_point next_request;
		};
		std::map<uint8_t, SlaveState> states_;
		std::function<void(uint8_t, COMMUNICATIONSTATUS)> status_callback_;
		std::vector<TransactionEvent> transaction_events_;
		void SetStatus(uint8_t slave_id, COMMUNICATIONSTATUS status);
		bool StartRequest(uint8_t slave_id, REQUEST request, const std::vector<uint8_t>& message);
		void FailRequest(uint8_t slave_id, TRANSACTIONEVENT reason);
		bool ProcessTimers();
		Config configuration_;
		std::shared_ptr<ProtocolGenisysMaster> protocol_;
		std::map<uint8_t, sockaddr_in> endpoints_;
		SOCKET socket_ = INVALID_SOCKET;
		bool wsa_started_ = false;
		uint16_t local_port_ = 0;
		std::vector<ProtocolGenisysMaster::Response> responses_;
		bool Send(uint8_t slave_id, const std::vector<uint8_t>& message);
		bool SourceMatches(const std::vector<uint8_t>& message, const sockaddr_in& source) const;
	};
}
