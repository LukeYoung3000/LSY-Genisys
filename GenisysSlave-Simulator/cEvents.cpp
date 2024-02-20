#include <wx/msgdlg.h>

#include "cEvents.h"

#include "NetworkGenisysSlave.h"
using namespace LSY;


cEvents::cEvents() : cFrame(nullptr)
{

	// Connect Idle Event
	this->Bind(wxEVT_IDLE, &cEvents::IdleEvent, this);

}

void cEvents::GridClickEvent(wxGridEvent& event)
{
	// Get Coord Clicked
	wxGridCellCoords coord = wxGridCellCoords(event.GetRow(), event.GetCol());

	// Flip Value
	wxString value = m_grid3->GetCellValue(coord);
	if (value == wxString("1"))
	{
		m_grid3->SetCellValue(coord, wxString("0"));
		m_grid3->SetCellBackgroundColour(coord.GetRow(), coord.GetCol(), *wxRED);
	}	
	else
	{
		m_grid3->SetCellValue(coord, wxString("1"));
		m_grid3->SetCellBackgroundColour(coord.GetRow(), coord.GetCol(), *wxGREEN);
	}	

	event.Skip();
}

void cEvents::StartStopServerEvent(wxCommandEvent& event)
{

	if (m_toggleStartStop->GetValue() == true)
	{

		// Get UDP Reciving Port
		wxString udp_rx = m_textUDPrx->GetValue();

		// Get UPD Transmit Port
		wxString udp_tx = m_textUDPtx->GetValue();
		
		// Get Slave ID
		wxString gen_slave_id = m_textSlaveID->GetValue();
		
		// Get Frame Size
		unsigned int frame_size = 0;
		m_textFrameSize->GetValue().ToUInt(&frame_size);
		if (frame_size > 125)
		{
			frame_size = 125;
			m_textFrameSize->SetValue("125");
		}
			



		// Spin Up A Genisys UDP Slave

		// Data Frame Setup
		std::shared_ptr<DataFrame> data_1 = std::make_shared<DataFrame>("Genisys Slave 1", frame_size);

		// Protocol Setup
		std::shared_ptr<ProtocolGenisysSlave> protocol = std::make_shared<ProtocolGenisysSlave>();
		protocol->AddDataFrame(1, data_1);

		// Network Configuration
		NetworkGenisysSlave::Config network_config;
		network_config.server_port_ = 7169;
		network_config.destination_port_ = 7169;

		// Network Setup
		NetworkGenisysSlave network(network_config);
		network.AddProtocol(protocol);

		// ------- Run Network Server ---------
		network.StartServer();
		//network.ServerLoop();




		// Enable User Controls On The Grid
		m_grid3->Enable(true);

		// Setup Grid Size Based On Genisys Slave DB Bits
		m_grid3->DeleteRows(0, m_grid3->GetNumberRows());
		m_grid3->AppendRows(frame_size);

		// Setup Grid Values Based On Genisys Slave DB Bits
		// Note: For Now Just Assume 0
		for (int i = 0; i < frame_size; i++)
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

		// Shutdown Genisys UDP Slave

	}


	event.Skip();
}

void cEvents::MenuHelpAboutOnMenuSelection(wxCommandEvent& event)
{

	std::string msg = "";
	msg += "Genisys Application\n";
	msg += "\n";
	msg += "Author: Luke Young\n";
	msg += "GitHub: LukeYoung3000\n";

	wxMessageBox(wxString(msg), "About", wxOK | wxICON_INFORMATION);

	event.Skip();
}


void cEvents::IdleEvent(wxIdleEvent& event)
{
	//wxMessageBox("Yo", "IdleEvent Without Skip", wxOK | wxICON_INFORMATION);
	//m_toggleStartStop->SetValue(!m_toggleStartStop->GetValue());
	event.Skip();
}