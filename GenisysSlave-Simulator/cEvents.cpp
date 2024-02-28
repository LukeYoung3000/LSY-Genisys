
// STL
#include <chrono>

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
	protocol_ = nullptr;
	network_ = nullptr;


	// Setup Logging
	logger = new wxLogWindow(this, "Log", true, false);
	wxLog::SetActiveTarget(logger);
	wxLogNull no_log;
	

	// Connect Idle Event
	//this->Bind(wxEVT_IDLE, &cEvents::IdleEvent, this);
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

			// Setup Grid Size Based On Genisys Slave DB Bits
			m_grid3->DeleteRows(0, m_grid3->GetNumberRows());
			m_grid3->AppendRows(genisys_frame_size_);

			// Setup Grid Values Based On Genisys Slave DB Bits
			// Note: For Now Just Assume 0
			for (int i = 0; i < genisys_frame_size_; i++)
			{
				for (int j = 0; j < 8; j++)
				{
					m_grid3->SetCellValue(wxGridCellCoords(i, j), "0");
					m_grid3->SetCellBackgroundColour(i, j, *wxRED);
				}
			}

		}

		else
		{
			// Disable User Controls On The Grid
			m_grid3->Enable(false);

			// Set Start/Stop Button State
			m_toggleStartStop->SetValue(false);
		}
	}


	else
	{
		wxLogMessage("cEvents::StartStopServerEvent: Stop Button Clicked");

		// Disable User Controls On The Grid
		m_grid3->Enable(false);

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
		bit_offset_ = coord.GetCol();
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

	// Get Parameters From GUI
	bool int_check = true;
	int_check = int_check && m_textUDPrx->GetValue().ToInt(&udp_rx_port_);					// Get UDP Reciving Port
	int_check = int_check && m_textUDPtx->GetValue().ToInt(&udp_tx_port_);					// Get UPD Transmit Port
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
	data_frame_1_ = std::make_shared<DataFrame>("Genisys Slave 1", genisys_frame_size_);

	// Protocol Setup
	protocol_ = std::make_shared<ProtocolGenisysSlave>();
	if (!protocol_->AddDataFrame(genisys_slave_id_, data_frame_1_))
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

	network_->ShutdownServer();

	data_frame_1_.reset();
	protocol_.reset();
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
		wxLogMessage("cEvents::RunningloopGenisysNetwork: Loop %d", loop_cnt);

		
		// Check If Thread Is Requested To Stop From GUI
		if (stop_thread_)
		{
			return;
		}


		// Check If GUI Has Updated A Bit Value In The Table
		if (bit_write_request_)
		{
			if (data_frame_1_->WriteBit(byte_offset_, bit_offset_, bit_value_))
			{

				// Update The wxWidgets GUI On GUI Thread
				int row = (int)byte_offset_;
				int col = (int)bit_offset_;
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

		// Run Server Loop
		// To Do: Make This Nonblocking
		network_->ServerLoop();

		std::this_thread::sleep_for(50ms);
	}

}