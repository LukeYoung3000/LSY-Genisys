#include "NetworkGenisysSlave.h"


#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <WinSock2.h>
#include <Ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")


namespace LSY
{

	NetworkGenisysSlave::NetworkGenisysSlave(NetworkGenisysSlave::Config configuration)
	{
		configuration_ = configuration;

		// WSA
		is_wsa_started_ = false;
		server_socket_ = INVALID_SOCKET;

		// Buffer
		bytes_received = 0;
		server_buf_len = 1024;
	}

	bool NetworkGenisysSlave::AddProtocol(std::shared_ptr<ProtocolGenisysSlave>& protocol)
	{
		protocol_ = protocol;

		return true;
	}

	bool NetworkGenisysSlave::StartServer()
	{

		// Startup WSA
		int res = WSAStartup(MAKEWORD(2, 2), &wsa_data);
		if (res != NO_ERROR)
		{
			is_wsa_started_ = false;
			Logging::LogErrorF("NetworkGenisysSlave::StartServer: WSAStartup Failed With Error Code [%d]", res);
			return false;
		}
		is_wsa_started_ = true;


		// Create UDP Server Socket
		server_socket_ = INVALID_SOCKET;
		server_socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (server_socket_ == INVALID_SOCKET)
		{
			Logging::LogErrorF("NetworkGenisysSlave::StartServer: [%d] Socket Setup Failed", configuration_.server_port_);
			LogWSAError();
			return false;
		}


		// Bind Server Socket
		struct sockaddr_in server_addr;
		server_addr.sin_family = AF_INET;							// Address family							
		server_addr.sin_port = htons(configuration_.server_port_);	// Bind the socket to listen on this port
		// serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");		// Bind the socket to listen on IP 127.0.0.1
		server_addr.sin_addr.s_addr = htonl(INADDR_ANY);			// Bind the socket to all IP addresses on the local machine.

		if (bind(server_socket_, (SOCKADDR*)&server_addr, sizeof(server_addr)))
		{
			Logging::LogErrorF("NetworkGenisysSlave::StartServer: [%d] Socket Bind Failed", configuration_.server_port_);
			LogWSAError();
			return false;
		}



		// Make Server Socket Non Blocking
		u_long non_blocking_option = 1;
		int result = ioctlsocket(server_socket_, FIONBIO, &non_blocking_option);
		if (result == SOCKET_ERROR)
		{
			Logging::LogErrorF("NetworkGenisysSlave::StartServer: [%d] Socket Option Non Blocking Failed", configuration_.server_port_);
			LogWSAError();
			return false;
		}



		Logging::LogDebugF("NetworkGenisysSlave::StartServer: [%d] Server Started", configuration_.server_port_);
		return true;
	}

	bool NetworkGenisysSlave::ServerLoop()
	{

		// Check "server_socket_" is valid
		if (server_socket_ == INVALID_SOCKET)
		{
			// Error - Server Socket Is Invalid
			Logging::LogErrorF("NetworkGenisysSlave::ServerLoop: [%d] Invalid Socket", configuration_.server_port_);
			LogWSAError();
			return false;
		}


		// Get Message From Sender (Genisys Master) 
		struct sockaddr_in sender_addr;
		int sender_addr_size = sizeof(sender_addr);
		bytes_received = recvfrom(server_socket_, server_buf, server_buf_len, 0 /* no flags*/, (SOCKADDR*)&sender_addr, &sender_addr_size);


		// Check For Any Receive Errors
		if (bytes_received == SOCKET_ERROR)
		{
			// To Do: Now That Socket In Non Blocking We Should Check For "WSAEWOULDBLOCK" Here.
			return false;

			// Old Code:
			// No Data Received Or An Error Was Found
			Logging::LogErrorF("NetworkGenisysSlave::ServerLoop: [%d] No Data Received", configuration_.server_port_);
			LogWSAError();
			return false;
		}

		// Get IP Address Of Sender (Genisys Master)
		char source_ip_buf[100];
		PCSTR WSAAPI source_ip = inet_ntop(AF_INET, &(sender_addr.sin_addr), source_ip_buf, (size_t)80);
		if (source_ip == NULL)
		{
			Logging::LogErrorF("NetworkGenisysSlave::ServerLoop: [%d] Unable To Get Source IP Of Incoming Datagram", configuration_.server_port_);
			LogWSAError();
		}
		else
		{
			// Log Senders Details
			Logging::LogDebugF("NetworkGenisysSlave::ServerLoop: [%d] Received Datagram From: [%s]:[%d]", configuration_.server_port_, source_ip, ntohs(sender_addr.sin_port));
		}






		// Convert Server Buffer To Vector
		std::vector<uint8_t> recv_msg;
		for (int i = 0; i < bytes_received; i++)
			recv_msg.push_back(server_buf[i]);

		// Get Genisys Protocol Response Message
		std::vector<uint8_t> respond_msg = recv_msg;
		if (!protocol_->ProcessMessages(respond_msg))
			return false;

		// Pack Response Message Vector Into "server_buf";
		if (respond_msg.size() > server_buf_len)
		{
			// Error - Buffer Is To Small
			Logging::LogErrorF("NetworkGenisysSlave::ServerLoop: [%d] Message Buffer To Small", configuration_.server_port_);
			return false;
		}

		size_t responce_size = respond_msg.size();
		for (size_t i = 0; i < responce_size; i++)
			server_buf[i] = respond_msg[i];





		// Send Response Message Over The Network

		if (configuration_.destination_port_ > 0)
			sender_addr.sin_port = htons(configuration_.destination_port_);

		int send_result = sendto(server_socket_, server_buf, (int)responce_size, 0, (struct sockaddr*)&sender_addr, sizeof(sender_addr));

		// Check For Any Send Errors
		if (send_result == SOCKET_ERROR)
		{
			Logging::LogErrorF("NetworkGenisysSlave::ServerLoop: [%d] Unable To Send Response Message", configuration_.server_port_);
			LogWSAError();
			return false;
		}


		return true;
	}


	bool NetworkGenisysSlave::ShutdownServer()
	{

		if (server_socket_ != INVALID_SOCKET)
		{
			if (closesocket(server_socket_) == SOCKET_ERROR)
			{
				// Error - Unable To Close Socket
				Logging::LogErrorF("NetworkGenisysSlave::ShutdownServer: [%d] Unable To Close Socket", configuration_.server_port_);
				LogWSAError();
			}
			server_socket_ = INVALID_SOCKET;
		}


		if (is_wsa_started_)
			WSACleanup();

		return true;
	}



	bool NetworkGenisysSlave::LogWSAError()
	{
		char log_msg_buffer[300];
		log_msg_buffer[0] = '\0'; // Make first char null terminator
		const size_t log_msg_buffer_size = 280;

		int err = WSAGetLastError();


		FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,   // flags
			NULL,										// lpsource
			err,										// message id
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),  // languageid
			log_msg_buffer,								// output buffer
			log_msg_buffer_size,						// size of msgbuf, bytes
			NULL);										// va_list of arguments

		if (log_msg_buffer[0] == '\0') // Message Buffer Is Empty
			Logging::LogErrorF("NetworkGenisysSlave::LogWSAError: WSA Error [%d]", err);
		else
			Logging::LogErrorF("NetworkGenisysSlave::LogWSAError: %s", log_msg_buffer);


		return true;
	}

}