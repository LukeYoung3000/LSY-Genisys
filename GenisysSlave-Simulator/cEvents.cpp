
// STL
#include <algorithm>
#include <chrono>
#include <sstream> 

// wxWidgets
#include <wx/msgdlg.h>
#include <wx/app.h>
#include <wx/filename.h>
#include <wx/stdpaths.h>
#include <wx/textdlg.h>

// GenisysSlave-Lib
#include "GenisysSlave-Lib-Version.h"
#include "NetworkGenisysSlave.h"

// GenisysSlave-Simulator
#include "cApp.h"
#include "cEvents.h"


// Namespaces
using namespace std::chrono_literals;
using namespace LSY;

cEvents::cEvents() : cFrame(nullptr), refresh_timer_(this)
{
	// UI States - Genisys Server Setup
	udp_rx_port_ = -1;
	udp_tx_port_ = -1;
	serial_com_port_ = "";
	serial_baud_rate_ = 9600;
	serial_data_bits_ = 8;
	serial_stop_bits_ = 1;
	serial_parity_ = "None";

	// Genisys Slave
	protocol_ = nullptr;
	network_ = nullptr;


	// Setup Logging
	logger = new wxLogWindow(this, "Log", false, false);
	protocol_logger = new wxLogWindow(this, "Protocol Log", false, false);
	protocol_logger->DetachOldLog();
	for (wxWindowList::compatibility_iterator node = protocol_logger->GetFrame()->GetChildren().GetFirst(); node; node = node->GetNext())
	{
		wxTextCtrl* log_text = wxDynamicCast(node->GetData(), wxTextCtrl);
		if (log_text)
			log_text->SetFont(wxFont(wxFontInfo(log_text->GetFont().GetPointSize()).Family(wxFONTFAMILY_TELETYPE)));
	}
	wxLog::SetActiveTarget(logger);
	wxLogNull no_log;
	wxString protocol_log_path = wxFileName(wxStandardPaths::Get().GetExecutablePath()).GetPathWithSep() + "Protocol Log.txt";
	if (!protocol_log_file_.Open(protocol_log_path, "ab"))
		wxMessageBox("Unable to open the protocol log file:\n" + protocol_log_path, "Protocol Log", wxOK | wxICON_ERROR, this);
	
	// Add Logging Functions To "GenisysSlave-Lib"
	Logging::AddLogDebugFunc(LogDebug);
	Logging::AddLogInfoFunc(LogInfo);
	Logging::AddLogWarningFunc(LogWarning);
	Logging::AddLogErrorFunc(LogError);
	Logging::AddLogProtoFunc([this](const std::string& log_msg) { return LogProto(log_msg); });

	m_slaveList->Append("Slave 1");
	m_slaveList->SetSelection(0);
	m_slaveList->Bind(wxEVT_LISTBOX, [this](wxCommandEvent&) { SelectSlave(); });
	m_addSlave->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { AddSlave(); });
	m_removeSlave->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { RemoveSlave(); });
	m_saveSlave->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { SaveSelectedSlave(); });
	Bind(wxEVT_TIMER, [this](wxTimerEvent&) { RefreshSlaveView(); }, refresh_timer_.GetId());
	SelectSlave();
	UpdateSlaveControls();

	// Connect Idle Event
	//this->Bind(wxEVT_IDLE, &cEvents::IdleEvent, this);
}

cEvents::~cEvents()
{
	ShutdownGenisysNetwork();
	Logging::AddLogProtoFunc([](const std::string&) { return false; });
	delete protocol_logger;
}

void cEvents::TopLevelWindowCloseEvent(wxCloseEvent& event)
{
	// Shut Down Network Coms
	ShutdownGenisysNetwork();

	// Turn Logging Off
	event.SetLoggingOff(true);

	event.Skip();
}

