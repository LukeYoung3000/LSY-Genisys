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



// Logging Functions
bool LogDebug(const std::string& log_msg);
bool LogInfo(const std::string& log_msg);
bool LogWarning(const std::string& log_msg);
bool LogError(const std::string& log_msg);



int main()
{

	// Add Logging Functions To "GenisysSlave-Lib"
	Logging::AddLogDebugFunc(LogDebug);
	Logging::AddLogInfoFunc(LogInfo);
	Logging::AddLogWarningFunc(LogWarning);
	Logging::AddLogErrorFunc(LogError);


	// Run Unit Tests
	//DataFrame_Tests data_frame_tests;
	//data_frame_tests.RunTests();



	// Configuration
	uint64_t num_bytes	= 125;
	uint8_t slave_id	= 34;		// Hex 22
	uint16_t port_rx	= 7169;
	uint16_t port_tx	= 7170;

	Logging::LogInfoF("Num Bytes [%d]", num_bytes);
	Logging::LogInfoF("Slave ID  [%d]", slave_id);
	Logging::LogInfoF("Port RX   [%d]", port_rx);
	Logging::LogInfoF("Port TX   [%d]", port_tx);

	// Data Frame Setup
	std::shared_ptr<DataFrame> data_1 = std::make_shared<DataFrame>("Genisys Slave 1", num_bytes);

	// Protocol Setup
	std::shared_ptr<ProtocolGenisysSlave> protocol = std::make_shared<ProtocolGenisysSlave>();
	protocol->AddDataFrame(slave_id, data_1);

	// Network Configuration
	NetworkGenisysSlave::Config network_config;
	network_config.server_port_ = port_rx;
	network_config.destination_port_ = port_tx;

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

