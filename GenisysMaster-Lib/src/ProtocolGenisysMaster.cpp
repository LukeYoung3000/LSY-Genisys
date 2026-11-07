#include "ProtocolGenisysMaster.h"
#include "GenisysMaster-Lib-Logging.h"

namespace LSY
{
	bool ProtocolGenisysMaster::AddDataFrame(uint8_t slave_id, bool is_control_frame,
		std::shared_ptr<MasterDataFrame> & data_frame_obj)
	{
		if (slave_id == 0 || !data_frame_obj || data_frame_obj->GetNumBytes() == 0 ||
			data_frame_obj->GetNumBytes() > 224 || pending_requests_.count(slave_id))
			return false;

		// Sharing a frame between directions or stations would mix their baselines.
		for (const auto & entry : indication_data_frames)
			if (entry.second == data_frame_obj) return false;
		for (const auto & entry : control_data_frames)
			if (entry.second == data_frame_obj) return false;

		auto & frames = is_control_frame ? control_data_frames : indication_data_frames;
		return frames.emplace(slave_id, data_frame_obj).second;
	}

	uint16_t ProtocolGenisysMaster::CalculateCRC(const std::vector<uint8_t> & data)
	{
		uint16_t crc = 0;
		for (uint8_t byte : data)
		{
			crc ^= byte;
			for (int bit = 0; bit < 8; ++bit)
				crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
		}
		return crc;
	}

	std::vector<uint8_t> ProtocolGenisysMaster::Encode(std::vector<uint8_t> data, bool secure)
	{
		if (secure)
		{
			const uint16_t crc = CalculateCRC(data);
			data.push_back(static_cast<uint8_t>(crc));
			data.push_back(static_cast<uint8_t>(crc >> 8));
		}
		std::vector<uint8_t> wire{data[0]};
		for (size_t i = 1; i < data.size(); ++i)
		{
			if (data[i] >= 0xF0)
			{
				wire.push_back(0xF0);
				wire.push_back(data[i] & 0x0F);
			}
			else wire.push_back(data[i]);
		}
		wire.push_back(0xF6);
		return wire;
	}

	bool ProtocolGenisysMaster::Decode(const std::vector<uint8_t> & wire, std::vector<uint8_t> & data)
	{
		data.clear();
		if (wire.size() < 3 || wire.back() != 0xF6) return false;
		data.push_back(wire[0]);
		for (size_t i = 1; i + 1 < wire.size(); ++i)
		{
			uint8_t byte = wire[i];
			if (byte == 0xF0)
			{
				if (++i >= wire.size() - 1 || wire[i] > 0x0F) return false;
				byte = 0xF0 | wire[i];
			}
			else if (byte >= 0xF0) return false;
			data.push_back(byte);
		}
		return true;
	}

	bool ProtocolGenisysMaster::BeginRequest(uint8_t slave_id, REQUESTTYPE request,
		std::vector<uint8_t> & message)
	{
		message.clear();
		if (!indication_data_frames.count(slave_id) || pending_requests_.count(slave_id)) return false;
		pending_requests_.emplace(slave_id, request);
		return true;
	}

	bool ProtocolGenisysMaster::BuildPoll(uint8_t slave_id, std::vector<uint8_t> & message, bool secure)
	{
		if (!BeginRequest(slave_id, REQUESTTYPE::POLL, message)) return false;
		message = Encode({0xFB, slave_id}, secure);
		return true;
	}

	bool ProtocolGenisysMaster::BuildRecall(uint8_t slave_id, std::vector<uint8_t> & message)
	{
		if (!BeginRequest(slave_id, REQUESTTYPE::RECALL, message)) return false;
		message = Encode({0xFD, slave_id});
		return true;
	}

	bool ProtocolGenisysMaster::BuildAcknowledgeSlave(uint8_t slave_id, std::vector<uint8_t> & message)
	{
		if (!BeginRequest(slave_id, REQUESTTYPE::ACK_SLAVE, message)) return false;
		message = Encode({0xFA, slave_id});
		return true;
	}

	bool ProtocolGenisysMaster::BuildControl(uint8_t slave_id, std::vector<uint8_t> & message,
		bool all_data_flag, bool include_mode, uint8_t mode)
	{
		message.clear();
		if (!indication_data_frames.count(slave_id) || !control_data_frames.count(slave_id) ||
			pending_requests_.count(slave_id)) return false;
		std::vector<uint64_t> offsets;
		std::vector<uint8_t> values;
		if (!control_data_frames.at(slave_id)->GetUpdates(offsets, values, all_data_flag)) return false;
		if (values.empty() && !include_mode) return false;

		std::vector<uint8_t> data{0xFC, slave_id};
		for (size_t i = 0; i < values.size(); ++i)
		{
			data.push_back(static_cast<uint8_t>(offsets[i]));
			data.push_back(values[i]);
		}
		if (include_mode)
		{
			data.push_back(0xE0);
			data.push_back(mode);
		}
		message = Encode(data);
		pending_requests_.emplace(slave_id, REQUESTTYPE::CONTROL);
		return true;
	}

