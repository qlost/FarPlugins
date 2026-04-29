#include <windows.h>
#include "guid.hpp"
#include "stuff.h"

Options Opt;
PluginStartupInfo PsInfo;
FarStandardFunctions FSF;
HANDLE hRegexpSize = nullptr
     , hRegexpMem = nullptr
     , hRegexpPart1 = nullptr
     , hRegexpPart2 = nullptr
     , hRegexpFile = nullptr;

BOOL ExecuteCommandLine(const wchar_t *command, const wchar_t *path, const wchar_t *parameters, bool wait)
{
  SHELLEXECUTEINFO ShExecInfo = { 0 };
  ShExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
  ShExecInfo.fMask = SEE_MASK_DOENVSUBST | (wait ? SEE_MASK_NOCLOSEPROCESS : SEE_MASK_FLAG_NO_UI);
  ShExecInfo.hwnd = nullptr;
  ShExecInfo.lpVerb = nullptr;
  ShExecInfo.lpFile = command;
  ShExecInfo.lpParameters = parameters;
  ShExecInfo.lpDirectory = path;
  ShExecInfo.nShow = SW_HIDE;
  ShExecInfo.hInstApp = nullptr;

  if (ShellExecuteEx(&ShExecInfo))
  {
    if (!wait)
      return TRUE;

    WaitForSingleObject(ShExecInfo.hProcess, INFINITE);
    CloseHandle(ShExecInfo.hProcess);
    return TRUE;
  }

  return FALSE;
}

string& EscapeCommand(string &cmd, bool quoted)
{
  if (quoted) {
    cmd.Replace(L"\\", L"\\\\");
    cmd.Replace(L"\"", L"\\\"");
    cmd.Replace(L"$", L"\\\\\\$");
    cmd.Replace(L"`", L"\\\\\\`");
  }
  else {
    cmd.Replace(L"$", L"\\$");
    cmd.Replace(L"`", L"\\`");
  }
  return cmd;
}

string ConcatPath(string &left, const wchar_t *right)
{
  string path = left;
  if (path[path.Len()-1] != L'/' && path[path.Len()-1] != L'\\') {
    path += L'\0';
    FSF.AddEndSlash((wchar_t*)path.CPtr());
  }
  path += right;
  return path;
}

HANDLE RegexpMake(const wchar_t *regex)
{
  HANDLE hRegex = nullptr;
  if (PsInfo.RegExpControl(nullptr, RECTL_CREATE, 0, (void*)&hRegex))
    PsInfo.RegExpControl(hRegex, RECTL_COMPILE, 0, (void*)regex);
  return hRegex;
}

void RegexpFree(HANDLE hRegex)
{
  if (hRegex)
    PsInfo.RegExpControl(hRegex, RECTL_FREE, 0, nullptr);
}

intptr_t RegExTokenize(wchar_t *str, HANDLE hRegex, RegExpMatch **match, bool set_end)
{
  intptr_t brackets = 0;
  *match = NULL;
  if (hRegex) {
    brackets = PsInfo.RegExpControl(hRegex, RECTL_BRACKETSCOUNT, 0, nullptr);
    if (brackets > 1) {
      *match = new RegExpMatch[brackets];
      if (*match) {
        RegExpSearch search{str, 0, lstrlen(str), *match, brackets, nullptr};

        if (!PsInfo.RegExpControl(hRegex, RECTL_SEARCHEX, 0, (void*)&search)) {
          delete[] *match;
          *match = NULL;
          brackets = 0;
        }
        else if (set_end)
          for (intptr_t i = 0; i < brackets; i++) //конец строки после каждой скобки (т.к. после них всегда есть пробел)
            if ((*match)[i].end > 0)
              str[(*match)[i].end] = L'\0';
      }
      else
        brackets = 0;
    }
    else
      brackets = 0;
  }
  return brackets;
}

UINT64 ParseSizeInfo(wchar_t *s)
{
  RegExpMatch *match;
  static wchar_t suffix[] = L"KMGTP";
  UINT64 size = 0ULL;

  if (RegExTokenize(s, hRegexpSize, &match, false) && match[1].start >= 0) {
    size = FSF.atoi64(s + match[1].start);
    if (match[2].start >= 0) {
      unsigned shift = 10;
      for (wchar_t *p = suffix; *p; p++) {
        if (wcschr(s + match[2].start, *p) || wcschr(s + match[2].start, (*p)|0x20)) {
          size <<= shift;
          break;
        }
        shift += 10;
      }
    }
  }
  return size;
}

