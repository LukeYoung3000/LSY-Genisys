#pragma once

// Included after the test executable headers and Check helper.
struct Peer
{
	SOCKET socket_ = INVALID_SOCKET;
	uint16_t port = 0;
	LSY::ProtocolGenisysSlave protocol;
	std::map<uint8_t, std::shared_ptr<LSY::DataFrame>> frames;
	std::map<uint8_t, unsigned> requests;
	std::map<uint8_t, std::shared_ptr<LSY::DataFrame>> control_frames;
	std::map<uint8_t, unsigned> control_callbacks;
	std::map<uint8_t, Bytes> last_controls;
	bool online = true;
	std::map<uint8_t, bool> offline_ids;
	std::map<uint8_t, std::map<uint8_t, unsigned>> station_requests;
	unsigned drop_requests = 0;
	unsigned drop_ack_replies = 0;
	unsigned drop_indication_replies = 0;
	bool corrupt_next_reply = false;
	Peer()
	{
		socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		Check(socket_ != INVALID_SOCKET, "Create peer socket");
		sockaddr_in address{}; address.sin_family = AF_INET; address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		Check(bind(socket_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0, "Bind peer");
		int length = sizeof(address);
		Check(getsockname(socket_, reinterpret_cast<sockaddr*>(&address), &length) == 0, "Get peer port");
		port = ntohs(address.sin_port);
		u_long nonblocking = 1;
		Check(ioctlsocket(socket_, FIONBIO, &nonblocking) == 0, "Nonblocking peer");
	}
	~Peer() { if (socket_ != INVALID_SOCKET) closesocket(socket_); }
	void Add(uint8_t id, uint8_t value)
	{
		auto indications = std::make_shared<LSY::DataFrame>("Slave indications", 8);
		auto controls = std::make_shared<LSY::DataFrame>("Slave controls", 8);
		indications->WriteByte(0, value);
		frames[id] = indications;
		control_frames[id] = controls;
		std::function<void(std::string, LSY::DataFrame::CALLBACKEVENT)> callback =
			[this, id](std::string, LSY::DataFrame::CALLBACKEVENT event)
			{ if (event == LSY::DataFrame::CALLBACKEVENT::CONTROL_CHANGE) ++control_callbacks[id]; };
		controls->AddEventCallback(callback);
		Check(protocol.AddDataFrame(id, false, indications) && protocol.AddDataFrame(id, true, controls), "Configure slave protocol");
	}
	void Send(const Bytes& bytes, uint16_t master_port)
	{
		sockaddr_in address{}; address.sin_family = AF_INET; address.sin_addr.s_addr = htonl(INADDR_LOOPBACK); address.sin_port = htons(master_port);
		Check(sendto(socket_, reinterpret_cast<const char*>(bytes.data()), static_cast<int>(bytes.size()), 0,
			reinterpret_cast<sockaddr*>(&address), sizeof(address)) == static_cast<int>(bytes.size()), "Send peer datagram");
	}
	void Process(uint16_t master_port)
	{
		for (int i = 0; i < 16; ++i)
		{
			char buffer[2048]; sockaddr_in source{}; int length = sizeof(source);
			int size = recvfrom(socket_, buffer, sizeof(buffer), 0, reinterpret_cast<sockaddr*>(&source), &length);
			if (size == SOCKET_ERROR) { Check(WSAGetLastError() == WSAEWOULDBLOCK, "Receive peer datagram"); return; }
			Check(ntohs(source.sin_port) == master_port, "Requests and acknowledgements use the bound master socket");
			Bytes bytes(buffer, buffer + size);
			++requests[bytes[0]];
			++station_requests[bytes[1]][bytes[0]];
			const uint8_t request_type = bytes[0];
			if (request_type == 0xFC) last_controls[bytes[1]] = bytes;
			if (!online || offline_ids[bytes[1]]) continue;
			if (drop_requests) { --drop_requests; continue; }
			Check(protocol.ProcessMessages(bytes, "127.0.0.1"), "Existing slave protocol processes master request");
			if (request_type == 0xFA && drop_ack_replies) { --drop_ack_replies; continue; }
			if (!bytes.empty() && bytes[0] == 0xF2 && drop_indication_replies) { --drop_indication_replies; continue; }
			if (corrupt_next_reply && bytes.size() > 5) { bytes[2] ^= 1; corrupt_next_reply = false; }
			if (!bytes.empty()) Send(bytes, master_port);
		}
	}
};
