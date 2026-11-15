// GenisysSlave-UnitTests.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <chrono>
#include <ostream>
#include <thread>
#include <chrono>

#include "NetworkGenisysSlave.h"

#include "Test.h"


using namespace std::chrono_literals;
using namespace LSY;


// Event Callback Functions
void EventCallback(std::string table_name, LSY::DataFrame::CALLBACKEVENT event_type);

// Logging Functions
bool LogDebug(const std::string& log_msg);
bool LogInfo(const std::string& log_msg);
bool LogWarning(const std::string& log_msg);
bool LogError(const std::string& log_msg);

bool ModeByteTests()
{
	auto indications = std::make_shared<DataFrame>("Indications", 1);
	auto controls = std::make_shared<DataFrame>("Controls", 1);
	ProtocolGenisysSlave protocol;
	ProtocolGenisysSlave::SlaveData data;
	if (!protocol.AddDataFrame(1, indications, controls) ||
		!protocol.GetSlaveData(1, data) || data.has_received_mode_byte ||
		data.last_mode_byte_recived != 0 || data.mode_byte_return_value != 0x04 ||
		protocol.SetModeByteReturnValue(2, 0x10))
		return false;

	std::vector<uint8_t> message = { 0xFC, 0x01, 0xE0, 0x00, 0x00, 0x00, 0xF6 };
	if (!protocol.ProcessMessages(message, "master") || message.size() < 4 ||
		message[0] != 0xF2 || message[2] != 0xE0 || message[3] != 0x04 ||
		!protocol.GetSlaveData(1, data) || !data.has_received_mode_byte || data.last_mode_byte_recived != 0)
		return false;

	if (!protocol.SetModeByteReturnValue(1, 0xFF))
		return false;
	message = { 0xFC, 0x01, 0xE0, 0xF0, 0x06, 0x00, 0x00, 0xF6 };
	if (!protocol.ProcessMessages(message, "master") || message.size() < 5 ||
		message[0] != 0xF2 || message[2] != 0xE0 || message[3] != 0xF0 || message[4] != 0x0F ||
		!protocol.GetSlaveData(1, data) || data.last_mode_byte_recived != 0xF6 || data.mode_byte_return_value != 0xFF)
		return false;

	message = { 0xFD, 0x01, 0x00, 0x00, 0xF6 };
	if (!protocol.ProcessMessages(message, "master") || message.size() < 7 ||
		message[0] != 0xF2 || message[4] != 0xE0 || message[5] != 0xF0 || message[6] != 0x0F ||
		!protocol.GetSlaveData(1, data) || data.last_mode_byte_recived != 0xF6)
		return false;

	return true;
}

bool ProtocolLoggingTests()
{
	auto indications = std::make_shared<DataFrame>("Indications", 1);
	auto controls = std::make_shared<DataFrame>("Controls", 1);
	ProtocolGenisysSlave protocol;
	if (!protocol.AddDataFrame(1, indications, controls))
		return false;

	std::vector<std::string> info_messages;
	std::vector<std::string> protocol_messages;
	Logging::AddLogInfoFunc([&info_messages](std::string message) { info_messages.push_back(message); return true; });

	auto process_message = [&protocol]()
		{
			std::vector<uint8_t> message = { 0xFC, 0x01, 0xE0, 0x00, 0x00, 0x00, 0xF6 };
			return protocol.ProcessMessages(message, "master");
		};

	bool passed = process_message() && !info_messages.empty();
	info_messages.clear();
	Logging::AddLogProtoFunc([&protocol_messages](std::string message) { protocol_messages.push_back(message); return true; });
	passed = process_message() && passed && !protocol_messages.empty() && info_messages.empty();
	passed = passed && protocol_messages[0].find(",master,1,RECV,") != std::string::npos;

	Logging::AddLogProtoFunc([](const std::string&) { return false; });
	passed = process_message() && passed && !info_messages.empty();
	Logging::AddLogInfoFunc(LogInfo);
	return passed;
}


