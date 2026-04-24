//EMB file viewer plugin for FAR
#include "..\FarHeaders\crt.hpp"
#include "..\FarHeaders\plugin.hpp"
#include "..\FarHeaders\farcolor.hpp"
#include "..\FarHeaders\farkeys.hpp"
#pragma hdrstop
#include "embfile.h"

PluginStartupInfo Info;
FARSTANDARDFUNCTIONS FSF;

InitDialogItem idiEdit[]={
 {DI_DOUBLEBOX, 3, 1, 76, 10, false, false, 0, false, NULL},
 {DI_TEXT,      5, 2, 14,  2, false, false, 0, false, (char*)lbCard},
 {DI_EDIT,     15, 2, 34,  2, true,  NULL,  0, false, NULL},
 {DI_TEXT,      5, 3, 14,  3, false, false, 0, false, (char*)lbDate},
 {DI_EDIT,     15, 3, 33,  3, false, NULL,  0, false, NULL},
 {DI_TEXT,      5, 4, 14,  4, false, false, 0, false, (char*)lbCardStamp},
 {DI_EDIT,     15, 4, 42,  4, false, NULL,  0, false, NULL},
 {DI_TEXT,      5, 5, 14,  5, false, false, 0, false, (char*)lbTrack1},
 {DI_EDIT,     15, 5, 74,  5, false, NULL,  0, false, NULL},
 {DI_TEXT,      5, 6, 14,  6, false, false, 0, false, (char*)lbTrack2},
 {DI_EDIT,     15, 6, 52,  6, false, NULL,  0, false, NULL},
 {DI_TEXT,      5, 7, 14,  7, false, false, 0, false, (char*)lbTrack3},
 {DI_EDIT,     15, 7, 74,  7, false, NULL,  0, false, NULL},
 {DI_TEXT,      0, 8,  0,  8, false, false, DIF_BOXCOLOR|DIF_SEPARATOR, 0, ""},
 {DI_BUTTON,    0, 9,  0,  9, false, false, DIF_CENTERGROUP, true, (char*)bnOk},
 {DI_BUTTON,    0, 9,  0,  9, false, false, DIF_CENTERGROUP, false, (char*)bnCancel}
};
FarDialogItem diEdit[sizeof(idiEdit)/sizeof(idiEdit[0])];
//---------------------------------------------------------------------------
void InitDialogItems(const struct InitDialogItem *Init, struct FarDialogItem *Item, int ItemsNumber) {
 int I;
 const struct InitDialogItem *PInit=Init;
 struct FarDialogItem *PItem=Item;
 for (I=0; I < ItemsNumber; I++, PItem++, PInit++) {
  PItem->Type=PInit->Type;
  PItem->X1=PInit->X1;
  PItem->Y1=PInit->Y1;
  PItem->X2=PInit->X2;
  PItem->Y2=PInit->Y2;
  PItem->Focus=PInit->Focus;
  PItem->Selected=PInit->Selected;
  PItem->Flags=PInit->Flags;
  PItem->DefaultButton=PInit->DefaultButton;
  if ((unsigned int)PInit->Data!=NULL)
   if ((unsigned int)PInit->Data<2000)
    lstrcpy(PItem->Data, GetMsg((unsigned int)PInit->Data));
   else
    lstrcpy(PItem->Data, PInit->Data);
 }
}
//---------------------------------------------------------------------------
/*
Функция GetMsg возвращает строку сообщения из языкового файла.
А это надстройка над Info.GetMsg для сокращения кода :-)
*/
const char* GetMsg(int MsgId) {
 return Info.GetMsg(Info.ModuleNumber, MsgId);
}

int WINAPI _export GetMinFarVersion(void) {/*1*/
 return MAKEFARVERSION(1, 70, 1634);
}

/*
Функция SetStartupInfo вызывается один раз, перед всеми
другими функциями. Она передает плагину информацию,
необходимую для дальнейшей работы.
*/
void WINAPI _export SetStartupInfo(const struct PluginStartupInfo *psi) {/*2*/
 Info=*psi;
 FSF=*psi->FSF;
 Info.FSF=&::FSF;

 InitDialogItems(idiEdit, diEdit, sizeof(idiEdit)/sizeof(idiEdit[0]));
}

//Функция GetPluginInfo вызывается для получения основной (general) информации о плагине
void WINAPI _export GetPluginInfo(struct PluginInfo *pi) {/*3*/
// static const char *PluginMenuStrings[]={"EMBView"};

 pi->StructSize=sizeof(struct PluginInfo);
 pi->Flags=PF_DISABLEPANELS;
// pi->PluginMenuStrings=PluginMenuStrings; pi->PluginMenuStringsNumber=1;
// pi->PluginConfigStrings=PluginMenuStrings; pi->PluginConfigStringsNumber=1;
// pi->CommandPrefix=GetMsg(prefix);
}

HANDLE WINAPI _export OpenFilePlugin(char *Name, const unsigned char *Data, int DataSize) {/*4*/
 if (Name==NULL || DataSize<0x30 || *(unsigned short*)Data!=0xDEAD)
  return INVALID_HANDLE_VALUE;
 tEMBfile *obj=new tEMBfile(Name);
 if (obj->PrepareData())//Открыли файл успешно?
  return (HANDLE)obj;
 else {
  delete obj;
  return INVALID_HANDLE_VALUE;
 }
}

void WINAPI _export ClosePlugin(HANDLE hPlugin) {
 delete (tEMBfile*)hPlugin;
}

