#pragma once
#include "NetworkGenisysMaster.h"

struct ConsoleSlaveConfiguration
{
	uint8_t id = 34;
	LSY::NetworkGenisysMaster::Endpoint endpoint;
	unsigned frame_bytes = 125;
};

struct ConsoleConfiguration
{
	LSY::NetworkGenisysMaster::Config network;
	std::vector<ConsoleSlaveConfiguration> slaves;
	bool plain = false;
	bool recall_all = false;
	unsigned wait_ms = 3000;
	unsigned run_ms = 0;
};

// Loads into a temporary object; failures leave the caller's settings unchanged.
// Throws an actionable error for malformed XML, unknown settings or bad values.
void LoadConsoleConfiguration(const std::string& path, ConsoleConfiguration& configuration);
