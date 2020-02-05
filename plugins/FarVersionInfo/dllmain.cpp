#define WIN32_LEAN_AND_MEAN    // Exclude rarely-used stuff from Windows headers
#define _INC_STRING
#include <CRT\crt.hpp>
#include <plugin.hpp>
#include <farversion.hpp>
#include <initguid.h>
#pragma hdrstop
#pragma comment(linker,"/merge:.rdata=.data")
#pragma comment(linker,"/merge:.pdata=.data")

#define PLUGIN_BUILD 9
#define PLUGIN_NAME L"VersionInfo"
#define PLUGIN_DESC PLUGIN_NAME
#define PLUGIN_AUTHOR L"Lost"
#define PLUGIN_VERSION MAKEFARVERSION(FARMANAGERVERSION_MAJOR,FARMANAGERVERSION_MINOR,FARMANAGERVERSION_REVISION,PLUGIN_BUILD,VS_RELEASE)

// {CAC00BED-0617-4B7B-B9F9-73DAF412D823}
DEFINE_GUID(MainGuid, 0xcac00bed, 0x617, 0x4b7b, 0xb9, 0xf9, 0x73, 0xda, 0xf4, 0x12, 0xd8, 0x23);
// {A57FF8D7-DB32-4CA0-9EF1-4D0A431FBEA4}
DEFINE_GUID(MenuGuid, 0xa57ff8d7, 0xdb32, 0x4ca0, 0x9e, 0xf1, 0x4d, 0xa, 0x43, 0x1f, 0xbe, 0xa4);

/*--WinAPI
unsigned int Len;
if (!VerQueryValue(pVer, L"StringFileInfo\\000004B0\\FileVersion", (void**)&MsgItems[2], &Len))
  ShowError(L"VerQueryValue");
else
  Info.Message(&MainGuid, NULL, FMSG_LEFTALIGN|FMSG_MB_OK, NULL, MsgItems, ARRAYSIZE(MsgItems), 0);
*/

struct VS_VERSIONINFO {
  WORD  wLength;
  WORD  wValueLength;
  WORD  wType;
  WCHAR szKey[1];
  WORD  Padding1[1];
  VS_FIXEDFILEINFO Value;
  WORD  Padding2[1];
  WORD  Children[1];
};

struct String {
  WORD   wLength;
  WORD   wValueLength;
  WORD   wType;
  WCHAR  szKey[1];
  WORD   Padding[1];
  WORD   Value[1];
};

struct StringTable {
  WORD   wLength;
  WORD   wValueLength;
  WORD   wType;
  WCHAR  szKey[1];
  WORD   Padding[1];
  String Children[1];
};

struct StringFileInfo {
  WORD        wLength;
  WORD        wValueLength;
  WORD        wType;
  WCHAR       szKey[1];
  WORD        Padding[1];
  StringTable Children[1];
};

struct Var {
  WORD  wLength;
  WORD  wValueLength;
  WORD  wType;
  WCHAR szKey[1];
  WORD  Padding[1];
  DWORD Value[1];
};

struct VarFileInfo {
  WORD  wLength;
  WORD  wValueLength;
  WORD  wType;
  WCHAR szKey[1];
  WORD  Padding[1];
  Var   Children[1];
};
// ----------------------------------------------------------------------------

static struct PluginStartupInfo Info;

void ShowError(wchar_t *sErrTitle) {
  wchar_t *ErrMsgItems[] = {PLUGIN_NAME, sErrTitle, NULL};

  FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_MAX_WIDTH_MASK, NULL, GetLastError(), 0, (LPTSTR)&ErrMsgItems[2], 0, NULL);
  Info.Message(&MainGuid, NULL, FMSG_WARNING|FMSG_MB_OK, NULL, ErrMsgItems, ARRAYSIZE(ErrMsgItems), 0);
  LocalFree(ErrMsgItems[2]);
}

