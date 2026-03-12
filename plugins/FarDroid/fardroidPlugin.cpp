#include <CRT\crt.hpp>
#include <windows.h>
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _FAR_USE_FARFINDDATA
#include <DlgBuilder.hpp>
#include <initguid.h>
#include "guid.hpp"
#include "lang.hpp"
#include "fardroid.h"
#include "stuff.h"

int ID_WorkModeBB, ID_ShowLinksAsDir, ID_CopySD, ID_CopySDWarning, ID_KillServer, ID_KillServerWarning;

struct KeyBarLabel Label[] = {
  { { VK_F7, SHIFT_PRESSED }, L"DevName", L"Device Name" },
  { { VK_F10, LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED }, L"ScrShot", L"Screenshot" },
  { { VK_F10, SHIFT_PRESSED | LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED }, L"Sys RW", L"Mount /system RW" },
  { { VK_F11, SHIFT_PRESSED | LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED }, L"Sys RO", L"Mount /system RO" }
};
struct KeyBarTitles KeyBar = {_ARRAYSIZE(Label), Label};

BOOL WINAPI DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID)
{
  if (ul_reason_for_call == DLL_PROCESS_ATTACH)
    DisableThreadLibraryCalls(hModule);
  return TRUE;
}

void WINAPI GetGlobalInfoW(struct GlobalInfo* Info)
{
  Info->StructSize = sizeof(struct GlobalInfo);
  Info->MinFarVersion = FARMANAGERVERSION;
  Info->Version = PLUGIN_VERSION;
  Info->Guid = MainGuid;
  Info->Title = PLUGIN_NAME;
  Info->Description = PLUGIN_DESC;
  Info->Author = PLUGIN_AUTHOR;
  DELETELOG();
}

void WINAPI SetStartupInfoW(const struct PluginStartupInfo* Info)
{FUNCTION
  if (Info->StructSize >= sizeof(struct PluginStartupInfo))
  {
    PsInfo = *Info;
    FSF = *Info->FSF;
    PsInfo.FSF = &FSF;

    PluginSettings settings(MainGuid, PsInfo.SettingsControl);
    settings.Get(0, L"Prefix", Opt.Prefix, _ARRAYSIZE(Opt.Prefix), L"fardroid");
    settings.Get(0, L"ADBPath", Opt.ADBPath, _ARRAYSIZE(Opt.ADBPath), L"");
    Opt.PanelMode      = settings.Get(0, L"PanelMode", 1);
    Opt.WorkMode       = settings.Get(0, L"WorkMode", 0);
    Opt.SortMode       = settings.Get(0, L"SortMode", 1);
    Opt.SortOrder      = settings.Get(0, L"SortOrder", false);
    Opt.ShowLinksAsDir = settings.Get(0, L"ShowLinksAsDir", false);
    Opt.KillServer     = settings.Get(0, L"KillServer", false);
    Opt.UseSU          = settings.Get(0, L"UseSU", false);
    Opt.CopySD         = settings.Get(0, L"CopySD", false);
    Opt.AddToDiskMenu  = settings.Get(0, L"AddToDiskMenu", false);
    Opt.RemountSystem  = settings.Get(0, L"RemountSystem", false);

    if (!hRegexpSize)
      hRegexpSize = RegexpMake(LR"(/([\d.]+)(.*)/)");
    if (!hRegexpMem)
      hRegexpMem = RegexpMake(LR"(/(\w+): +(.+)$/)");
    if (!hRegexpPart1)
      //                             1            2                  3
      hRegexpPart1 = RegexpMake(LR"(/(.*(?=:)):\W+(\w+(?= total)).+, (\w+(?= available))/)");
    if (!hRegexpPart2)
      //                              1 fs   2 blks       3 used       4 avail      use%          5 mount
      hRegexpPart2 = RegexpMake(LR"(/^(\S+) +([\d.]+\S*) +([\d.]+\S*) +([\d.]+\S*) +(?:[\d.]+% +)?(\S+)/)");
    if (!hRegexpFile)
      //                             1 privs                2 own  3 grp  4 size                5 year  6 mon   7 day    8 hour  9 min   10 mon   11 day 12 year 13 mon   14 day 15 hh   16 min   17 file  18  19 link
      hRegexpFile = RegexpMake(LR"(/^([\w-]{10}) +(?:\d+ +)?(\w+) +(\w+) +(\d+(?:, *\d+)?)? +(?:(\d{4})-(\d{2})-(\d{2}) +(\d{2}):(\d{2})|(\w{3}) +(\d+) +(\d{4})|(\w{3}) +(\d+) +(\d{2}):(\d{2})) (.+?)(?: (-> (.+)))?$/)");
  }
}

