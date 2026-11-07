#include "MasterDataFrame.h"

#include <iostream>
#include <limits>
#include <stdexcept>

void Check(bool condition, const char* message)
{
	if (!condition)
		throw std::runtime_error(message);
}

int main()
{
	try
	{
		using Frame = LSY::MasterDataFrame;
		using Event = Frame::CALLBACKEVENT;
		Frame controls("Controls", 4);
		Frame indications("Indications", 4);
		std::vector<uint64_t> offsets;
		std::vector<uint8_t> values;
		int indication_events = 0;
		int mode_events = 0;
		std::function<void(std::string, Event)> callback = [&](std::string name, Event event)
		{
			Check(name == "Indications", "Callback frame name");
			if (event == Event::INDICATION_CHANGE)
			{
				++indication_events;
				Check(indications[0] == 0x80 && indications[3] == 0x55,
					"Callback must see the entire received update");
				Check(indications.HasModeByte() && indications.GetModeByte() == 5,
					"Callback must see the updated mode");
			}
			else if (event == Event::MODE_CHANGE)
				++mode_events;
			else
				Check(false, "Unexpected callback type");
		};
		Check(indications.AddEventCallback(callback), "Register indication callback");

		bool bit = false;
		uint8_t byte = 0;
		Check(controls.WriteBit(0, 7, true), "Write bit");
		Check(controls.ReadBit(0, 7, bit) && bit, "Read bit");
		Check(controls.WriteByte(1, 0x12), "Write byte");
		Check(controls.ReadByte(1, byte) && byte == 0x12, "Read byte");
		Check(controls.WriteBytes(2, {0x34, 0x56}), "Write bytes");
		Check(controls.ReadBytes(0, 4, values) && values == std::vector<uint8_t>({0x80, 0x12, 0x34, 0x56}), "Read bytes");
		Check(!controls.WriteBit(0, 8, true) && !controls.WriteByte(4, 1), "Reject invalid bit/byte offsets");
		const auto huge = (std::numeric_limits<uint64_t>::max)();
		Check(!controls.WriteBytes(huge, {1, 2}) && !controls.ReadBytes(huge, 2, values), "Reject overflowing ranges");
		std::cout << "PASS: bit/byte API and bounds\n";

		Check(!controls.DataAck(), "No snapshot to acknowledge initially");
		Check(controls.GetUpdates(offsets, values) && offsets == std::vector<uint64_t>({0, 1, 2, 3}), "Collect changed controls");
		const auto pending_values = values;
		Check(controls.WriteByte(1, 0x99), "Stage a newer control while pending");
		for (int i = 0; i < 6; ++i)
			Check(controls.GetUpdates(offsets, values) && values == pending_values, "Pending controls must not auto-ack or change");
		Check(controls.DataAck(), "Acknowledge prepared controls");
		Check(controls.GetUpdates(offsets, values) && offsets == std::vector<uint64_t>({1}) && values == std::vector<uint8_t>({0x99}), "Newer control survives acknowledgement");
		Check(controls.DataAck(), "Acknowledge newer control");
		Check(controls.GetUpdates(offsets, values) && values.empty(), "Acknowledged controls are clean");
		Check(controls.WriteByte(2, 0x77) && controls.WriteByte(2, 0x34), "Revert unsent control");
		Check(controls.GetUpdates(offsets, values) && values.empty(), "Reverted control must not be sent");
		Frame zeros("Zero controls", 2);
		Check(zeros.GetUpdates(offsets, values, true) && values == std::vector<uint8_t>({0, 0}), "Full snapshot includes zero bytes");
		Check(zeros.GetUpdates(offsets, values) && values.size() == 2, "Full snapshot remains pending");
		Check(zeros.DataAck(), "Acknowledge full snapshot");
		Check(zeros.GetUpdates(offsets, values) && values.empty(), "Full snapshot acknowledged");
		std::cout << "PASS: control snapshots and explicit acknowledgement\n";

		offsets = {0, 3, 0xE0}; values = {0x80, 0x55, 5};
		Check(indications.RecvBytes(offsets, values) == 3, "Receive indication and mode update");
		Check(indication_events == 1 && mode_events == 1, "One callback per event type");
		Check(indications.RecvBytes(offsets, values) == 3 && indication_events == 1 && mode_events == 1, "Repeated data must not fire callbacks");
		Check(indications.GetUpdates(offsets, values) && values.empty(), "Received indications must not become outgoing changes");
		offsets = {0xE0}; values = {4};
		Check(indications.RecvBytes(offsets, values) == 1 && indication_events == 1 && mode_events == 2, "Mode-only change is separate");
		offsets = {0, 4}; values = {1, 2};
		Check(indications.RecvBytes(offsets, values) == -1 && indications[0] == 0x80, "Invalid payload must not partially apply");
		offsets = {0}; values.clear();
		Check(indications.RecvBytes(offsets, values) == -1, "Reject mismatched pairs");
		offsets = {0xE1}; values = {1};
		Check(indications.RecvBytes(offsets, values) == -1, "Reject unsupported status byte");
		offsets.clear(); values.clear();
		Check(indications.RecvBytes(offsets, values) == 0 && indication_events == 1 && mode_events == 2, "Empty update has no events");
		offsets = {0, 0}; values = {1, 0x80};
		Check(indications.RecvBytes(offsets, values) == 2 && indication_events == 1, "Duplicate offsets use final value without a false change event");
		std::cout << "PASS: indications, callbacks, mode separation and payload validation\n";
		std::cout << "All MasterDataFrame checks passed.\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << "FAIL: " << error.what() << '\n';
		return 1;
	}
}