void cEvents::ProtocolSelectionEvent(wxCommandEvent& event)
{
	int selection = m_radioBox1->GetSelection();

	ShowTransportSettings(selection);

	if (selection == 2)
	{
		// Populate COM port dropdown with available ports
		this->PopulateComPorts();
	}

	event.Skip();
}

bool cEvents::GetModeByteReturnValue(uint8_t& value)
{
	unsigned long parsed_value = 0;
	if (!m_textModeByteReturn->GetValue().ToULong(&parsed_value, 16) || parsed_value > 0xFF)
	{
		wxMessageBox("Enter a hexadecimal mode byte from 00 to FF.", "Invalid Mode Byte", wxOK | wxICON_ERROR, this);
		return false;
	}

	value = static_cast<uint8_t>(parsed_value);
	m_textModeByteReturn->ChangeValue(wxString::Format("0x%02X", static_cast<unsigned int>(value)));
	return true;
}

void cEvents::SelectSlave()
{
	const auto& slave = slaves_[m_slaveList->GetSelection()];
	m_textSlaveID->ChangeValue(wxString::Format("%d", slave.id));
	m_textFrameSize->ChangeValue(wxString::Format("%d", slave.frameSize));
	m_textModeByteReturn->ChangeValue(wxString::Format("0x%02X", static_cast<unsigned int>(slave.responseMode)));
	m_alwaysRecallResponse->SetValue(slave.alwaysRecallResponse);
	m_includeRecallMode->SetValue(slave.includeRecallMode);
	RefreshSlaveView();
}

bool cEvents::SaveSelectedSlave()
{
	const int selection = m_slaveList->GetSelection();
	int id = 0;
	int frameSize = 0;
	if (!m_textSlaveID->GetValue().ToInt(&id) || id < 1 || id > 255 ||
		!m_textFrameSize->GetValue().ToInt(&frameSize) || frameSize < 1 || frameSize > 125)
	{
		wxMessageBox("Enter a slave ID from 1 to 255 and a frame size from 1 to 125 bytes.",
			"Invalid Slave Settings", wxOK | wxICON_ERROR, this);
		return false;
	}
	for (int i = 0; i < static_cast<int>(slaves_.size()); ++i)
	{
		if (i != selection && slaves_[i].id == id)
		{
			wxMessageBox("Each slave must have a unique ID.", "Duplicate Slave ID", wxOK | wxICON_ERROR, this);
			return false;
		}
	}
	uint8_t mode = 0;
	if (!GetModeByteReturnValue(mode))
		return false;

	slaves_[selection] = {id, frameSize, mode, m_alwaysRecallResponse->GetValue(), m_includeRecallMode->GetValue()};
	RefreshSlaveView();
	return true;
}

void cEvents::AddSlave()
{
	for (int id = 1; id <= 255; ++id)
	{
		if (std::any_of(slaves_.begin(), slaves_.end(), [id](const SlaveConfig& slave) { return slave.id == id; }))
			continue;

		slaves_.push_back({id, 125});
		m_slaveList->SetSelection(m_slaveList->Append(wxString::Format("Slave %d", id)));
		SelectSlave();
		UpdateSlaveControls();
		return;
	}
}

void cEvents::RemoveSlave()
{
	const int selection = m_slaveList->GetSelection();
	slaves_.erase(slaves_.begin() + selection);
	m_slaveList->Delete(selection);
	m_slaveList->SetSelection(wxMin(selection, static_cast<int>(slaves_.size()) - 1));
	SelectSlave();
	UpdateSlaveControls();
}