	bool ProtocolGenisysMaster::ProcessMessage(const std::vector<uint8_t> & wire,
		std::vector<uint8_t> & replies)
	{
		std::vector<uint8_t> data;
		if (!Decode(wire, data) || data.size() < 2) return false;
		const uint8_t type = data[0], slave_id = data[1];
		auto pending = pending_requests_.find(slave_id);
		if (pending == pending_requests_.end()) return false;
		const bool control_acknowledged = pending->second == REQUESTTYPE::CONTROL;

		if (type == 0xF1)
		{
			if (data.size() != 2) return false;
			if (control_acknowledged) control_data_frames.at(slave_id)->DataAck();
			pending_requests_.erase(pending);
			responses_.push_back({slave_id, RESPONSETYPE::ACK_MASTER, control_acknowledged, true});
			return true;
		}
		if (type != 0xF2 || data.size() < 6 || (data.size() - 4) % 2 != 0) return false;
		const uint16_t received_crc = data[data.size() - 2] | (data.back() << 8);
		data.resize(data.size() - 2);
		if (CalculateCRC(data) != received_crc) return false;

		std::vector<uint64_t> offsets;
		std::vector<uint8_t> values;
		for (size_t i = 2; i < data.size(); i += 2)
		{
			offsets.push_back(data[i]);
			values.push_back(data[i + 1]);
		}
		if (indication_data_frames.at(slave_id)->RecvBytes(offsets, values) < 0) return false;
		if (control_acknowledged) control_data_frames.at(slave_id)->DataAck();
		pending->second = REQUESTTYPE::ACK_SLAVE;
		const auto ack = Encode({0xFA, slave_id});
		replies.insert(replies.end(), ack.begin(), ack.end());
		responses_.push_back({slave_id, RESPONSETYPE::INDICATION_DATA, control_acknowledged, false});
		return true;
	}

	bool ProtocolGenisysMaster::ProcessMessages(std::vector<uint8_t> & data_buffer, std::string slave_ip)
	{
		// Endpoint routing belongs to the transport layer in the next checkpoint.
		(void)slave_ip;
		responses_.clear();
		std::vector<uint8_t> incoming;
		incoming.swap(data_buffer);
		bool valid = true;
		// Maximum valid encoded F2: header, escaped address, 224 data pairs,
		// one mode pair, CRC and terminator. Leave room for duplicate pairs,
		// but bound malformed/unclosed messages independently of input size.
		constexpr size_t max_wire_size = 1024;
		for (uint8_t byte : incoming)
		{
			if (receive_buffer_.empty())
			{
				if (byte == 0xF1 || byte == 0xF2) receive_buffer_.push_back(byte);
				else valid = false;
				continue;
			}
			if (byte == 0xF1 || byte == 0xF2)
			{
				// A raw response header cannot be interior data: resynchronize.
				receive_buffer_.assign(1, byte);
				valid = false;
				continue;
			}
			receive_buffer_.push_back(byte);
			if (byte == 0xF6)
			{
				if (!ProcessMessage(receive_buffer_, data_buffer))
				{
					MasterLogging::LogWarning("ProtocolGenisysMaster: Rejected slave response");
					valid = false;
				}
				receive_buffer_.clear();
			}
			else if (receive_buffer_.size() >= max_wire_size)
			{
				receive_buffer_.clear();
				valid = false;
			}
		}
		return valid;
	}

	bool ProtocolGenisysMaster::ProcessDatagram(std::vector<uint8_t>& data_buffer)
	{
		std::vector<uint8_t> stream_buffer;
		stream_buffer.swap(receive_buffer_);
		const bool valid = ProcessMessages(data_buffer);
		const bool complete = receive_buffer_.empty();
		receive_buffer_.swap(stream_buffer);
		return valid && complete;
	}

	bool ProtocolGenisysMaster::HasSlave(uint8_t slave_id) const
	{
		return indication_data_frames.count(slave_id) != 0;
	}

	bool ProtocolGenisysMaster::CancelRequest(uint8_t slave_id)
	{
		const auto request = pending_requests_.find(slave_id);
		if (request == pending_requests_.end()) return false;
		if (request->second == REQUESTTYPE::CONTROL)
			control_data_frames.at(slave_id)->CancelUpdate();
		pending_requests_.erase(request);
		return true;
	}

	const std::vector<ProtocolGenisysMaster::Response> & ProtocolGenisysMaster::GetResponses() const
	{
		return responses_;
	}
}
