#include "ProtocolGenisysMaster.h"

#include <iostream>
#include <sstream>
#include <stdexcept>

using Bytes = std::vector<uint8_t>;
using Protocol = LSY::ProtocolGenisysMaster;
using Frame = LSY::MasterDataFrame;

void Check(bool condition, const char* message)
{
	if (!condition) throw std::runtime_error(message);
}

Bytes Hex(const char* text)
{
	Bytes result;
	std::istringstream input(text);
	unsigned value;
	while (input >> std::hex >> value) result.push_back(static_cast<uint8_t>(value));
	return result;
}

// Used to make valid envelopes for negative payload tests and exhaustive byte
// tests. Fixed GE/capture vectors below independently check production CRC/escaping.
Bytes Envelope(Bytes data, bool secure = true)
{
	if (secure)
	{
		uint16_t crc = 0;
		for (auto byte : data)
		{
			crc ^= byte;
			for (int i = 0; i < 8; ++i)
				crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
		}
		data.push_back(static_cast<uint8_t>(crc));
		data.push_back(static_cast<uint8_t>(crc >> 8));
	}
	Bytes wire{data[0]};
	for (size_t i = 1; i < data.size(); ++i)
	{
		if (data[i] >= 0xF0) { wire.push_back(0xF0); wire.push_back(data[i] & 15); }
		else wire.push_back(data[i]);
	}
	wire.push_back(0xF6);
	return wire;
}

struct Fixture
{
	Protocol protocol;
	std::shared_ptr<Frame> indications = std::make_shared<Frame>("Indications", 32);
	std::shared_ptr<Frame> controls = std::make_shared<Frame>("Controls", 32);
	int indication_events = 0;
	int mode_events = 0;
	uint8_t id;
	Fixture(uint8_t slave_id = 3) : id(slave_id)
	{
		Check(protocol.AddDataFrame(id, false, indications), "Register indications");
		Check(protocol.AddDataFrame(id, true, controls), "Register controls");
		std::function<void(std::string, Frame::CALLBACKEVENT)> callback =
			[this](std::string name, Frame::CALLBACKEVENT event)
			{
				Check(name == "Indications", "Callback frame name");
				if (event == Frame::CALLBACKEVENT::INDICATION_CHANGE) ++indication_events;
				if (event == Frame::CALLBACKEVENT::MODE_CHANGE) ++mode_events;
			};
		indications->AddEventCallback(callback);
	}
	void Complete()
	{
		auto data = Envelope({0xF1, id}, false);
		Check(protocol.ProcessMessages(data) && data.empty(), "Complete with F1");
		const auto& events = protocol.GetResponses();
		Check(events.size() == 1 && events[0].type == Protocol::RESPONSETYPE::ACK_MASTER &&
			events[0].transaction_complete, "Expose F1 response and completion");
	}
};

void RequestChecks()
{
	Fixture f;
	Bytes wire;
	Check(f.protocol.BuildRecall(3, wire) && wire == Hex("FD 03 00 91 F6"), "GE recall bytes");
	Check(!f.protocol.BuildPoll(3, wire) && wire.empty(), "Reject overlapping requests");
	f.Complete();
	Check(f.protocol.BuildAcknowledgeSlave(3, wire) && wire == Hex("FA 03 02 A1 F6"), "GE acknowledge bytes");
	f.Complete();
	Check(f.protocol.BuildPoll(3, wire, false) && wire == Hex("FB 03 F6"), "Non-secure poll");
	f.Complete();
	Check(f.protocol.BuildPoll(3, wire), "Secure poll default");
	Check(wire == Envelope({0xFB, 3}), "Secure poll encoding");
	f.Complete();
	Check(!f.protocol.BuildControl(3, wire) && wire.empty(), "No controls to send");
	Check(f.protocol.BuildControl(3, wire, false, true, 1) &&
		wire == Hex("FC 03 E0 01 48 50 F6"), "Mode-only control");
	f.Complete();
	Check(f.protocol.GetResponses()[0].control_acknowledged, "Mode-only control acknowledgement");

	Fixture poll(0x0B);
	Check(poll.protocol.BuildPoll(0x0B, wire) && wire == Hex("FB 0B 02 F0 07 F6"), "ELIXS poll with escaped CRC");
	Fixture captured(0x52);
	captured.controls->WriteByte(31, 0x6C);
	Check(captured.protocol.BuildControl(0x52, wire, false, true, 5) &&
		wire == Hex("FC 52 1F 6C E0 05 E3 F0 0A F6"), "Capture2 control bytes");

	Fixture zeros;
	Check(zeros.protocol.BuildControl(3, wire, true), "Full control snapshot including zero bytes");
	Bytes full{0xFC, 3};
	for (uint8_t i = 0; i < 32; ++i) { full.push_back(i); full.push_back(0); }
	Check(wire == Envelope(full), "Full control encoding");
	std::cout << "PASS: request generation against GE and capture vectors\n";
}

