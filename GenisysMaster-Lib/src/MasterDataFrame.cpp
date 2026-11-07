#include "GenisysMaster-Lib-Logging.h"
#include "MasterDataFrame.h"

namespace LSY
{

	MasterDataFrame::MasterDataFrame(std::string name, uint64_t num_bytes)	:
		name_(name), num_bytes_(num_bytes)
	{
		update_pending_ = false;
		pending_all_data_ = false;

		bytes_api = std::vector<uint8_t>(num_bytes_, 0);
		bytes_tx  = std::vector<uint8_t>(num_bytes_, 0);
		bytes_ack = std::vector<uint8_t>(num_bytes_, 0);


		last_master_ip = "";
		responce_counter = 0;
	}

	bool MasterDataFrame::AddEventCallback(std::function<void(std::string, LSY::MasterDataFrame::CALLBACKEVENT)> & func)
	{
		callback_ = func;
		return true;
	}

	std::string MasterDataFrame::GetName()
	{
		return name_;
	}

	uint64_t MasterDataFrame::GetNumBytes()
	{
		return num_bytes_;
	}

	std::string MasterDataFrame::GetLastMasterIp()
	{
		return last_master_ip;
	}

	uint64_t MasterDataFrame::GetResponceCounter()
	{
		return responce_counter;
	}



	/* ---------- User API ---------- */

