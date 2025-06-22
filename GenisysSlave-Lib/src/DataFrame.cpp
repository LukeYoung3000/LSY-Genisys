#include "GenisysSlave-Lib-Logging.h"
#include "DataFrame.h"

namespace LSY
{

	DataFrame::DataFrame(std::string name, uint64_t num_bytes)	:
		name_(name), num_bytes_(num_bytes)
	{
		tx_cnt = 0;

		bytes_api = std::vector<uint8_t>(num_bytes_, 0);
		bytes_tx  = std::vector<uint8_t>(num_bytes_, 0);
		bytes_ack = std::vector<uint8_t>(num_bytes_, 0);

		seq_id_api = 0;
		seq_id_tx  = 0;
		seq_id_ack = 0;

		last_master_ip = "";
		responce_counter = 0;
	}

	bool DataFrame::AddEventCallback(std::function<void(std::string, LSY::DataFrame::CALLBACKEVENT)> & func)
	{
		callback_ = func;
		return true;
	}

	std::string DataFrame::GetName()
	{
		return name_;
	}

	uint64_t DataFrame::GetNumBytes()
	{
		return num_bytes_;
	}

	std::string DataFrame::GetLastMasterIp()
	{
		return last_master_ip;
	}

	uint64_t DataFrame::GetResponceCounter()
	{
		return responce_counter;
	}



	/* ---------- User API ---------- */

	bool DataFrame::WriteBit(uint64_t byte_offset, uint8_t bit_offset, bool value)
	{

		if (byte_offset >= num_bytes_ || bit_offset >= 8)
		{
			// Error - byte_offset/bit_offset Out Of Range
			Logging::LogErrorF("DataFrame::WriteBit: Out of Range, byte_offset [%d], bit_offset [%d]", byte_offset, bit_offset);
			return false;
		}

		uint8_t current_byte = bytes_api[byte_offset];
		uint8_t new_byte = bytes_api[byte_offset];
		uint8_t mask = 1 << bit_offset;

		if (value == true)
			new_byte = new_byte | mask;
		else
			new_byte = new_byte & ~mask;

		if (current_byte == new_byte)
			return true; // OK - No Change

		bytes_api[byte_offset] = new_byte;
		seq_id_api++;
		return true; // OK
	}

	bool DataFrame::WriteByte(uint64_t byte_offset, uint8_t value)
	{
		if (byte_offset >= num_bytes_)
		{
			// Error - byte_offset Out Of Range
			Logging::LogErrorF("DataFrame::WriteByte: Out of Range, byte_offset [%d]", byte_offset);
			return false;
		}

		if (bytes_api[byte_offset] == value)
			return true; // OK - No Change

		bytes_api[byte_offset] = value;
		seq_id_api++;
		return true; // OK
	}

	bool DataFrame::WriteBytes(uint64_t byte_offset, std::vector<uint8_t>values)
	{
		if (values.empty())
		{
			// Error - No Data Values Provided
			Logging::LogErrorF("DataFrame::WriteBytes: No Values Provided");
			return false;
		}

		if ((byte_offset + values.size()) > num_bytes_)
		{
			// Error - Data Out of Range
			Logging::LogErrorF("DataFrame::WriteBytes: Out of Range");
			return false;
		}

		bool change_found = false;
		for (uint64_t i = 0; i < values.size(); i++)
		{
			if (bytes_api[byte_offset + i] == values[i])
				continue;

			change_found = true;
			bytes_api[byte_offset + i] = values[i];
		}

		if (change_found)
			seq_id_api++;

		return true;
	}



	bool DataFrame::ReadBit(uint64_t byte_offset, uint8_t bit_offset, bool & value) const
	{
		if (byte_offset >= num_bytes_ || bit_offset >= 8)
		{
			// Error - Data Out of Range
			Logging::LogErrorF("DataFrame::ReadBit: Out of Range, byte_offset [%d], bit_offset [%d]", byte_offset, bit_offset);
			return false;
		}

		uint8_t byte = bytes_api[byte_offset];
		uint8_t mask = 1 << bit_offset;

		byte = byte & mask;
		byte = byte >> bit_offset;

		value = byte;
		return true;
	}

	bool DataFrame::ReadByte(uint64_t byte_offset, uint8_t & value) const
	{
		if (byte_offset >= num_bytes_)
		{
			// Error - byte_offset Out Of Range
			Logging::LogErrorF("DataFrame::ReadByte: Out of Range, byte_offset [%d]", byte_offset);
			return false;
		}

		value = bytes_api[byte_offset];
		return true;
	}

