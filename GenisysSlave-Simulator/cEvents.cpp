#include <wx/msgdlg.h>

#include "cEvents.h"

#include "NetworkGenisysSlave.h"
using namespace LSY;


cEvents::cEvents() : cFrame(nullptr)
{
	// UI States
	start_stop_server_request_ = m_toggleStartStop->GetValue();
	udp_rx_port_ = -1;
	udp_tx_port_ = -1;
	genisys_slave_id_ = -1;
	genisys_frame_size_ = -1;

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
	this->Bind(wxEVT_IDLE, &cEvents::IdleEvent, this);
}


void cEvents::StartStopServerEvent(wxCommandEvent& event)
{

	if (m_toggleStartStop->GetValue() == true) // Turn On Genisys Server
	{
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
	if (value == wxString("1"))
		bit_value_ = false;
	else
		bit_value_ = true;
	byte_offset_ = coord.GetRow();
	bit_offset_ = coord.GetCol();
	bit_write_request_ = true;




	// Old Code
	if (value == wxString("1"))
	{
		//m_grid3->SetCellValue(coord, wxString("0"));
		//m_grid3->SetCellBackgroundColour(coord.GetRow(), coord.GetCol(), *wxRED);
	}
	else
	{
		//m_grid3->SetCellValue(coord, wxString("1"));
		//m_grid3->SetCellBackgroundColour(coord.GetRow(), coord.GetCol(), *wxGREEN);
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
	msg += ("Version: " + std::string(version) + "\n");

	wxMessageBox(wxString(msg), "About", wxOK | wxICON_INFORMATION);

	logger->Show();

	event.Skip();
}


void cEvents::IdleEvent(wxIdleEvent& event)
{
	//wxMessageBox("Yo", "IdleEvent Without Skip", wxOK | wxICON_INFORMATION);
	//m_toggleStartStop->SetValue(!m_toggleStartStop->GetValue());
	static int i = 0;
	i++;
	wxLogMessage("Idle Event %d", i);

	event.Skip();
}




bool cEvents::StartupGenisysNetwork()
{

	// Check Server Thread Is NOT Running
	if (network_thread_.joinable())
	{
		// Error: Thread Is Already Running
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
		ShutdownGenisysNetwork();
		return false;
	}

	// Start Server
	if (!network_->StartServer())
	{
		ShutdownGenisysNetwork();
		return false;
	}
	
	
	// Setup Thread For Main Loop





}


void cEvents::ShutdownGenisysNetwork()
{

	// Check If Thread Is Active
	if (network_thread_.joinable())
	{
		// Shutdown Thread
		stop_thread_ = true;
		network_thread_.join();
		stop_thread_ = false;
	}


	data_frame_1_.reset();
	protocol_.reset();
	network_.reset();

	bit_write_request_ = false;

}


void cEvents::RunningloopGenisysNetwork()
{

	while (true)
	{
		
		if (stop_thread_)
		{
			return;
		}


		if (bit_write_request_)
		{
			if (!data_frame_1_->WriteBit(byte_offset_, bit_offset_, bit_value_))
			{
				// Error: Bit Failed To Write
			}
			else
			{
				// Update The wxWidgets GUI
			}
		}


		network_->ServerLoop();

	}





}