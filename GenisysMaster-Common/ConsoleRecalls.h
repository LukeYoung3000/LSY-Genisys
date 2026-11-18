#pragma once
#include "NetworkGenisysMaster.h"
#include <algorithm>
#include <deque>

// Explicit recalls get priority over new scheduled polls, never over an active
// exchange. Controls are never queued or retransmitted by this helper.
class ConsoleRecalls
{
public:
	struct Result { uint8_t id; bool sent; };
	ConsoleRecalls(std::vector<uint8_t> ids, bool serial) : ids_(std::move(ids)), serial_(serial) {}
	bool Queue(uint8_t id)
	{
		if (std::find(ids_.begin(), ids_.end(), id) == ids_.end()) return false;
		if (std::find(pending_.begin(), pending_.end(), id) == pending_.end()) pending_.push_back(id);
		return true;
	}
	void QueueAll() { for (auto id : ids_) Queue(id); }
	bool Pending() const { return !pending_.empty(); }
	std::vector<Result> Process(LSY::NetworkGenisysMaster& network)
	{
		std::vector<Result> result;
		if (serial_)
			for (auto id : ids_) { LSY::NetworkGenisysMaster::SlaveStatus status; if (network.GetSlaveStatus(id, status) && status.busy) return result; }
		for (size_t count = pending_.size(); count && !pending_.empty(); --count)
		{
			const auto id = pending_.front(); pending_.pop_front();
			LSY::NetworkGenisysMaster::SlaveStatus status;
			if (network.GetSlaveStatus(id, status) && status.busy) { pending_.push_back(id); continue; }
			result.push_back({id, network.RequestRecall(id)});
			if (serial_) break;
		}
		return result;
	}
private:
	std::vector<uint8_t> ids_;
	bool serial_;
	std::deque<uint8_t> pending_;
};