void WINAPI ExitFARW(const struct ExitInfo*)
{FUNCTION
  RegexpFree(hRegexpFile);
  RegexpFree(hRegexpPart2);
  RegexpFree(hRegexpPart1);
  RegexpFree(hRegexpMem);
  RegexpFree(hRegexpSize);
}

void WINAPI GetPluginInfoW(struct PluginInfo* Info)
{FUNCTION
  Info->StructSize = sizeof(PluginInfo);
  Info->Flags = 0;

  static const wchar_t *PluginConfigMenuStrings[1], *PluginMenuStrings[1];

  PluginConfigMenuStrings[0] = GetMsg(MTitle);
  Info->PluginConfig.Strings = PluginConfigMenuStrings;
  Info->PluginConfig.Count = 1;
  Info->PluginConfig.Guids = &MenuGuid;

  PluginMenuStrings[0] = PluginConfigMenuStrings[0];
  Info->PluginMenu.Strings = PluginMenuStrings;
  Info->PluginMenu.Count = 1;
  Info->PluginMenu.Guids = &MenuGuid;

  Info->CommandPrefix = Opt.Prefix;

  static const wchar_t* DiskMenuString[1];

  DiskMenuString[0] = PluginConfigMenuStrings[0];
  if (Opt.AddToDiskMenu)
  {
    Info->DiskMenu.Strings = DiskMenuString;
    Info->DiskMenu.Count = 1;
    Info->DiskMenu.Guids = &MenuGuid;
  }
  else
  {
    Info->DiskMenu.Strings = nullptr;
    Info->DiskMenu.Count = 0;
    Info->DiskMenu.Guids = nullptr;
  }
}

HANDLE WINAPI OpenW(const struct OpenInfo* Info)
{FUNCTION
  HANDLE res = NULL;
  switch (Info->OpenFrom) {
  case OPEN_LEFTDISKMENU:
  case OPEN_RIGHTDISKMENU:
  case OPEN_PLUGINSMENU:
  case OPEN_COMMANDLINE:
    fardroid *android = new fardroid();
    if (android) {
      if (Info->OpenFrom == OPEN_COMMANDLINE)
        res = android->OpenFromCommandLine(((OpenCommandLineInfo*)Info->Data)->CommandLine);
      else
        res = android->OpenFromMainMenu();
      if (!res)
        delete android;
    }
  }
  return res;
}

void WINAPI ClosePanelW(const struct ClosePanelInfo* Info)
{FUNCTION
  if (Info->hPanel)
    delete (fardroid*)Info->hPanel;
}

intptr_t WINAPI ConfigDlgProc(HANDLE hDlg, intptr_t Msg, intptr_t Param1, void* Param2)
{
  if (Msg == DN_CTLCOLORDLGITEM)
  {
    if (Param1 == ID_KillServerWarning && PsInfo.SendDlgMessage(hDlg, DM_GETCHECK, ID_KillServer, nullptr)
    ||  Param1 == ID_CopySDWarning && PsInfo.SendDlgMessage(hDlg, DM_GETCHECK, ID_CopySD, nullptr))
    {
      if (Param2)
        ((FarDialogItemColors*)Param2)->Colors[0].ForegroundColor = 0x4;
    }
  }
  else if (Msg == DN_BTNCLICK && Param1 == ID_WorkModeBB)
    PsInfo.SendDlgMessage(hDlg, DM_ENABLE, ID_ShowLinksAsDir, Param2);

  return PsInfo.DefDlgProc(hDlg, Msg, Param1, Param2);
}

