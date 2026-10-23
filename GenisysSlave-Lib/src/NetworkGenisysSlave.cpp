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
		client_socket_ = INVALID_SOCKET;

		// Serial
		com_handle_ = INVALID_HANDLE_VALUE;
		ZeroMemory(&dcb_config_, sizeof(DCB));
		ZeroMemory(&timeouts_, sizeof(COMMTIMEOUTS));

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
		// Handle Serial Connection Type
		if (configuration_.connection_type_ == NetworkGenisysSlave::Config::CONNECTIONTYPE::SERIAL)
		{
			return StartSerialServer();
		}

		// Startup WSA for TCP/UDP
		int res = WSAStartup(MAKEWORD(2, 2), &wsa_data);
		if (res != NO_ERROR)
		{
			is_wsa_started_ = false;
			Logging::LogErrorF("NetworkGenisysSlave::StartServer: WSAStartup Failed With Error Code [%d]", res);
			return false;
		}
		is_wsa_started_ = true;




		// Create TCP or UDP Socket
		server_socket_ = INVALID_SOCKET;

		if (configuration_.connection_type_ == NetworkGenisysSlave::Config::CONNECTIONTYPE::TCP)
		{
			// Create TCP Server Socket To Listen On
			server_socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		}
		else
		{
			// Create UDP Server Socket
			server_socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		}

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





		if (configuration_.connection_type_ == NetworkGenisysSlave::Config::CONNECTIONTYPE::TCP)
		{
			// Put the TCP socket into listen mode
			// backlog = 1 means the socket will only listin for one connection
			if (listen(server_socket_, 0))
			{
				Logging::LogErrorF("NetworkGenisysSlave::StartServer: [%d] Socket Listen Failed", configuration_.server_port_);
				LogWSAError();
				return false;
			}
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
		// Handle Serial Connection Type
		if (configuration_.connection_type_ == NetworkGenisysSlave::Config::CONNECTIONTYPE::SERIAL)
		{
			return SerialServerLoop();
		}

		// Check "server_socket_" is valid
		if (server_socket_ == INVALID_SOCKET)
		{
			// Error - Server Socket Is Invalid
			Logging::LogErrorF("NetworkGenisysSlave::ServerLoop: [%d] Invalid Socket", configuration_.server_port_);
			LogWSAError();
			return false;
		}

		
		struct sockaddr_in sender_addr;						// Used to store remote machines address
		int sender_addr_size = sizeof(sender_addr);


		if (configuration_.connection_type_ == NetworkGenisysSlave::Config::CONNECTIONTYPE::TCP)
		{
			// Check if client is connected, if not check if we have an incoming connection
			if (client_socket_ == INVALID_SOCKET)
			{
				client_socket_ = accept(server_socket_, (SOCKADDR*)&sender_addr, &sender_addr_size);
			}

			// Check if client is still not connected
			// We are still waiting for a client connection
			if (client_socket_ == INVALID_SOCKET)
			{
				return false;
			}

			// Check if there are any additional pending connections & reject the socket
			// We reject the socket because we will only accept one connection at a time for now
			SOCKET temp_socket_ = INVALID_SOCKET;
			temp_socket_ = accept(server_socket_, nullptr, nullptr);
			if (temp_socket_ != INVALID_SOCKET)
				closesocket(temp_socket_);


			// Get TCP Message From Sender (Genisys Master)
			// Add a client timeout code check here, if data not recived for a while?
			bytes_received = recv(client_socket_, server_buf, server_buf_len, 0 /* no flags */);

			//remote side (client) has preformed gracefull shut down.
			if (bytes_received == 0) 
			{
				shutdown(client_socket_, SD_SEND);
				closesocket(client_socket_);
				client_socket_ = INVALID_SOCKET;
				return false;
			}

		}
		else // UDP
		{
			// Get UDP Message From Sender (Genisys Master) 
			bytes_received = recvfrom(server_socket_, server_buf, server_buf_len, 0 /* no flags*/, (SOCKADDR*)&sender_addr, &sender_addr_size);
		}








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
		std::string source_ip_str = "";
		PCSTR WSAAPI source_ip = inet_ntop(AF_INET, &(sender_addr.sin_addr), source_ip_buf, (size_t)80);
		if (source_ip == NULL)
		{
			Logging::LogErrorF("NetworkGenisysSlave::ServerLoop: [%d] Unable To Get Source IP Of Incoming Datagram", configuration_.server_port_);
			LogWSAError();
		}
		else
		{
			source_ip_str = std::string(source_ip_buf);
			// Log Senders Details
			Logging::LogDebugF("NetworkGenisysSlave::ServerLoop: [%d] Received Datagram From: [%s]:[%d]", configuration_.server_port_, source_ip, ntohs(sender_addr.sin_port));
		}

		// Convert Server Buffer To Vector
		std::vector<uint8_t> recv_msg;
		for (int i = 0; i < bytes_received; i++)
			recv_msg.push_back(server_buf[i]);

		// Get Genisys Protocol Response Message
		std::vector<uint8_t> respond_msg = recv_msg;
		if (!protocol_->ProcessMessages(respond_msg, source_ip_str))
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
		int send_result = 0;

		if (configuration_.connection_type_ == NetworkGenisysSlave::Config::CONNECTIONTYPE::TCP)
		{
			send_result = send(client_socket_, server_buf, (int)responce_size, 0);
		}
		else // UDP
		{
			if (configuration_.destination_port_ > 0)
				sender_addr.sin_port = htons(configuration_.destination_port_);

			send_result = sendto(server_socket_, server_buf, (int)responce_size, 0, (struct sockaddr*)&sender_addr, sizeof(sender_addr));
		}





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
		// Handle Serial Connection Type
		if (configuration_.connection_type_ == NetworkGenisysSlave::Config::CONNECTIONTYPE::SERIAL)
		{
			return ShutdownSerialServer();
		}

		if (server_socket_ != INVALID_SOCKET)
		{
			if (closesocket(server_socket_) == SOCKET_ERROR)
			{
				// Error - Unable To Close Socket
				Logging::LogErrorF("NetworkGenisysSlave::ShutdownServer: [%d] Unable To Close Server Socket", configuration_.server_port_);
				LogWSAError();
			}
			server_socket_ = INVALID_SOCKET;
		}

		if (client_socket_ != INVALID_SOCKET)
		{
			if (closesocket(client_socket_) == SOCKET_ERROR)
			{
				// Error - Unable To Close Socket
				Logging::LogErrorF("NetworkGenisysSlave::ShutdownServer: [%d] Unable To Close Client TCP Socket", configuration_.server_port_);
				LogWSAError();
			}
			client_socket_ = INVALID_SOCKET;
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


	bool NetworkGenisysSlave::StartSerialServer()
	{
		// Convert COM port string to wide string for CreateFileW
		int wchar_count = MultiByteToWideChar(CP_ACP, 0, configuration_.com_port_.c_str(), -1, NULL, 0);
		wchar_t com_port_wide[20];
		MultiByteToWideChar(CP_ACP, 0, configuration_.com_port_.c_str(), -1, com_port_wide, wchar_count);

		// Open COM Port
		com_handle_ = CreateFileW(
			com_port_wide,
			GENERIC_READ | GENERIC_WRITE,
			0,                              // Exclusive access
			NULL,
			OPEN_EXISTING,
			0,                              // No special attributes
			NULL
		);

		if (com_handle_ == INVALID_HANDLE_VALUE)
		{
			DWORD error_code = GetLastError();
			Logging::LogErrorF("NetworkGenisysSlave::StartSerialServer: Failed to open COM port [%s], Error Code [%lu]",
				configuration_.com_port_.c_str(), error_code);
			return false;
		}

		// Get current DCB settings
		if (!GetCommState(com_handle_, &dcb_config_))
		{
			DWORD error_code = GetLastError();
			Logging::LogErrorF("NetworkGenisysSlave::StartSerialServer: Failed to get COM port state [%s], Error Code [%lu]",
				configuration_.com_port_.c_str(), error_code);
			CloseHandle(com_handle_);
			com_handle_ = INVALID_HANDLE_VALUE;
			return false;
		}

		// Configure DCB settings based on configuration
		dcb_config_.BaudRate = configuration_.baud_rate_;
		dcb_config_.ByteSize = configuration_.data_bits_;
		dcb_config_.StopBits = (configuration_.stop_bits_ == 2) ? TWOSTOPBITS : ONESTOPBIT;
		dcb_config_.fParity = TRUE;

		// Set parity
		switch (configuration_.parity_)
		{
		case 'N':
		case 'n':
			dcb_config_.Parity = NOPARITY;
			dcb_config_.fParity = FALSE;
			break;
		case 'E':
		case 'e':
			dcb_config_.Parity = EVENPARITY;
			break;
		case 'O':
		case 'o':
			dcb_config_.Parity = ODDPARITY;
			break;
		default:
			dcb_config_.Parity = NOPARITY;
			dcb_config_.fParity = FALSE;
		}

		// Disable flow control
		dcb_config_.fOutxCtsFlow = FALSE;
		dcb_config_.fRtsControl = RTS_CONTROL_DISABLE;
		dcb_config_.fOutxDsrFlow = FALSE;
		dcb_config_.fDtrControl = DTR_CONTROL_DISABLE;
		dcb_config_.fInX = FALSE;
		dcb_config_.fOutX = FALSE;

		// Set the new DCB settings
		if (!SetCommState(com_handle_, &dcb_config_))
		{
			DWORD error_code = GetLastError();
			Logging::LogErrorF("NetworkGenisysSlave::StartSerialServer: Failed to set COM port state [%s], Error Code [%lu]",
				configuration_.com_port_.c_str(), error_code);
			CloseHandle(com_handle_);
			com_handle_ = INVALID_HANDLE_VALUE;
			return false;
		}

		// Configure timeouts for non-blocking read/write
		timeouts_.ReadIntervalTimeout = MAXDWORD;
		timeouts_.ReadTotalTimeoutMultiplier = 0;
		timeouts_.ReadTotalTimeoutConstant = configuration_.read_timeout_ms_;
		timeouts_.WriteTotalTimeoutMultiplier = 0;
		timeouts_.WriteTotalTimeoutConstant = configuration_.write_timeout_ms_;

		if (!SetCommTimeouts(com_handle_, &timeouts_))
		{
			DWORD error_code = GetLastError();
			Logging::LogErrorF("NetworkGenisysSlave::StartSerialServer: Failed to set COM port timeouts [%s], Error Code [%lu]",
				configuration_.com_port_.c_str(), error_code);
			CloseHandle(com_handle_);
			com_handle_ = INVALID_HANDLE_VALUE;
			return false;
		}

		// Purge any existing data in the buffers
		if (!PurgeComm(com_handle_, PURGE_RXCLEAR | PURGE_TXCLEAR))
		{
			DWORD error_code = GetLastError();
			Logging::LogErrorF("NetworkGenisysSlave::StartSerialServer: Failed to purge COM port buffers [%s], Error Code [%lu]",
				configuration_.com_port_.c_str(), error_code);
			CloseHandle(com_handle_);
			com_handle_ = INVALID_HANDLE_VALUE;
				return false;
		}

				Logging::LogDebugF("NetworkGenisysSlave::StartSerialServer: [%s] Serial Server Started at %lu baud",
					configuration_.com_port_.c_str(), configuration_.baud_rate_);

				return true;
	}


	bool NetworkGenisysSlave::SerialServerLoop()
	{
		// ==================================================================================
		// VALIDITY CHECK
		// ==================================================================================
		// Verify the serial port handle is valid and open
		if (com_handle_ == INVALID_HANDLE_VALUE)
		{
			Logging::LogErrorF("NetworkGenisysSlave::SerialServerLoop: [%s] Invalid COM port handle",
				configuration_.com_port_.c_str());
			return false;
		}
	
	
		// ==================================================================================
		// DATA RECEPTION - Read message from serial port
		// ==================================================================================
		// Clear the server buffer before reading
		ZeroMemory(server_buf, sizeof(server_buf));
	
		// Attempt to read data from the serial port
		// ReadFile() is synchronous but respects the timeout values set in SetCommTimeouts()
		DWORD bytes_read = 0;
		BOOL read_status = ReadFile(
			com_handle_,                    // Serial port handle
			server_buf,                     // Buffer to store received data
			server_buf_len,                 // Maximum bytes to read
			&bytes_read,                    // Output: actual bytes read
			NULL                            // No overlapped I/O
		);
	
		// Check if Read operation failed
		if (!read_status)
		{
			DWORD error_code = GetLastError();
			// Note: ERROR_SEM_TIMEOUT is expected when no data is available and timeout expires
			if (error_code != ERROR_SEM_TIMEOUT)
			{
				Logging::LogErrorF("NetworkGenisysSlave::SerialServerLoop: [%s] Serial read failed, Error Code [%lu]",
					configuration_.com_port_.c_str(), error_code);
			}
			return false;
		}
	
		// If no bytes were read, the timeout expired with no data available
		if (bytes_read == 0)
		{
			// This is normal operation - just no data received yet
			// Return false to indicate no processing occurred this cycle
			return false;
		}
	
		// Store number of bytes received for later processing
		bytes_received = bytes_read;
	
		// ==================================================================================
		// LOG RECEPTION
		// ==================================================================================
		// Log successful reception of serial data
		Logging::LogDebugF("NetworkGenisysSlave::SerialServerLoop: [%s] Received %d bytes from serial port",
			configuration_.com_port_.c_str(), bytes_received);
	
	
		// ==================================================================================
		// MESSAGE CONVERSION - Convert buffer to vector format
		// ==================================================================================
		// The protocol layer expects std::vector<uint8_t>, so convert the char buffer
		std::vector<uint8_t> recv_msg;
		for (int i = 0; i < bytes_received; i++)
		{
			recv_msg.push_back(static_cast<uint8_t>(server_buf[i]));
		}
	
		// Note: Serial communication doesn't have source IP concept like network protocols
		// Using empty string for source_ip_str parameter in protocol processing
		std::string source_ip_str = "SERIAL:" + configuration_.com_port_;
		
	
		// ==================================================================================
		// PROTOCOL PROCESSING - Process message through Genisys protocol layer
		// ==================================================================================
		// The protocol layer validates, parses, and generates response message
		std::vector<uint8_t> respond_msg = recv_msg;
		if (!protocol_->ProcessMessages(respond_msg, source_ip_str))
		{
			Logging::LogErrorF("NetworkGenisysSlave::SerialServerLoop: [%s] Protocol processing failed",
				configuration_.com_port_.c_str());
			return false;
		}
	
	
		// ==================================================================================
		// RESPONSE BUFFER PREPARATION - Pack response vector back into buffer
		// ==================================================================================
		// Verify the response message fits in the server buffer
		if (respond_msg.size() > server_buf_len)
		{
			Logging::LogErrorF("NetworkGenisysSlave::SerialServerLoop: [%s] Response message too large [%zu bytes]",
				configuration_.com_port_.c_str(), respond_msg.size());
			return false;
		}
	
		// Clear the server buffer
		ZeroMemory(server_buf, sizeof(server_buf));
	
		// Copy response vector into character buffer
		size_t response_size = respond_msg.size();
		for (size_t i = 0; i < response_size; i++)
		{
			server_buf[i] = static_cast<char>(respond_msg[i]);
		}
	
	
		// ==================================================================================
		// DATA TRANSMISSION - Write response message back to serial port
		// ==================================================================================
		// Attempt to write the response message to the serial port
		DWORD bytes_written = 0;
		BOOL write_status = WriteFile(
			com_handle_,                    // Serial port handle
			server_buf,                     // Buffer containing data to send
			static_cast<DWORD>(response_size), // Number of bytes to write
			&bytes_written,                 // Output: actual bytes written
			NULL                            // No overlapped I/O
		);
	
		// Check if Write operation failed
		if (!write_status)
		{
			DWORD error_code = GetLastError();
			Logging::LogErrorF("NetworkGenisysSlave::SerialServerLoop: [%s] Serial write failed, Error Code [%lu]",
				configuration_.com_port_.c_str(), error_code);
			return false;
		}
	
		// Verify all bytes were written
		// Note: On Windows serial ports, bytes_written should match response_size unless error occurred
		if (bytes_written != response_size)
		{
			Logging::LogErrorF("NetworkGenisysSlave::SerialServerLoop: [%s] Incomplete write - sent %lu of %zu bytes",
				configuration_.com_port_.c_str(), bytes_written, response_size);
			return false;
		}
	
		// ==================================================================================
				// LOG TRANSMISSION
				// ==================================================================================
				// Log successful transmission of response
				Logging::LogDebugF("NetworkGenisysSlave::SerialServerLoop: [%s] Sent %lu bytes to serial port",
					configuration_.com_port_.c_str(), bytes_written);
	
				// Successful completion
				return true;
	}


	bool NetworkGenisysSlave::ShutdownSerialServer()
	{
		// ==================================================================================
		// VALIDITY CHECK
		// ==================================================================================
		// Check if the serial port handle is valid
		if (com_handle_ == INVALID_HANDLE_VALUE)
		{
			Logging::LogDebugF("NetworkGenisysSlave::ShutdownSerialServer: [%s] Serial port already closed",
				configuration_.com_port_.c_str());
			return true;  // Already closed, not an error
		}


		// ==================================================================================
		// PURGE BUFFERS
		// ==================================================================================
		// Flush any pending data in the transmit buffer before closing
		// This ensures any final messages are sent before connection closes
		if (!PurgeComm(com_handle_, PURGE_TXCLEAR | PURGE_RXCLEAR))
		{
			DWORD error_code = GetLastError();
			Logging::LogErrorF("NetworkGenisysSlave::ShutdownSerialServer: [%s] Failed to purge buffers before close, Error Code [%lu]",
				configuration_.com_port_.c_str(), error_code);
			// Continue with close even if purge fails
		}


		// ==================================================================================
		// CLOSE SERIAL PORT
		// ==================================================================================
		// Close the serial port handle
		// CloseHandle() will flush any pending writes and release system resources
		if (!CloseHandle(com_handle_))
		{
			DWORD error_code = GetLastError();
			Logging::LogErrorF("NetworkGenisysSlave::ShutdownSerialServer: [%s] Failed to close COM port, Error Code [%lu]",
				configuration_.com_port_.c_str(), error_code);
			// Still mark as closed even if CloseHandle reports error
		}

		// Mark handle as invalid to prevent further operations
		com_handle_ = INVALID_HANDLE_VALUE;


		// ==================================================================================
		// CLEAN UP STRUCTURES
		// ==================================================================================
		// Clear the DCB and timeout structures to indicate serial port is not configured
		ZeroMemory(&dcb_config_, sizeof(DCB));
		ZeroMemory(&timeouts_, sizeof(COMMTIMEOUTS));


		// ==================================================================================
		// LOG CLOSURE
		// ==================================================================================
		// Log successful serial port closure
		Logging::LogDebugF("NetworkGenisysSlave::ShutdownSerialServer: [%s] Serial Server Shutdown Complete",
			configuration_.com_port_.c_str());

		return true;
	}


}