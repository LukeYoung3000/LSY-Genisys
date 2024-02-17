#pragma once


#include "wx/wx.h"
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