void cEvents::UpdateSlaveControls()
{
	const bool running = network_thread_.joinable();
	m_toggleStartStop->SetValue(running);
	m_toggleStartStop->SetLabel(running ? "Stop" : "Start");
	m_addSlave->Enable(!running && slaves_.size() < 255);
	m_removeSlave->Enable(!running && slaves_.size() > 1);
	m_saveSlave->Enable(!running);
	m_textSlaveID->Enable(!running);
	m_textFrameSize->Enable(!running);
	m_alwaysRecallResponse->Enable(!running);
	m_includeRecallMode->Enable(!running);
	m_radioBox1->Enable(!running);
	m_textPollTimeout->Enable(!running);
	m_textUDPrx->Enable(!running);
	m_textUDPtx->Enable(!running);
	for (auto* field : {m_comboComPort, m_comboBaudRate, m_comboDataBits, m_comboStopBits, m_comboParity})
		field->Enable(!running);
	m_grid3->Enable(running);
	m_grid31->Enable(running);
}

void cEvents::ApplyModeByteEvent(wxCommandEvent& event)
{
	uint8_t value = 0;
	if (!GetModeByteReturnValue(value))
		return;

	auto& slave = slaves_[m_slaveList->GetSelection()];
	slave.responseMode = value;
	if (network_thread_.joinable())
	{
		std::lock_guard<std::mutex> lock(slave_state_mutex_);
		edits_.push_back({slave.id, -1, -1, value});
	}
}

void cEvents::StartStopServerEvent(wxCommandEvent& event)
{
	if (m_toggleStartStop->GetValue())
	{
		if (SaveSelectedSlave() && StartupGenisysNetwork())
			refresh_timer_.Start(50);
	}
	else
	{
		ShutdownGenisysNetwork();
	}

	UpdateSlaveControls();
	RefreshSlaveView();
	event.Skip();
}

void cEvents::GridClickEvent(wxGridEvent& event)
{
	if (network_thread_.joinable() && event.GetRow() >= 0 && event.GetCol() > 0)
	{
		const auto& slave = slaves_[m_slaveList->GetSelection()];
		// Capture the ID on click so a queued edit follows the original slave.
		std::lock_guard<std::mutex> lock(slave_state_mutex_);
		edits_.push_back({slave.id, event.GetRow(), 8 - event.GetCol(), -1});
	}
	event.Skip();
}

void cEvents::GridDoubleClickEvent(wxGridEvent& event)
{
	if (!network_thread_.joinable() || event.GetRow() < 0 || event.GetCol() != 0)
	{
		event.Skip();
		return;
	}

	wxTextEntryDialog dialog(this, "Enter a byte in hexadecimal (00-FF)", "Edit indication byte",
		m_grid3->GetCellValue(event.GetRow(), 0));
	if (dialog.ShowModal() == wxID_OK)
	{
		unsigned long value = 0;
		if (dialog.GetValue().ToULong(&value, 16) && value <= 0xFF)
		{
			const auto& slave = slaves_[m_slaveList->GetSelection()];
			std::lock_guard<std::mutex> lock(slave_state_mutex_);
			edits_.push_back({slave.id, event.GetRow(), -1, -1, static_cast<int>(value)});
		}
		else wxMessageBox("Enter a hexadecimal byte from 00 to FF.", "Invalid byte", wxOK | wxICON_WARNING, this);
	}
	event.Skip();
}

void cEvents::MenuHelpAboutOnMenuSelection(wxCommandEvent& event)
{

	std::string msg = "";
	msg += "About - Genisys UDP Slave Application\n";
	msg += "\n";
	msg += "Author: Luke Young\n";
	msg += "GitHub: LukeYoung3000\n";
	msg += ("Version - Lib: " + std::string(genisys_slave_lib_version) + "\n");
	msg += ("Version - App: " + std::string(version) + "\n");

	wxMessageBox(wxString(msg), "About", wxOK | wxICON_INFORMATION);

	event.Skip();
}


void cEvents::MenuHelpLogOnMenuSelection(wxCommandEvent& event)
{
	// Show the log window
	logger->Show();
	event.Skip();
}

void cEvents::MenuHelpProtocolLogOnMenuSelection(wxCommandEvent& event)
{
	protocol_logger->Show();
	event.Skip();
}

