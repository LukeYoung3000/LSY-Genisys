#include "ProtocolGenisysSlave.h"




/*****************************************************************************
 * GENISYS - BYTE OFFSETS
 *****************************************************************************/

#define GEN_MSG_TYPE_BYTE		0
#define GEN_SLAVE_ADDR_BYTE		1


 /*****************************************************************************
  * GENISYS - MESSAGE TYPES
  *****************************************************************************/

#define GEN_MSG_TYPE_ACK_MASTER (uint8_t)0xF1
#define GEN_MSG_TYPE_IND_RESP	(uint8_t)0xF2
#define GEN_MSG_TYPE_END_MSG	(uint8_t)0xF6
#define GEN_MSG_TYPE_ACK_SLAVE	(uint8_t)0xFA
#define GEN_MSG_TYPE_POLL		(uint8_t)0xFB
#define GEN_MSG_TYPE_CONTROL	(uint8_t)0xFC
#define GEN_MSG_TYPE_RECALL		(uint8_t)0xFD


  /*****************************************************************************
   * GENISYS - OTHER
   *****************************************************************************/

// Note: Genisys Control & Indication Byte Numbers 244 - 255 Are Reserved For Status Information.
#define GEN_MAX_BYTES (uint8_t)223
#define GEN_MODE_BYTE (uint8_t)0xE0

// Note: The mode byte return is normally based on internal settings
// withen the slave device. This is a temp hard coded value to use before the settings
// feature is implemented.
#define GEN_MODE_BYTE_RETURN_VALUE (uint8_t)0x04


namespace LSY
{

	bool ProtocolGenisysSlave::AddDataFrame(uint8_t slave_id, bool is_control_frame, std::shared_ptr<DataFrame> & data_frame_obj)
	{
		std::map<uint8_t, std::shared_ptr<DataFrame>> * data_frames = &indication_data_frames;
		if (is_control_frame)
			data_frames = &control_data_frames;


		if (slave_id == 0 || data_frames->count(slave_id))
		{
			Logging::LogErrorF("ProtocolGenisysSlave::AddDataFrame: Invalid/Duplicate Slave ID [%d] (Is Control Frame = [%d])", slave_id, is_control_frame);
			return false;
		}

		uint64_t table_size = data_frame_obj->GetNumBytes();
		if (table_size > GEN_MAX_BYTES)
		{
			Logging::LogErrorF("ProtocolGenisysSlave::AddDataFrame: Table Size Is Too Large [%d] Slave ID [%d] (Is Control Frame = [%d])", table_size, slave_id, is_control_frame);
			return false;
		}

		// To Do - Check that "data_frame_obj" raw pointer is not already used in
		// "indication_data_frames" or "control_data_frames".

		(*data_frames)[slave_id] = data_frame_obj;
		return true;
	}

	bool ProtocolGenisysSlave::ProcessMessages(std::vector<uint8_t> & data_buffer)
	{

		if (!SeperateMessages(data_buffer))
		{
			return false;
		}

		data_buffer.clear();

		for (size_t i = 0; i < input_messages.size(); i++)
		{
			std::vector<uint8_t> responce_message;

			if (!ProcessMessage(input_messages[i], responce_message))
			{
				continue;
			}

			data_buffer.insert(data_buffer.end(), responce_message.begin(), responce_message.end());
		}

		return true;
	}




	bool ProtocolGenisysSlave::SeperateMessages(std::vector<uint8_t> & data_buffer)
	{

		input_messages.clear();

		// Separate "data_buffer" by creating a seperate message
		// divided by at each end of message char 0xF6

		// To Do:
		// Put a limit for the number of buffered messages that can be received.
		// If buffer is full pushout the oldest message.


		bool end_msg_found = false;
		std::vector<uint8_t>::iterator start_msg_itr = data_buffer.begin();
		
		for (auto i = data_buffer.begin(); i != data_buffer.end(); i++)
		{
			if (end_msg_found)
			{
				start_msg_itr = i;
				end_msg_found = false;
			}

			if (*i == GEN_MSG_TYPE_END_MSG)
			{
				std::vector<uint8_t> msg(start_msg_itr, (i+1));
				input_messages.push_back(msg);
				end_msg_found = true;
			}
		}

		return true;
	}

