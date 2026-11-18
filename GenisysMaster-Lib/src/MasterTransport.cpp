#include "MasterTransport.h"
#include "GenisysMaster-Lib-Logging.h"
#include <Ws2tcpip.h>
#include <Windows.h>
#include <array>
#include <algorithm>
#include <cctype>
#pragma comment(lib, "Ws2_32.lib")
namespace LSY
{
namespace
{
	class UdpTransport final : public MasterTransport
	{
		NetworkGenisysMaster::Config config_;
		SOCKET socket_ = INVALID_SOCKET;
		bool wsa_ = false;
		uint16_t port_ = 0;
	public:
		explicit UdpTransport(const NetworkGenisysMaster::Config& config) : config_(config) {}
		~UdpTransport() override { Stop(); }
		bool IsOpen() const override { return socket_ != INVALID_SOCKET; }
		uint16_t LocalPort() const override { return port_; }
		void Stop() override
		{
			if (IsOpen()) { closesocket(socket_); socket_ = INVALID_SOCKET; }
			if (wsa_) { WSACleanup(); wsa_ = false; }
			port_ = 0;
		}
		bool Start() override
		{
			if (IsOpen()) return false;
			sockaddr_in local{}; local.sin_family = AF_INET; local.sin_port = htons(config_.receive_port_);
			if (InetPtonA(AF_INET, config_.local_ip_.c_str(), &local.sin_addr) != 1) return false;
			WSADATA data{};
			if (WSAStartup(MAKEWORD(2, 2), &data)) return false;
			wsa_ = true; socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
			BOOL exclusive = TRUE; u_long nonblocking = 1; int size = sizeof(local);
			if (!IsOpen() || setsockopt(socket_, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char*>(&exclusive), sizeof(exclusive)) == SOCKET_ERROR ||
				bind(socket_, reinterpret_cast<sockaddr*>(&local), sizeof(local)) == SOCKET_ERROR ||
				ioctlsocket(socket_, FIONBIO, &nonblocking) == SOCKET_ERROR ||
				getsockname(socket_, reinterpret_cast<sockaddr*>(&local), &size) == SOCKET_ERROR)
			{
				MasterLogging::LogErrorF("NetworkGenisysMaster: UDP start failed [%d]", WSAGetLastError()); Stop(); return false;
			}
			port_ = ntohs(local.sin_port); return true;
		}
		bool Send(const sockaddr_in& endpoint, const std::vector<uint8_t>& bytes) override
		{
			if (!IsOpen() || bytes.empty()) return false;
			if (sendto(socket_, reinterpret_cast<const char*>(bytes.data()), static_cast<int>(bytes.size()), 0,
				reinterpret_cast<const sockaddr*>(&endpoint), sizeof(endpoint)) == static_cast<int>(bytes.size())) return true;
			MasterLogging::LogErrorF("NetworkGenisysMaster: UDP send failed [%d]", WSAGetLastError()); return false;
		}
		READ Receive(std::vector<uint8_t>& bytes, sockaddr_in& source) override
		{
			char buffer[65536]; int size = sizeof(source);
			const int count = recvfrom(socket_, buffer, sizeof(buffer), 0, reinterpret_cast<sockaddr*>(&source), &size);
			if (count == SOCKET_ERROR)
			{
				if (WSAGetLastError() == WSAEWOULDBLOCK) return READ::EMPTY;
				MasterLogging::LogErrorF("NetworkGenisysMaster: UDP receive failed [%d]", WSAGetLastError()); return READ::FAILED;
			}
			bytes.assign(buffer, buffer + count); return READ::DATA;
		}
	};