void cEvents::IdleEvent(wxIdleEvent& event)
{
	//wxMessageBox("Yo", "IdleEvent Without Skip", wxOK | wxICON_INFORMATION);
	//m_toggleStartStop->SetValue(!m_toggleStartStop->GetValue());
	//static int i = 0;
	//i++;
	//wxLogMessage("Idle Event %d", i);

	event.Skip();
}




bool cEvents::StartupGenisysNetwork()
{

	wxLogMessage("cEvents::StartupGenisysNetwork: Starting");

	// Check Server Thread Is NOT Running
	if (network_thread_.joinable())
	{
		// Error: Thread Is Already Running
		wxLogMessage("cEvents::StartupGenisysNetwork: Thread 1 Error");
		ShutdownGenisysNetwork();
		return false;
	}



	// Radio button "1" is "TCP"
	// Note: We could use "m_radioBox1->GetString()" to confirm the choice strings. 
	int protocol_selection = m_radioBox1->GetSelection();
	bool is_tcp = (protocol_selection == 1);
	bool is_serial = (protocol_selection == 2);


	// Get Parameters From GUI
	bool int_check = true;

	if (is_serial)
	{
		// ToDo: Add serial port checking/validation here.

		// Serial protocol - read from combo boxes
		int com_port_idx = m_comboComPort->GetSelection();
		if (com_port_idx == wxNOT_FOUND)
		{
			wxLogMessage("cEvents::StartupGenisysNetwork: No COM port selected");
			ShutdownGenisysNetwork();
			return false;
		}

		serial_com_port_ = std::string(m_comboComPort->GetString(com_port_idx).mb_str());
		serial_baud_rate_ = std::stoi(std::string(m_comboBaudRate->GetString(m_comboBaudRate->GetSelection()).mb_str()));
		serial_data_bits_ = std::stoi(std::string(m_comboDataBits->GetString(m_comboDataBits->GetSelection()).mb_str()));
		wxString stop_bits_str = m_comboStopBits->GetString(m_comboStopBits->GetSelection());
		serial_stop_bits_ = (stop_bits_str == wxT("1.5")) ? 1 : std::stoi(std::string(stop_bits_str.mb_str()));
		wxString parity_str = m_comboParity->GetString(m_comboParity->GetSelection());
		serial_parity_ = parity_str.Contains(wxT("E")) ? "E" : (parity_str.Contains(wxT("O")) ? "O" : "N");
	}
	else // TCP UDP Parameters
	{
		int_check = int_check && m_textUDPrx->GetValue().ToInt(&udp_rx_port_);

		if (!is_tcp)
			int_check = int_check && m_textUDPtx->GetValue().ToInt(&udp_tx_port_);				// Get Transmit Port (UDP Only)
	}


	if (int_check == false)
	{
		// Error: Invalid Data Input
		wxLogMessage("cEvents::StartupGenisysNetwork: Invalid Data Input");
		ShutdownGenisysNetwork();
		return false;
	}

	// This protocol setting applies to every transport.
	const wxString poll_timeout_text = m_textPollTimeout->GetValue().Strip(wxString::both);
	unsigned long poll_timeout_ms = 0;
	if (!poll_timeout_text.IsNumber() || !poll_timeout_text.ToULong(&poll_timeout_ms) || poll_timeout_ms > UINT32_MAX)
	{
		wxLogMessage("Poll timeout must be a whole number from 0 to 4294967295 milliseconds.");
		ShutdownGenisysNetwork();
		return false;
	}

	protocol_ = std::make_shared<ProtocolGenisysSlave>();
	protocol_->SetPollTimeout(static_cast<uint32_t>(poll_timeout_ms));
	poll_tracking_enabled_ = poll_timeout_ms != 0;
	protocol_->AddEventCallback([this](uint8_t id, std::string, ProtocolGenisysSlave::CALLBACKEVENT event)
		{
			master_connections_[id] = event == ProtocolGenisysSlave::CALLBACKEVENT::MASTER_CONNECTED;
		});

	for (const auto& slave : slaves_)
	{
		const auto name = "Slave " + std::to_string(slave.id);
		auto indications = std::make_shared<DataFrame>(name + " Indications", slave.frameSize);
		auto controls = std::make_shared<DataFrame>(name + " Controls", slave.frameSize);
		if (!protocol_->AddDataFrame(static_cast<uint8_t>(slave.id), indications, controls))
		{
			wxLogMessage("Unable to register slave %d.", slave.id);
			ShutdownGenisysNetwork();
			return false;
		}
		protocol_->SetModeByteReturnValue(static_cast<uint8_t>(slave.id), slave.responseMode);
		protocol_->SetAlwaysRecallResponse(static_cast<uint8_t>(slave.id), slave.alwaysRecallResponse);
		protocol_->SetRecallModeByteEnabled(static_cast<uint8_t>(slave.id), slave.includeRecallMode);
	}

	// Network/Serial Configuration
	NetworkGenisysSlave::Config network_config_;

	if (is_serial)
	{
		// Setup serial connection configuration
		network_config_.connection_type_ = NetworkGenisysSlave::Config::CONNECTIONTYPE::SERIAL;
		network_config_.com_port_ = serial_com_port_;
		network_config_.baud_rate_ = serial_baud_rate_;
		network_config_.data_bits_ = serial_data_bits_;
		network_config_.stop_bits_ = serial_stop_bits_;
		network_config_.parity_ = serial_parity_[0];  // Convert string to char
		network_config_.read_timeout_ms_ = 100;
		network_config_.write_timeout_ms_ = 100;
	}
	else
	{
		// Setup UDP/TCP connection configuration
		network_config_.server_port_ = udp_rx_port_;
		network_config_.destination_port_ = udp_tx_port_;
		if (is_tcp)
			network_config_.connection_type_ = NetworkGenisysSlave::Config::CONNECTIONTYPE::TCP;
		else
			network_config_.connection_type_ = NetworkGenisysSlave::Config::CONNECTIONTYPE::UDP;
	}

	network_ = std::make_shared<NetworkGenisysSlave>(network_config_);
	if (!network_->AddProtocol(protocol_))
	{
		// Error: Protocol Issues
		wxLogMessage("cEvents::StartupGenisysNetwork: Network Setup Error");
		ShutdownGenisysNetwork();
		return false;
	}

	// Start Server
	if (!network_->StartServer())
	{
		wxLogMessage("cEvents::StartupGenisysNetwork: Start Server Error");
		ShutdownGenisysNetwork();
		return false;
	}
	
	
	// Setup Thread For Genisys Loop
	network_thread_ = std::thread(&cEvents::RunningloopGenisysNetwork, this);
	bool thread_ok = network_thread_.joinable();

	if (!thread_ok)
	{
		wxLogMessage("cEvents::StartupGenisysNetwork: Thread 2 Error");
		return false;
	}

	wxLogMessage("cEvents::StartupGenisysNetwork: Start Complete");
	return true;
}