	bool ProtocolGenisysSlave::ProcessMessage(std::vector<uint8_t> & gen_msg_buffer, std::vector<uint8_t> & gen_msg_responce)
	{

		static int msg_count = 0;

		gen_msg_responce.clear();

		// Check For A Valid Message:
		if (!IsMasterMessageValid(gen_msg_buffer))
			return false;

		Logging::LogDebugF("ProtocolGenisysSlave::ProcessMessage: From Master (Including ESCAPE Chars)");
		LogGenisysMsg(gen_msg_buffer);

		// Remove Genisys Escape Characters (e.g 0xF00C -> 0xFC)
		// This will only ever reduce the size of "gen_msg_buffer"
		if (!RemoveEscapeChars(gen_msg_buffer))
		{
			Logging::LogErrorF("ProtocolGenisysSlave::ProcessMessage: Failed To Remove ESCAPE Chars");
			return false;
		}
			

		Logging::LogDebugF("ProtocolGenisysSlave::ProcessMessage: From Master (Excluding ESCAPE Chars)");
		LogGenisysMsg(gen_msg_buffer);





		// Get Genisys Header Byte
		uint8_t msg_type = gen_msg_buffer[GEN_MSG_TYPE_BYTE];
		uint8_t slave_address_from_master = gen_msg_buffer[GEN_SLAVE_ADDR_BYTE];
		uint8_t crc_low = 0;
		uint8_t crc_high = 0;


		// Check That Slave Address From Master Exists In Stored DataFrames
		if (!indication_data_frames.count(slave_address_from_master) && !control_data_frames.count(slave_address_from_master))
		{
			// Disregard Message As The Slave ID Doesn't Match
			// To Do: Check For Broadcast Slave ID 0

			Logging::LogWarningF("ProtocolGenisysSlave::ProcessMessage: No Data For Slave ID [%d]", slave_address_from_master);
			return false;
		}




		if (msg_type == GEN_MSG_TYPE_POLL)
		{
			crc_low = gen_msg_buffer[2];
			crc_high = gen_msg_buffer[3];

			// To Do: Check That CRC From Master Is Correct
			// Also Take Non-Secure Polls Into Account




			// Check For Table Updates
			std::vector<uint64_t> byte_offsets;
			std::vector<uint8_t> values;
			if (!indication_data_frames[slave_address_from_master]->GetUpdates(byte_offsets, values))
			{
				// Warning
				Logging::LogWarningF("ProtocolGenisysSlave::ProcessMessage: Failed To Get Data Updates For Slave ID [%d]", slave_address_from_master);
				return false;
			}

			bool is_updates = !values.empty();

			// Send "Acknowledge Master Message" Back To Master
			if (!is_updates)
			{
				gen_msg_responce.push_back(GEN_MSG_TYPE_ACK_MASTER);
				gen_msg_responce.push_back(slave_address_from_master);
			}

			// Send "Indication Data Response" Back To Master
			else 
			{
				gen_msg_responce.push_back(GEN_MSG_TYPE_IND_RESP);
				gen_msg_responce.push_back(slave_address_from_master);

				for (size_t i = 0; i < byte_offsets.size(); i++)
				{
					// Pack updated table data into bytes
					gen_msg_responce.push_back((uint8_t)byte_offsets[i]);	// Register Number (Byte Offset)
					gen_msg_responce.push_back((uint8_t)values[i]);			// Register Value
				}

				// Calculate CRC
				uint8_t crc_low = 0;
				uint8_t crc_high = 0;
				if (!CalculateCRC(gen_msg_responce, &crc_low, &crc_high))
					return false;
					
				gen_msg_responce.push_back((uint8_t)crc_low);
				gen_msg_responce.push_back((uint8_t)crc_high);
			}
		}



		else if (msg_type == GEN_MSG_TYPE_ACK_SLAVE)
		{
			crc_low = gen_msg_buffer[2];
			crc_high = gen_msg_buffer[3];

			// Notify User Code That The Master Has Received Our Last Update
			// We Can Reset Our User Updates Now
			indication_data_frames[slave_address_from_master]->DataAck();

			// Send "Acknowledge Master Message" Back To Master
			gen_msg_responce.push_back(GEN_MSG_TYPE_ACK_MASTER);
			gen_msg_responce.push_back(slave_address_from_master);
		}



		else if (msg_type == GEN_MSG_TYPE_RECALL)
		{
			crc_low = gen_msg_buffer[2];
			crc_high = gen_msg_buffer[3];

			// Get All Data From User Code
			std::vector<uint64_t> byte_offsets;
			std::vector<uint8_t> values;
			if(!indication_data_frames[slave_address_from_master]->GetUpdates(byte_offsets, values, true))
			{
				// Warning
				Logging::LogWarningF("ProtocolGenisysSlave::ProcessMessage: Failed To Get Data Updates For Slave ID [%d]", slave_address_from_master);
				return false;
			}

			// Create Response Message
			gen_msg_responce.push_back(GEN_MSG_TYPE_IND_RESP);
			gen_msg_responce.push_back(slave_address_from_master);

			for (size_t i = 0; i < byte_offsets.size(); i++)
			{
				// Pack updated table data into bytes
				gen_msg_responce.push_back((uint8_t)byte_offsets[i]);	// Register Number (Byte Offset)
				gen_msg_responce.push_back((uint8_t)values[i]);			// Register Value
			}

			// Mode Byte (From Example)
			gen_msg_responce.push_back(GEN_MODE_BYTE);
			gen_msg_responce.push_back(GEN_MODE_BYTE_RETURN_VALUE);

			// Calculate CRC
			uint8_t crc_low = 0;
			uint8_t crc_high = 0;
			if (!CalculateCRC(gen_msg_responce, &crc_low, &crc_high))
				return false;

			gen_msg_responce.push_back(crc_low);
			gen_msg_responce.push_back(crc_high);
		}



		else if (msg_type == GEN_MSG_TYPE_CONTROL)
		{
	
			// Check: Message Should Be At Least 7 Bytes Long
			// FC SLAVE_ID MODE_BYTE_OFFSET MODE_BYTE_VALUE CRC_LOW CRC_HIGH F6
			if (gen_msg_buffer.size() < 7)
			{
				// Error - Control Message Should Be 7 Bytes Long Or More
				return false;
			}
			
			// Check: Message Should Have An Odd Number Of Bytes
			if ((gen_msg_buffer.size() % 2) != 1)
			{
				// Error - Control Message Should Contain An Odd Number Of Bytes
				return false;
			}
			
			// Remove Message Byte & Slave ID Byte (First 2 Bytes)
			gen_msg_buffer.erase(gen_msg_buffer.begin());
			gen_msg_buffer.erase(gen_msg_buffer.begin());

			// Remove End Of Message Byte
			// This Has Been Check As "F6" In IsMasterMessageValid()
			gen_msg_buffer.pop_back();

			// Get CRC Bytes From Last 2 Chars
			// To Do: Should Check CRC Is Correct
			crc_high = gen_msg_buffer.back(); gen_msg_buffer.pop_back();
			crc_low = gen_msg_buffer.back(); gen_msg_buffer.pop_back();
			

			// Control Bytes And Values
			// Loop Through Remaining Data And Save Updates
			std::vector<uint64_t> byte_offsets;
			std::vector<uint8_t> values;
			for (int i = 0; i < gen_msg_buffer.size(); i++)
			{
				if (i % 2)	
				{
					// Odd Number Index
					values.push_back(gen_msg_buffer[i]);
				}
					
				else
				{
					// Even Number Index
					byte_offsets.push_back(gen_msg_buffer[i]);
				}		
			}


			// Check For Mode Byte & Mode Byte Value
			// Note: Mode Bytes Will Be At The End, Maybe Loop From Back To Front
			uint8_t mode_byte_value = 0;
			for (int i = 0; i < values.size(); i++)
			{
				if (byte_offsets[i] == GEN_MODE_BYTE)
				{
					mode_byte_value = values[i];
					byte_offsets.erase(byte_offsets.begin() + i);	// Remove Mode Byte From byte_offsets
					values.erase(values.begin() + i);				// Remove Mode Byte From values
					break;
				}
			}



			// To Do: Create A Different "WriteBytes" Function For The The Interface To Genisys Protocol
			for (int i = 0; i < values.size(); i++)
			{
				if (!control_data_frames[slave_address_from_master]->WriteByte(byte_offsets[i], values[i]))
				{
					// Warning
					Logging::LogWarningF("ProtocolGenisysSlave::ProcessMessage: Failed To Write Controls For Slave ID [%d]", slave_address_from_master);
					return false;
				}
			}



			// Construct Responce Message
			// Slave responce format regardless of how many bytes recieved
			// F2 SLAVE_ID MODE_BYTE_OFFSET MODE_BYTE_VALUE CRC_LOW CRC_HIGH F6

			// Master Ack Slave :
			// Example: fa52c35df6

			// Slave Ack Master :
			// Exampe: f152f6

			// Construct Responce Message
			gen_msg_responce.push_back(GEN_MSG_TYPE_IND_RESP);
			gen_msg_responce.push_back(slave_address_from_master);
			gen_msg_responce.push_back(GEN_MODE_BYTE);
			gen_msg_responce.push_back(GEN_MODE_BYTE_RETURN_VALUE);

			// Calculate CRC
			uint8_t crc_low = 0;
			uint8_t crc_high = 0;
			if (!CalculateCRC(gen_msg_responce, &crc_low, &crc_high))
				return false;

			gen_msg_responce.push_back(crc_low);
			gen_msg_responce.push_back(crc_high);

		}



		else
		{
			Logging::LogDebugF("ProtocolGenisysSlave::ProcessMessage: [%d] Unable To Handle Message", slave_address_from_master);
			return false;
		}

		Logging::LogDebugF("ProtocolGenisysSlave::ProcessMessage: [%d] To Master (Excluding ESCAPE Chars)", slave_address_from_master);
		LogGenisysMsg(gen_msg_responce);

		AddEscapeChars(gen_msg_responce);
		gen_msg_responce.push_back(GEN_MSG_TYPE_END_MSG);

		Logging::LogDebugF("ProtocolGenisysSlave::ProcessMessage: [%d] To Master (Including ESCAPE Chars)", slave_address_from_master);
		LogGenisysMsg(gen_msg_responce);

		return true;
	}

