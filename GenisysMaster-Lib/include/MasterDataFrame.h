#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <functional>

namespace LSY
{
	class MasterDataFrame
	{

	public:

		enum class CALLBACKEVENT
		{
			DEFAULT = 0,			// Generaly means something went wrong
			INDICATION_CHANGE = 1,	// Indication values changed after a received update
			MODE_CHANGE = 2			// Mode byte changed independently of indications
		};

	private:

		std::string name_;
		uint64_t num_bytes_;

		bool update_pending_;
		bool pending_all_data_;
		uint8_t mode_byte_ = 0;
		bool has_mode_byte_ = false;

		std::vector<uint8_t> bytes_api;	// Desired controls or received indications
		std::vector<uint8_t> bytes_tx;	// Prepared controls awaiting slave acknowledgement
		std::vector<uint8_t> bytes_ack;	// Controls acknowledged by the slave

		std::string last_master_ip;		// IP address that the last responce message was sent to from this frame
		uint64_t responce_counter;		// Counter for the number of responce messages sent from this frame

		std::function<void(std::string, LSY::MasterDataFrame::CALLBACKEVENT)> callback_;


	public: // User API

		MasterDataFrame(std::string name, uint64_t num_bytes);
		bool AddEventCallback(std::function<void(std::string, LSY::MasterDataFrame::CALLBACKEVENT)> & func);

		std::string GetName();
		uint64_t GetNumBytes();
		std::string GetLastMasterIp();
		uint64_t GetResponceCounter();

		// Write methods stage outgoing control values; they do not fire callbacks.
		bool WriteBit(uint64_t byte_offset, uint8_t bit_offset, bool value);
		bool WriteByte(uint64_t byte_offset, uint8_t value);
		bool WriteBytes(uint64_t byte_offset, std::vector<uint8_t>values);

		bool ReadBit(uint64_t byte_offset, uint8_t bit_offset, bool & value) const;
		bool ReadByte(uint64_t byte_offset, uint8_t & value) const;
		bool ReadBytes(uint64_t byte_offset, uint64_t num_bytes, std::vector<uint8_t> & values) const;
		bool HasModeByte() const;
		uint8_t GetModeByte() const;

		uint8_t operator[](uint64_t byte_offset) const; // ReadByte



	public: // Interface To Genisys Protocol

		// Freeze a control snapshot until DataAck(); repeated calls return the same update.
		bool GetUpdates(std::vector<uint64_t> & byte_offsets, std::vector<uint8_t> & values, bool all_data_flag = false);
		bool DataAck();
		// Release an abandoned snapshot without changing acknowledged data.
		void CancelUpdate();
		bool FindChanges(std::vector<uint8_t> & bytes_old, std::vector<uint8_t> & bytes_new, std::vector<uint64_t> & byte_offsets, std::vector<uint8_t> & values);

		void SetLastMasterIp(std::string master_ip);
		void IncrementResponceCounter();

		// Apply an indication payload all-or-nothing. E0 is the separate mode byte.
		// Returns pair count (including mode), or -1 for an invalid payload.
		// Callbacks run after all values are applied, on the calling thread.
		int RecvBytes(std::vector<uint64_t> & byte_offsets, std::vector<uint8_t> & values);

	};
}