intptr_t WINAPI ConfigureW(const struct ConfigureInfo* Info)
{FUNCTION
  PluginDialogBuilder Builder(PsInfo, MainGuid, DialogGuid, MConfTitle, L"Config", ConfigDlgProc);
  Builder.AddCheckbox(MConfAddToDisk, &Opt.AddToDiskMenu);
  Builder.AddTextBefore(Builder.AddEditField(Opt.Prefix, _ARRAYSIZE(Opt.Prefix), 15, L"fardroidPrefix", true), MConfPrefix);
  Builder.AddSeparator();
  const int ModeIDs[] = {MConfSafeMode, MConfNative, MConfBusybox};
  Builder.AddRadioButtons(&Opt.WorkMode, _ARRAYSIZE(ModeIDs), ModeIDs, true);
  ID_WorkModeBB = Builder.GetLastID();
  FarDialogItem *item = Builder.AddCheckbox(MConfShowLinksAsDirs, &Opt.ShowLinksAsDir);
  ID_ShowLinksAsDir = Builder.GetLastID();
  if (Opt.WorkMode != _ARRAYSIZE(ModeIDs)-1)
    item->Flags |= DIF_DISABLE;
  Builder.AddTextBefore(item, L"   ");
  Builder.AddSeparator();
  Builder.AddCheckbox(MConfUseSU, &Opt.UseSU);
  Builder.AddTextBefore(Builder.AddCheckbox(MConfCopySD, &Opt.CopySD), L"   ");
  ID_CopySD = Builder.GetLastID()-1;
  Builder.AddText(MConfCopySDWarning)->Flags |= DIF_CENTERTEXT;
  ID_CopySDWarning = Builder.GetLastID();
  Builder.AddCheckbox(MConfRemountSystem, &Opt.RemountSystem);
  Builder.AddSeparator();
  Builder.AddTextBefore(Builder.AddEditField(Opt.ADBPath, _ARRAYSIZE(Opt.ADBPath), 30, L"fardroidADBPath", true), MConfADBPath);
  Builder.AddCheckbox(MConfKillServer, &Opt.KillServer);
  ID_KillServer = Builder.GetLastID();
  Builder.AddText(MConfKillServerWarning)->Flags |= DIF_CENTERTEXT;
  ID_KillServerWarning = Builder.GetLastID();
  Builder.AddOKCancel(MOk, MCancel);
  if (Builder.ShowDialog())
  {
    PluginSettings settings(MainGuid, PsInfo.SettingsControl);
    settings.Set(0, L"Prefix", Opt.Prefix);
    settings.Set(0, L"ADBPath", Opt.ADBPath);
    settings.Set(0, L"PanelMode", Opt.PanelMode);
    settings.Set(0, L"WorkMode", Opt.WorkMode);
    settings.Set(0, L"SortMode", Opt.SortMode);
    settings.Set(0, L"SortOrder", Opt.SortOrder);
    settings.Set(0, L"ShowLinksAsDir", Opt.ShowLinksAsDir);
    settings.Set(0, L"KillServer", Opt.KillServer);
    settings.Set(0, L"UseSU", Opt.UseSU);
    settings.Set(0, L"CopySD", Opt.CopySD);
    settings.Set(0, L"AddToDiskMenu", Opt.AddToDiskMenu);
    settings.Set(0, L"RemountSystem", Opt.RemountSystem);
    return true;
  }
  else
    return false;
}

void WINAPI GetOpenPanelInfoW(struct OpenPanelInfo* Info)
{
  if (Info->hPanel) {
    Info->StructSize = sizeof(OpenPanelInfo);
    Info->Flags = OPIF_SHOWPRESERVECASE | OPIF_USEFREESIZE | OPIF_USECRC32;

    Info->Format = GetMsg(MTitle);

    ((fardroid*)Info->hPanel)->PreparePanel(Info);

    Info->DescrFiles = nullptr;
    Info->DescrFilesNumber = 0;

    Info->StartPanelMode = L'0' + Opt.PanelMode;
    Info->StartSortMode = (OPENPANELINFO_SORTMODES)Opt.SortMode;
    Info->StartSortOrder = Opt.SortOrder;

    Info->PanelModesArray = nullptr;
    Info->PanelModesNumber = 0;

    Info->KeyBar = &KeyBar;
  }
}