	bool ProtocolGenisysSlave::IsMasterMessageValid(const std::vector<uint8_t> & gen_msg_buffer)
	{

		if (gen_msg_buffer.empty())
		{
			// Error - Message Buffer Is Empty
			Logging::LogDebugF("ProtocolGenisysSlave::IsMasterMessageValid: Message Buffer Is Empty");
			return false;
		}

		if (gen_msg_buffer.size() < 5)
		{
			// Error - Message From Master Is Too Small
			Logging::LogDebugF("ProtocolGenisysSlave::IsMasterMessageValid: Message From Master Is Too Small");
			return false;
		}

		if ((gen_msg_buffer[0] & 0xF0) != 0xF0)
		{
			// Error - First Nimble Of Message MUST Be 0xF
			Logging::LogDebugF("ProtocolGenisysSlave::IsMasterMessageValid: First Nimble Of Message MUST Be 0xF");
			return false;
		}

		// Check End Of Message Char
		if (gen_msg_buffer.back() != GEN_MSG_TYPE_END_MSG)
		{
			// Error - Genisys End Message NOT Found
			Logging::LogDebugF("ProtocolGenisysSlave::IsMasterMessageValid: Genisys End Message NOT Found");
			return false;
		}

		return true;
	}

	bool ProtocolGenisysSlave::RemoveEscapeChars(std::vector<uint8_t> & gen_msg_buffer)
	{
		// Remove Genisys Escape Characters (e.g 0xF00C -> 0xFC)
		// This will only ever reduce the size of "gen_msg_buffer"

		std::vector<uint8_t> new_buff;
		new_buff.push_back(gen_msg_buffer[0]);

		for (int i = 1; i < (gen_msg_buffer.size() - 1); i++)
		{
			uint8_t byte = 0;
			if (gen_msg_buffer[i] == 0xF0)
			{
				// Add 0xF To First Nibble Of The Next Byte
				// Note: Should Also Check That The Next Bytes First Nibble Is 0
				i++;
				byte = 0xF0 + gen_msg_buffer[i];
			}
			else
				byte = gen_msg_buffer[i];

			new_buff.push_back(byte);
		}

		new_buff.push_back(gen_msg_buffer.back());

		// Add Values From "new_buff" Back Into "gen_msg_buffer"
		gen_msg_buffer = new_buff;

		return !gen_msg_buffer.empty();
	}