void CopyMatch(string &dst, wchar_t *src, RegExpMatch &m)
{
  if (m.start >= 0)
    dst.Copy(src + m.start, m.end - m.start);
  else
    dst.Clear();
}

wchar_t* GetDeviceName(wchar_t *device)
{
  return wcstok(device, L"\t");
}

unsigned short GetMonth(const wchar_t *sMonth)
{
  static const wchar_t* months[12] = {L"Jan", L"Feb", L"Mar", L"Apr", L"May", L"Jun", L"Jul", L"Aug", L"Sep", L"Oct", L"Nov", L"Dec"};
  for (unsigned short i = 0; i < 12; i++)
    if (!lstrcmp(sMonth, months[i]))
      return i+1;
  return 0;
}

// Преобразование числа в 8-ричную строку максимум из maxlen символов (\0 не входит)
void i2octal(uintptr_t v, wchar_t *buf, unsigned maxlen)
{
  wchar_t *p = buf+maxlen;
  *p = L'\0';
  while (p > buf) {
    *(--p) = (v % 8) + L'0';
    v /= 8;
  }
}

uintptr_t StringToMode(const wchar_t *sAttr)
{
  if (lstrlen(sAttr) != 10)
    return -1;

  uintptr_t p = 0;
  switch (sAttr[0])
  {
  case 'd': //directory
    p |= S_IFDIR;
    break;
  case '-': //file
    p |= S_IFREG;
    break;
  case 'p': //FIFO
    p |= S_IFIFO;
    break;
  case 'c': //character device
    p |= S_IFCHR;
    break;
  case 'l': //symlink
    p |= S_IFLNK;
    break;
  case 'b': //block
    p |= S_IFBLK;
    break;
  case 's': //socket
    p |= S_IFSOCK;
    break;
  default:
    return -1;
  }

  if (sAttr[1] == 'r') p |= S_IRUSR;
  if (sAttr[2] == 'w') p |= S_IWUSR;
  if (sAttr[3] == 'x' || sAttr[3] == 's') p |= S_IXUSR;
  if (sAttr[3] == 's' || sAttr[3] == 'S') p |= S_ISUID;

  if (sAttr[4] == 'r') p |= S_IRGRP;
  if (sAttr[5] == 'w') p |= S_IWGRP;
  if (sAttr[6] == 'x' || sAttr[6] == 's') p |= S_IXGRP;
  if (sAttr[6] == 's' || sAttr[6] == 'S') p |= S_ISGID;

  if (sAttr[7] == 'r') p |= S_IROTH;
  if (sAttr[8] == 'w') p |= S_IWOTH;
  if (sAttr[9] == 'x' || sAttr[9] == 't') p |= S_IXOTH;
  if (sAttr[9] == 't' || sAttr[9] == 'T') p |= S_ISVTX;

  return p;
}

unsigned StringOctalToMode(const wchar_t *octal)
{
  unsigned x = 0;
  for (const wchar_t *p = octal; *p; p++) {
    x <<= 3;
    if (*p >= L'0' && *p <= L'9')
      x |= *p - L'0';
  }
  return x;
}

uintptr_t ModeToAttr(const UINT64 p)
{
  if (p == -1)
    return FILE_ATTRIBUTE_OFFLINE;

  uintptr_t res = IS_FLAG(p, S_IWRITE) ? 0 : FILE_ATTRIBUTE_READONLY;
  if (IS_FLAG(p, S_IFSOCK))
    res |= FILE_ATTRIBUTE_DEVICE;
  else if (IS_FLAG(p, S_IFLNK))
    res |= FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT;
  else if (IS_FLAG(p, S_IFREG))
    res |= FILE_ATTRIBUTE_ARCHIVE;
  else if (IS_FLAG(p, S_IFBLK))
    res |= FILE_ATTRIBUTE_DEVICE;
  else if (IS_FLAG(p, S_IFDIR))
    res |= FILE_ATTRIBUTE_DIRECTORY;
  else if (IS_FLAG(p, S_IFCHR))
    res |= FILE_ATTRIBUTE_DEVICE;
  else if (IS_FLAG(p, S_IFIFO))
    res |= FILE_ATTRIBUTE_DEVICE;
  return res;
}

