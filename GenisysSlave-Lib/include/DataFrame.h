#pragma once

#include <string>
#include <vector>

namespace LSY
{
	class DataFrame
	{

	private:

		std::string name_;
		uint64_t num_bytes_;

		uint8_t tx_cnt;

		std::vector<uint8_t> bytes_api;	// Data From User API
		std::vector<uint8_t> bytes_tx;	// Data Transmitted To Master (Not Yet Acknowledged)
		std::vector<uint8_t> bytes_ack;	// Data Acknowledged From Master

		uint64_t seq_id_api;
		uint64_t seq_id_tx;
		uint64_t seq_id_ack;


	public: // User API

		DataFrame(std::string name, uint64_t num_bytes);
		std::string GetName();
		uint64_t GetNumBytes();

		bool WriteBit(uint64_t byte_offset, uint8_t bit_offset, bool value);
		bool WriteByte(uint64_t byte_offset, uint8_t value);
		bool WriteBytes(uint64_t byte_offset, std::vector<uint8_t>values);

		bool ReadBit(uint64_t byte_offset, uint8_t bit_offset, bool & value) const;
		bool ReadByte(uint64_t byte_offset, uint8_t & value) const;
		bool ReadBytes(uint64_t byte_offset, uint64_t num_bytes, std::vector<uint8_t> & values) const;
		uint8_t operator[](uint64_t byte_offset) const; // ReadByte



	public: // Interface To Genisys Protocol

		bool GetUpdates(std::vector<uint64_t> & byte_offsets, std::vector<uint8_t> & values, bool all_data_flag = false);
		bool DataAck();
		bool FindChanges(std::vector<uint8_t> & bytes_old, std::vector<uint8_t> & bytes_new, std::vector<uint64_t> & byte_offsets, std::vector<uint8_t> & values);

	};
}