#define roundoffs(a,b,r)  (((unsigned char*)(b) - (unsigned char*)(a) + ((r)-1)) & ~((r)-1))
#define roundpos(b, a, r)  (((unsigned char*)(a))+roundoffs(a,b,r))
void parseVerInfo(VS_VERSIONINFO *pVS, DWORD size) {
  const wchar_t *MsgItems[20] = {PLUGIN_NAME, NULL};
  unsigned item_count;
  wchar_t* buf;

  item_count = 1;
  if (lstrcmp(pVS->szKey, L"VS_VERSION_INFO")) {
    MsgItems[1] = L"Unknown resource format";
    Info.Message(&MainGuid, NULL, FMSG_WARNING|FMSG_MB_OK, NULL, MsgItems, 2, 0);
    return;
  }
  unsigned char* pVt = (unsigned char*) &pVS->szKey[lstrlen(pVS->szKey)+1];
  VS_FIXEDFILEINFO* pValue = (VS_FIXEDFILEINFO*) roundpos(pVt, pVS, 4);

  // Iterate over the 'Children' elements of VS_VERSIONINFO (either StringFileInfo or VarFileInfo)
  StringFileInfo* pSFI = (StringFileInfo*) roundpos(((unsigned char*)pValue) + pVS->wValueLength, pValue, 4);
  for ( ; ((unsigned char*) pSFI) < (((unsigned char*) pVS) + pVS->wLength); pSFI = (StringFileInfo*)roundpos((((unsigned char*) pSFI) + pSFI->wLength), pSFI, 4)) { // StringFileInfo / VarFileInfo
    if (!lstrcmp(pSFI->szKey, L"StringFileInfo") && !pSFI->wValueLength) {// The current child is a StringFileInfo element
      // Iterate through the StringTable elements of StringFileInfo
      StringTable* pST = (StringTable*)roundpos(&pSFI->szKey[lstrlen(pSFI->szKey) + 1], pSFI, 4);
      for (; ((unsigned char*)pST) < (((unsigned char*)pSFI) + pSFI->wLength); pST = (StringTable*)roundpos((((unsigned char*)pST) + pST->wLength), pST, 4)) {
        /*buf = new wchar_t[lstrlen(pST->szKey)+10];
        wsprintf(buf, L"LangID             %s", pST->szKey);
        if (item_count < ARRAYSIZE(MsgItems))
          MsgItems[item_count++] = buf;*/

        if (!pST->wValueLength) {
          // Iterate through the String elements of StringTable
          String* pS = (String*)roundpos(&pST->szKey[lstrlen(pST->szKey) + 1], pST, 4);
          for (; ((unsigned char*)pS) < (((unsigned char*)pST) + pST->wLength); pS = (String*)roundpos((((unsigned char*)pS) + pS->wLength), pS, 4)) {
            wchar_t* psVal = (pS->wValueLength == 0) ? psVal = L"" : (wchar_t*)roundpos(&pS->szKey[lstrlen(pS->szKey) + 1], pS, 4);
            buf = new wchar_t[lstrlen(pS->szKey) + lstrlen(psVal) + 20];
            if (buf) {
              wsprintf(buf, L"%-18s %s", pS->szKey, psVal); // print <sKey> : <sValue>
              if (item_count < ARRAYSIZE(MsgItems))
                MsgItems[item_count++] = buf;
            }
          }
        }
      }
    }
    else {
      // The current child is a VarFileInfo element
      VarFileInfo* pVFI = (VarFileInfo*) pSFI;
      if (!lstrcmp(pVFI->szKey, L"VarFileInfo") && !pVFI->wValueLength) {
        // Iterate through the Var elements of VarFileInfo (there should be only one, but just in case...)
        Var* pV = (Var*)roundpos(&pVFI->szKey[lstrlen(pVFI->szKey) + 1], pVFI, 4);
        for (; ((unsigned char*)pV) < (((unsigned char*)pVFI) + pVFI->wLength); pV = (Var*)roundpos((((unsigned char*)pV) + pV->wLength), pV, 4)) {
          //printf(" %S: ", pV->szKey);
          // Iterate through the array of pairs of 16-bit language ID values that make up the standard 'Translation' VarFileInfo element.
          WORD* pwV = (WORD*)roundpos(&pV->szKey[lstrlen(pV->szKey) + 1], pV, 4);
          for (WORD* wpos = pwV; ((unsigned char*)wpos) < (((unsigned char*)pwV) + pV->wValueLength); wpos += 2) {
            //printf("%04x%04x ", (int)*wpos++, (int)(*(wpos + 1)));
          }
          //printf("\n");
        }
      }
    }
  }
  Info.Message(&MainGuid, NULL, FMSG_LEFTALIGN | FMSG_MB_OK, NULL, MsgItems, item_count, 0);
  for (unsigned i = 1; i < item_count; i++)
    free((void*)MsgItems[i]);
}

// ----------------------------------------------------------------------------