const wchar_t* ModeToType(const UINT64 p)
{
  if (IS_FLAG(p, S_IFSOCK))
    return L"Socket";
  if (IS_FLAG(p, S_IFLNK))
    return L"Symbolic Link";
  if (IS_FLAG(p, S_IFREG))
    return L"File";
  if (IS_FLAG(p, S_IFBLK))
    return L"Block Device";
  if (IS_FLAG(p, S_IFDIR))
    return L"Directory";
  if (IS_FLAG(p, S_IFCHR))
    return L"Character Device";
  if (IS_FLAG(p, S_IFIFO))
    return L"FIFO";
  return L"Unknown";
}

FILETIME UnixTimeToFileTime(time_t time)
{
  time += EPOCH_DIFFERENCE;
  time *= TICKS_PER_SECOND;
  return *(FILETIME*)(&time);
}

time_t FileTimeToUnixTime(FILETIME ft)
{
  return *(UINT64*)(&ft) / TICKS_PER_SECOND - EPOCH_DIFFERENCE;
}

const wchar_t* GetMsg(int MsgId)
{
  return PsInfo.GetMsg(&MainGuid, MsgId);
}

void SetItemText(FarMenuItem *item, const string &text)
{
  delete[] item->Text;
  item->Text = new wchar_t[text.Len() + 1];
  lstrcpy((wchar_t*)item->Text, text.CPtr());
}

PluginPanelItem* GetSelectedPanelItem(unsigned i)
{
  size_t size = PsInfo.PanelControl(PANEL_ACTIVE, FCTL_GETSELECTEDPANELITEM, i, nullptr);
  if (size > 0)
  {
    struct FarGetPluginPanelItem item{sizeof(FarGetPluginPanelItem), size, (PluginPanelItem*)malloc(size)};
    PsInfo.PanelControl(PANEL_ACTIVE, FCTL_GETSELECTEDPANELITEM, i, &item);
    return item.Item;
  }
  else
    return NULL;
}

PluginPanelItem* GetCurrentPanelItem()
{
  size_t size = PsInfo.PanelControl(PANEL_ACTIVE, FCTL_GETCURRENTPANELITEM, 0, nullptr);
  if (size > 0)
  {
    struct FarGetPluginPanelItem item{sizeof(FarGetPluginPanelItem), size, (PluginPanelItem*)malloc(size)};
    PsInfo.PanelControl(PANEL_ACTIVE, FCTL_GETCURRENTPANELITEM, 0, &item);
    return item.Item;
  }
  else
    return NULL;
}

string GetCurrentFileName()
{
  PluginPanelItem *item = GetCurrentPanelItem();
  string ret;
  if (item) {
    ret = item->FileName;
    free(item);
  }
  return ret;
}

string GetSysError(bool is_sock) {
  wchar_t *sError;
  FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_MAX_WIDTH_MASK, NULL, is_sock ? WSAGetLastError() : GetLastError(), 0, (wchar_t*)&sError, 0, NULL);
  string s = sError;
  LocalFree(sError);
  return s;
}

const char *g_log_file_name = ".\\.fardroid.log";

void DeleteLog()
{
  DeleteFileA(g_log_file_name);
}

void DebugLog(const wchar_t *str)
{
  DWORD size;
  HANDLE f = CreateFileA(g_log_file_name, GENERIC_WRITE, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  SetFilePointer(f, 0, NULL, FILE_END);

  SYSTEMTIME st;
  char stime[25];
  GetLocalTime(&st);
  int len = wsprintfA(stime, "%04d-%02d-%02d %02d:%02d:%02d.%03d\t", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
  WriteFile(f, stime, len, &size, NULL);

  len = WideCharToMultiByte(CP_UTF8, 0, str, -1, NULL, 0, NULL, NULL);
  char *s = (char*)malloc(len);
  WideCharToMultiByte(CP_UTF8, 0, str, -1, s, len, NULL, NULL);
  WriteFile(f, s, len-1, &size, NULL);
  WriteFile(f, "\n", 1, &size, NULL);
  CloseHandle(f);
  free(s);
}

void DebugBuf(void *s, DWORD sz)
{
  DWORD size;
  HANDLE f = CreateFileA(g_log_file_name, GENERIC_WRITE, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  SetFilePointer(f, 0, NULL, FILE_END);
  WriteFile(f, s, sz ? sz : lstrlenA((char*)s), &size, NULL);
  CloseHandle(f);
}
