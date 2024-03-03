#pragma once

// wxWidgets
#include "wx/wx.h"
#include "wx/app.h"

// GenisysSlave-Simulator
#include "cEvents.h"

class cApp : public wxApp
{
public:
	cApp();
	~cApp();

	virtual bool OnInit();

private:
	cEvents* m_frame1 = nullptr;
};

wxDECLARE_APP(cApp);