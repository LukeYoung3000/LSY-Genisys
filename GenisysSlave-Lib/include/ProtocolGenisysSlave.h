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


	public:

		struct SlaveData
		{
			// Data Objects
			uint8_t slave_id;
			std::shared_ptr<DataFrame> indication_data_frame;
			bool is_control_frame;
			std::shared_ptr<DataFrame> control_data_frame;
			uint8_t mode_byte_return_value = (uint8_t)0x04;		// This hardcoded value is how the slave instance will repond with a modebyte after a control or recall frame.

			// Metrics
			std::string last_master_device_to_poll_name;		// IP address of the last master device that sent a poll message to this slave, example: "127.0.0.1".
			uint64_t recevied_counter;							// Number of messages received from master devices.
			uint64_t responce_counter;							// Number of responce messages sent from this slave to master.
			uint8_t last_mode_byte_recived = 0;					// Last mode byte received from master device.
			bool has_received_mode_byte = false;
		};



	private:
		std::map<uint8_t, SlaveData> slave_data;				// Key = SlaveID, Value = SlaveData
		std::vector<std::vector<uint8_t>> input_messages;

	public: // User API
		bool AddDataFrame(uint8_t slave_id, std::shared_ptr<DataFrame>& ind_data_frame_obj, std::shared_ptr<DataFrame>& ctl_data_frame_obj);
		bool GetSlaveData(uint8_t slave_id, SlaveData& out_slave_data);
		bool SetModeByteReturnValue(uint8_t slave_id, uint8_t mode_byte_value);

	public: // Interface To Transport/Network Layers
		bool ProcessMessages(std::vector<uint8_t> & data_buffer, std::string master_device = "");

	private: // Helpers
		bool SeperateMessages(std::vector<uint8_t> & data_buffer);
		bool ProcessMessage(std::vector<uint8_t> & gen_msg_buffer, std::vector<uint8_t> & gen_msg_responce, std::string master_device = "");
		bool IsMasterMessageValid(const std::vector<uint8_t> & gen_msg_buffer);
		bool CalculateCRC(const std::vector<uint8_t> & gen_msg_buffer, uint8_t *crc_low, uint8_t *crc_high);
		bool RemoveEscapeChars(std::vector<uint8_t> & gen_msg_buffer);
		void AddEscapeChars(std::vector<uint8_t> & gen_msg_buffer);


		std::string GetMessageHexString(const std::vector<uint8_t>& gen_msg_buffer);
		void LogGenisysMsg(const std::vector<uint8_t> & gen_msg_buffer);
		void LogGenisysMsg(std::string master_ip, uint8_t slave_id, bool is_recv, uint8_t message_type, bool is_mode_byte, uint8_t mode_byte, const std::vector<uint8_t> & raw_data, std::string additional_notes);

	};

}