void cEvents::ShutdownGenisysNetwork()
{
	refresh_timer_.Stop();
	if (network_thread_.joinable())
	{
		stop_thread_ = true;
		network_thread_.join();
		stop_thread_ = false;
	}

	if (network_)
		network_->ShutdownServer();
	network_.reset();
	protocol_.reset();
	master_connections_.clear();
	snapshots_.clear();
	edits_.clear();
}

void cEvents::RunningloopGenisysNetwork()
{
	while (!stop_thread_)
	{
		std::vector<SlaveEdit> edits;
		{
			std::lock_guard<std::mutex> lock(slave_state_mutex_);
			edits.swap(edits_);
		}
		for (const auto& edit : edits)
		{
			const auto id = static_cast<uint8_t>(edit.slaveId);
			if (edit.mode >= 0)
			{
				protocol_->SetModeByteReturnValue(id, static_cast<uint8_t>(edit.mode));
				continue;
			}

			ProtocolGenisysSlave::SlaveData data;
			if (edit.value >= 0)
			{
				if (protocol_->GetSlaveData(id, data)) data.indication_data_frame->WriteByte(edit.row, static_cast<uint8_t>(edit.value));
				continue;
			}
			bool value = false;
			if (protocol_->GetSlaveData(id, data) && data.indication_data_frame->ReadBit(edit.row, edit.bit, value))
				data.indication_data_frame->WriteBit(edit.row, edit.bit, !value);
		}

		network_->ServerLoop();

		// Publish copies keyed by slave ID. Only the network thread accesses live frames.
		std::map<int, SlaveSnapshot> snapshots;
		for (const auto& slave : slaves_)
		{
			ProtocolGenisysSlave::SlaveData data;
			if (!protocol_->GetSlaveData(static_cast<uint8_t>(slave.id), data))
				continue;

			auto& snapshot = snapshots[slave.id];
			data.indication_data_frame->ReadBytes(0, slave.frameSize, snapshot.indications);
			data.control_data_frame->ReadBytes(0, slave.frameSize, snapshot.controls);
			snapshot.indicationDirty.resize(slave.frameSize);
			for (int offset = 0; offset < slave.frameSize; ++offset)
				snapshot.indicationDirty[offset] = data.indication_data_frame->IsByteDirty(offset);
			snapshot.master = data.last_master_device_to_poll_name;
			snapshot.responses = data.responce_counter;
			snapshot.connected = master_connections_[slave.id];
			if (data.has_received_mode_byte)
				snapshot.mode = wxString::Format("0x%02X", static_cast<unsigned int>(data.last_mode_byte_recived)).ToStdString();
		}
		{
			std::lock_guard<std::mutex> lock(slave_state_mutex_);
			snapshots_.swap(snapshots);
		}
		std::this_thread::sleep_for(50ms);
	}
}

