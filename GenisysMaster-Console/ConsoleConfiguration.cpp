#include "ConsoleConfiguration.h"
#include "third_party/pugixml/pugixml.hpp"
#include <Ws2tcpip.h>
#include <set>
#include <stdexcept>

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

void LoadConsoleConfiguration(const std::string& path, ConsoleConfiguration& configuration)
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
		Shape(root, {"version"}, {"Udp", "Polling", "Console", "Slaves"});
		if (Required(root, "version") != "1") Fail("Unsupported configuration version (expected 1)");
		ConsoleConfiguration loaded;
		if (auto udp = Section(root, "Udp"))
		{
			Shape(udp, {"local-ip", "receive-port"}, {});
			loaded.network.local_ip_ = Address(udp, "local-ip", loaded.network.local_ip_);
			loaded.network.receive_port_ = static_cast<uint16_t>(Number(udp, "receive-port", 0, 65535, loaded.network.receive_port_));
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
			slave.endpoint.ip_address_ = Address(node, "ip", "", true);
			slave.endpoint.port_ = static_cast<uint16_t>(Number(node, "port", 1, 65535, 0, true));
			slave.frame_bytes = Number(node, "frame-bytes", 1, 224, 0, true);
			if (!ids.insert(slave.id).second) Fail("Duplicate slave ID " + std::to_string(slave.id));
			loaded.slaves.push_back(slave);
		}
		if (loaded.slaves.empty()) Fail("<Slaves> must contain at least one <Slave>");
		configuration = loaded;
	}
	catch (const std::exception& error) { throw std::runtime_error("Config '" + path + "': " + error.what()); }
}
