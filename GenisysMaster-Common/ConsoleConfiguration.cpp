#include "ConsoleConfiguration.h"
#include "third_party/pugixml/pugixml.hpp"
#include <Ws2tcpip.h>
#include <set>
#include <stdexcept>
#include <algorithm>
#include <cctype>
#include <limits>

#pragma comment(lib, "Ws2_32.lib")

namespace
{
	using Node = pugi::xml_node;
	void Fail(const std::string& message) { throw std::runtime_error(message); }
	void Shape(Node node, std::initializer_list<const char*> attributes, std::initializer_list<const char*> children)
	{
		std::set<std::string> allowed_attributes(attributes.begin(), attributes.end());
		std::set<std::string> allowed_children(children.begin(), children.end());
		std::set<std::string> seen;
		for (auto attribute : node.attributes())
		{
			if (!allowed_attributes.count(attribute.name())) Fail("Unknown attribute '" + std::string(attribute.name()) + "' on <" + node.name() + ">");
			if (!seen.insert(attribute.name()).second) Fail("Duplicate attribute on <" + std::string(node.name()) + ">");
		}
		for (auto child : node.children())
		{
			if (child.type() == pugi::node_comment) continue;
			if (child.type() != pugi::node_element || !allowed_children.count(child.name()))
				Fail("Unexpected content '" + std::string(child.name()) + "' in <" + node.name() + ">");
		}
	}
	Node Section(Node root, const char* name)
	{
		auto node = root.child(name);
		if (node && node.next_sibling(name)) Fail("Duplicate <" + std::string(name) + "> section");
		return node;
	}
	std::string Required(Node node, const char* name)
	{
		auto attribute = node.attribute(name);
		if (!attribute || !*attribute.value()) Fail("Missing attribute '" + std::string(name) + "' on <" + node.name() + ">");
		return attribute.value();
	}
	unsigned Number(Node node, const char* name, unsigned low, unsigned high, unsigned fallback, bool required = false)
	{
		if (!required && !node.attribute(name)) return fallback;
		const auto text = Required(node, name);
		try
		{
			const bool hex = text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X');
			for (size_t i = hex ? 2 : 0; i < text.size(); ++i)
				if (!((text[i] >= '0' && text[i] <= '9') || (hex && ((text[i] >= 'a' && text[i] <= 'f') || (text[i] >= 'A' && text[i] <= 'F')))))
					throw std::runtime_error("Not numeric");
			size_t used = 0; const auto value = std::stoul(text, &used, hex ? 16 : 10);
			if (used != text.size() || value < low || value > high) throw std::runtime_error("Out of range");
			return static_cast<unsigned>(value);
		}
		catch (const std::exception&)
		{
			Fail("Invalid '" + std::string(name) + "' on <" + node.name() + ">: expected " + std::to_string(low) + ".." + std::to_string(high));
		}
		return fallback;
	}
	bool Boolean(Node node, const char* name, bool fallback)
	{
		if (!node.attribute(name)) return fallback;
		const auto text = Required(node, name);
		if (text == "true") return true;
		if (text == "false") return false;
		Fail("Invalid '" + std::string(name) + "' on <" + node.name() + ">: expected true or false");
		return false;
	}
	std::string Address(Node node, const char* name, const std::string& fallback, bool required = false)
	{
		const auto text = node.attribute(name) || required ? Required(node, name) : fallback;
		in_addr address{};
		if (InetPtonA(AF_INET, text.c_str(), &address) != 1) Fail("Invalid IPv4 address '" + text + "' on <" + node.name() + ">");
		return text;
	}
}

