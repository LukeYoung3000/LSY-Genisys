#pragma once

// STL
#include <vector>
#include <string>
#include <mutex>
#include <memory>

// Windows
#include <WinSock2.h>
#include <winbase.h>
#include <setupapi.h>

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
				TCP = 1,
				SERIAL = 2
			};


			// Conection Type (UDP, TCP, Serial)
			CONNECTIONTYPE connection_type_ = CONNECTIONTYPE::UDP;


			// IP Specific Settings
			uint16_t server_port_ = 0;
			uint16_t destination_port_ = 0;	// For UDP Only
			// if 0 the Genisys slave will respond to master on the same port the master used to send a UDP message from


			// Serial Specific Settings
			std::string com_port_ = "COM1";           // e.g., "COM1", "COM3"
			uint32_t baud_rate_ = 9600;               // 9600, 19200, 38400, 115200, etc.
			uint8_t data_bits_ = 8;
			uint8_t stop_bits_ = 1;
			char parity_ = 'N';                       // 'N'=None, 'E'=Even, 'O'=Odd

			// Serial Timeouts
			uint32_t read_timeout_ms_ = 100;		// Serial read timeout in milliseconds for ReadFile operations
			uint32_t write_timeout_ms_ = 100;		// Serial write timeout in milliseconds for WriteFile operations	

			};



	private: // Buffers
		int bytes_received;
		char server_buf[1025];
		int server_buf_len;


	private: // Network (WSA)
		bool is_wsa_started_;
		WSADATA wsa_data;
		SOCKET server_socket_;
		SOCKET client_socket_;		// For TCP use only


	private: // Serial
		HANDLE com_handle_;					// Serial port file handle
		DCB dcb_config_;					// Device Control Block for serial settings
		COMMTIMEOUTS timeouts_;				// Read/Write timeouts for serial port




	private: // Data
		Config configuration_;
		std::shared_ptr<ProtocolGenisysSlave> protocol_;



	public:

		NetworkGenisysSlave(Config configuration);
		bool AddProtocol(std::shared_ptr<ProtocolGenisysSlave> & protocol);

		bool StartServer();
		bool ServerLoop();
		bool ShutdownServer();


	private: // General Helpers
		bool LogWSAError();

	private: // Serial Helpers
		bool StartSerialServer();
		bool SerialServerLoop();
		bool ShutdownSerialServer();

	};

}