void WINAPI GetGlobalInfoW(struct GlobalInfo *gi) {
  gi->StructSize=sizeof(struct GlobalInfo);
  gi->MinFarVersion=FARMANAGERVERSION;
  gi->Version=PLUGIN_VERSION;
  gi->Guid=MainGuid;
  gi->Title=PLUGIN_NAME;
  gi->Description=PLUGIN_DESC;
  gi->Author=PLUGIN_AUTHOR;
}

/*
 Функция GetMsg возвращает строку сообщения из языкового файла.
 А это надстройка над Info.GetMsg для сокращения кода :-)
*/
const wchar_t *GetMsg(int MsgId) {
  return Info.GetMsg(&MainGuid,MsgId);
}

/*
Функция SetStartupInfoW вызывается один раз, перед всеми
другими функциями. Она передается плагину информацию,
необходимую для дальнейшей работы.
*/
void WINAPI SetStartupInfoW(const struct PluginStartupInfo *psi) {
  Info=*psi;
}

//Функция GetPluginInfoW вызывается для получения информации о плагине
void WINAPI GetPluginInfoW(struct PluginInfo *pi) {
  pi->StructSize=sizeof(*pi);
  pi->Flags=PF_NONE;
  static const wchar_t *PluginMenuStrings[1];
  PluginMenuStrings[0] = PLUGIN_NAME;
  pi->PluginMenu.Guids = &MenuGuid;
  pi->PluginMenu.Strings = PluginMenuStrings;
  pi->PluginMenu.Count = ARRAYSIZE(PluginMenuStrings);
}

//Функция OpenPluginW вызывается при создании новой копии плагина.
HANDLE WINAPI OpenW(const struct OpenInfo *OInfo) {
  //const wchar_t *MsgItems[] = {PLUGIN_NAME, NULL};//!!DEBUG

  int Size = (int)Info.PanelControl(PANEL_ACTIVE, FCTL_GETPANELDIRECTORY, 0, NULL);
  FarPanelDirectory* dirInfo = (FarPanelDirectory*)malloc(Size);
  if (dirInfo) {
    dirInfo->StructSize = sizeof(FarPanelDirectory);
    Info.PanelControl(PANEL_ACTIVE, FCTL_GETPANELDIRECTORY, Size, dirInfo);

    FarGetPluginPanelItem gpi = { sizeof(FarGetPluginPanelItem), (size_t)Info.PanelControl(PANEL_ACTIVE, FCTL_GETCURRENTPANELITEM, 0, NULL), 0 };
    gpi.Item = (PluginPanelItem*)malloc(gpi.Size);
    if (gpi.Item) {
      Info.PanelControl(PANEL_ACTIVE, FCTL_GETCURRENTPANELITEM, 0, &gpi);
      wchar_t* FullName = new wchar_t[lstrlen(dirInfo->Name) + lstrlen(gpi.Item->FileName) + 4];
      if (FullName) {
        lstrcpy(FullName, dirInfo->Name);
        int len = lstrlen(FullName);
        if (FullName[len - 1] != L'\\')
          *(__int32*)&FullName[len] = (__int32)'\\';
        lstrcat(FullName, gpi.Item->FileName);

        //MsgItems[1] = FullName;
        //Info.Message(&MainGuid, NULL, FMSG_LEFTALIGN|FMSG_MB_OK, NULL, MsgItems, ARRAYSIZE(MsgItems), 0);

        unsigned long tmp, size = GetFileVersionInfoSize(FullName, &tmp);
        if (!size)
          ShowError(L"GetFileVersionInfoSize");
        else {
          /*MsgItems[1] = new wchar_t[50];
          wsprintf((wchar_t*)MsgItems[1], L"GetFileVersionInfoSize %08X", size);
          Info.Message(&MainGuid, NULL, FMSG_LEFTALIGN|FMSG_MB_OK, NULL, MsgItems, ARRAYSIZE(MsgItems), 0);
          delete[] MsgItems[1];*/

          void* pVer = malloc(size);
          if (pVer) {
            if (!GetFileVersionInfo(FullName, 0, size, pVer))
              ShowError(L"GetFileVersionInfo");
            else
              parseVerInfo((VS_VERSIONINFO*)pVer, size);
            free(pVer);
          }
        }
        delete[] FullName;
      }
      free(gpi.Item);
    }
    free(dirInfo);
  }
  return NULL;
}