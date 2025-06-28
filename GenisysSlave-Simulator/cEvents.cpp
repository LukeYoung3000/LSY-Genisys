
// STL
#include <chrono>
#include <sstream> 

// wxWidgets
#include <wx/msgdlg.h>
#include <wx/app.h>

// GenisysSlave-Lib
#include "GenisysSlave-Lib-Version.h"
#include "NetworkGenisysSlave.h"

// GenisysSlave-Simulator
#include "cApp.h"
#include "cEvents.h"


// Namespaces
using namespace std::chrono_literals;
using namespace LSY;



cEvents::cEvents() : cFrame(nullptr)
{
	// UI States - Genisys Server Setup
	udp_rx_port_ = -1;
	udp_tx_port_ = -1;
	genisys_slave_id_ = -1;
	genisys_frame_size_ = -1;

	// UI States - Genisys Database Updates
	bit_write_request_ = false;
	byte_offset_ = -1;
	bit_offset_ = -1;
	bit_value_ = false;

	// Genisys Slave
	data_frame_1_ = nullptr;
	data_frame_2_ = nullptr;
	protocol_ = nullptr;
	network_ = nullptr;


	// Setup Logging
	logger = new wxLogWindow(this, "Log", false, false);
	wxLog::SetActiveTarget(logger);
	wxLogNull no_log;
	
	// Add Logging Functions To "GenisysSlave-Lib"
	Logging::AddLogDebugFunc(LogDebug);
	Logging::AddLogInfoFunc(LogInfo);
	Logging::AddLogWarningFunc(LogWarning);
	Logging::AddLogErrorFunc(LogError);

	// Connect Idle Event
	//this->Bind(wxEVT_IDLE, &cEvents::IdleEvent, this);
}

void cEvents::TopLevelWindowCloseEvent(wxCloseEvent& event)
{
	// Shut Down Network Coms
	ShutdownGenisysNetwork();

	// Turn Logging Off
	event.SetLoggingOff(true);

	event.Skip();
}

void cEvents::StartStopServerEvent(wxCommandEvent& event)
{

	if (m_toggleStartStop->GetValue() == true) // Turn On Genisys Server
	{
		wxLogMessage("cEvents::StartStopServerEvent: Start Button Clicked");

		if (StartupGenisysNetwork())
		{
			// Enable User Controls On The Grid
			m_grid3->Enable(true);
			m_grid31->Enable(true);

			// Setup Grid Size Based On Genisys Slave DB Bits

			// Indications Grid
			m_grid3->DeleteRows(0, m_grid3->GetNumberRows());
			m_grid3->AppendRows(genisys_frame_size_);

			// Controls Grid
			m_grid31->DeleteRows(0, m_grid31->GetNumberRows());
			m_grid31->AppendRows(genisys_frame_size_);


			// Setup Grid Values Based On Genisys Slave DB Bits
			// Note: For Now Just Assume 0
			for (int i = 0; i < genisys_frame_size_; i++)
			{
				for (int j = 0; j < 8; j++)
				{
					// Indications
					m_grid3->SetCellValue(wxGridCellCoords(i, j), "0");
					m_grid3->SetCellBackgroundColour(i, j, *wxRED);

					// Controls
					m_grid31->SetCellValue(wxGridCellCoords(i, j), "0");
					m_grid31->SetCellBackgroundColour(i, j, *wxRED);
				}
			}

		}

		else
		{
			// Disable User Controls On The Grid
			m_grid3->Enable(false);
			m_grid31->Enable(false);

			// Set Start/Stop Button State
			m_toggleStartStop->SetValue(false);
		}
	}


	else
	{
		wxLogMessage("cEvents::StartStopServerEvent: Stop Button Clicked");

		// Disable User Controls On The Grid
		m_grid3->Enable(false);
		m_grid31->Enable(false);

		ShutdownGenisysNetwork();
	}


	event.Skip();
}

