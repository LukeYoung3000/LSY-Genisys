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

		struct SlaveData
		{
			// Data Objects
			uint8_t slave_id;
			std::shared_ptr<DataFrame> indication_data_frame;
			bool is_control_frame;
			std::shared_ptr<DataFrame> control_data_frame;

			// Metrics
			std::string last_master_device_to_poll_name;		// IP address of the last master device that sent a poll message to this slave, example: "127.0.0.1".
			uint64_t recevied_counter;							// Number of messages received from master devices.
			uint64_t responce_counter;							// Number of responce messages sent from this slave to master.
			uint8_t last_mode_byte_recived;						// Last mode byte received from master device.
		};

		std::map<uint8_t, SlaveData> slave_data;				// Key = SlaveID, Value = SlaveData
		std::vector<std::vector<uint8_t>> input_messages;

	public: // User API
		bool AddDataFrame(uint8_t slave_id, std::shared_ptr<DataFrame>& ind_data_frame_obj, std::shared_ptr<DataFrame>& ctl_data_frame_obj);

	public: // Interface To Transport/Network Layers
		bool ProcessMessages(std::vector<uint8_t> & data_buffer, std::string master_device = "");

	private: // Helpers
		bool SeperateMessages(std::vector<uint8_t> & data_buffer);
		bool ProcessMessage(std::vector<uint8_t> & gen_msg_buffer, std::vector<uint8_t> & gen_msg_responce, std::string master_device = "");
		bool IsMasterMessageValid(const std::vector<uint8_t> & gen_msg_buffer);
		bool CalculateCRC(const std::vector<uint8_t> & gen_msg_buffer, uint8_t *crc_low, uint8_t *crc_high);
		bool RemoveEscapeChars(std::vector<uint8_t> & gen_msg_buffer);
		void AddEscapeChars(std::vector<uint8_t> & gen_msg_buffer);
		void LogGenisysMsg(const std::vector<uint8_t> & gen_msg_buffer);

	};

}


