#pragma once
#include "NetworkGenisysMaster.h"

// Local control draft and the outcome of its last explicitly sent snapshot.
// Values are desired controls, not readback of the slave's physical outputs.
class ConsoleControls
{
public:
	void Initialize(uint8_t id, uint64_t bytes)
	{
		frame = std::make_shared<LSY::MasterDataFrame>("Controls " + std::to_string(id), bytes);
		acknowledged_.assign(static_cast<size_t>(bytes), 0);
	}
	std::shared_ptr<LSY::MasterDataFrame> frame;
	bool include_mode = false;
	uint8_t mode = 0;

	bool ByteChanged(size_t offset) const { return (*frame)[offset] != acknowledged_.at(offset); }
	bool Dirty() const
	{
		for (size_t i = 0; i < acknowledged_.size(); ++i) if (ByteChanged(i)) return true;
		return include_mode && (!mode_acknowledged_ || mode != acknowledged_mode_);
	}
	const std::string& Result() const { return result_; }
	bool Send(LSY::NetworkGenisysMaster& network, uint8_t id, bool full)
	{
		std::vector<uint8_t> snapshot;
		frame->ReadBytes(0, frame->GetNumBytes(), snapshot);
		if (!network.RequestControl(id, full, include_mode, mode)) return false;
		sent_ = snapshot;
		sent_mode_ = mode; sent_include_mode_ = include_mode;
		active_ = true; response_acknowledged_ = false;
		result_ = "WAITING for control response";
		return true;
	}
	void OnResponse(const LSY::ProtocolGenisysMaster::Response& response)
	{
		if (!active_ || !response.control_acknowledged) return;
		response_acknowledged_ = true;
		acknowledged_ = sent_;
		if (sent_include_mode_) { mode_acknowledged_ = true; acknowledged_mode_ = sent_mode_; }
		result_ = "ACKNOWLEDGED; finishing exchange";
	}
	void OnTransaction(const LSY::NetworkGenisysMaster::TransactionEvent& event)
	{
		using Event = LSY::NetworkGenisysMaster::TRANSACTIONEVENT;
		if (!active_ || !event.control_request || event.type == Event::RETRY) return;
		active_ = false;
		if (event.type == Event::COMPLETE && response_acknowledged_) result_ = "ACKNOWLEDGED";
		else if (response_acknowledged_) result_ = "ACKNOWLEDGED; exchange failed";
		else if (event.type == Event::TIMEOUT) result_ = "TIMEOUT - outcome unknown";
		else result_ = "SEND FAILED - outcome unknown";
	}
private:
	std::vector<uint8_t> acknowledged_, sent_;
	bool mode_acknowledged_ = false, sent_include_mode_ = false;
	uint8_t acknowledged_mode_ = 0, sent_mode_ = 0;
	bool active_ = false, response_acknowledged_ = false;
	std::string result_ = "Not sent (initial local zeros)";
};