intptr_t WINAPI GetFindDataW(struct GetFindDataInfo* Info)
{FUNCTION
  return Info->hPanel && ((fardroid*)Info->hPanel)->GetFindData(&Info->PanelItem, &Info->ItemsNumber, Info->OpMode);
}

void WINAPI FreeFindDataW(const struct FreeFindDataInfo* Info)
{FUNCTION
  if (Info->hPanel && Info->PanelItem) {
    for (size_t i = 0; i < Info->ItemsNumber; i++)
    {
      if (Info->PanelItem[i].FileName)
        free((void*)Info->PanelItem[i].FileName);
      if (Info->PanelItem[i].AlternateFileName)
        free((void*)Info->PanelItem[i].AlternateFileName);
      if (Info->PanelItem[i].Owner)
        free((void*)Info->PanelItem[i].Owner);
      if (Info->PanelItem[i].Description)
        free((void*)Info->PanelItem[i].Description);
    }
    free(Info->PanelItem);
  }
}

intptr_t WINAPI ProcessPanelInputW(const struct ProcessPanelInputInfo* Info)
{
  if (!Info->hPanel)
    return FALSE;

  fardroid *android = (fardroid*)Info->hPanel;

  if (Info->Rec.EventType != KEY_EVENT) return FALSE;

  DWORD dwControl = Info->Rec.Event.KeyEvent.dwControlKeyState;
  DWORD dwCTRL = dwControl & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED);
  DWORD dwALT = dwControl & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED);
  DWORD dwSHIFT = dwControl & SHIFT_PRESSED;

  if (dwCTRL && !dwSHIFT && !dwALT && Info->Rec.Event.KeyEvent.wVirtualKeyCode == L'A')
  {
    PanelInfo PInfo;
    PInfo.StructSize = sizeof(PanelInfo);
    PsInfo.PanelControl(Info->hPanel, FCTL_GETPANELINFO, 0, (void*)&PInfo);
    if (android->ChangePermissionsDialog(PInfo.SelectedItemsNumber)) {
      PsInfo.PanelControl(Info->hPanel, FCTL_UPDATEPANEL, 1, nullptr);
      PsInfo.PanelControl(Info->hPanel, FCTL_REDRAWPANEL, 0, nullptr);
    }
    return TRUE;
  }
  else if (!dwCTRL && dwSHIFT && !dwALT)
  {
    switch (Info->Rec.Event.KeyEvent.wVirtualKeyCode)
    {
    case VK_F5:
    case VK_F6:
    {
      if (android->Copy_Rename(Info->Rec.Event.KeyEvent.wVirtualKeyCode == VK_F5)) {
        PsInfo.PanelControl(Info->hPanel, FCTL_UPDATEPANEL, 1, nullptr);
        PsInfo.PanelControl(Info->hPanel, FCTL_REDRAWPANEL, 0, nullptr);
      }
      return TRUE;
    }
    case VK_F7:
    {
      android->DeviceNameDialog();
      return TRUE;
    }
    }
  }
  else if (!dwCTRL && !dwSHIFT && dwALT && Info->Rec.Event.KeyEvent.wVirtualKeyCode == VK_F10)
  {
    android->GetFramebuffer();
    return TRUE;
  }
  else if (!dwCTRL && dwSHIFT && dwALT)
  {
    switch (Info->Rec.Event.KeyEvent.wVirtualKeyCode)
    {
    case VK_F10:
      android->Remount(L"rw");
      return TRUE;
    case VK_F11:
      android->Remount(L"ro");
      return TRUE;
    }
  }
  return FALSE;
}

