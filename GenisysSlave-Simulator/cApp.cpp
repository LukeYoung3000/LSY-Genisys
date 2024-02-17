#include "cApp.h"

wxIMPLEMENT_APP(cApp);

cApp::cApp()
{

}

cApp::~cApp()
{

}

bool cApp::OnInit()
{

	//m_frame1 = new cFrameEvents(nullptr);
	m_frame1 = new cEvents();
	m_frame1->Show();

	return true;
}