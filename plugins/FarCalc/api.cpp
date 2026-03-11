//
//  Copyright (c) uncle-vunkis 2009-2011 <uncle-vunkis@yandex.ru>
//  You can use, modify, distribute this code or any other part
//  of this program in sources or in binaries only according
//  to License (see /doc/license.txt for more information).
//

#include <stdio.h>
#include <windows.h>

#include <hashmap/hashmap.h>

#include "api.h"
#include "calc.h"
#include "messages.h"


CalcApi *api = NULL;
CalcDialogFuncs *dlg_funcs = NULL;
static bool far3 = false;

// exports

extern "C"
{
	void   WINAPI SetStartupInfoW(void *info);
	void   WINAPI GetPluginInfoW(void *pinfo);
	void   WINAPI GetGlobalInfoW(void *ginfo);
	int    WINAPI GetMinFarVersionW();
	HANDLE WINAPI OpenPluginW(int idx, INT_PTR);
	HANDLE WINAPI OpenW(void *oinfo);
	int    WINAPI ConfigureW(void *);
}


//////////////////////////////////////////////////////////////
// FAR exports
void WINAPI SetStartupInfoW(void *info)
{
	if (api)
		delete api;
	api = (far3) ? CreateApiFar3(info) : CreateApiFar2(info);
	if (api)
	{
		dlg_funcs = api->GetDlgFuncs();
		CalcStartup();
	}
}

void WINAPI GetPluginInfoW(void *pinfo)
{
	api->GetPluginInfo(pinfo, api->GetMsg(mName));
}

HANDLE WINAPI OpenPluginW(int OpenFrom, INT_PTR Item)
{
	CalcOpen(api->IsOpenedFromEditor(NULL, OpenFrom));
	return INVALID_HANDLE_VALUE;
}

HANDLE WINAPI OpenW(void *oinfo)
{
	CalcOpen(api->IsOpenedFromEditor(oinfo, 0));
	return NULL;
}

int WINAPI ConfigureW(void *c)
{
	return CalcConfig();
}

void WINAPI GetGlobalInfoW(void *ginfo)
{
	GetGlobalInfoFar3(ginfo, L"calculator");
	far3 = true;
}

int WINAPI GetMinFarVersionW()
{
	return (2<<8)|(994<<16); // major=2, build=994
}

////////////////////////////////////////////////////////////////////

struct tMap {
	DLGHANDLE hdlg;
	CalcDialog* dlg;
};

tMap dlg_hash[2];
int dlg_cur = -1;

static CALC_INT_PTR __stdcall dlgProc(DLGHANDLE hdlg, int msg, int param1, void *param2)
{
	if ((dlg_cur == -1) || (dlg_cur >= ARRAYSIZE(dlg_hash) - 1))
		return -1;

	CALC_INT_PTR ret = -1;
	CalcDialog *dlg = dlg_hash[dlg_cur].dlg;

	if (dlg)
	{
		if (dlg->msg_tbl[msg])
		{
			dlg_funcs->PreProcessMessage(hdlg, msg, param1, param2);

			ret = (dlg->*(dlg->msg_tbl[msg]))(param1, param2);

			dlg_funcs->PostProcessMessage(hdlg, msg, ret, param1, param2);
		}
	}

	if (ret == -1)
		return dlg_funcs->DefDlgProc1(hdlg, msg, param1, param2);
	return ret;
}

/////////////////////////////////////////////////////////////////////////

CalcDialog::CalcDialog()
{
	msg_tbl = dlg_funcs->GetMessageTable();
	hdlg = NULL;
}

CalcDialog::~CalcDialog()
{
	if (hdlg)
	{
		dlg_hash[dlg_cur].hdlg = NULL;
		dlg_hash[dlg_cur].dlg = NULL;
		dlg_cur--;
		dlg_funcs->DialogFree(hdlg);
	}
}

bool CalcDialog::Init(int id, int X1, int Y1, int X2, int Y2, const wchar_t *HelpTopic,
							struct CalcDialogItem *Item, unsigned int ItemsNumber)
{
	if (dlg_cur >= ARRAYSIZE(dlg_hash)-1)
	{
		const wchar_t *MsgItems[] = {L"Calc error", L"Count out of range!"};//!!DEBUG
		api->Message(0, NULL, MsgItems, 3, 1);
		return false;
	}
	else
	{
		hdlg = dlg_funcs->DialogInit(id, X1, Y1, X2, Y2, HelpTopic, Item, ItemsNumber, dlgProc);
		if (hdlg == INVALID_HANDLE_VALUE)
			return false;
		dlg_hash[dlg_cur].hdlg = hdlg;
		dlg_hash[dlg_cur].dlg = this;
		dlg_cur++;
		return true;
	}
}

intptr_t CalcDialog::Run()
{
	return dlg_funcs->DialogRun(hdlg);
}

void CalcDialog::EnableRedraw(bool en)
{
	dlg_funcs->EnableRedraw(hdlg, en);
}
void CalcDialog::ResizeDialog(const CalcCoord & dims)
{
	dlg_funcs->ResizeDialog(hdlg, dims);
}
void CalcDialog::RedrawDialog()
{
	dlg_funcs->RedrawDialog(hdlg);
}
void CalcDialog::GetDlgRect(CalcRect *rect)
{
	dlg_funcs->GetDlgRect(hdlg, rect);
}
void CalcDialog::Close(int exitcode)
{
	dlg_funcs->Close(hdlg, exitcode);
}

void CalcDialog::GetDlgItemShort(int id, CalcDialogItem *item)
{
	dlg_funcs->GetDlgItemShort(hdlg, id, item);
}
void CalcDialog::SetDlgItemShort(int id, const CalcDialogItem & item)
{
	dlg_funcs->SetDlgItemShort(hdlg, id, item);
}
void CalcDialog::SetItemPosition(int id, const CalcRect & rect)
{
	dlg_funcs->SetItemPosition(hdlg, id, rect);
}
int  CalcDialog::GetFocus()
{
	return dlg_funcs->GetFocus(hdlg);
}
void CalcDialog::SetFocus(int id)
{
	dlg_funcs->SetFocus(hdlg, id);
}
void CalcDialog::EditChange(int id, const CalcDialogItem & item)
{
	dlg_funcs->EditChange(hdlg, id, item);
}
void CalcDialog::SetSelection(int id, const CalcEditorSelect & sel)
{
	dlg_funcs->SetSelection(hdlg, id, sel);
}
void CalcDialog::SetCursorPos(int id, const CalcCoord & pos)
{
	dlg_funcs->SetCursorPos(hdlg, id, pos);
}
void CalcDialog::GetText(int id, std::wstring &str)
{
	dlg_funcs->GetText(hdlg, id, str);
}
void CalcDialog::SetText(int id, const std::wstring & str)
{
	dlg_funcs->SetText(hdlg, id, str);
}
void CalcDialog::AddHistory(int id, const std::wstring & str)
{
	dlg_funcs->AddHistory(hdlg, id, str);
}
bool CalcDialog::IsChecked(int id)
{
	return dlg_funcs->IsChecked(hdlg, id);
}