void cEvents::RefreshSlaveView()
{
	const int selection = m_slaveList->GetSelection();
	const bool running = network_thread_.joinable();
	SlaveSnapshot selected;
	{
		std::lock_guard<std::mutex> lock(slave_state_mutex_);
		for (int i = 0; i < static_cast<int>(slaves_.size()); ++i)
		{
			const int id = slaves_[i].id;
			const auto snapshot = snapshots_.find(id);
			const bool connected = snapshot != snapshots_.end() && snapshot->second.connected;
			wxString label = wxString::Format("Slave %d", id);
			if (running)
				label += !poll_tracking_enabled_ ? "  |  Tracking off" : connected ? "  |  Connected" : "  |  Disconnected";
			if (m_slaveList->GetString(i) != label)
			{
				m_slaveList->SetString(i, label);
				m_slaveList->SetSelection(selection);
			}
			if (i == selection && snapshot != snapshots_.end())
				selected = snapshot->second;
		}
	}

	const int frameSize = slaves_[selection].frameSize;
	auto updateGrid = [frameSize](wxGrid* grid, const std::vector<uint8_t>& bytes,
		const std::vector<bool>& dirty, bool includeHex)
	{
		grid->BeginBatch();
		const int rows = grid->GetNumberRows();
		if (rows > frameSize)
			grid->DeleteRows(frameSize, rows - frameSize);
		if (rows < frameSize)
			grid->AppendRows(frameSize - rows);

		for (int row = 0; row < frameSize; ++row)
		{
			const auto rowLabel = wxString::Format("Byte %d", row);
			if (grid->GetRowLabelValue(row) != rowLabel)
				grid->SetRowLabelValue(row, rowLabel);
			const bool available = row < static_cast<int>(bytes.size());
			if (includeHex)
			{
				const wxString value = available ? wxString::Format("%02X", static_cast<unsigned>(bytes[row])) : "";
				const wxColour colour = available && row < static_cast<int>(dirty.size()) && dirty[row] ?
					wxColour(255, 221, 127) : grid->GetDefaultCellBackgroundColour();
				if (grid->GetCellValue(row, 0) != value) grid->SetCellValue(row, 0, value);
				if (grid->GetCellBackgroundColour(row, 0) != colour) grid->SetCellBackgroundColour(row, 0, colour);
			}
			for (int bitColumn = 0; bitColumn < 8; ++bitColumn)
			{
				const int col = bitColumn + (includeHex ? 1 : 0);
				const bool set = available && (bytes[row] & (1 << (7 - bitColumn)));
				const wxString value = available ? (set ? "1" : "0") : "";
				if (grid->GetCellValue(row, col) != value)
				{
					grid->SetCellValue(row, col, value);
				}
				const wxColour colour = available ? (set ? *wxGREEN : *wxRED) : grid->GetDefaultCellBackgroundColour();
				if (grid->GetCellBackgroundColour(row, col) != colour) grid->SetCellBackgroundColour(row, col, colour);
			}
		}
		grid->EndBatch();
	};
	updateGrid(m_grid3, selected.indications, selected.indicationDirty, true);
	updateGrid(m_grid31, selected.controls, {}, false);
	m_staticTextMasterStatus->SetLabel(running && !poll_tracking_enabled_ ? "Master tracking disabled"
		: selected.connected ? "Master connected" : "Master disconnected");
	m_staticTextLastMasterIp->SetLabel(selected.master.empty() ? wxString("--") : wxString(selected.master));
	m_staticTextLastModeByte->SetLabel(selected.mode.empty() ? wxString("--") : wxString(selected.mode));
	m_staticTextResponceCounter->SetLabel(wxString::Format("%llu", static_cast<unsigned long long>(selected.responses)));
}

