#include "NetworkGenisysMaster.h"
#include "ProtocolGenisysSlave.h"
#include <Windows.h>
#include <Ws2tcpip.h>
#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <thread>
#pragma comment(lib, "Ws2_32.lib")
using Bytes = std::vector<uint8_t>;
void Check(bool value, const char* text) { if (!value) throw std::runtime_error(text); }
#include "UdpTestPeer.h"
struct File
{
	char path[MAX_PATH]{};
	File() { char directory[MAX_PATH]{}; Check(GetTempPathA(MAX_PATH, directory) && GetTempFileNameA(directory, "gma", 0, path), "Create application check file"); }
	~File() { DeleteFileA(path); }
	void Write(const std::string& text) { std::ofstream stream(path); stream << text; Check(bool(stream), "Write application configuration"); }
	std::string Read() const { std::ifstream stream(path); return std::string(std::istreambuf_iterator<char>(stream), {}); }
};
struct Child
{
	File log;
	HANDLE process = nullptr;
	Child(const std::string& arguments)
	{
		SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
		HANDLE output = CreateFileA(log.path, GENERIC_WRITE, FILE_SHARE_READ, &security, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		HANDLE input = CreateFileA("NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &security, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		Check(output != INVALID_HANDLE_VALUE && input != INVALID_HANDLE_VALUE, "Redirect child console handles");
		STARTUPINFOA startup{}; startup.cb = sizeof(startup); startup.dwFlags = STARTF_USESTDHANDLES;
		startup.hStdOutput = startup.hStdError = output; startup.hStdInput = input;
		PROCESS_INFORMATION info{};
		std::string command = "\"..\\GenisysMaster-Console\\builds\\x64\\Debug\\GenisysMaster-Console.exe\" " + arguments;
		const BOOL started = CreateProcessA(nullptr, &command[0], nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &info);
		CloseHandle(output); CloseHandle(input); Check(started != FALSE, "Launch built master console");
		process = info.hProcess; CloseHandle(info.hThread);
	}
	~Child()
	{
		if (process) { if (!Done()) { TerminateProcess(process, 1); WaitForSingleObject(process, 2000); } CloseHandle(process); }
	}
	bool Done() const { return WaitForSingleObject(process, 0) == WAIT_OBJECT_0; }
	DWORD Finish(const std::function<void()>& pump = [] {})
	{
		const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(6);
		while (!Done() && std::chrono::steady_clock::now() < deadline) { pump(); std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
		if (!Done()) { std::cerr << log.Read(); throw std::runtime_error("Master console did not exit within six seconds"); }
		DWORD code = 1; Check(GetExitCodeProcess(process, &code), "Read console exit code"); return code;
	}
};
uint16_t AvailablePort()
{
	SOCKET socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP); Check(socket_ != INVALID_SOCKET, "Create port probe");
	sockaddr_in address{}; address.sin_family = AF_INET; address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	Check(bind(socket_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0, "Bind port probe");
	int size = sizeof(address); Check(getsockname(socket_, reinterpret_cast<sockaddr*>(&address), &size) == 0, "Read available port");
	closesocket(socket_); return ntohs(address.sin_port);
}
int main()
{
	WSADATA wsa{}; if (WSAStartup(MAKEWORD(2, 2), &wsa)) return 1;
	int result = 0;
	try
	{
		{
			Child help("--config nonexistent.xml --help"); Check(help.Finish() == 0 && help.log.Read().find("--serial-slave") != std::string::npos, "Serial help is available without opening XML or ports");
			Child bad("--transport serial --com-port NUL --plain --run-ms 1"); Check(bad.Finish() == 1 && bad.log.Read().find("Invalid serial port") != std::string::npos, "Invalid serial arguments fail before I/O");
			Child missing("--transport serial --com-port COM999999999 --baud 19200 --parity e --stop-bits 2 --dtr true --rts false --serial-slave 3 8 --plain --recall-all");
			Check(missing.Finish() == 1 && missing.log.Read().find("Cannot open serial port") != std::string::npos && missing.log.Read().find("Unknown argument") == std::string::npos, "Unavailable COM port produces a serial-specific startup error");
		}
		std::cout << "PASS: console help, serial CLI validation and unavailable-port diagnostics\n";
		{
			Peer peer; peer.Add(3, 0x81); peer.Add(4, 0x42); const auto port = AvailablePort();
			const std::string options = "--plain --local-ip 127.0.0.1 --local-port " + std::to_string(port) +
				" --slave 3 127.0.0.1 " + std::to_string(peer.port) + " 8 --slave 4 127.0.0.1 " + std::to_string(peer.port) + " 8 --recall-all --wait-ms 3000";
			Child child(options); const DWORD code = child.Finish([&] { peer.Process(port); });
			if (code) std::cerr << child.log.Read();
			Check(code == 0 && peer.station_requests[3][0xFD] == 1 && peer.station_requests[4][0xFD] == 1 && peer.requests[0xFA] == 2 && peer.requests[0xFC] == 0, "Actual console recalls every UDP ID and acknowledges without sending controls");
			Check(child.log.Read().find("UDP 127.0.0.1:" + std::to_string(port)) != std::string::npos && child.log.Read().find("Slave 4 F1 received") != std::string::npos, "Console transport banner and completion output");
		}
		std::cout << "PASS: built console one-shot recalls against multiple UDP slave IDs\n";
		{
			Peer peer; peer.Add(3, 0x24); const auto port = AvailablePort(); File xml;
			xml.Write("<GenisysMaster version='1' transport='serial'><Serial port='COM999999999'/><Slaves><Slave id='34' frame-bytes='8'/></Slaves></GenisysMaster>");
			Child child("--plain --config \"" + std::string(xml.path) + "\" --transport udp --local-ip 127.0.0.1 --local-port " + std::to_string(port) +
				" --slave 3 127.0.0.1 " + std::to_string(peer.port) + " 8 --run-ms 450 --poll-ms 25 --timeout-ms 100");
			const DWORD code = child.Finish([&] { peer.Process(port); }); if (code) std::cerr << child.log.Read();
			Check(code == 0 && peer.requests[0xFD] == 1 && peer.requests[0xFB] > 1 && peer.requests[0xFC] == 0 && child.log.Read().find("indications-current=1") != std::string::npos,
				"XML-to-UDP CLI override and bounded polling still work in the built console");
		}
		std::cout << "PASS: built console XML precedence and bounded polling\nAll ConsoleApp checks passed.\n";
	}
	catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; result = 1; }
	WSACleanup(); return result;
}