	bool MasterDataFrame::WriteBit(uint64_t byte_offset, uint8_t bit_offset, bool value)
	{

		if (byte_offset >= num_bytes_ || bit_offset >= 8)
		{
			// Error - byte_offset/bit_offset Out Of Range
			MasterLogging::LogErrorF("MasterDataFrame::WriteBit: Out of Range, byte_offset [%llu], bit_offset [%u]", static_cast<unsigned long long>(byte_offset), static_cast<unsigned>(bit_offset));
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
		return true; // OK
	}

	bool MasterDataFrame::WriteByte(uint64_t byte_offset, uint8_t value)
	{
		if (byte_offset >= num_bytes_)
		{
			// Error - byte_offset Out Of Range
			MasterLogging::LogErrorF("MasterDataFrame::WriteByte: Out of Range, byte_offset [%llu]", static_cast<unsigned long long>(byte_offset));
			return false;
		}

		if (bytes_api[byte_offset] == value)
			return true; // OK - No Change

		bytes_api[byte_offset] = value;
		return true; // OK
	}

	bool MasterDataFrame::WriteBytes(uint64_t byte_offset, std::vector<uint8_t>values)
	{
		if (values.empty())
		{
			// Error - No Data Values Provided
			MasterLogging::LogErrorF("MasterDataFrame::WriteBytes: No Values Provided");
			return false;
		}

		if (byte_offset > num_bytes_ || values.size() > num_bytes_ - byte_offset)
		{
			// Error - Data Out of Range
			MasterLogging::LogErrorF("MasterDataFrame::WriteBytes: Out of Range");
			return false;
		}

		for (uint64_t i = 0; i < values.size(); i++)
		{
			if (bytes_api[byte_offset + i] == values[i])
				continue;

			bytes_api[byte_offset + i] = values[i];
		}


		return true;
	}



	bool MasterDataFrame::ReadBit(uint64_t byte_offset, uint8_t bit_offset, bool & value) const
	{
		if (byte_offset >= num_bytes_ || bit_offset >= 8)
		{
			// Error - Data Out of Range
			MasterLogging::LogErrorF("MasterDataFrame::ReadBit: Out of Range, byte_offset [%llu], bit_offset [%u]", static_cast<unsigned long long>(byte_offset), static_cast<unsigned>(bit_offset));
			return false;
		}

		uint8_t byte = bytes_api[byte_offset];
		uint8_t mask = 1 << bit_offset;

		byte = byte & mask;
		byte = byte >> bit_offset;

		value = byte;
		return true;
	}

	bool MasterDataFrame::ReadByte(uint64_t byte_offset, uint8_t & value) const
	{
		if (byte_offset >= num_bytes_)
		{
			// Error - byte_offset Out Of Range
			MasterLogging::LogErrorF("MasterDataFrame::ReadByte: Out of Range, byte_offset [%llu]", static_cast<unsigned long long>(byte_offset));
			return false;
		}

		value = bytes_api[byte_offset];
		return true;
	}

	bool MasterDataFrame::ReadBytes(uint64_t byte_offset, uint64_t num_bytes, std::vector<uint8_t> & values) const
	{
		values.clear();

		if (byte_offset > num_bytes_ || num_bytes > num_bytes_ - byte_offset)
		{
			// Error - Data Out of Range
			MasterLogging::LogErrorF("MasterDataFrame::ReadBytes: Out of Range");
			return false;
		}

		for (uint64_t i = 0; i < num_bytes; i++)
			values.push_back(bytes_api[byte_offset + i]);

		return true;
	}

	uint8_t MasterDataFrame::operator[](uint64_t byte_offset) const
	{
		uint8_t value = 0;
		ReadByte(byte_offset, value);
		return value;
	}









	/* ---------- Interface To Genisys Protocol ---------- */


	bool MasterDataFrame::GetUpdates(std::vector<uint64_t> & byte_offsets, std::vector<uint8_t> & values, bool all_data_flag)
	{
		byte_offsets.clear();
		values.clear();

		if (!update_pending_)
		{
			bytes_tx = bytes_api;
			pending_all_data_ = all_data_flag;
		}

		if (pending_all_data_)
		{
			for (uint64_t i = 0; i < bytes_tx.size(); i++)
			{
				byte_offsets.push_back(i);
				values.push_back(bytes_tx[i]);
			}
		}
		else if (!FindChanges(bytes_ack, bytes_tx, byte_offsets, values))
			return false;

		update_pending_ = !values.empty();
		return true;
	}

	bool MasterDataFrame::DataAck()
	{
		if (!update_pending_)
			return false;

		// Acknowledge only the prepared snapshot, preserving newer user writes.
		bytes_ack = bytes_tx;
		update_pending_ = false;
		pending_all_data_ = false;
		return true;
	}

	void MasterDataFrame::CancelUpdate()
	{
		update_pending_ = false;
		pending_all_data_ = false;
	}

	bool MasterDataFrame::HasModeByte() const
	{
		return has_mode_byte_;
	}

	uint8_t MasterDataFrame::GetModeByte() const
	{
		return mode_byte_;
	}


	bool MasterDataFrame::FindChanges(std::vector<uint8_t> & bytes_old, std::vector<uint8_t> & bytes_new, std::vector<uint64_t> & byte_offsets, std::vector<uint8_t> & values)
	{

		byte_offsets.clear();
		values.clear();

		if (bytes_old.size() != bytes_new.size())
		{
			// Error - Byte Vectors To Compare Are Of Different Size
			MasterLogging::LogErrorF("MasterDataFrame::FindChanges: Out of Range");
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

	void MasterDataFrame::SetLastMasterIp(std::string master_ip)
	{
		last_master_ip = master_ip;
	}

	void MasterDataFrame::IncrementResponceCounter()
	{
		responce_counter++;
	}


	int MasterDataFrame::RecvBytes(std::vector<uint64_t> & byte_offsets, std::vector<uint8_t> & values)
	{
		if (byte_offsets.size() != values.size())
			return -1;

		// Validate the complete payload before changing any local data.
		for (uint64_t offset : byte_offsets)
		{
			if (offset != 0xE0 && (offset >= num_bytes_ || offset >= 0xE0))
				return -1;
		}

		auto updated_bytes = bytes_api;
		uint8_t updated_mode = mode_byte_;
		bool mode_received = false;
		for (size_t i = 0; i < values.size(); i++)
		{
			if (byte_offsets[i] == 0xE0)
			{
				updated_mode = values[i];
				mode_received = true;
			}
			else
				updated_bytes[byte_offsets[i]] = values[i];
		}

		const bool indication_changed = updated_bytes != bytes_api;
		const bool mode_changed = mode_received && (!has_mode_byte_ || updated_mode != mode_byte_);
		bytes_api = updated_bytes;
		// Received indications are a baseline, never outgoing control changes.
		bytes_tx = updated_bytes;
		bytes_ack = updated_bytes;
		update_pending_ = false;
		pending_all_data_ = false;
		if (mode_received)
		{
			mode_byte_ = updated_mode;
			has_mode_byte_ = true;
		}

		// Copy the handler so a callback can safely replace its registration.
		auto callback = callback_;
		if (callback && indication_changed)
			callback(name_, CALLBACKEVENT::INDICATION_CHANGE);
		if (callback && mode_changed)
			callback(name_, CALLBACKEVENT::MODE_CHANGE);

		return static_cast<int>(values.size());
	}
}