bool LogDebug(const std::string& log_msg)
{
	std::stringstream ss;

	auto time_now = std::chrono::system_clock::now();
	ss << std::chrono::system_clock::to_time_t(time_now);
	ss << ",DEBUG," << log_msg << std::endl;

	wxLogMessage(wxString(ss.str()));
	return true;
}

bool LogInfo(const std::string& log_msg)
{
	std::stringstream ss;

	auto time_now = std::chrono::system_clock::now();
	ss << std::chrono::system_clock::to_time_t(time_now);
	ss << ",INFO," << log_msg << std::endl;

	wxLogMessage(wxString(ss.str()));
	return true;
}

bool LogWarning(const std::string& log_msg)
{
	std::stringstream ss;

	auto time_now = std::chrono::system_clock::now();
	ss << std::chrono::system_clock::to_time_t(time_now);
	ss << ",WARNING," << log_msg << std::endl;

	wxLogMessage(wxString(ss.str()));
	return true;
}

bool LogError(const std::string& log_msg)
{
	std::stringstream ss;

	auto time_now = std::chrono::system_clock::now();
	ss << std::chrono::system_clock::to_time_t(time_now);
	ss << ",ERROR," << log_msg << std::endl;

	wxLogMessage(wxString(ss.str()));
	return true;
}

bool cEvents::LogProto(const std::string& log_msg)
{
	bool write_failed = false;
	{
		std::lock_guard<std::mutex> lock(protocol_log_mutex_);
		if (protocol_log_file_.IsOpened() &&
			(protocol_log_file_.Write(log_msg.data(), log_msg.size()) != log_msg.size() ||
				protocol_log_file_.Write("\r\n", 2) != 2 || !protocol_log_file_.Flush()))
		{
			protocol_log_file_.Close();
			write_failed = true;
		}
	}
	if (write_failed)
		wxLogError("Unable to write the protocol log file.");

	CallAfter([this, log_msg]()
		{
			protocol_logger->LogTextAtLevel(wxLOG_Message, wxString(log_msg));
		});
	return true;
}
