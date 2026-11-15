#pragma once
// Included after the executable Check helper and Bytes alias.
#include "../src/MasterTransport.h"
#include "ProtocolGenisysSlave.h"
#include <deque>
#include <map>
struct StreamPeer final : LSY::MasterTransport
{
	bool open = false, pending = false, holdWrite = false, failWrite = false, failRead = false;
	unsigned fragment = 1;
	LSY::ProtocolGenisysSlave protocol;
	std::map<uint8_t, std::shared_ptr<LSY::DataFrame>> frames, controls;
	std::map<uint8_t, bool> offline;
	std::map<uint8_t, unsigned> requests;
	std::map<uint8_t, std::map<uint8_t, unsigned>> byId;
	std::vector<uint8_t> requestOrder;
	std::deque<Bytes> incoming;
	Bytes tx;
	bool corrupt = false, dropF1 = false;
	void Add(uint8_t id, uint8_t value)
	{
		auto indication = std::make_shared<LSY::DataFrame>("Serial slave", 8);
		auto control = std::make_shared<LSY::DataFrame>("Serial control", 8);
		indication->WriteByte(0, value); frames[id] = indication; controls[id] = control;
		Check(protocol.AddDataFrame(id, indication, control), "Register serial slave peer");
	}
	bool Start() override { if (open) return false; open = true; return true; }
	void Stop() override { open = pending = false; incoming.clear(); }
	bool IsOpen() const override { return open; }
	bool WritePending() const override { return pending; }
	bool DiscardInput() override { incoming.clear(); return true; }
	bool Send(const sockaddr_in&, const Bytes& data) override
	{
		if (!open || pending || failWrite) return false;
		tx = data; pending = true;
		uint8_t id = data[1] == 0xF0 ? 0xF0 | data[2] : data[1];
		++requests[data[0]]; ++byId[id][data[0]];
		if (data[0] != 0xFA) requestOrder.push_back(id);
		return true;
	}
	void Enqueue(const Bytes& response)
	{
		for (size_t offset = 0; offset < response.size(); offset += fragment)
			incoming.push_back(Bytes(response.begin() + offset, response.begin() + (std::min)(offset + fragment, response.size())));
	}
	bool PollWrite() override
	{
		if (!pending || holdWrite) return true;
		if (failWrite) { Stop(); return false; }
		pending = false;
		const uint8_t id = tx[1] == 0xF0 ? 0xF0 | tx[2] : tx[1];
		if (offline[id]) return true;
		Bytes response = tx;
		Check(protocol.ProcessMessages(response, "SERIAL:TEST"), "Unmodified slave protocol accepts serial request");
		if (dropF1 && response.size() && response[0] == 0xF1) return true;
		if (corrupt && response.size() > 4) { response[response.size() - 2] ^= 1; corrupt = false; }
		Enqueue(response); return true;
	}
	READ Receive(Bytes& bytes, sockaddr_in&) override
	{
		if (failRead) { Stop(); return READ::FAILED; }
		if (pending || incoming.empty()) return READ::EMPTY;
		bytes = incoming.front(); incoming.pop_front(); return READ::DATA;
	}
};