void ResponseChecks()
{
	Fixture f;
	Bytes wire;
	Check(f.protocol.BuildRecall(3, wire), "Start recall");
	wire = Hex("F2 03 00 16 01 3E 02 08 03 50 E0 01 25 9B F6");
	std::function<void(std::string, Frame::CALLBACKEVENT)> callback =
		[&](std::string, Frame::CALLBACKEVENT event)
		{
			Check((*f.indications)[0] == 0x16 && (*f.indications)[3] == 0x50 &&
				f.indications->GetModeByte() == 1, "Callback reads complete GE indication payload");
			if (event == Frame::CALLBACKEVENT::INDICATION_CHANGE) ++f.indication_events;
			if (event == Frame::CALLBACKEVENT::MODE_CHANGE) ++f.mode_events;
		};
	f.indications->AddEventCallback(callback);
	Check(f.protocol.ProcessMessages(wire) && wire == Hex("FA 03 02 A1 F6"), "GE response generates acknowledgement");
	Check(f.indication_events == 1 && f.mode_events == 1, "Indication/mode callbacks");
	const auto event = f.protocol.GetResponses().at(0);
	Check(event.slave_id == 3 && event.type == Protocol::RESPONSETYPE::INDICATION_DATA &&
		!event.control_acknowledged && !event.transaction_complete, "Expose F2 response type");
	wire = Hex("F2 03 00 16 01 3E 02 08 03 50 E0 01 25 9B F6");
	Check(f.protocol.ProcessMessages(wire) && !wire.empty() && f.indication_events == 1 &&
		f.mode_events == 1, "Duplicate indication is acknowledged without duplicate callbacks");
	f.Complete();
	wire.clear();
	Check(f.protocol.ProcessMessages(wire) && f.protocol.GetResponses().empty(), "Responses reset per call");

	Fixture controls(0x52);
	controls.controls->WriteByte(31, 0x6C);
	Check(controls.protocol.BuildControl(0x52, wire, false, true, 5), "Start captured control");
	controls.controls->WriteByte(31, 0x6D);
	wire = Hex("F2 52 E0 05 1A AA F6");
	Check(controls.protocol.ProcessMessages(wire) && wire == Hex("FA 52 C3 5D F6"), "Captured control reply and ack");
	Check(controls.protocol.GetResponses()[0].control_acknowledged && controls.indication_events == 0 &&
		controls.mode_events == 1, "Mode-only F2 acknowledges control without indication callback");
	Check(!controls.protocol.BuildControl(0x52, wire), "Wait for acknowledgement sequence to finish");
	controls.Complete();
	Check(controls.protocol.BuildControl(0x52, wire) &&
		wire == Envelope({0xFC, 0x52, 31, 0x6D}), "Newer control survives F2 acknowledgement");
	controls.Complete();
	Check(controls.protocol.GetResponses()[0].control_acknowledged, "F1 acknowledges control directly");
	Check(!controls.protocol.BuildControl(0x52, wire), "Acknowledged control not resent");
	std::cout << "PASS: response parsing, callbacks and control acknowledgement\n";
}

void FramingChecks()
{
	// Every split point includes boundaries in escaped data and CRC sequences.
	const auto response = Hex("F2 0B 02 F0 0C E0 05 F0 08 98 F6");
	for (size_t split = 0; split <= response.size(); ++split)
	{
		Fixture f(0x0B);
		Bytes wire;
		Check(f.protocol.BuildPoll(f.id, wire), "Start fragmented response test");
		wire.assign(response.begin(), response.begin() + split);
		Check(f.protocol.ProcessMessages(wire), "Accept partial message");
		if (split < response.size()) Check(wire.empty() && f.indication_events == 0, "No early callback/reply");
		wire.assign(response.begin() + split, response.end());
		Check(f.protocol.ProcessMessages(wire) && (*f.indications)[2] == 0xFC &&
			f.indication_events == 1 && f.mode_events == 1, "Finish fragmented escaped response");
	}
	Fixture f;
	Bytes wire;
	Check(f.protocol.BuildPoll(3, wire), "Start concatenated responses");
	wire = Envelope({0xF2, 3, 0, 0x11});
	const auto second = Envelope({0xF2, 3, 1, 0x22});
	wire.insert(wire.end(), second.begin(), second.end());
	const auto end = Hex("F1 03 F6");
	wire.insert(wire.end(), end.begin(), end.end());
	Check(f.protocol.ProcessMessages(wire) && wire == Hex("FA 03 02 A1 F6 FA 03 02 A1 F6"), "Concatenated responses generate two acknowledgements");
	Check(f.protocol.GetResponses().size() == 3 && (*f.indications)[0] == 0x11 &&
		(*f.indications)[1] == 0x22, "Process chained indications and F1");

	Fixture high_id(0xF6);
	Check(high_id.protocol.BuildPoll(high_id.id, wire, false) && wire == Hex("FB F0 06 F6"), "Escaped slave address");
	for (unsigned value = 0; value <= 255; ++value)
	{
		wire = Envelope({0xF2, high_id.id, 0, static_cast<uint8_t>(value)});
		Check(high_id.protocol.ProcessMessages(wire) && (*high_id.indications)[0] == value,
			"All 256 byte values survive response encoding and decoding");
	}
	high_id.Complete();
	std::cout << "PASS: fragmentation, concatenation, escaping and all byte values\n";
}