void cEvents::GridClickEvent(wxGridEvent& event)
{
	// Get Coord Clicked
	wxGridCellCoords coord = wxGridCellCoords(event.GetRow(), event.GetCol());

	// Get Value
	wxString value = m_grid3->GetCellValue(coord);


	// Request To Genisys Thread To Change A Bit Value
	// Note: These Variable Are Atomic
	// The Genisys Server Thread Check For Changes To "bit_write_request_"
	if (!bit_write_request_)
	{
		if (value == wxString("1"))
			bit_value_ = false;
		else
			bit_value_ = true;
		byte_offset_ = coord.GetRow();
		bit_offset_ = 7 - coord.GetCol();
		bit_write_request_ = true;
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
	bool is_tcp = false;
	if (m_radioBox1->GetSelection() == 1)
		is_tcp = true;


	// Get Parameters From GUI
	bool int_check = true;
	int_check = int_check && m_textUDPrx->GetValue().ToInt(&udp_rx_port_);					// Get Reciving/Listening Port
	if (!is_tcp)
		int_check = int_check && m_textUDPtx->GetValue().ToInt(&udp_tx_port_);				// Get Transmit Port (UDP Only)
	int_check = int_check && m_textSlaveID->GetValue().ToInt(&genisys_slave_id_);			// Get Genisys Slave ID
	int_check = int_check && m_textFrameSize->GetValue().ToInt(&genisys_frame_size_);		// Get Genisys Frame Size



	if (int_check == false)
	{
		// Error: Invalid Data Input
		wxLogMessage("cEvents::StartupGenisysNetwork: Invalid Data Input");
		ShutdownGenisysNetwork();
		return false;
	}

	// Max Frame Size Is Hardcoded At 125
	if (genisys_frame_size_ > 125)
	{
		genisys_frame_size_ = 125;
		m_textFrameSize->SetValue("125");
	}




	// Setup Genisys UDP Slave

	// Data Frame Setup
	data_frame_1_ = std::make_shared<DataFrame>("Genisys Slave 1 Indications", genisys_frame_size_);
	data_frame_2_ = std::make_shared<DataFrame>("Genisys Slave 1 Controls", genisys_frame_size_);

	// Create Callback For Control Message Events On Data Frame 2
	std::function<void(std::string, LSY::DataFrame::CALLBACKEVENT)> callback_func = \
		[this](std::string table_name, LSY::DataFrame::CALLBACKEVENT callback_event_type)
		{
			// I had already writen a class method for when events were implemented
			// So just call that method.
			this->EventCallbackGenisysNetwork(table_name, callback_event_type);
		};

	data_frame_2_->AddEventCallback(callback_func); // Add callback to the data frame object.





	// Protocol Setup
	protocol_ = std::make_shared<ProtocolGenisysSlave>();
	if (!protocol_->AddDataFrame(genisys_slave_id_, false, data_frame_1_) || \
		!protocol_->AddDataFrame(genisys_slave_id_, true, data_frame_2_) )
	{
		// Error: Data Frame Issue
		wxLogMessage("cEvents::StartupGenisysNetwork: Protocol Setup Error");
		ShutdownGenisysNetwork();
		return false;
	}

	// Network Configuration
	NetworkGenisysSlave::Config network_config_;
	network_config_.server_port_ = udp_rx_port_;
	network_config_.destination_port_ = udp_tx_port_;
	if (is_tcp)
		network_config_.connection_type_ = NetworkGenisysSlave::Config::CONNECTIONTYPE::TCP;
	else
		network_config_.connection_type_ = NetworkGenisysSlave::Config::CONNECTIONTYPE::UDP;

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
	wxLogMessage("cEvents::ShutdownGenisysNetwork: Shuting Down");

	// Check If Thread Is Active
	if (network_thread_.joinable())
	{
		// Shutdown Thread
		stop_thread_ = true;
		network_thread_.join();
		stop_thread_ = false;
	}

	if (network_ != nullptr)
		network_->ShutdownServer();

	if (data_frame_1_ != nullptr)
		data_frame_1_.reset();
	
	if (data_frame_2_ != nullptr)
		data_frame_2_.reset();
	
	if (protocol_ != nullptr)
		protocol_.reset();

	if (network_ != nullptr)
		network_.reset();

	bit_write_request_ = false;
	byte_offset_ = -1;
	bit_offset_ = -1;
	bit_value_ = false;

}


void cEvents::RunningloopGenisysNetwork()
{

	static int loop_cnt = 0;

	while (true)
	{

		loop_cnt++;
		//wxLogMessage("cEvents::RunningloopGenisysNetwork: Loop %d", loop_cnt);

		
		// Check If Thread Is Requested To Stop From GUI
		if (stop_thread_)
		{
			return;
		}


		// Check If GUI Has Updated A Bit Value In The Indication Table
		if (bit_write_request_)
		{
			if (data_frame_1_->WriteBit(byte_offset_, bit_offset_, bit_value_))
			{

				// Update The wxWidgets GUI On GUI Thread
				int row = (int)byte_offset_;
				int col = 7 - (int)bit_offset_;
				bool val = (bool)bit_value_;
				wxGetApp().CallAfter([this, row, col, val]()
					{

						wxGridCellCoords coord = wxGridCellCoords(row, col);
						if (val)
						{
							this->m_grid3->SetCellValue(coord, wxString("1"));
							this->m_grid3->SetCellBackgroundColour(coord.GetRow(), coord.GetCol(), *wxGREEN);
						}
						else
						{
							this->m_grid3->SetCellValue(coord, wxString("0"));
							this->m_grid3->SetCellBackgroundColour(coord.GetRow(), coord.GetCol(), *wxRED);
						}

					});
				
			}
			else
			{
				// Error: Bit Failed To Write
				
			}

			// Reset Atomic Bits
			byte_offset_ = -1;
			bit_offset_ = -1;
			bit_value_ = false;
			bit_write_request_ = false;

		}



		// Update Last Master IP & Responce Counter On GUI
		// Note: I'm Using The Values From The Indication Data Frame.
		// The Control & Indication Will Have The Exact Same Values Anyway.
		std::string last_master_ip = data_frame_1_->GetLastMasterIp();
		std::string responce_cnt = std::to_string(data_frame_1_->GetResponceCounter());
		wxGetApp().CallAfter([this, last_master_ip, responce_cnt]()
			{

				m_staticTextLastMasterIp->SetLabel(last_master_ip);
				m_staticTextResponceCounter->SetLabel(responce_cnt);

			});



		// Run Server Loop
		// To Do: Make This Nonblocking
		network_->ServerLoop();



		std::this_thread::sleep_for(50ms);
	}

}

void cEvents::EventCallbackGenisysNetwork(std::string table_name, LSY::DataFrame::CALLBACKEVENT event_type)
{
	// This function should be mapped as an event call back to the data control frame.
	// So the function should be called whenever the control frame is updated via the network.

	if (event_type != LSY::DataFrame::CALLBACKEVENT::CONTROL_CHANGE)
	{
		// Error - All events other than control changes are not expected
		return;
	}

	if (table_name != data_frame_2_->GetName())
	{
		// Error - Callback is mapped incorrectly
		return;
	}


	// Update The Control Table In The GUI
	// Note: Currently this is not effiencent.
	// Would be better to detect the values that have change and only update the changes. 

	for (int i = 0; i < data_frame_2_->GetNumBytes(); i++)
	{

		int row = i;
		uint8_t byte_val = 0;
		data_frame_2_->ReadByte(i, byte_val);

		wxGetApp().CallAfter([this, row, byte_val]()
			{

				// Loop Through Columns In The Row
				for (int j = 0; j < 8; j++)
				{
					int col = j;
					int bit_offset = 7 - col;

					// Extract the bit value from the byte
					uint8_t mask = 1 << bit_offset;
					uint8_t temp_byte = byte_val & mask;
					temp_byte = temp_byte >> bit_offset;
					bool bit_val = temp_byte;

					// Update The Cell In The GUI
					wxGridCellCoords coord = wxGridCellCoords(row, col);
					if (bit_val)
					{
						this->m_grid31->SetCellValue(coord, wxString("1"));
						this->m_grid31->SetCellBackgroundColour(coord.GetRow(), coord.GetCol(), *wxGREEN);
					}
					else
					{
						this->m_grid31->SetCellValue(coord, wxString("0"));
						this->m_grid31->SetCellBackgroundColour(coord.GetRow(), coord.GetCol(), *wxRED);
					}
				}

			});

	}


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