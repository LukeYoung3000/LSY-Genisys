#include "ConsoleConfiguration.h"
#include <Windows.h>
#include <fstream>
#include <iostream>
#include <stdexcept>

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
		std::cout << "PASS: XML defaults, all settings, shared endpoints and commented example\n"
			"PASS: malformed/unknown/duplicate/invalid settings rejected without partial application\n"
			"All ConsoleConfiguration checks passed.\n";
		return 0;
	}
	catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