intptr_t WINAPI ProcessPanelEventW(const struct ProcessPanelEventInfo* Info)
{
  if (Info->hPanel && Info->Event == FE_CHANGEVIEWMODE)
  {
    PanelInfo PInfo;
    PInfo.StructSize = sizeof(PanelInfo);
    PsInfo.PanelControl(Info->hPanel, FCTL_GETPANELINFO, 0, (void*)&PInfo);

    if ((PInfo.Flags & (PFLAGS_PLUGIN|PFLAGS_VISIBLE)) == (PFLAGS_PLUGIN|PFLAGS_VISIBLE))
    {
      Opt.PanelMode = (int)PInfo.ViewMode;
      Opt.SortMode = PInfo.SortMode;
      Opt.SortOrder = (PInfo.Flags & PFLAGS_REVERSESORTORDER) != 0;
      PluginSettings settings(MainGuid, PsInfo.SettingsControl);
      settings.Set(0, L"PanelMode", Opt.PanelMode);
      settings.Set(0, L"SortMode", Opt.SortMode);
      settings.Set(0, L"SortOrder", Opt.SortOrder);
    }
  }
  return FALSE;
}

intptr_t WINAPI SetDirectoryW(const struct SetDirectoryInfo* Info)
{FUNCTION
  if (Info->hPanel) {
    ((fardroid*)Info->hPanel)->ChangeDir(Info->Dir);
    return TRUE;
  }
  else
    return FALSE;
}

intptr_t WINAPI DeleteFilesW(const struct DeleteFilesInfo* Info)
{FUNCTION
  return Info->hPanel && ((fardroid*)Info->hPanel)->DeleteFiles(Info->PanelItem, Info->ItemsNumber, Info->OpMode);
}

intptr_t WINAPI MakeDirectoryW(struct MakeDirectoryInfo* Info)
{FUNCTION
  return Info->hPanel ? ((fardroid*)Info->hPanel)->CreateDir(&Info->Name, Info->OpMode) : 0;
}

intptr_t WINAPI GetFilesW(struct GetFilesInfo* Info)
{FUNCTION
  if (!Info->hPanel || Info->ItemsNumber == 1 && !lstrcmp(Info->PanelItem[0].FileName, L".."))
    return -1;

  int result = ((fardroid*)Info->hPanel)->GetFiles(Info->PanelItem, Info->ItemsNumber, &Info->DestPath, Info->Move, Info->OpMode);
  if (result <= 0)
    return result;

  return !Info->Move || ((fardroid*)Info->hPanel)->DeleteFiles(Info->PanelItem, Info->ItemsNumber, Info->OpMode | OPM_SILENT);
}

intptr_t WINAPI PutFilesW(const struct PutFilesInfo* Info)
{FUNCTION
  if (!Info->hPanel || Info->ItemsNumber == 1 && !lstrcmp(Info->PanelItem[0].FileName, L".."))
    return -1;

  fardroid* android = (fardroid*)Info->hPanel;
  int result = ((fardroid*)Info->hPanel)->PutFiles(Info->PanelItem, Info->ItemsNumber, Info->SrcPath, Info->Move, Info->OpMode);
  if (result <= 0)
    return result;

  if (Info->Move) {
    intptr_t Size = PsInfo.PanelControl(PANEL_ACTIVE, FCTL_GETPANELDIRECTORY, 0, nullptr);
    FarPanelDirectory *dirInfo = (FarPanelDirectory*)malloc(Size);
    dirInfo->StructSize = sizeof(FarPanelDirectory);
    PsInfo.PanelControl(PANEL_ACTIVE, FCTL_GETPANELDIRECTORY, Size, dirInfo);

    string path = dirInfo->Name;
    for (size_t i = 0; i < Info->ItemsNumber; i++) {
      string sName = ConcatPath(path, Info->PanelItem[i].FileName);
      if (Info->PanelItem[i].FileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        sName += L'\0';
        SHFILEOPSTRUCT file_op{NULL, FO_DELETE, sName.CPtr(), NULL, FOF_NO_UI, false, 0, NULL};
        SHFileOperation(&file_op);
      }
      else
        DeleteFile(sName.CPtr());
    }
    free(dirInfo);
  }
  return 2;
}
