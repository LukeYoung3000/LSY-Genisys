#pragma once
// Private transport contract. Applications use NetworkGenisysMaster, not this header.
#include "NetworkGenisysMaster.h"
namespace LSY
{
	class MasterTransport
	{
	public:
		enum class READ { DATA, EMPTY, FAILED };
		virtual ~MasterTransport() = default;
		virtual bool Start() = 0;
		virtual void Stop() = 0;
		virtual bool IsOpen() const = 0;
		virtual uint16_t LocalPort() const { return 0; }
		// True accepts one complete write; PollWrite progresses it without waiting.
		virtual bool Send(const sockaddr_in& endpoint, const std::vector<uint8_t>& bytes) = 0;
		virtual bool PollWrite() { return true; }
		virtual bool WritePending() const { return false; }
		virtual READ Receive(std::vector<uint8_t>& bytes, sockaddr_in& source) = 0;
		virtual bool DiscardInput() { return true; }
	};
	std::unique_ptr<MasterTransport> CreateMasterTransport(const NetworkGenisysMaster::Config& config);
}
