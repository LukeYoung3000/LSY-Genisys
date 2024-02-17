// GenisysSlave-UnitTests.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <chrono>
#include <ostream>



#include "NetworkGenisysSlave.h"
using namespace LSY;


#include "Test.h"


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
	DataFrame_Tests data_frame_tests;
	data_frame_tests.RunTests();




	/*

	// Data Frame Setup
	std::shared_ptr<DataFrame> data_1 = std::make_shared<DataFrame>("Genisys Slave 1", 8 / 1000);
	std::shared_ptr<DataFrame> data_2 = std::make_shared<DataFrame>("Genisys Slave 2", 20);

	// Protocol Setup
	std::shared_ptr<ProtocolGenisysSlave> protocol = std::make_shared<ProtocolGenisysSlave>();
	protocol->AddDataFrame(1, data_1);
	protocol->AddDataFrame(2, data_2);

	// Network Configuration
	NetworkGenisysSlave::Config network_config;
	network_config.server_port_ = 7169;
	network_config.destination_port_ = 7169;

	// Network Setup
	NetworkGenisysSlave network(network_config);
	network.AddProtocol(protocol);




	// ------- Run Network Server ---------

	network.StartServer();

	while (TRUE)
	{
		network.ServerLoop();
	}

	network.ShutdownServer();


	*/



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

