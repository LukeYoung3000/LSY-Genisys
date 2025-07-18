#pragma once

// STL
#include <utility>
#include <atomic>
#include <thread>

// wxWidgets
#include <wx/event.h>
#include <wx/log.h>

// Genisys Lib
#include "NetworkGenisysSlave.h"

// Genisys Slave Simulator
#include "Version.h"
#include "cFrame.h"


// Logging Functions
bool LogDebug(const std::string& log_msg);
bool LogInfo(const std::string& log_msg);
bool LogWarning(const std::string& log_msg);
bool LogError(const std::string& log_msg);


class cEvents : public cFrame
{
public:

	// UI States
	int udp_rx_port_;
	int udp_tx_port_;
	int genisys_slave_id_;
	int genisys_frame_size_;

	std::atomic<bool> bit_write_request_;
	std::atomic<int> byte_offset_;
	std::atomic<int> bit_offset_;
	std::atomic<bool> bit_value_;

	// Logging
	wxLogWindow* logger;

	// Genisys Network
	std::atomic<bool> stop_thread_;
	std::thread network_thread_;
	std::shared_ptr<LSY::DataFrame> data_frame_1_;
	std::shared_ptr<LSY::DataFrame> data_frame_2_;
	std::shared_ptr<LSY::ProtocolGenisysSlave> protocol_;
	std::shared_ptr <LSY::NetworkGenisysSlave> network_;

public:

	cEvents();

	// UI Events
	void TopLevelWindowCloseEvent(wxCloseEvent& event) override;
	void StartStopServerEvent(wxCommandEvent& event) override;
	void GridClickEvent(wxGridEvent& event) override;
	void MenuHelpAboutOnMenuSelection(wxCommandEvent& event) override;
	void MenuHelpLogOnMenuSelection(wxCommandEvent& event) override;
	void IdleEvent(wxIdleEvent& event);

	// Network
	bool StartupGenisysNetwork();
	void ShutdownGenisysNetwork();
	void RunningloopGenisysNetwork();
	void EventCallbackGenisysNetwork(std::string table_name, LSY::DataFrame::CALLBACKEVENT event_type);

};