	void ProtocolGenisysSlave::AddEscapeChars(std::vector<uint8_t> & gen_msg_buffer)
	{
		std::vector<uint8_t> temp_buff;
		temp_buff.push_back(gen_msg_buffer[0]); // Header Byte

		for (int i = 1; i < gen_msg_buffer.size(); i++)
		{
			uint8_t byte = gen_msg_buffer[i];

			if ((byte & 0xF0) == 0xF0)
			{
				temp_buff.push_back(0xF0);
				temp_buff.push_back(byte & 0x0F);
			}
			else
			{
				temp_buff.push_back(byte);
			}
		}

		gen_msg_buffer = temp_buff;
	}

	bool ProtocolGenisysSlave::CalculateCRC(const std::vector<uint8_t> & gen_msg_buffer, uint8_t *crc_low, uint8_t *crc_high)
	{
		uint16_t crc = 0;

		uint8_t *data = (uint8_t *)gen_msg_buffer.data();
		size_t len = gen_msg_buffer.size();

		if (data == NULL)
		{
			Logging::LogErrorF("ProtocolGenisysSlave::CalculateCRC: Failed To Calculate CRC");
			return false;
		}	

		for (size_t i = 0; i < len; i++)
		{
			crc ^= data[i];
			for (uint8_t k = 0; k < 8; k++)
				crc = crc & 1 ? (crc >> 1) ^ 0xa001 : crc >> 1;
		}

		*crc_high = crc >> 8;
		*crc_low = crc & (uint16_t)0x00FF;

		return true;
	}

	void ProtocolGenisysSlave::LogGenisysMsg(const std::vector<uint8_t> & gen_msg_buffer)
	{
		// To Do: Size check log_msg_buffer
		// This will caase a crash if a large enough "gen_msg_buffer" is received.

		char log_msg_buffer[1000];
		size_t log_msg_buffer_size = 980;

		sprintf_s(log_msg_buffer, log_msg_buffer_size, "GENISYS MESSAGE HEX: ");
		for (int i = 0; i < gen_msg_buffer.size(); i++)
			sprintf_s(log_msg_buffer, log_msg_buffer_size, "%s%02x ", log_msg_buffer, gen_msg_buffer[i]);


		std::string str = log_msg_buffer;
		Logging::LogDebug(str);
	}

}