template<typename Path> void LoadConfigurationImpl(const Path& path, const std::string& displayPath, ConsoleConfiguration& configuration)
{
	try
	{
		pugi::xml_document document;
		const auto result = document.load_file(path.c_str(), pugi::parse_default | pugi::parse_declaration | pugi::parse_doctype);
		if (!result) Fail(std::string(result.description()) + " (XML offset " + std::to_string(result.offset) + ")");
		const auto root = document.document_element();
		if (std::string(root.name()) != "GenisysMaster") Fail("Expected <GenisysMaster> root");
		unsigned elements = 0;
		for (auto node : document.children())
		{
			if (node.type() == pugi::node_element) ++elements;
			else if (node.type() != pugi::node_declaration && node.type() != pugi::node_comment) Fail("Unsupported XML document content");
		}
		if (elements != 1) Fail("Expected exactly one root element");
		Shape(root, {"version", "transport"}, {"Udp", "Serial", "Polling", "Console", "Slaves"});
		if (Required(root, "version") != "1") Fail("Unsupported configuration version (expected 1)");
		ConsoleConfiguration loaded;
		const std::string transport = root.attribute("transport").as_string("udp");
		if (transport == "serial") loaded.network.connection_type_ = LSY::NetworkGenisysMaster::Config::CONNECTIONTYPE::SERIAL;
		else if (transport != "udp") Fail("Invalid transport: expected udp or serial");
		if (auto udp = Section(root, "Udp"))
		{
			Shape(udp, {"local-ip", "receive-port"}, {});
			loaded.network.local_ip_ = Address(udp, "local-ip", loaded.network.local_ip_);
			loaded.network.receive_port_ = static_cast<uint16_t>(Number(udp, "receive-port", 0, 65535, loaded.network.receive_port_));
		}
		if (auto serial = Section(root, "Serial"))
		{
			Shape(serial, {"port", "baud-rate", "data-bits", "parity", "stop-bits", "write-timeout-ms", "dtr", "rts"}, {});
			auto& config = loaded.network;
			config.com_port_ = serial.attribute("port").as_string(config.com_port_.c_str());
			config.baud_rate_ = Number(serial, "baud-rate", 1, (std::numeric_limits<unsigned>::max)(), config.baud_rate_);
			config.data_bits_ = static_cast<uint8_t>(Number(serial, "data-bits", 8, 8, 8));
			config.stop_bits_ = static_cast<uint8_t>(Number(serial, "stop-bits", 1, 2, 1));
			const std::string parity = serial.attribute("parity").as_string("N");
			if (parity.size() != 1) Fail("Serial parity must be N, E or O");
			config.parity_ = static_cast<char>(std::toupper(static_cast<unsigned char>(parity[0])));
			config.write_timeout_ms_ = Number(serial, "write-timeout-ms", 1, 600000, config.write_timeout_ms_);
			config.dtr_enable_ = Boolean(serial, "dtr", false); config.rts_enable_ = Boolean(serial, "rts", false);
		}
		if (auto polling = Section(root, "Polling"))
		{
			Shape(polling, {"enabled", "secure", "interval-ms", "response-timeout-ms", "retries", "recovery-interval-ms"}, {});
			loaded.network.automatic_polling_ = Boolean(polling, "enabled", true);
			loaded.network.secure_poll_ = Boolean(polling, "secure", true);
			loaded.network.poll_interval_ms_ = Number(polling, "interval-ms", 1, 600000, 500);
			loaded.network.response_timeout_ms_ = Number(polling, "response-timeout-ms", 1, 600000, 500);
			loaded.network.retry_count_ = Number(polling, "retries", 0, 100, 2);
			loaded.network.recovery_interval_ms_ = Number(polling, "recovery-interval-ms", 1, 600000, 1000);
		}
		if (auto console = Section(root, "Console"))
		{
			Shape(console, {"plain", "recall-all", "wait-ms", "run-ms"}, {});
			loaded.plain = Boolean(console, "plain", false);
			loaded.recall_all = Boolean(console, "recall-all", false);
			loaded.wait_ms = Number(console, "wait-ms", 1, 600000, 3000);
			loaded.run_ms = Number(console, "run-ms", 0, 600000, 0);
		}
		auto slaves = Section(root, "Slaves");
		if (!slaves) Fail("Missing <Slaves> section");
		Shape(slaves, {}, {"Slave"});
		std::set<uint8_t> ids;
		for (auto node : slaves.children("Slave"))
		{
			Shape(node, {"id", "ip", "port", "frame-bytes"}, {});
			ConsoleSlaveConfiguration slave;
			slave.id = static_cast<uint8_t>(Number(node, "id", 1, 255, 0, true));
			slave.has_endpoint = node.attribute("ip") && node.attribute("port");
			if (bool(node.attribute("ip")) != bool(node.attribute("port"))) Fail("Slave endpoint requires both ip and port");
			if (slave.has_endpoint)
			{
				slave.endpoint.ip_address_ = Address(node, "ip", "", true);
				slave.endpoint.port_ = static_cast<uint16_t>(Number(node, "port", 1, 65535, 0, true));
			}
			slave.frame_bytes = Number(node, "frame-bytes", 1, 224, 0, true);
			if (!ids.insert(slave.id).second) Fail("Duplicate slave ID " + std::to_string(slave.id));
			loaded.slaves.push_back(slave);
		}
		if (loaded.slaves.empty()) Fail("<Slaves> must contain at least one <Slave>");
		ValidateConsoleConfiguration(loaded);
		configuration = loaded;
	}
	catch (const std::exception& error) { throw std::runtime_error("Config '" + displayPath + "': " + error.what()); }
}

