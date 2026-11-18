#pragma once
#include "NetworkGenisysMaster.h"

struct ConsoleSlaveConfiguration
{
	uint8_t id = 34;
	LSY::NetworkGenisysMaster::Endpoint endpoint;
	unsigned frame_bytes = 125;
	bool has_endpoint = true; // False for serial-only ID/frame-size entries.
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
void LoadConsoleConfiguration(const std::wstring& path, ConsoleConfiguration& configuration);

struct ConsoleArguments
{
	ConsoleConfiguration settings;
	std::string config_path;
	bool help = false;
};
// Arguments exclude argv[0]. XML loads first; CLI scalar/list overrides follow.
ConsoleArguments ParseConsoleArguments(const std::vector<std::string>& arguments);
void ValidateConsoleConfiguration(const ConsoleConfiguration& configuration);
std::string ConsoleTransportDescription(const LSY::NetworkGenisysMaster::Config& config, uint16_t local_port);
std::string ConsoleSlaveDestination(const LSY::NetworkGenisysMaster::Config& config,
	const LSY::NetworkGenisysMaster::Endpoint& endpoint);