	class SerialTransport final : public MasterTransport
	{
		using Clock = std::chrono::steady_clock;
		NetworkGenisysMaster::Config config_;
		HANDLE port_ = INVALID_HANDLE_VALUE;
		OVERLAPPED read_{}, write_{};
		bool reading_ = false, discard_read_ = false, writing_io_ = false, writing_ = false;
		std::array<uint8_t, 4096> read_buffer_{};
		std::vector<uint8_t> write_buffer_;
		size_t written_ = 0;
		Clock::time_point write_deadline_;
		bool Error(const char* operation)
		{
			MasterLogging::LogErrorF("NetworkGenisysMaster: Serial %s failed [%lu]", operation, GetLastError());
			Stop(); return false;
		}
		bool Status(COMSTAT& status)
		{
			DWORD errors = 0;
			if (!ClearCommError(port_, &errors, &status)) return Error("status");
			if (errors)
			{
				MasterLogging::LogErrorF("NetworkGenisysMaster: Serial line errors [0x%lx]", errors);
				Stop(); return false;
			}
			return true;
		}
		bool BeginWrite()
		{
			ResetEvent(write_.hEvent);
			// Retrieve byte counts through GetOverlappedResult for both immediate
			// and deferred completions; do not use synchronous byte-count outputs.
			if (!WriteFile(port_, write_buffer_.data() + written_, static_cast<DWORD>(write_buffer_.size() - written_), nullptr, &write_) &&
				GetLastError() != ERROR_IO_PENDING) return Error("write");
			writing_io_ = true; return true;
		}
	public:
		explicit SerialTransport(const NetworkGenisysMaster::Config& config) : config_(config) {}
		~SerialTransport() override { Stop(); }
		bool IsOpen() const override { return port_ != INVALID_HANDLE_VALUE; }
		void Stop() override
		{
			if (IsOpen())
			{
				CancelIoEx(port_, nullptr);
				PurgeComm(port_, PURGE_RXABORT | PURGE_RXCLEAR | PURGE_TXABORT | PURGE_TXCLEAR);
				// CancelIoEx requests cancellation; storage must survive until completion.
				DWORD ignored = 0;
				if (reading_) GetOverlappedResult(port_, &read_, &ignored, TRUE);
				if (writing_io_) GetOverlappedResult(port_, &write_, &ignored, TRUE);
				CloseHandle(port_); port_ = INVALID_HANDLE_VALUE;
			}
			if (read_.hEvent) CloseHandle(read_.hEvent);
			if (write_.hEvent) CloseHandle(write_.hEvent);
			read_ = {}; write_ = {}; reading_ = discard_read_ = writing_io_ = writing_ = false;
			write_buffer_.clear(); written_ = 0;
		}
		bool Start() override
		{
			if (IsOpen()) return false;
			std::string name = config_.com_port_;
			for (auto& ch : name) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
			if (name.compare(0, 4, "\\\\.\\") == 0) name.erase(0, 4);
			bool valid = name.size() > 3 && name.size() <= 12 && name.compare(0, 3, "COM") == 0;
			bool nonzero = false;
			for (size_t i = 3; i < name.size(); ++i) { valid = valid && name[i] >= '0' && name[i] <= '9'; nonzero = nonzero || name[i] != '0'; }
			const char parity = static_cast<char>(std::toupper(static_cast<unsigned char>(config_.parity_)));
			if (!valid || !nonzero || !config_.baud_rate_ || config_.data_bits_ != 8 ||
				(config_.stop_bits_ != 1 && config_.stop_bits_ != 2) || (parity != 'N' && parity != 'E' && parity != 'O') || !config_.write_timeout_ms_)
			{
				MasterLogging::LogError("NetworkGenisysMaster: Invalid serial settings (COMn, baud>0, 8 data bits, 1/2 stop bits, N/E/O parity, write timeout>0)"); return false;
			}
			name = "\\\\.\\" + name;
			const std::wstring path(name.begin(), name.end());
			port_ = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
			if (!IsOpen()) return Error("open COM port");
			read_.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr); write_.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
			if (!read_.hEvent || !write_.hEvent) return Error("create I/O events");
			DCB dcb{}; dcb.DCBlength = sizeof(dcb);
			if (!GetCommState(port_, &dcb)) return Error("get settings");
			dcb.BaudRate = config_.baud_rate_; dcb.ByteSize = 8; dcb.StopBits = config_.stop_bits_ == 2 ? TWOSTOPBITS : ONESTOPBIT;
			dcb.Parity = parity == 'E' ? EVENPARITY : parity == 'O' ? ODDPARITY : NOPARITY;
			dcb.fBinary = TRUE; dcb.fParity = parity != 'N'; dcb.fOutxCtsFlow = FALSE; dcb.fOutxDsrFlow = FALSE;
			dcb.fDtrControl = config_.dtr_enable_ ? DTR_CONTROL_ENABLE : DTR_CONTROL_DISABLE;
			dcb.fRtsControl = config_.rts_enable_ ? RTS_CONTROL_ENABLE : RTS_CONTROL_DISABLE;
			dcb.fDsrSensitivity = FALSE; dcb.fTXContinueOnXoff = TRUE; dcb.fOutX = FALSE; dcb.fInX = FALSE;
			dcb.fErrorChar = FALSE; dcb.fNull = FALSE; dcb.fAbortOnError = FALSE;
			if (!SetCommState(port_, &dcb)) return Error("set settings");
			COMMTIMEOUTS timeouts{}; timeouts.ReadIntervalTimeout = MAXDWORD;
			timeouts.WriteTotalTimeoutConstant = config_.write_timeout_ms_;
			if (!SetCommTimeouts(port_, &timeouts) || !SetupComm(port_, 16384, 16384) ||
				!PurgeComm(port_, PURGE_RXCLEAR | PURGE_TXCLEAR)) return Error("configure buffers/timeouts");
			return true;
		}
		bool Send(const sockaddr_in&, const std::vector<uint8_t>& bytes) override
		{
			if (!IsOpen() || writing_ || bytes.empty()) return false;
			write_buffer_ = bytes; written_ = 0; writing_ = true;
			write_deadline_ = Clock::now() + std::chrono::milliseconds(config_.write_timeout_ms_);
			return BeginWrite();
		}
		bool WritePending() const override { return writing_; }
		bool PollWrite() override
		{
			if (!writing_) return IsOpen();
			if (writing_io_)
			{
				DWORD count = 0;
				if (!GetOverlappedResult(port_, &write_, &count, FALSE))
				{
					if (GetLastError() != ERROR_IO_INCOMPLETE) return Error("complete write");
				}
				else
				{
					writing_io_ = false;
					if (!count) { SetLastError(ERROR_WRITE_FAULT); return Error("zero-byte write"); }
					written_ += count;
				}
			}
			if (!writing_io_)
			{
				if (written_ < write_buffer_.size())
				{
					if (Clock::now() >= write_deadline_) { SetLastError(ERROR_TIMEOUT); return Error("write timeout"); }
					if (!BeginWrite()) return false;
				}
				else
				{
					COMSTAT status{}; if (!Status(status)) return false;
					if (!status.cbOutQue) { writing_ = false; write_buffer_.clear(); return true; }
				}
			}
			if (Clock::now() >= write_deadline_) { SetLastError(ERROR_TIMEOUT); return Error("write timeout"); }
			return true;
		}
		READ Receive(std::vector<uint8_t>& bytes, sockaddr_in&) override
		{
			if (!IsOpen()) return READ::FAILED;
			if (writing_) return READ::EMPTY;
			DWORD count = 0;
			if (reading_)
			{
				if (!GetOverlappedResult(port_, &read_, &count, FALSE))
				{
					if (GetLastError() == ERROR_IO_INCOMPLETE) return READ::EMPTY;
					if (discard_read_ && GetLastError() == ERROR_OPERATION_ABORTED)
					{ reading_ = discard_read_ = false; return READ::EMPTY; }
					Error("complete read"); return READ::FAILED;
				}
				reading_ = false;
				if (discard_read_) { discard_read_ = false; return READ::EMPTY; }
			}
			else
			{
				COMSTAT status{}; if (!Status(status)) return READ::FAILED;
				if (!status.cbInQue) return READ::EMPTY;
				ResetEvent(read_.hEvent);
				if (!ReadFile(port_, read_buffer_.data(), (std::min)(status.cbInQue, static_cast<DWORD>(read_buffer_.size())), nullptr, &read_) &&
					GetLastError() != ERROR_IO_PENDING) { Error("read"); return READ::FAILED; }
				reading_ = true; return READ::EMPTY;
			}
			COMSTAT checked{}; if (!Status(checked)) return READ::FAILED;
			if (!count) return READ::EMPTY;
			bytes.assign(read_buffer_.begin(), read_buffer_.begin() + count); return READ::DATA;
		}
		bool DiscardInput() override
		{
			if (!IsOpen()) return false;
			// Abandon stale input without waiting or reusing an in-flight read buffer.
			if (reading_)
			{
				CancelIoEx(port_, &read_); discard_read_ = true;
			}
			if (!PurgeComm(port_, PURGE_RXCLEAR)) return Error("discard stale input");
			return true;
		}
	};
}
	std::unique_ptr<MasterTransport> CreateMasterTransport(const NetworkGenisysMaster::Config& config)
	{
		using Type = NetworkGenisysMaster::Config::CONNECTIONTYPE;
		if (config.connection_type_ == Type::UDP) return std::make_unique<UdpTransport>(config);
		if (config.connection_type_ == Type::SERIAL) return std::make_unique<SerialTransport>(config);
		return nullptr;
	}
}