void RejectionChecks()
{
	Fixture f;
	Bytes wire;
	f.controls->WriteByte(0, 1);
	Check(f.protocol.BuildControl(3, wire), "Start rejection test control");
	std::vector<Bytes> bad{
		Hex("F2 03 00 16 01 3E 02 08 03 50 E0 01 24 9B F6"), // Wrong CRC.
		Hex("F2 03 00 F0 10 00 00 F6"), // Escape is not a low nibble.
		Hex("F2 03 00 F0 F6"), // Dangling escape.
		Hex("F1 03 00 F6"), // F1 has no CRC or payload.
		Hex("F2 03 F6"), // Missing CRC/payload.
		Envelope({0xF2, 3, 0}), // Odd pair length with valid CRC.
		Envelope({0xF2, 3, 0, 1, 32, 2}), // Valid pair then out-of-frame pair.
		Envelope({0xF2, 3, 0xE1, 1}), // Unsupported reserved byte.
		Envelope({0xF2, 4, 0, 1}), // Wrong slave address.
		Hex("F1 04 F6"),
		Envelope({0xF3, 3, 0, 1}), // Unsupported check-back message.
		Hex("F2 03 00 FB 00 00 F6") // Unescaped reserved interior byte.
	};
	for (auto message : bad)
	{
		Check(!f.protocol.ProcessMessages(message) && message.empty(), "Reject bad response without reply");
		Check((*f.indications)[0] == 0 && !f.indications->HasModeByte() &&
			f.indication_events == 0 && f.mode_events == 0 && f.protocol.GetResponses().empty(),
			"Rejected input leaves frame and callbacks unchanged");
		std::vector<uint64_t> offsets;
		Bytes values;
		Check(f.controls->GetUpdates(offsets, values) && values == Bytes({1}), "Invalid response must not acknowledge controls");
	}
	f.Complete();
	Check(f.protocol.GetResponses()[0].control_acknowledged, "Valid reply recovers after malformed messages");
	Check(f.protocol.BuildPoll(3, wire), "Start oversized response test");
	wire.assign(2000, 0); wire[0] = 0xF2;
	Check(!f.protocol.ProcessMessages(wire), "Bound incomplete input buffering");
	f.Complete();
	Check(f.protocol.BuildPoll(3, wire), "Start resynchronization test");
	wire = Hex("F2 03 00 F1 03 F6");
	Check(!f.protocol.ProcessMessages(wire) && f.protocol.GetResponses().size() == 1 &&
		f.protocol.GetResponses()[0].transaction_complete, "Recover at next raw response header and report damaged prefix");
	wire = Hex("F1 03 F6");
	Check(!f.protocol.ProcessMessages(wire), "Reject unsolicited acknowledgement");

	Protocol registration;
	std::shared_ptr<Frame> null_frame;
	auto frame = std::make_shared<Frame>("Valid", 1);
	auto too_large = std::make_shared<Frame>("Too large", 225);
	Check(!registration.AddDataFrame(0, false, frame) && !registration.AddDataFrame(3, false, null_frame) &&
		!registration.AddDataFrame(3, false, too_large), "Validate registrations");
	Check(registration.AddDataFrame(3, false, frame) && !registration.AddDataFrame(3, true, frame), "Reject shared control/indication frame");
	auto other = std::make_shared<Frame>("Other", 1);
	Check(!registration.AddDataFrame(3, false, other), "Reject duplicate slave/direction");
	Check(!registration.BuildPoll(4, wire) && wire.empty(), "Reject unconfigured slave request");
	Protocol max_protocol;
	auto max_frame = std::make_shared<Frame>("Maximum", 224);
	Check(max_protocol.AddDataFrame(3, false, max_frame) && max_protocol.BuildRecall(3, wire), "Allow all ordinary byte offsets");
	Bytes full{0xF2, 3};
	for (unsigned i = 0; i < 224; ++i) { full.push_back(static_cast<uint8_t>(i)); full.push_back(0xFF); }
	full.push_back(0xE0); full.push_back(5);
	wire = Envelope(full);
	Check(max_protocol.ProcessMessages(wire) && (*max_frame)[223] == 0xFF && max_frame->GetModeByte() == 5,
		"Accept maximum-sized escaped indication frame");
	std::cout << "PASS: invalid input, CRC rejection, limits and recovery\n";
}

int main()
{
	try
	{
		RequestChecks();
		ResponseChecks();
		FramingChecks();
		RejectionChecks();
		std::cout << "All ProtocolGenisysMaster checks passed.\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << "FAIL: " << error.what() << '\n';
		return 1;
	}
}