	bool DataFrame::ReadBytes(uint64_t byte_offset, uint64_t num_bytes, std::vector<uint8_t> & values) const
	{
		values.clear();

		if ((byte_offset + num_bytes) > num_bytes_)
		{
			// Error - Data Out of Range
			Logging::LogErrorF("DataFrame::ReadBytes: Out of Range");
			return false;
		}

		for (uint64_t i = 0; i < num_bytes; i++)
			values.push_back(bytes_api[byte_offset + i]);

		return true;
	}

	uint8_t DataFrame::operator[](uint64_t byte_offset) const
	{
		uint8_t value = 0;
		ReadByte(byte_offset, value);
		return value;
	}









	/* ---------- Interface To Genisys Protocol ---------- */


	bool DataFrame::GetUpdates(std::vector<uint64_t> & byte_offsets, std::vector<uint8_t> & values, bool all_data_flag)
	{
		// This method is expected to be called when Genisys Master sends a poll/role call request

		byte_offsets.clear();
		values.clear();


		if (all_data_flag)
		{
			// This Is The Response To A Genisys RECALL Message

			tx_cnt = 0;
			seq_id_tx = seq_id_api;
			bytes_tx = bytes_api;

			for (uint64_t i = 0; i < bytes_tx.size(); i++)
			{
				byte_offsets.push_back(i);
				values.push_back(bytes_tx[i]);
			}

			return true;
		}


		if (seq_id_api == seq_id_ack)
		{
			// Nothing Has Changed
			// User Data Is The Same As Master Data
			tx_cnt = 0;
			return true;
		}


		if (seq_id_tx != seq_id_ack)
		{
			// Currently In The Process Of Waiting For
			// Acknowledge From Last Transmit To Master

			tx_cnt++;
			if (tx_cnt <= 3)
			{
				return FindChanges(bytes_ack, bytes_tx, byte_offsets, values);
			}
				
			else
			{
				// Slave Has Transmitted The Same Data Over 3 Times
				// Treat Data As If Master Has Send An Ack
				return DataAck();
			}		
		}


		if (!FindChanges(bytes_tx, bytes_api, byte_offsets, values))
		{
			// Sequence_ID_API has been updated but no data has changed.
			seq_id_tx = seq_id_api;
			seq_id_ack = seq_id_api;
			return true;
		}
		else
		{
			// An Update Has Been Received By User Data
			seq_id_tx = seq_id_api;
			bytes_tx = bytes_api;
			tx_cnt = 1;
		}
		
		return true;
	}


	bool DataFrame::DataAck()
	{
		// Last Transmitted Data Has Been Acknowledged By Master
		seq_id_ack = seq_id_tx;
		bytes_ack = bytes_tx;
		tx_cnt = 0;
		return true;
	}


	bool DataFrame::FindChanges(std::vector<uint8_t> & bytes_old, std::vector<uint8_t> & bytes_new, std::vector<uint64_t> & byte_offsets, std::vector<uint8_t> & values)
	{

		byte_offsets.clear();
		values.clear();

		if (bytes_old.size() != bytes_new.size())
		{
			// Error - Byte Vectors To Compare Are Of Different Size
			Logging::LogErrorF("DataFrame::FindChanges: Out of Range");
			return false;
		}

		for (uint64_t i = 0; i < bytes_new.size(); i++)
		{
			if (bytes_old[i] != bytes_new[i])
			{
				byte_offsets.push_back(i);
				values.push_back(bytes_new[i]);
			}
		}

		return true;
	}

	void DataFrame::SetLastMasterIp(std::string master_ip)
	{
		last_master_ip = master_ip;
	}

	void DataFrame::IncrementResponceCounter()
	{
		responce_counter++;
	}


	int DataFrame::RecvBytes(std::vector<uint64_t> & byte_offsets, std::vector<uint8_t> & values)
	{
		if (byte_offsets.size() != values.size())
		{
			// Error - offsets and values need to be the same size
			return false;
		}

		int success_cnt = 0;
		for (int i = 0; i < values.size(); i++)
		{
			if (!WriteByte(byte_offsets[i], values[i]))
			{
				// Warning
				Logging::LogWarningF("DataFrame::RecvBytes: Failed To Write Controls For Data Table [%d]", name_);
			}
			else
			{
				success_cnt++;
			}
		}

		// Send a control change callback event to user
		if (callback_ != nullptr)
			callback_(name_, LSY::DataFrame::CALLBACKEVENT::CONTROL_CHANGE);


		return success_cnt;
	}

}



