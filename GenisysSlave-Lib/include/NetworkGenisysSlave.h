#pragma once

// STL
#include <vector>
#include <string>
#include <mutex>
#include <memory>

// Windows
#include <WinSock2.h>

// LSY
#include "GenisysSlave-Lib-Logging.h"
#include "ProtocolGenisysSlave.h"



namespace LSY
{

	class NetworkGenisysSlave
	{


	public: // Stuctures

		struct Config
		{
			enum class CONNECTIONTYPE
			{
				UDP = 0,
				TCP = 1
			};

			uint16_t server_port_ = 0;
			uint16_t destination_port_ = 0;	// For UDP Only
			// if 0 the Genisys slave will respond to master on the same port the master used to send a UDP message from

			CONNECTIONTYPE connection_type_ = CONNECTIONTYPE::UDP;
		};



	private: // Network

		// WSA
		bool is_wsa_started_;
		WSADATA wsa_data;
		SOCKET server_socket_;
		SOCKET client_socket_;		// For TCP use only

		// Buffers
		int bytes_received;
		char server_buf[1025];
		int server_buf_len;



	private: // Data
		Config configuration_;
		std::shared_ptr<ProtocolGenisysSlave> protocol_;



	public:

		NetworkGenisysSlave(Config configuration);
		bool AddProtocol(std::shared_ptr<ProtocolGenisysSlave> & protocol);

		bool StartServer();
		bool ServerLoop();
		bool ShutdownServer();


	private:
		bool LogWSAError();

	};

}

