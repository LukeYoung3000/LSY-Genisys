#pragma once

#include <vector>
#include <map>
#include <memory>

#include "GenisysSlave-Lib-Logging.h"
#include "DataFrame.h"


namespace LSY
{

	class ProtocolGenisysSlave
	{
	private:
		std::map<uint8_t, std::shared_ptr<DataFrame>> indication_data_frames;
		std::map<uint8_t, std::shared_ptr<DataFrame>> control_data_frames;
		std::vector<std::vector<uint8_t>> input_messages;

	public: // User API
		bool AddDataFrame(uint8_t slave_id, bool is_control_frame, std::shared_ptr<DataFrame> & data_frame_obj);

	public: // Interface To Transport/Network Layers
		bool ProcessMessages(std::vector<uint8_t> & data_buffer, std::string master_ip = "");

	private: // Helpers
		bool SeperateMessages(std::vector<uint8_t> & data_buffer);
		bool ProcessMessage(std::vector<uint8_t> & gen_msg_buffer, std::vector<uint8_t> & gen_msg_responce, std::string master_ip = "");
		bool IsMasterMessageValid(const std::vector<uint8_t> & gen_msg_buffer);
		bool CalculateCRC(const std::vector<uint8_t> & gen_msg_buffer, uint8_t *crc_low, uint8_t *crc_high);
		bool RemoveEscapeChars(std::vector<uint8_t> & gen_msg_buffer);
		void AddEscapeChars(std::vector<uint8_t> & gen_msg_buffer);
		void LogGenisysMsg(const std::vector<uint8_t> & gen_msg_buffer);

	};

}