int main(int argc, char* argv[])
{
	 
	// Add Logging Functions To "GenisysSlave-Lib"
	Logging::AddLogDebugFunc(LogDebug);
	Logging::AddLogInfoFunc(LogInfo);
	Logging::AddLogWarningFunc(LogWarning);
	Logging::AddLogErrorFunc(LogError);

	if (argc > 1 && std::string(argv[1]) == "--protocol-logging-tests")
	{
		bool passed = ProtocolLoggingTests();
		std::cout << (passed ? "Protocol logging tests passed" : "Protocol logging tests failed") << std::endl;
		return passed ? 0 : 1;
	}

	if (argc > 1 && std::string(argv[1]) == "--mode-byte-tests")
	{
		bool passed = ModeByteTests();
		std::cout << (passed ? "Mode byte tests passed" : "Mode byte tests failed") << std::endl;
		return passed ? 0 : 1;
	}

	// Run Unit Tests
	//DataFrame_Tests data_frame_tests;
	//data_frame_tests.RunTests();



	// Configuration
	uint64_t num_bytes	= 125;
	uint8_t slave_id	= 34;		// Hex 22
	uint16_t port_rx	= 7169;
	uint16_t port_tx	= 7170;
	bool is_tcp			= false;

	Logging::LogInfoF("Num Bytes [%d]", num_bytes);
	Logging::LogInfoF("Slave ID  [%d]", slave_id);
	Logging::LogInfoF("Port RX   [%d]", port_rx);
	Logging::LogInfoF("Port TX   [%d]", port_tx);
	if (is_tcp)
		Logging::LogInfoF("Port TYPE   [TCP]");
	else
		Logging::LogInfoF("Port TYPE   [UDP]");
	

	// Data Frame Setup
	std::shared_ptr<DataFrame> data_ind = std::make_shared<DataFrame>("Genisys Slave 1 Indications", num_bytes);
	std::shared_ptr<DataFrame> data_ctrl = std::make_shared<DataFrame>("Genisys Slave 1 Controls", num_bytes);
	std::function<void(std::string, LSY::DataFrame::CALLBACKEVENT)> callback = &EventCallback;
	data_ctrl->AddEventCallback(callback); // Add callback fuction for control frame events


	// Protocol Setup
	std::shared_ptr<ProtocolGenisysSlave> protocol = std::make_shared<ProtocolGenisysSlave>();
	protocol->AddDataFrame(slave_id, data_ind, data_ctrl);
	


	// Network Configuration
	NetworkGenisysSlave::Config network_config;
	network_config.server_port_ = port_rx;
	network_config.destination_port_ = port_tx;
	if (is_tcp)
		network_config.connection_type_ = NetworkGenisysSlave::Config::CONNECTIONTYPE::TCP;
	else
		network_config.connection_type_ = NetworkGenisysSlave::Config::CONNECTIONTYPE::UDP;



	// Network Setup
	NetworkGenisysSlave network(network_config);
	network.AddProtocol(protocol);


	// ------- Run Network Server ---------

	network.StartServer();

	while (TRUE)
	{
		network.ServerLoop();
		std::this_thread::sleep_for(100ms);
	}

	network.ShutdownServer();


	return true;

}




void EventCallback(std::string table_name, LSY::DataFrame::CALLBACKEVENT event_type)
{
	
	switch (event_type)
	{
		case (LSY::DataFrame::CALLBACKEVENT::CONTROL_CHANGE):
			LogInfo("---- EventCallback - Control Change - " + table_name);
			break;

		case (LSY::DataFrame::CALLBACKEVENT::DEFAULT):
			LogInfo("---- EventCallback - Default - " + table_name);
			break;

		default:
			LogInfo("---- EventCallback - Invalid - " + table_name);
			break;
	}

}

bool LogDebug(const std::string& log_msg)
{
	auto time_now = std::chrono::system_clock::now();
	std::cout << std::chrono::system_clock::to_time_t(time_now);
	std::cout << ",DEBUG," << log_msg << std::endl;

	return true;
}

bool LogInfo(const std::string& log_msg)
{
	auto time_now = std::chrono::system_clock::now();
	std::cout << std::chrono::system_clock::to_time_t(time_now);
	std::cout << ",INFO," << log_msg << std::endl;

	return true;
}

bool LogWarning(const std::string& log_msg)
{
	auto time_now = std::chrono::system_clock::now();
	std::cout << std::chrono::system_clock::to_time_t(time_now);
	std::cout << ",WARNING," << log_msg << std::endl;

	return true;
}

bool LogError(const std::string& log_msg)
{
	auto time_now = std::chrono::system_clock::now();
	std::cout << std::chrono::system_clock::to_time_t(time_now);
	std::cout << ",ERROR," << log_msg << std::endl;

	return true;
}

