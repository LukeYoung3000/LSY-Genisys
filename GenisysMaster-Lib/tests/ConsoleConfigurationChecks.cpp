#include "ConsoleConfiguration.h"
#include <Windows.h>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <functional>

void Check(bool ok, const char* text) { if (!ok) throw std::runtime_error(text); }
struct File
{
	char path[MAX_PATH]{};
	File() { char directory[MAX_PATH]{}; Check(GetTempPathA(MAX_PATH, directory) != 0 && GetTempFileNameA(directory, "gmc", 0, path) != 0, "Create config fixture"); }
	~File() { DeleteFileA(path); }
	void Write(const std::string& xml) { std::ofstream output(path, std::ios::binary); output << xml; Check(bool(output), "Write config fixture"); }
};
int main()
{
	try
	{
		File file;
		const std::string slave = "<Slaves><Slave id='34' ip='127.0.0.1' port='7169' frame-bytes='125'/></Slaves>";
		ConsoleConfiguration settings;
		file.Write("<?xml version='1.0'?><!--comment--><GenisysMaster version='1'>" + slave + "</GenisysMaster>");
		LoadConsoleConfiguration(file.path, settings);
		Check(settings.slaves.size() == 1 && settings.slaves[0].id == 34 && settings.network.poll_interval_ms_ == 500, "Minimal XML uses documented defaults");
		file.Write("<GenisysMaster version='1'><Udp local-ip='127.0.0.1' receive-port='0'/>"
			"<Polling enabled='false' secure='false' interval-ms='25' response-timeout-ms='40' retries='0' recovery-interval-ms='50'/>"
			"<Console plain='true' recall-all='true' wait-ms='1234' run-ms='0'/>"
			"<Slaves><Slave id='034' ip='127.0.0.1' port='7169' frame-bytes='224'/>"
			"<Slave id='0x23' ip='127.0.0.1' port='7169' frame-bytes='32'/></Slaves></GenisysMaster>");
		LoadConsoleConfiguration(file.path, settings);
		Check(settings.slaves.size() == 2 && settings.slaves[0].id == 34 && settings.slaves[1].id == 35 &&
			settings.slaves[0].frame_bytes == 224 && settings.network.receive_port_ == 0 &&
			!settings.network.automatic_polling_ && !settings.network.secure_poll_ && settings.network.retry_count_ == 0 &&
			settings.network.response_timeout_ms_ == 40 && settings.plain && settings.recall_all && settings.wait_ms == 1234,
			"Load all settings, shared endpoints, decimal and hex values");
		const std::vector<std::string> invalid{
			"<GenisysMaster>",
			"<Wrong version='1'/>",
			"<GenisysMaster version='2'>" + slave + "</GenisysMaster>",
			"<GenisysMaster version='1' typo='x'>" + slave + "</GenisysMaster>",
			"<GenisysMaster version='1'><Polling retries='-1'/>" + slave + "</GenisysMaster>",
			"<GenisysMaster version='1'><Polling retries='101'/>" + slave + "</GenisysMaster>",
			"<GenisysMaster version='1'><Polling enabled='yes'/>" + slave + "</GenisysMaster>",
			"<GenisysMaster version='1'><Polling interval-ms='0'/>" + slave + "</GenisysMaster>",
			"<GenisysMaster version='1'><Udp receive-port='65536'/>" + slave + "</GenisysMaster>",
			"<GenisysMaster version='1'><Udp local-ip='not-an-ip'/>" + slave + "</GenisysMaster>",
			"<GenisysMaster version='1'><Udp/><Udp/>" + slave + "</GenisysMaster>",
			"<GenisysMaster version='1'><Slaves/></GenisysMaster>",
			"<GenisysMaster version='1'/>",
			"<GenisysMaster version='1'><Slaves><Slave id='0' ip='127.0.0.1' port='7169' frame-bytes='8'/></Slaves></GenisysMaster>",
			"<GenisysMaster version='1'><Slaves><Slave id='1' ip='127.0.0.1' port='7169' frame-bytes='225'/></Slaves></GenisysMaster>",
			"<GenisysMaster version='1'><Slaves><Slave id='1' ip='127.0.0.1' port='0' frame-bytes='8'/></Slaves></GenisysMaster>",
			"<GenisysMaster version='1'><Slaves><Slave id='1' ip='127.0.0.1' port='7169'/></Slaves></GenisysMaster>",
			"<GenisysMaster version='1'><Slaves><Slave id='1' ip='127.0.0.1' port='7169' frame-bytes='8'/>"
			"<Slave id='1' ip='127.0.0.2' port='7169' frame-bytes='8'/></Slaves></GenisysMaster>",
			"<GenisysMaster version='1'><Slaves><Slvae/></Slaves></GenisysMaster>",
			"<GenisysMaster version='1'>unexpected text" + slave + "</GenisysMaster>",
			"<GenisysMaster version='1'>" + slave + "</GenisysMaster><Extra/>",
			"<!DOCTYPE GenisysMaster><GenisysMaster version='1'>" + slave + "</GenisysMaster>"
		};
		for (const auto& xml : invalid)
		{
			file.Write(xml); bool rejected = false;
			try { LoadConsoleConfiguration(file.path, settings); }
			catch (const std::exception& error) { rejected = std::string(error.what()).find(file.path) != std::string::npos; }
			Check(rejected, "Invalid XML/settings must fail with the file path");
			Check(settings.slaves.size() == 2 && settings.network.response_timeout_ms_ == 40, "Invalid file must not partially apply settings");
		}
		LoadConsoleConfiguration("../GenisysMaster-Console/GenisysMaster.example.xml", settings);
		Check(settings.slaves.size() == 1 && settings.slaves[0].id == 34 && !settings.plain, "Commented example is loadable with the simulator defaults");
		LoadConsoleConfiguration("../GenisysMaster-Console/GenisysMaster.serial.example.xml", settings);
		using Type = LSY::NetworkGenisysMaster::Config::CONNECTIONTYPE;
		Check(settings.network.connection_type_ == Type::SERIAL && settings.network.com_port_ == "COM10" &&
			settings.network.baud_rate_ == 9600 && settings.network.data_bits_ == 8 && settings.network.parity_ == 'N' &&
			settings.network.write_timeout_ms_ == 2000 && !settings.slaves[0].has_endpoint, "Serial example loads ID-only slaves and line settings");
		const auto rejected = [&](const std::function<void()>& action) {
			bool failed = false; try { action(); } catch (const std::exception&) { failed = true; }
			Check(failed, "Invalid serial XML or CLI must be rejected before opening a transport");
		};
		for (const std::string& section : std::vector<std::string>{
			"<Serial port='NUL'/>", "<Serial port='COM0'/>", "<Serial baud-rate='0'/>", "<Serial parity='X'/>",
			"<Serial data-bits='7'/>", "<Serial stop-bits='3'/>", "<Serial write-timeout-ms='0'/>", "<Serial dtr='yes'/>", "<Serial/><Serial/>"})
		{
			file.Write("<GenisysMaster version='1' transport='serial'>" + section + "<Slaves><Slave id='3' frame-bytes='8'/></Slaves></GenisysMaster>");
			rejected([&] { LoadConsoleConfiguration(file.path, settings); });
			Check(settings.network.com_port_ == "COM10" && settings.slaves[0].id == 34, "Failed serial load preserves previous settings");
		}
		file.Write("<GenisysMaster version='1' transport='serial'><Serial port='COM11' baud-rate='19200' parity='e' stop-bits='2' dtr='true' rts='true'/>"
			"<Slaves><Slave id='3' frame-bytes='8'/><Slave id='4' frame-bytes='16'/></Slaves></GenisysMaster>");
		LoadConsoleConfiguration(file.path, settings);
		Check(settings.network.parity_ == 'E' && settings.network.stop_bits_ == 2 && settings.network.dtr_enable_ && settings.network.rts_enable_, "Serial optional line settings");
		auto args = ParseConsoleArguments({"--baud", "38400", "--config", file.path, "--com-port", "COM12", "--serial-slave", "0x22", "32", "--serial-slave", "35", "64", "--plain"});
		Check(args.settings.network.connection_type_ == Type::SERIAL && args.settings.network.baud_rate_ == 38400 && args.settings.network.com_port_ == "COM12" &&
			args.settings.slaves.size() == 2 && args.settings.slaves[0].id == 34 && args.settings.slaves[0].frame_bytes == 32 && args.settings.plain,
			"CLI scalar precedence is independent of config argument order; serial slaves replace XML list");
		args = ParseConsoleArguments({"--transport", "serial", "--com-port", "\\\\.\\COM10", "--baud", "115200", "--parity", "o", "--data-bits", "8", "--stop-bits", "2", "--dtr", "true", "--rts", "false", "--write-timeout-ms", "3000", "--recall-all"});
		Check(args.settings.network.parity_ == 'O' && args.settings.network.stop_bits_ == 2 && args.settings.network.dtr_enable_ && !args.settings.network.rts_enable_ &&
			args.settings.recall_all && !args.settings.network.automatic_polling_ && args.settings.slaves[0].id == 34, "All serial CLI options and default slave");
		Check(ConsoleTransportDescription(args.settings.network, 0).find("115200 8O2") != std::string::npos &&
			ConsoleSlaveDestination(args.settings.network, {}).find("127.0.0.1") == std::string::npos, "Serial banner and slave rows do not show fictitious UDP endpoints");
		args = ParseConsoleArguments({"--config", file.path, "--transport", "udp", "--slave", "9", "127.0.0.1", "7169", "8", "--local-port", "0"});
		Check(args.settings.network.connection_type_ == Type::UDP && args.settings.slaves[0].id == 9 && args.settings.network.receive_port_ == 0, "Explicit UDP override with endpoint replacement");
		Check(ParseConsoleArguments({}).settings.network.connection_type_ == Type::UDP, "CLI defaults stay UDP");
		Check(ParseConsoleArguments({"--config", "missing.xml", "--help"}).help, "Help does not open XML or a serial port");
		for (const auto& bad : std::vector<std::vector<std::string>>{
			{"--transport", "tcp"}, {"--com-port", "NUL"}, {"--baud", "0"}, {"--parity", "even"}, {"--parity", "X"}, {"--data-bits", "7"},
			{"--stop-bits", "3"}, {"--write-timeout-ms", "0"}, {"--dtr", "yes"}, {"--serial-slave", "3", "8"},
			{"--transport", "serial", "--serial-slave", "3"}, {"--config", file.path, "--transport", "udp"},
			{"--config", file.path, "--config", file.path}, {"--recall-all", "--run-ms", "100"}, {"--unknown"},
			{"--slave", "3", "bad-ip", "7169", "8"}, {"--slave", "3", "127.0.0.1", "7169", "8", "--slave", "3", "127.0.0.1", "7169", "8"}})
			rejected([&] { ParseConsoleArguments(bad); });
		std::cout << "PASS: serial XML/CLI, line validation, scalar/list precedence, help and transport-aware display\n";
		std::cout << "PASS: XML defaults, all settings, shared endpoints and commented example\n"
			"PASS: malformed/unknown/duplicate/invalid settings rejected without partial application\n"
			"All ConsoleConfiguration checks passed.\n";
		return 0;
	}
	catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