void WINAPI _export GetOpenPluginInfo(HANDLE hPlugin, struct OpenPluginInfo *pi) {/*5*/
 static char *CustomColumnTitles[]={(char*)GetMsg(lbCard), (char*)GetMsg(lbDate), (char*)GetMsg(lbCardStamp)};
 static PanelMode CustomPanelModes[1]={{"NM,C0,Z", "21,18,0", CustomColumnTitles, true, false, true, true, "NM,C0,Z", "21,18,0", {0, 0}}};
 static KeyBarTitles KeyBar;

 pi->StructSize=sizeof(struct OpenPluginInfo);
 pi->Flags=OPIF_ADDDOTS|OPIF_SHOWPRESERVECASE;
 pi->HostFile=((tEMBfile*)hPlugin)->GetFName();
 pi->CurDir=""; pi->Format="EMB";
 pi->PanelTitle=FSF.PointToName(pi->HostFile);
 pi->PanelModesArray=CustomPanelModes; pi->PanelModesNumber=1;
 pi->StartPanelMode='0'; pi->StartSortMode=SM_UNSORTED;

 memset(&KeyBar, 0, sizeof(KeyBar));
 KeyBar.Titles[6]=(char*)GetMsg(keyF7);
 KeyBar.CtrlTitles[2]=(char*)GetMsg(keyCtrlF3); KeyBar.CtrlTitles[3]="";
 KeyBar.CtrlTitles[4]=(char*)GetMsg(keyCtrlF5); KeyBar.CtrlTitles[5]="";
 KeyBar.CtrlTitles[7]="";                       KeyBar.CtrlTitles[8]="";
 KeyBar.CtrlTitles[9]=(char*)GetMsg(keyCtrlF4); KeyBar.CtrlTitles[10]="";
 pi->KeyBar=&KeyBar;
}
//---------------------------------------------------------------------------
int WINAPI _export GetFindData(HANDLE hPlugin, struct PluginPanelItem **pPanelItem, int *pItemsNumber, int /*OpMode*/) {/*6*/
 return ((tEMBfile*)hPlugin)->GetFindData(pPanelItem, pItemsNumber);
}

void WINAPI _export FreeFindData(HANDLE hPlugin, struct PluginPanelItem *PanelItem, int ItemsNumber) {/*7*/
 ((tEMBfile*)hPlugin)->FreeFindData(PanelItem, ItemsNumber);
}
//---------------------------------------------------------------------------
int WINAPI _export DeleteFiles(HANDLE hPlugin, struct PluginPanelItem *PanelItem, int ItemsNumber, int OpMode) {
 if (ItemsNumber>0 && PanelItem[0].FindData.cFileName[0]!='.'
   && ((OpMode&(OPM_SILENT|OPM_FIND))!=0 || (Info.AdvControl(Info.ModuleNumber, ACTL_GETCONFIRMATIONS, 0) & FCS_DELETE)==0 || Info.Message(Info.ModuleNumber, FMSG_ALLINONE|FMSG_MB_YESNO, NULL, (const char * const *)GetMsg(lbDelete), 0, 0)==0))
  return ((tEMBfile*)hPlugin)->DeleteFiles(PanelItem, ItemsNumber);
 return false;
}
//---------------------------------------------------------------------------
int WINAPI _export ProcessKey(HANDLE hPlugin, int Key, unsigned int ControlState) {
 if (ControlState==PKF_CONTROL && (Key==VK_F4 || Key==VK_F6 || Key==VK_F8 || Key==VK_F9 || Key==VK_F11))
  return true;

 PanelInfo pi;
 Info.Control(hPlugin, FCTL_GETPANELINFO, &pi);

 if (ControlState==0 && (Key==VK_F4 || Key==VK_RETURN)) {
  if (pi.PanelItems[pi.CurrentItem].FindData.cFileName[0]=='.')//стоим на ".."?
   return false;
  if (((tEMBfile*)hPlugin)->EditItem(((tUserData*)pi.PanelItems[pi.CurrentItem].UserData)->index, diEdit, sizeof(idiEdit)/sizeof(idiEdit[0])))
   goto l_update;
  else
   return true;
 }

 if (ControlState==0 && (Key==VK_F7 || Key==VK_DELETE)) {
  ((tEMBfile*)hPlugin)->CleanItems(pi.SelectedItems, pi.SelectedItemsNumber);
l_update:
  Info.Control(hPlugin, FCTL_UPDATEPANEL, NULL);
  Info.Control(hPlugin, FCTL_REDRAWPANEL, NULL);
  return true;
 }

 if (ControlState==PKF_CONTROL && Key==VK_F5) {
  char buf[8];
  if (Info.InputBox(GetMsg(lbJump), GetMsg(lbJumpLine), NULL, NULL, buf, 8, NULL, FIB_NOUSELASTHISTORY)) {
   PanelRedrawInfo pri;
   pri.CurrentItem=FSF.atoi(buf); pri.TopPanelItem=0;
   Info.Control(hPlugin, FCTL_REDRAWPANEL, &pri);
  }
  return true;
 }
 return false;
}

int WINAPI _export GetFiles(HANDLE hPlugin, struct PluginPanelItem *PanelItem, int ItemsNumber, int Move, char *DestPath, int /*OpMode*/) {
 if (ItemsNumber>0 && PanelItem[0].FindData.cFileName[0]!='.') {
  int ret=((tEMBfile*)hPlugin)->GetFiles(PanelItem, ItemsNumber, DestPath);
  if (Move && ret==1)
   ret=((tEMBfile*)hPlugin)->DeleteFiles(PanelItem, ItemsNumber);
  return ret;
 }
 return 0;
}

int WINAPI _export PutFiles(HANDLE hPlugin, struct PluginPanelItem *PanelItem, int ItemsNumber, int /*Move*/, int /*OpMode*/) {
 return ((tEMBfile*)hPlugin)->PutFiles(PanelItem, ItemsNumber);
}
