#pragma once

#include <map>
#include <memory>
#include <vector>

#include "MasterDataFrame.h"

namespace LSY
{
	class ProtocolGenisysMaster
	{
	public:
		enum class RESPONSETYPE { ACK_MASTER, INDICATION_DATA };
		struct Response
		{
			uint8_t slave_id;
			RESPONSETYPE type;
			bool control_acknowledged;
			bool transaction_complete;
		};

		bool AddDataFrame(uint8_t slave_id, bool is_control_frame, std::shared_ptr<MasterDataFrame> & data_frame_obj);

		// Builders start one outstanding request per slave. They return false and
		// clear message if unconfigured, busy, or there is no control data to send.
		bool BuildPoll(uint8_t slave_id, std::vector<uint8_t> & message, bool secure = true);
		bool BuildRecall(uint8_t slave_id, std::vector<uint8_t> & message);
		bool BuildControl(uint8_t slave_id, std::vector<uint8_t> & message,
			bool all_data_flag = false, bool include_mode = false, uint8_t mode = 0);
		bool BuildAcknowledgeSlave(uint8_t slave_id, std::vector<uint8_t> & message);

		// Replaces received bytes with generated FA replies. Partial messages are
		// buffered. False means at least one message was rejected; other valid
		// messages in the same call are still processed. Empty/partial input is OK.
		// One instance represents one ordered input stream; it is not thread safe.
		bool ProcessMessages(std::vector<uint8_t> & data_buffer, std::string slave_ip = "");
		const std::vector<Response> & GetResponses() const;
		// UDP payloads are self-contained; fragments never cross datagrams.
		bool ProcessDatagram(std::vector<uint8_t> & data_buffer);
		bool HasSlave(uint8_t slave_id) const;
		// Abandon request state without acknowledging any prepared controls.
		bool CancelRequest(uint8_t slave_id);

	private:
		enum class REQUESTTYPE { POLL, RECALL, CONTROL, ACK_SLAVE };
		std::map<uint8_t, std::shared_ptr<MasterDataFrame>> indication_data_frames;
		std::map<uint8_t, std::shared_ptr<MasterDataFrame>> control_data_frames;
		std::map<uint8_t, REQUESTTYPE> pending_requests_;
		std::vector<uint8_t> receive_buffer_;
		std::vector<Response> responses_;

		bool BeginRequest(uint8_t slave_id, REQUESTTYPE request, std::vector<uint8_t> & message);
		bool ProcessMessage(const std::vector<uint8_t> & wire, std::vector<uint8_t> & replies);
		static uint16_t CalculateCRC(const std::vector<uint8_t> & data);
		static std::vector<uint8_t> Encode(std::vector<uint8_t> data, bool secure = true);
		static bool Decode(const std::vector<uint8_t> & wire, std::vector<uint8_t> & data);
	};
}