void LoadConsoleConfiguration(const std::string& path, ConsoleConfiguration& configuration)
{
    LoadConfigurationImpl(path, path, configuration);
}
void LoadConsoleConfiguration(const std::wstring& path, ConsoleConfiguration& configuration)
{
    LoadConfigurationImpl(path, pugi::as_utf8(path.c_str()), configuration);
}

namespace
{
	using TransportType = LSY::NetworkGenisysMaster::Config::CONNECTIONTYPE;
	unsigned ArgumentNumber(const std::string& text, unsigned low, unsigned high)
	{
		const bool hex = text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X');
		try
		{
			if (text.empty()) throw std::runtime_error("empty");
			for (size_t i = hex ? 2 : 0; i < text.size(); ++i)
				if (!((text[i] >= '0' && text[i] <= '9') || (hex && ((text[i] >= 'a' && text[i] <= 'f') || (text[i] >= 'A' && text[i] <= 'F')))))
					throw std::runtime_error("not numeric");
			size_t used = 0; const auto value = std::stoul(text, &used, hex ? 16 : 10);
			if (used != text.size() || value < low || value > high) throw std::runtime_error("range");
			return static_cast<unsigned>(value);
		}
		catch (const std::exception&) { throw std::runtime_error("Invalid numeric argument: " + text + " (expected " + std::to_string(low) + ".." + std::to_string(high) + ")"); }
	}
	bool ArgumentBoolean(const std::string& text)
	{
		if (text == "true") return true;
		if (text == "false") return false;
		throw std::runtime_error("Expected true or false: " + text);
	}
	void ValidatePortName(const std::string& text)
	{
		std::string name = text;
		for (auto& ch : name) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
		if (name.compare(0, 4, "\\\\.\\") == 0) name.erase(0, 4);
		bool valid = name.size() > 3 && name.size() <= 12 && name.compare(0, 3, "COM") == 0, nonzero = false;
		for (size_t i = 3; i < name.size(); ++i) { valid = valid && name[i] >= '0' && name[i] <= '9'; nonzero = nonzero || name[i] != '0'; }
		if (!valid || !nonzero) throw std::runtime_error("Invalid serial port: expected COM1, COM10, etc.");
	}
	int ArgumentCount(const std::string& option)
	{
		if (option == "--plain" || option == "--manual" || option == "--recall-all") return 0;
		if (option == "--slave") return 4;
		if (option == "--serial-slave") return 2;
		const std::set<std::string> scalar{
			"--config", "--transport", "--com-port", "--baud", "--parity", "--data-bits", "--stop-bits", "--write-timeout-ms", "--dtr", "--rts",
			"--local-ip", "--local-port", "--wait-ms", "--poll-ms", "--timeout-ms", "--retries", "--recovery-ms", "--run-ms"};
		if (scalar.count(option)) return 1;
		throw std::runtime_error("Unknown argument: " + option);
	}
}
void ValidateConsoleConfiguration(const ConsoleConfiguration& configuration)
{
	const auto& config = configuration.network;
	if (config.connection_type_ != TransportType::UDP && config.connection_type_ != TransportType::SERIAL) Fail("Unknown transport");
	ValidatePortName(config.com_port_);
	if (!config.baud_rate_ || config.data_bits_ != 8 || (config.stop_bits_ != 1 && config.stop_bits_ != 2) ||
		(config.parity_ != 'N' && config.parity_ != 'E' && config.parity_ != 'O') || !config.write_timeout_ms_ || config.write_timeout_ms_ > 600000)
		Fail("Invalid serial settings: baud > 0, 8 data bits, N/E/O parity, 1/2 stop bits, write timeout 1..600000 ms");
	if (!config.poll_interval_ms_ || config.poll_interval_ms_ > 600000 || !config.response_timeout_ms_ || config.response_timeout_ms_ > 600000 ||
		!config.recovery_interval_ms_ || config.recovery_interval_ms_ > 600000 || config.retry_count_ > 100) Fail("Invalid polling settings");
	auto address = [](const std::string& text) { in_addr value{}; return InetPtonA(AF_INET, text.c_str(), &value) == 1; };
	if (config.connection_type_ == TransportType::UDP && !address(config.local_ip_)) Fail("Invalid local IPv4 address");
	if (configuration.slaves.empty()) Fail("Configure at least one slave");
	std::set<uint8_t> ids;
	for (const auto& slave : configuration.slaves)
	{
		if (!slave.id || !ids.insert(slave.id).second) Fail("Invalid or duplicate slave ID");
		if (!slave.frame_bytes || slave.frame_bytes > 224) Fail("Frame bytes must be 1..224");
		if (config.connection_type_ == TransportType::UDP && !slave.has_endpoint)
			Fail("UDP slaves require an IP/port; use --slave id IPv4 port frame-bytes when switching to UDP");
		if (slave.has_endpoint && (!address(slave.endpoint.ip_address_) || !slave.endpoint.port_)) Fail("Invalid slave IPv4 address or UDP port");
	}
}
ConsoleArguments ParseConsoleArguments(const std::vector<std::string>& arguments)
{
	ConsoleArguments result;
	if (std::find(arguments.begin(), arguments.end(), "--help") != arguments.end()) { result.help = true; return result; }
	for (size_t i = 0; i < arguments.size(); ++i)
	{
		const auto& option = arguments[i]; const int count = ArgumentCount(option);
		if (arguments.size() - i - 1 < static_cast<size_t>(count)) Fail("Missing arguments for " + option);
		if (option == "--config")
		{
			if (!result.config_path.empty()) Fail("Use --config only once");
			result.config_path = arguments[i + 1]; if (result.config_path.empty()) Fail("Empty --config path");
		}
		i += count;
	}
	if (!result.config_path.empty()) LoadConsoleConfiguration(result.config_path, result.settings);
	auto& settings = result.settings; auto& config = settings.network;
	bool cliSlaves = false;
	for (size_t i = 0; i < arguments.size(); ++i)
	{
		const auto& option = arguments[i];
		if (option == "--plain") { settings.plain = true; continue; }
		if (option == "--manual") { config.automatic_polling_ = false; continue; }
		if (option == "--recall-all") { settings.recall_all = true; continue; }
		if (option == "--slave" || option == "--serial-slave")
		{
			if (!cliSlaves) { settings.slaves.clear(); cliSlaves = true; }
			ConsoleSlaveConfiguration slave; slave.id = static_cast<uint8_t>(ArgumentNumber(arguments[++i], 1, 255));
			slave.has_endpoint = option == "--slave";
			if (slave.has_endpoint) { slave.endpoint.ip_address_ = arguments[++i]; slave.endpoint.port_ = static_cast<uint16_t>(ArgumentNumber(arguments[++i], 1, 65535)); }
			slave.frame_bytes = ArgumentNumber(arguments[++i], 1, 224); settings.slaves.push_back(slave); continue;
		}
		const auto& value = arguments[++i];
		if (option == "--config") continue;
		if (option == "--transport")
		{
			if (value == "serial") config.connection_type_ = TransportType::SERIAL;
			else if (value == "udp") config.connection_type_ = TransportType::UDP;
			else Fail("Transport must be udp or serial");
		}
		else if (option == "--com-port") config.com_port_ = value;
		else if (option == "--baud") config.baud_rate_ = ArgumentNumber(value, 1, (std::numeric_limits<unsigned>::max)());
		else if (option == "--data-bits") config.data_bits_ = static_cast<uint8_t>(ArgumentNumber(value, 8, 8));
		else if (option == "--stop-bits") config.stop_bits_ = static_cast<uint8_t>(ArgumentNumber(value, 1, 2));
		else if (option == "--parity")
		{
			if (value.size() != 1) Fail("Parity must be N, E or O");
			config.parity_ = static_cast<char>(std::toupper(static_cast<unsigned char>(value[0])));
		}
		else if (option == "--write-timeout-ms") config.write_timeout_ms_ = ArgumentNumber(value, 1, 600000);
		else if (option == "--dtr") config.dtr_enable_ = ArgumentBoolean(value);
		else if (option == "--rts") config.rts_enable_ = ArgumentBoolean(value);
		else if (option == "--local-ip") config.local_ip_ = value;
		else if (option == "--local-port") config.receive_port_ = static_cast<uint16_t>(ArgumentNumber(value, 0, 65535));
		else if (option == "--wait-ms") settings.wait_ms = ArgumentNumber(value, 1, 600000);
		else if (option == "--poll-ms") config.poll_interval_ms_ = ArgumentNumber(value, 1, 600000);
		else if (option == "--timeout-ms") config.response_timeout_ms_ = ArgumentNumber(value, 1, 600000);
		else if (option == "--retries") config.retry_count_ = ArgumentNumber(value, 0, 100);
		else if (option == "--recovery-ms") config.recovery_interval_ms_ = ArgumentNumber(value, 1, 600000);
		else if (option == "--run-ms") settings.run_ms = ArgumentNumber(value, 0, 600000);
	}
	if (settings.slaves.empty()) settings.slaves.push_back(ConsoleSlaveConfiguration{});
	if (settings.recall_all && settings.run_ms) Fail("Use either --recall-all or --run-ms");
	if (settings.recall_all) config.automatic_polling_ = false;
	ValidateConsoleConfiguration(settings); return result;
}
std::string ConsoleTransportDescription(const LSY::NetworkGenisysMaster::Config& config, uint16_t local_port)
{
	if (config.connection_type_ == TransportType::UDP) return "UDP " + config.local_ip_ + ":" + std::to_string(local_port);
	return "SERIAL " + config.com_port_ + " | " + std::to_string(config.baud_rate_) + " " + std::to_string(config.data_bits_) + config.parity_ +
		std::to_string(config.stop_bits_) + " | DTR " + (config.dtr_enable_ ? "on" : "off") + " RTS " + (config.rts_enable_ ? "on" : "off");
}
std::string ConsoleSlaveDestination(const LSY::NetworkGenisysMaster::Config& config, const LSY::NetworkGenisysMaster::Endpoint& endpoint)
{
	return config.connection_type_ == TransportType::SERIAL ? config.com_port_ : endpoint.ip_address_ + ":" + std::to_string(endpoint.port_);
}
