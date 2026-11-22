#pragma once

// STL
#include <utility>
#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

// wxWidgets
#include <wx/event.h>
#include <wx/ffile.h>
#include <wx/log.h>
#include <wx/timer.h>

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
bool LogProto(const std::string& log_msg);


class cEvents : public cFrame
{
public:

	// Configuration is edited on the GUI thread. Each registered slave gets its own frames.
	struct SlaveConfig
	{
		int id;
		int frameSize;
		uint8_t responseMode = 0x04;
		bool alwaysRecallResponse = false;
		bool includeRecallMode = true;
	};
	struct SlaveSnapshot
	{
		std::vector<uint8_t> indications;
		std::vector<uint8_t> controls;
		std::vector<bool> indicationDirty;
		std::string master;
		std::string mode;
		uint64_t responses = 0;
		bool connected = false;
	};
	struct SlaveEdit
	{
		int slaveId;
		int row;
		int bit;
		int mode;
		int value = -1;
	};
	std::vector<SlaveConfig> slaves_{{1, 125}};
	std::map<int, bool> master_connections_;
	std::map<int, SlaveSnapshot> snapshots_;
	std::vector<SlaveEdit> edits_;
	std::mutex slave_state_mutex_;
	wxTimer refresh_timer_;
	bool poll_tracking_enabled_ = true;

	void SelectSlave();
	void RefreshSlaveView();
	void UpdateSlaveControls();
	bool SaveSelectedSlave();
	void AddSlave();
	void RemoveSlave();

	// Network Configuration
	int udp_rx_port_;
	int udp_tx_port_;

	// Serial Configuration
	std::string serial_com_port_;
	int serial_baud_rate_;
	int serial_data_bits_;
	int serial_stop_bits_;
	std::string serial_parity_;



	// Logging
	wxLogWindow* logger;
	wxLogWindow* protocol_logger;
	wxFFile protocol_log_file_;
	std::mutex protocol_log_mutex_;

	// Genisys Network
	std::atomic<bool> stop_thread_;
	std::thread network_thread_;
	std::shared_ptr<LSY::ProtocolGenisysSlave> protocol_;
	std::shared_ptr <LSY::NetworkGenisysSlave> network_;

public:

	cEvents();
	~cEvents();
	bool LogProto(const std::string& log_msg);

	// UI Events
	void TopLevelWindowCloseEvent(wxCloseEvent& event) override;
	void ProtocolSelectionEvent(wxCommandEvent& event) override;
	void StartStopServerEvent(wxCommandEvent& event) override;
	void ApplyModeByteEvent(wxCommandEvent& event) override;
	void GridClickEvent(wxGridEvent& event) override;
	void GridDoubleClickEvent(wxGridEvent& event) override;
	void MenuHelpAboutOnMenuSelection(wxCommandEvent& event) override;
	void MenuHelpLogOnMenuSelection(wxCommandEvent& event) override;
	void MenuHelpProtocolLogOnMenuSelection(wxCommandEvent& event) override;
	void IdleEvent(wxIdleEvent& event);

	// Network
	bool StartupGenisysNetwork();
	bool GetModeByteReturnValue(uint8_t& value);
	void ShutdownGenisysNetwork();
	void RunningloopGenisysNetwork();

};
