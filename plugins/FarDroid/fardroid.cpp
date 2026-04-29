#include "fardroid.h"
#include "guid.hpp"
#include "lang.hpp"
#include <DlgBuilder.hpp>
#include "framebuffer.h"

int IDPRM_Octal, IDPRM_All, IDPRM_None, IDPRM_Min, IDPRM_Max, IDPRM_Bit[12];
uintptr_t prm;

// Sockets

void Socket::CreateADBSocket()
{FUNCTION
  sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock)
  {
    sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(5037);
    dest_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (connect(sock, (sockaddr*)&dest_addr, sizeof(dest_addr)) == 0)
      return;
    closesocket(sock);
  }
  sock = 0;
}

void Socket::CloseADBSocket()
{FUNCTION
  if (sock) {
    closesocket(sock);
    sock = 0;
  }
}

bool Socket::SendADBPacket(void *packet, int size)
{
  if (!sock)
    return false;

  char *p = (char*)packet;
  while (size > 0)
  {
    int r = send(sock, p, size, 0);
    if (r > 0)
    {
      DEBUGBUF(p, r);
      size -= r;
      p += r;
    }
    else if (r < 0) return false;
    else if (r == 0) return true;
  }
  return true;
}

int Socket::ReadADBPacket(void *packet, int size)
{
  char *p = (char*)packet;
  int received = 0;

  while (size > 0)
  {
    int r = recv(sock, p, size, 0);
    if (r > 0)
    {
      DEBUGBUF(p, r);
      received += r;
      size -= r;
      p += r;
    }
    else if (r == 0) break;
    else return r;
  }

  return received;
}

bool Socket::SendADBCommand(string &cmd)
{FUNCTION
  char buf[5], *s = cmd.toUTF8();
  if (!s)
    return false;
  unsigned size = (unsigned)cmd.UTFLen();
  wsprintfA(buf, "%04X", size);
  SendADBPacket(buf, 4);
  SendADBPacket(s, size);
  DEBUGNL();
  long msg;
  ReadADBPacket(&msg, sizeof(msg));
  DEBUGNL();
  return msg == ID_OKAY;
}

void Socket::PrepareADBSocket()
{FUNCTION
  int lastError = S_OK;
  CreateADBSocket();
  if (sock)
  {
    if (android->currentDevice.IsEmpty())
    {
      string cmd = L"host:devices";
      if (SendADBCommand(cmd))
      {
        string devices;
        unsigned tmp;
        ReadADBPacket(&tmp, 4);
        DEBUGNL();
        if (tmp == ID_0000) {
          lastError = ERROR_DEV_NOT_EXIST;
          CloseADBSocket();
        }
        else {
          char buf[4097];
          while (true)
          {
            int len = ReadADBPacket(buf, 4096);
            if (len <= 0)
              break;

            buf[len] = 0;
            devices += string(buf, CP_UTF8);
          }
          CloseADBSocket();

          switch (android->DeviceMenu(devices))
          {
          case TRUE:
            PrepareADBSocket();
            return;
          case ABORT:
            lastError = S_OK;
            break;
          default:
            lastError = ERROR_DEV_NOT_EXIST;
          }
        }
      }
      else //host:devices failed
      {
        CloseADBSocket();
        lastError = ERROR_DEV_NOT_EXIST;
      }
    }
    else //currentDevice не пустой
    {
      string cmd = L"host:transport:";
      cmd += android->currentDevice;
      if (!SendADBCommand(cmd))
      {
        CloseADBSocket();
        lastError = ERROR_DEV_NOT_EXIST;
      }
    }
  }
  else //CreateADBSocket failed
  {
    if (android->handleAdbServer == FALSE)
    {
      android->handleAdbServer = ExecuteCommandLine(L"adb.exe", Opt.ADBPath, L"start-server", true);
      if (android->handleAdbServer) {
        PrepareADBSocket();
        return;
      }
      android->handleAdbServer = ABORT;
    }
    else
    {
      lastError = ERROR_DEV_NOT_EXIST;
    }
  }

  if (lastError == ERROR_DEV_NOT_EXIST)
  {
    const wchar_t *msg[]{GetMsg(MTitle), GetMsg(MDeviceNotFound)};
    PsInfo.Message(&MainGuid, &MsgGuid, FMSG_WARNING | FMSG_MB_OK, NULL, msg, _ARRAYSIZE(msg), 0);
  }
}

bool Socket::ADBShellExecute(string &cmd, string &sRes)
{FUNCTION
  DEBUGLOG(cmd.CPtr());
  bool bOK = false;
  if (Opt.SU && Opt.SU0)
    cmd.Format(L"shell:su 0 %s", EscapeCommand(cmd).CPtr());
  else if (Opt.SU)
    cmd.Format(L"shell:su -c \"%s\"", EscapeCommand(cmd, true).CPtr());
  else
    cmd.Format(L"shell:%s", EscapeCommand(cmd).CPtr());

  if (SendADBCommand(cmd))
  {
    char buf[4097];
    while (true)
    {
      int len = ReadADBPacket(buf, _ARRAYSIZE(buf)-1);
      if (len <= 0)
        break;

      buf[len] = 0;
      sRes += string(buf, CP_UTF8);//TODO потенциальная проблема, если принятый пакет обрывается на середине многобайтового кода буквы
      //Нужно сохранять целиком в char*, а потом преобразовывать в wchar_t*
      //И где-то здесь же заменять A0 на C2,A0
    }
    sRes.Replace(L"\\ ", L" ").Replace(L"\\\\", L"\\").Replace(L"\r", L"");
    bOK = true;
  }
  return bOK;
}

void Socket::ADBSyncQuit()
{FUNCTION
  syncmsg msg;
  msg.req.id = ID_QUIT;
  msg.req.namelen = 0;
  SendADBPacket(&msg.req, sizeof(msg.req));
  DEBUGNL();
}

void Socket::ReadError(unsigned id, unsigned len, string &sRes)
{
  if (id != ID_FAIL)
    sRes = L"unknown reason";
  else if (len > 0) {
    char *buf = new char[len+1];
    len = ReadADBPacket(buf, len);
    if (len > 0) {
      buf[len] = '\0';
      sRes.fromChar(buf, CP_UTF8);
    }
    delete[] buf;
  }
  DEBUGNL();
}

bool Socket::ADBPullFile(string &sSrc, const wchar_t *sDst, string &sRes, const CCopyRecord *rec)
{FUNCTION
  syncmsg msg;
  HANDLE hFile = NULL;
  char *s = sSrc.toUTF8();
  if (sSrc.UTFLen() > 1024)
    return false;

  bool result = true;
  msg.req.id = ID_RECV;
  msg.req.namelen = (unsigned)sSrc.UTFLen();
  if (SendADBPacket(&msg.req, sizeof(msg.req)) && SendADBPacket(s, (int)sSrc.UTFLen())) {
    DEBUGNL();
    char *buffer = new char[SYNC_DATA_MAX];
    DWORD written;
    while (true) {
      int ret = ReadADBPacket(&msg.data, sizeof(msg.data));
      if (ret == 0 || msg.data.size > SYNC_DATA_MAX)
        break;
      else if (ret < 0) {
        sRes = GetSysError(true);
        result = false;
        break;
      }
      else if (msg.data.id != ID_DATA && msg.data.id != ID_DONE) {
        ReadError(msg.data.id, msg.data.size, sRes);
        result = false;
        break;
      }
      else {
        if (!hFile) {
          hFile = CreateFile(sDst, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
          if (hFile == INVALID_HANDLE_VALUE) {
            sRes = GetSysError(false);
            result = false;
            break;
          }
          SetFileTime(hFile, &rec->ctime, &rec->atime, &rec->mtime);
        }
        if (msg.data.id == ID_DATA) {
          ret = ReadADBPacket(buffer, msg.data.size);
          if (ret < 0) {
            sRes = GetSysError(true);
            result = false;
            break;
          }
          WriteFile(hFile, buffer, ret, &written, NULL);
          android->procStruct.data[PT_ONE].current += written;
          android->procStruct.data[PT_ALL].current += written;
          if (android->CheckForEsc()) {
            result = false;
            break;
          }
          android->ShowProgressMessage();
        }
        else //ID_DONE?
          break;
      }//read > 0
    }//while
    delete[] buffer;
  }
  else
    result = false;
  DEBUGNL();
  if (hFile)
    CloseHandle(hFile);
  return result;
}

bool Socket::ADBPushFile(const wchar_t *sSrc, string &sDst, string &sRes, unsigned mode)
{FUNCTION
  if (sDst.Len() > 1024)
    return false;

  bool result = true;
  string sData;
  sData.Format(L"%s,%u", sDst.CPtr(), mode);
  char *buf = sData.toUTF8();
  syncmsg msg;
  msg.req.id = ID_SEND;
  msg.req.namelen = (unsigned)sData.UTFLen();
  if (SendADBPacket(&msg.req, sizeof(msg.req)) && SendADBPacket(buf, msg.req.namelen)) {
    DEBUGNL();
    HANDLE hFile = CreateFile(sSrc, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
      return false;

    FILETIME ft;
    GetFileTime(hFile, nullptr, nullptr, &ft);
    time_t mtime = FileTimeToUnixTime(ft);

    syncsendbuf *sbuf = new syncsendbuf;
    sbuf->id = ID_DATA;
    while (true) {
      sbuf->size = 0;
      if (!ReadFile(hFile, sbuf->data, SYNC_DATA_MAX, (DWORD*)&sbuf->size, nullptr)) {
        result = false;
        break;
      }
      if (sbuf->size == 0)
        break;
      if (!SendADBPacket(sbuf, sizeof(unsigned) * 2 + sbuf->size)) {
        result = false;
        break;
      }
      android->procStruct.data[PT_ONE].current += sbuf->size;
      android->procStruct.data[PT_ALL].current += sbuf->size;
      if (android->CheckForEsc()) {
        result = false;
        break;
      }
      android->ShowProgressMessage();
    }
    delete sbuf;
    CloseHandle(hFile);
    DEBUGNL();

    if (result) {
      msg.data.id = ID_DONE;
      msg.data.size = (unsigned)mtime;
      if (
        SendADBPacket(&msg.data, sizeof(msg.data)) &&
        ReadADBPacket(&msg.status, sizeof(msg.status)) > 0
      ) {
        if (msg.status.id != ID_OKAY) {
          ReadError(msg.status.id, msg.status.msglen, sRes);
          result = false;
        }
      }
      else
        result = false;
    }
  }
  else
    result = false;
  DEBUGNL();
  return result;
}

Socket::Socket(fardroid *a): sock(0)
{FUNCTION
  android = a;
  PrepareADBSocket();
}

Socket::~Socket()
{FUNCTION
  CloseADBSocket();
}

// Android

fardroid::fardroid(): handleAdbServer(FALSE), InfoPanelLineArray(nullptr)
{FUNCTION
  currentPath = L"/";
  currentDevice = L"";
  hConInp = GetStdHandle(STD_INPUT_HANDLE);
  WSADATA wsaData;
  WSAStartup(MAKEWORD(2, 2), &wsaData);
}

fardroid::~fardroid()
{FUNCTION
  PluginSettings settings(MainGuid, PsInfo.SettingsControl);
  settings.Set(settings.OpenSubKey(0, L"devices"), currentDevice, currentPath);

  if (Opt.KillServer && handleAdbServer == TRUE)
    ExecuteCommandLine(L"adb.exe", Opt.ADBPath, L"kill-server", false);

  delete[] InfoPanelLineArray;
  WSACleanup();
}

bool fardroid::ReadFileList(string &sFileList, bool bSilent, CFileRecords &recs)
{
  RegExpMatch *match;
  wchar_t *p = (wchar_t*)sFileList.CPtr(), *sLine;
  while (true) {
    sLine = wcstok(p, L"\n");
    if (!sLine)
      break;
    p = NULL;
    unsigned len = lstrlen(sLine);
    if (
      *sLine &&
      wcsncmp(sLine, L"total", 5) &&
      wcsncmp(sLine, L"ls:", 3) &&
      lstrcmp(sLine + len - 3, L" ..") &&
      lstrcmp(sLine + len - 2, L" .")
    ) {
      bool isLink = sLine[0] == L'l';
      bool isDevice = sLine[0] == L'b' || sLine[0] == L'c' || sLine[0] == L's';

      if (RegExTokenize(sLine, hRegexpFile, &match, true)) {
        CFileRecord *rec = new CFileRecord;
        CopyMatch(rec->filename, sLine, match[17]);
        CopyMatch(rec->owner, sLine, match[2]);
        CopyMatch(rec->grp, sLine, match[3]);
        CopyMatch(rec->linkto, sLine, match[19]);
        if (isLink)
          CopyMatch(rec->desc, sLine, match[18]);
        else if (isDevice)
          CopyMatch(rec->desc, sLine, match[4]);
        else
          rec->desc.Clear();
        rec->size = isDevice || (match[4].start == -1) ? 0 : FSF.atoi64(sLine + match[4].start);

        if (match[1].start >= 0)
          rec->mode = StringToMode(&sLine[match[1].start]);

        SYSTEMTIME st{0};
        if (match[5].start > 0) { //2026-03-18 08:38
          st.wYear = (unsigned short)FSF.atoi(sLine + match[5].start);
          st.wMonth = (unsigned short)FSF.atoi(sLine + match[6].start);
          st.wDay = (unsigned short)FSF.atoi(sLine + match[7].start);
          st.wHour = (unsigned short)FSF.atoi(sLine + match[8].start);
          st.wMinute = (unsigned short)FSF.atoi(sLine + match[9].start);
        }
        else if (match[10].start > 0) { //Mar 22 2026
          st.wYear = (unsigned short)FSF.atoi(sLine + match[12].start);
          st.wMonth = (unsigned short)GetMonth(sLine + match[10].start);
          st.wDay = (unsigned short)FSF.atoi(sLine + match[11].start);
        }
        else if (match[13].start > 0) { //Jan 4 05:19
          GetLocalTime(&st);
          WORD mon = st.wMonth;
          st.wMonth = (unsigned short)GetMonth(sLine + match[13].start);
          st.wDay = (unsigned short)FSF.atoi(sLine + match[14].start);
          st.wHour = (unsigned short)FSF.atoi(sLine + match[15].start);
          st.wMinute = (unsigned short)FSF.atoi(sLine + match[16].start);
          st.wSecond = 0;
          st.wMilliseconds = 0;
          if (st.wMonth > mon)
            st.wYear--;
        }
        if (st.wMonth) {
          SystemTimeToFileTime(&st, &rec->ctime);
          rec->mtime = rec->atime = rec->ctime;
        }

        recs.Add(rec);
        delete[] match;
      }
    }
  }
  return true;
}

bool fardroid::ADB_ls(const wchar_t *sDir, bool bSilent, CFileRecords &recs)
{FUNCTION
  bool ret = false;
  Socket sock(this);
  recs.RemoveAll();
  if (Opt.WorkMode != WORKMODE_SAFE)
  {
    string cmd, sRes;
    switch (Opt.WorkMode)
    {
    case WORKMODE_NATIVE:
      cmd.Format(L"ls -la%s \"%s/\"", Opt.UseLS_N ? L"N" : L"", sDir);
      break;
    case WORKMODE_BUSYBOX:
      cmd.Format(L"busybox ls -la%s%s --color=never \"%s\"", Opt.UseLS_N ? L"N" : L"", Opt.ShowLinksAsDir ? L"" : L"L", sDir);
      break;
    }
    if (sock.ADBShellExecute(cmd, sRes))
      return ReadFileList(sRes, bSilent, recs);
  }
  else
  {
    string cmd = L"sync:";
    if (sock.SendADBCommand(cmd))
    {
      syncmsg msg;
      char buf[257];
      string file = sDir;
      char *dir = file.toUTF8();
      msg.req.id = Opt.UseLIS2 ? ID_LIS2 : ID_LIST;
      msg.req.namelen = (unsigned)file.UTFLen();
      if (msg.req.namelen > 1024)
        return false;

      if (sock.SendADBPacket(&msg.req, sizeof(msg.req)) && sock.SendADBPacket(dir, msg.req.namelen))
      {
        while (true) {
          DEBUGNL();
          if (Opt.UseLIS2) {
            if (sock.ReadADBPacket(&msg.dnt2, sizeof(msg.dnt2)) <= 0)
              break;
            if (msg.dnt2.id == ID_DONE) {
              ret = true;
              break;
            }
            if (msg.dnt2.id != ID_DNT2 || msg.dnt2.namelen > 256 || sock.ReadADBPacket(buf, msg.dnt2.namelen) <= 0)
              break;
            buf[msg.dnt2.namelen] = '\0';
          }
          else {
            if (sock.ReadADBPacket(&msg.dent, sizeof(msg.dent)) <= 0)
              break;
            if (msg.dent.id == ID_DONE) {
              ret = true;
              break;
            }
            if (msg.dent.id != ID_DENT || msg.dent.namelen > 256 || sock.ReadADBPacket(buf, msg.dent.namelen) <= 0)
              break;
            buf[msg.dent.namelen] = '\0';
          }

          if (lstrcmpA(buf, ".") != 0 && lstrcmpA(buf, "..") != 0)
          {
            CFileRecord *rec = new CFileRecord;
            rec->filename.fromChar(buf, CP_UTF8);
            if (Opt.UseLIS2) {
              rec->mode = msg.dnt2.mode;
              rec->size = msg.dnt2.size;
              rec->ctime = UnixTimeToFileTime(msg.dnt2.ctime);
              rec->mtime = UnixTimeToFileTime(msg.dnt2.mtime);
              rec->atime = UnixTimeToFileTime(msg.dnt2.atime);
            }
            else {
              rec->mode = msg.dent.mode;
              rec->size = msg.dent.size;
              rec->ctime = rec->mtime = rec->atime = UnixTimeToFileTime(msg.dent.time);
            }
            recs.Add(rec);
          }
        }//while
      }//send
      DEBUGNL();
    }
  }
  return ret;
}

bool fardroid::ADB_rm(const wchar_t *sDir, string &sRes)
{FUNCTION
  Socket sock(this);
  string s;
  s.Format(L"rm -rf \"%s\"", sDir);
  return sock && sock.ADBShellExecute(s, sRes) && !sRes.Len();
}

bool fardroid::ADB_chmod(const wchar_t *sSrc, const wchar_t *octal, string &sRes)
{FUNCTION
  Socket sock(this);
  string s = ConcatPath(currentPath, sSrc);
  s.Format(L"chmod %s \"%s\"", octal, s.CPtr());
  return sock && sock.ADBShellExecute(s, sRes) && !sRes.Len();
}

bool fardroid::ADB_chown(const wchar_t *sSrc, const wchar_t *user, const wchar_t *group, string &sRes)
{FUNCTION
  Socket sock(this);
  string s = ConcatPath(currentPath, sSrc);
  s.Format(L"chown %s:%s \"%s\"", user, group, s.CPtr());
  return sock && sock.ADBShellExecute(s, sRes) && !sRes.Len();
}

bool fardroid::ADB_mount(const wchar_t *sFS, const wchar_t *sMode, string &sRes)
{FUNCTION
  Socket sock(this);
  string s;
  s.Format(L"%smount -o %s,remount,%s %s", Opt.WorkMode == WORKMODE_BUSYBOX ? L"busybox " : L"", sMode, sMode, sFS);
  return sock && sock.ADBShellExecute(s, sRes);
}

bool fardroid::ADB_mkdir(const wchar_t *sDir, string &sRes)
{FUNCTION
  Socket sock(this);
  string s;
  s.Format(L"mkdir -p \"%s\"", sDir);
  return sock && sock.ADBShellExecute(s, sRes) && !sRes.Len();
}

bool fardroid::ADB_rename(const wchar_t *sSrc, const wchar_t *sDst, string &sRes)
{FUNCTION
  Socket sock(this);
  string s;
  s.Format(L"mv \"%s\" \"%s\"", sSrc, sDst);
  return sock && sock.ADBShellExecute(s, sRes) && !sRes.Len();
}

bool fardroid::ADB_copy(const wchar_t *sSrc, const wchar_t *sDst, string &sRes)
{FUNCTION
  Socket sock(this);
  string s;
  s.Format(L"cp \"%s\" \"%s\"", sSrc, sDst);
  return sock && sock.ADBShellExecute(s, sRes) && !sRes.Len();
}

bool fardroid::ADB_pull(string &sSrc, const wchar_t *sDst, string &sRes, const CCopyRecord *rec)
{FUNCTION
  Socket sock(this);
  string cmd = L"sync:";
  if (sock.SendADBCommand(cmd)) {
    bool res = sock.ADBPullFile(sSrc, sDst, sRes, rec);
    sock.ADBSyncQuit();
    return res;
  }
  return false;
}

bool fardroid::ADB_push(const wchar_t *sSrc, string &sDst, string &sRes, unsigned mode)
{FUNCTION
  Socket sock(this);
  string cmd = L"sync:";
  if (sock.SendADBCommand(cmd)) {
    bool res = sock.ADBPushFile(sSrc, sDst, sRes, mode);
    sock.ADBSyncQuit();
    return res;
  }
  return false;
}

unsigned fardroid::ADB_stat(string &sDst)
{FUNCTION
  Socket sock(this);
  syncmsg msg;
  string cmd = L"sync:";
  char *buf = sDst.toUTF8();
  msg.req.id = ID_STAT;
  msg.req.namelen = (unsigned)sDst.UTFLen();
  if (
    !sock.SendADBCommand(cmd) ||
    !sock.SendADBPacket(&msg.req, sizeof(msg.req)) ||
    !sock.SendADBPacket(buf, msg.req.namelen) ||
    sock.ReadADBPacket(&msg.stat, sizeof(msg.stat)) <= 0 ||
    msg.stat.id != ID_STAT
  )
    msg.stat.mode = 0;
  DEBUGNL();
  return msg.stat.mode;
}

// Interface

string fardroid::GetDeviceAliasName(const wchar_t *device)
{
  PluginSettings settings(MainGuid, PsInfo.SettingsControl);
  string s = settings.Get(settings.OpenSubKey(0, L"names"), device, device);
  return s;
}

string fardroid::GetDeviceCaption(wchar_t *device)
{
  string caption;
  wchar_t *name = GetDeviceName(device);
  string alias = GetDeviceAliasName(name);
  if (!lstrcmp(alias.CPtr(), name))
    caption = alias;
  else
    caption.Format(L"%s (%s)", alias.CPtr(), name);
  return caption;
}

void WINAPI fardroid::FreeUserData(void *UserData, const struct FarPanelItemFreeInfo *Info)
{
  free(UserData);
}

bool fardroid::CheckForEsc()
{
	if (hConInp == INVALID_HANDLE_VALUE)
		return false;

	static DWORD dwTicks = 0;
	DWORD dwNewTicks = GetTickCount();
	if (dwNewTicks - dwTicks < 500)
		return false;
	dwTicks = dwNewTicks;

  INPUT_RECORD rec;
  DWORD ReadCount;
  const wchar_t *msg[]{procStruct.title, GetMsg(MBreakWarn)};
  while (PeekConsoleInput(hConInp, &rec, 1, &ReadCount) && ReadCount)
  {
    ReadConsoleInput(hConInp, &rec, 1, &ReadCount);
    if (
      rec.EventType == KEY_EVENT &&
      rec.Event.KeyEvent.wVirtualKeyCode == VK_ESCAPE &&
      rec.Event.KeyEvent.bKeyDown &&
      PsInfo.Message(&MainGuid, &MsgGuid, FMSG_WARNING | FMSG_MB_YESNO, NULL, msg, _ARRAYSIZE(msg), 0) == 0
    )
      return true;
  }
  return false;
}

void fardroid::DeviceNameDialog(const wchar_t *name)
{
  wchar_t editbuf[100];
  lstrcpyW(editbuf, GetDeviceAliasName(name).CPtr());
  if (PsInfo.InputBox(&MainGuid, nullptr, name, GetMsg(MRenameDeviceName), L"fardroidDevice", editbuf, editbuf, _ARRAYSIZE(editbuf), NULL, FIB_NONE))
  {
    PluginSettings settings(MainGuid, PsInfo.SettingsControl);
    settings.Set(settings.OpenSubKey(0, L"names"), name, editbuf);
  }
}

void fardroid::DeviceNameDialog()
{
  DeviceNameDialog(currentDevice);
}

void fardroid::CheckCapabilities()
{FUNCTION
  string sRes, cmd = L"ls -Nla";
  {
    Socket sock(this);
    Opt.UseLS_N = false;
    if (sock && sock.ADBShellExecute(cmd, sRes)) {
      if (sRes.startsWith(L"total")) //хотя бы не ошибка?
        Opt.UseLS_N = true;
      else { //но встречаются случаи без total в начале
        wchar_t *sLine = wcstok((wchar_t*)sRes.CPtr(), L"\n");
        if (sLine && *sLine) {
          RegExpSearch search{sLine, 0, lstrlen(sLine), NULL, 0, nullptr};
          if (PsInfo.RegExpControl(hRegexpFile, RECTL_MATCHEX, 0, (void*)&search)) //первая строка сматчилась по файловому регэкспу?
            Opt.UseLS_N = true;
        }
      }
    }
  }

  {
    Socket sock(this);
    sRes.Clear();
    cmd = L"sync:";
    if (sock.SendADBCommand(cmd))
    {
      syncmsg msg;
      msg.req.id = ID_LIS2;
      msg.req.namelen = 1;
      if (sock.SendADBPacket(&msg.req, sizeof(msg.req)) && sock.SendADBPacket((void*)"/", msg.req.namelen))
      {
        DEBUGNL();
        int ret = sock.ReadADBPacket(&msg.data, sizeof(msg.data));
        if (ret <= 0)
          Opt.UseLIS2 = false;
        else if (msg.data.id != ID_DNT2) {
          sock.ReadError(msg.data.id, msg.data.size, sRes);
          Opt.UseLIS2 = false;
        }
        else
          Opt.UseLIS2 = true;
      }
    }
  }
  sRes.Format(L"UseLS_N=%u  UseLIS2=%u", Opt.UseLS_N, Opt.UseLIS2);
  DEBUGLOG(sRes.CPtr());
}

bool fardroid::GetDeviceInfo()
{FUNCTION
  Socket sock(this);
  string sRes, cmd = L"getprop";
  if (sock && sock.ADBShellExecute(cmd, sRes)) {
    CPanelLine *pl = new CPanelLine;
    pl->separator = false;
    wchar_t *p = (wchar_t*)sRes.CPtr(), *sLine;
    while (true) {
      sLine = wcstok(p, L"\n");
      if (!sLine)
        break;
      p = NULL;
      if (!wcsncmp(sLine, L"[ro.product.manufacturer]", 25))
        pl->text.Insert(0, sLine+28, lstrlen(sLine+28)-1);
      else if (!wcsncmp(sLine, L"[ro.product.model]", 18)) {
        pl->text += L' ';
        pl->text.Append(sLine+21, lstrlen(sLine+21)-1);
      }
      else if (!wcsncmp(sLine, L"[ro.build.version.release]", 26))
        pl->data.Copy(sLine+29, lstrlen(sLine+29)-1);
    }
    lines.Add(pl);
    return true;
  }
  else
    return false;
}

bool fardroid::GetMemoryInfo()
{FUNCTION
  Socket sock(this);
  string sRes, cmd = L"cat /proc/meminfo";
  if (sock && sock.ADBShellExecute(cmd, sRes)) {
    unsigned cnt = 0;
    CPanelLine *pl;
    RegExpMatch *match;
    wchar_t *p = (wchar_t*)sRes.CPtr(), *sLine;
    while (cnt < 3) {
      sLine = wcstok(p, L"\n");
      if (!sLine)
        break;
      p = NULL;

      if (RegExTokenize(sLine, hRegexpMem, &match, true)) {
        if (!cnt)
          lines.Add(new CPanelLine{{}, GetMsg(MMemoryInfo), true});
        cnt++;
        pl = new CPanelLine;
        pl->separator = false;
        CopyMatch(pl->text, sLine, match[1]);
        wchar_t sMem[13];
        FSF.FormatFileSize(((match[2].start >= 0) ? ParseSizeInfo(sLine + match[2].start) : 0), 12, FFFS_FLOATSIZE|FFFS_MINSIZEINDEX, sMem, _ARRAYSIZE(sMem));
        pl->data = sMem;
        lines.Add(pl);
        delete[] match;
      }
    }
    return true;
  }
  else
    return false;
}

void fardroid::ParsePartitionInfo(wchar_t *sLine)
{
  RegExpMatch *match;
  wchar_t *path;
  unsigned long long total = 0, free = 0, used = 0;

  if (RegExTokenize(sLine, hRegexpPart1, &match, true) && (match[1].start >= 0) && !wcschr(sLine + match[1].start, L'@')) {
    path = sLine + match[1].start;
    total = (match[2].start >= 0) ? ParseSizeInfo(sLine + match[2].start) : 0;
    free = (match[3].start >= 0) ? ParseSizeInfo(sLine + match[3].start) : 0;
    used = total - free;
  }
  else if (RegExTokenize(sLine, hRegexpPart2, &match, true)) {
    if ((match[5].start >= 0) && (sLine[match[5].start] == L'/'))
    {
      if (!wcschr(sLine + match[5].start, L'@')) {
        path = sLine + match[5].start;
        total = (match[2].start >= 0) ? ParseSizeInfo(sLine + match[2].start) * 1024ULL : 0;
        used = (match[3].start >= 0) ? ParseSizeInfo(sLine + match[3].start) * 1024ULL : 0;
        free = (match[4].start >= 0) ? ParseSizeInfo(sLine + match[4].start) * 1024ULL : 0;
      }
      else
        path = NULL;
    }
    else if ((match[1].start >= 0) && !wcschr(sLine + match[1].start, L'@'))
    {
      path = sLine + match[1].start;
      total = (match[2].start >= 0) ? ParseSizeInfo(sLine + match[2].start) : 0;
      used = (match[3].start >= 0) ? ParseSizeInfo(sLine + match[3].start) : 0;
      free = (match[4].start >= 0) ? ParseSizeInfo(sLine + match[4].start) : 0;
    }
    else
      path = NULL;
  }
  else
    path = NULL;
  delete[] match;

  if (path) {
    wchar_t sTotal[12], sUsed[12], sFree[12];
    FSF.FormatFileSize(total, 9, FFFS_FLOATSIZE|FFFS_ECONOMIC|FFFS_MINSIZEINDEX, sTotal, _ARRAYSIZE(sTotal));
    FSF.FormatFileSize(used, 9, FFFS_FLOATSIZE|FFFS_ECONOMIC|FFFS_MINSIZEINDEX, sUsed, _ARRAYSIZE(sUsed));
    FSF.FormatFileSize(free, 9, FFFS_FLOATSIZE|FFFS_ECONOMIC|FFFS_MINSIZEINDEX, sFree, _ARRAYSIZE(sFree));
    CPanelLine *pl = new CPanelLine;
    pl->text = path;
    pl->data = sTotal;
    pl->data += sUsed;
    pl->data += sFree;
    pl->separator = false;
    lines.Add(pl);

    infoSize.Add(new CInfoSize{path, total, used, free});
    if (wcsstr(path, L"emulated")) {
      infoSize.Add(new CInfoSize{L"/sdcard", total, used, free});
      infoSize.Add(new CInfoSize{L"/mnt/sdcard", total, used, free});
      //TODO добавить /storage, но проверить как это работает с физической sdcard
    }
  }
}

void fardroid::GetPartitionsInfo()
{FUNCTION
  Socket sock(this);
  string sRes, cmd = L"df";
  if (sock && sock.ADBShellExecute(cmd, sRes)) {
    lines.Add(new CPanelLine{{}, GetMsg(MPartitionsInfo), true});
    lines.Add(new CPanelLine{L"Total     Used     Free", {}, false});

    wchar_t *p = (wchar_t*)sRes.CPtr(), *sLine;
    while (true) {
      sLine = wcstok(p, L"\n");
      if (!sLine)
        break;
      p = NULL;
      ParsePartitionInfo(sLine);
    }
  }
}

bool fardroid::UpdateInfoLines()
{FUNCTION
  lines.RemoveAll();
  infoSize.RemoveAll();

  CPanelLine *pl = new CPanelLine;
  pl->text.Format(L"%s %u.%u.%u.%u", PLUGIN_NAME, PLUGIN_MAJOR, PLUGIN_MINOR, PLUGIN_REVISION, PLUGIN_BUILD);
  pl->separator = true;
  lines.Add(pl);

  Opt.SU = false;
  Opt.SU0 = false;
  if (!GetDeviceInfo())
    return false;

  CheckCapabilities();

  Opt.SU = Opt.UseSU;
  bool res = GetMemoryInfo();

  if (Opt.SU && !res) {
    Opt.SU0 = true;
    res = GetMemoryInfo();
  }

  if (Opt.SU && !res)
  {
    Opt.SU = false;
    GetMemoryInfo();
  }

  GetPartitionsInfo();

  if (InfoPanelLineArray) {
    delete[] InfoPanelLineArray;
    InfoPanelLineArray = NULL;
  }
  if (lines.size() > 0)
  {
    InfoPanelLineArray = new InfoPanelLine[lines.size()];
    for (size_t i = 0; i < lines.size(); i++)
    {
      InfoPanelLineArray[i].Text = lines[i]->text;
      InfoPanelLineArray[i].Data = lines[i]->data;
      InfoPanelLineArray[i].Flags = lines[i]->separator ? IPLFLAGS_SEPARATOR : 0;
    }
  }
  return true;
}

void fardroid::ChangeDir(const wchar_t *sDir)
{FUNCTION
  DEBUGLOG(sDir);
  if (sDir[0] == L'\\' && sDir[1] == L'\0') //корень
    currentPath = L"/";
  else if (sDir[0] == L'/') //полный путь
    currentPath = sDir;
  else if (sDir[0] == L'.' && sDir[1] == L'.' && sDir[2] == L'\0') { //..
    const wchar_t *p = wcsrchr(currentPath.CPtr(), L'/');
    currentPath.SetLen((p == currentPath.CPtr()) ? 1 : p - currentPath.CPtr());
  }
  else
    currentPath = ConcatPath(currentPath, sDir);
}

HANDLE fardroid::OpenFromMainMenu()
{FUNCTION
  if (!UpdateInfoLines())
    return NULL;
  string sRes;
  if (Opt.RemountSystem)
    ADB_mount(L"/system", L"rw", sRes);
  PluginSettings settings(MainGuid, PsInfo.SettingsControl);
  ChangeDir(settings.Get(settings.OpenSubKey(0, L"devices"), currentDevice.CPtr(), L"/"));
  return (HANDLE)this;
}

HANDLE fardroid::OpenFromCommandLine(const wchar_t *cmd)
{FUNCTION
  DEBUGLOG(cmd);
  if (!UpdateInfoLines())
    return NULL;

  const wchar_t *filename = NULL, *remount = NULL;
  wchar_t *p = (wchar_t*)cmd, *sLine;
  while (true) {
    sLine = wcstok(p, L" ");
    if (!sLine)
      break;
    p = NULL;
    if (sLine[0] != L'-')
      filename = sLine;
    else if (!lstrcmp(sLine, L"-remount:rw"))
      remount = L"rw";
    else if (!lstrcmp(sLine, L"-remount:ro"))
      remount = L"ro";
  }

  string sRes;
  bool isOk = false;
  if (remount)
    isOk = ADB_mount(filename ? filename : L"/system", remount, sRes);
  if (filename)
    ChangeDir(filename);

  if (isOk)
    return (HANDLE)this;
  else
    OpenFromMainMenu();
  return NULL;
}

void fardroid::PreparePanel(OpenPanelInfo *Info)
{
  unsigned long long size = 0;
  for (size_t i = 0; i < infoSize.size(); i++)
    if (currentPath.startsWith(infoSize[i]->path))
      size = infoSize[i]->free;
  Info->FreeSize = size;
  panelTitle = currentDeviceName;
  panelTitle += currentPath;
  panelTitle += Opt.SU ? L" #" : L" $";
  Info->PanelTitle = panelTitle.CPtr();
  Info->CurDir = currentPath.CPtr();
  Info->InfoLines = InfoPanelLineArray;
  Info->InfoLinesNumber = lines.size();
  if (currentPath != L"/")
    Info->Flags |= OPIF_ADDDOTS;
}

bool fardroid::GetFindData(struct PluginPanelItem **pPanelItem, size_t *pItemsNumber, OPERATION_MODES OpMode)
{
  CFileRecords recs;
  if (ADB_ls(currentPath.CPtr(), false, recs)) {
    *pItemsNumber = recs.size();
    if (*pItemsNumber > 0) {
      PluginPanelItem *NewPanelItem = (PluginPanelItem*)calloc(*pItemsNumber, sizeof(PluginPanelItem)); //+HEAP_ZERO_MEMORY
      *pPanelItem = NewPanelItem;
      if (!NewPanelItem)
        return false;

      for (size_t i = 0; i < *pItemsNumber; i++) {
        if (!recs[i]->filename.IsEmpty())
          NewPanelItem[i].FileName = wcsdup(recs[i]->filename.CPtr());
        if (!recs[i]->owner.IsEmpty())
          NewPanelItem[i].Owner = wcsdup(recs[i]->owner.CPtr());
        if (!recs[i]->desc.IsEmpty())
          NewPanelItem[i].Description = wcsdup(recs[i]->desc.CPtr());
        if (!recs[i]->linkto.IsEmpty())
          NewPanelItem[i].AlternateFileName = wcsdup(recs[i]->linkto.CPtr());
        NewPanelItem[i].FileSize = recs[i]->size;
        NewPanelItem[i].CreationTime = recs[i]->ctime;
        NewPanelItem[i].LastAccessTime = recs[i]->atime;
        NewPanelItem[i].LastWriteTime = NewPanelItem[i].ChangeTime = recs[i]->mtime;
        NewPanelItem[i].CRC32 = (uintptr_t)recs[i]->mode;
        NewPanelItem[i].FileAttributes = ModeToAttr(recs[i]->mode);
        if (!recs[i]->grp.IsEmpty()) {
          NewPanelItem[i].UserData.Data = wcsdup(recs[i]->grp.CPtr());
          NewPanelItem[i].UserData.FreeData = FreeUserData;
        }
      }
    }
  }
  else
    *pItemsNumber = 0;
  return true;
}

int fardroid::DeviceMenu(string &text)
{FUNCTION
  unsigned size = 0;
  wchar_t *p = (wchar_t*)text.CPtr();
  while (true) {
    wchar_t *tok = wcstok(p, L"\n");
    if (!tok)
      break;
    p = NULL;
    size++;
  }
  if (size == 0)
    return FALSE;
  if (size == 1)
  {
    currentDevice = GetDeviceName((wchar_t*)text.CPtr());
    currentDeviceName = GetDeviceAliasName(currentDevice.CPtr());
    return TRUE;
  }
  FarMenuItem *items = (FarMenuItem*)calloc(size, sizeof(FarMenuItem));

  p = (wchar_t*)text.CPtr();
  for (unsigned i = 0; i < size; i++)
  {
    int len = lstrlen(p)+1;
    SetItemText(&items[i], GetDeviceCaption(p));
    items[i].UserData = (intptr_t)p;
    p += len;
  }

  int res, prev_sel = -1;
  FarKey pBreakKeys[] = {{ VK_F4,0 }, { VK_DELETE,0 }, {0,0} };
  intptr_t nBreakCode;
  while (true)
  {
    res = (int)PsInfo.Menu(&MainGuid, &MenuGuid, -1, -1, 0, FMENU_WRAPMODE | FMENU_AUTOHIGHLIGHT | FMENU_CHANGECONSOLETITLE, GetMsg(MSelectDevice), L"F4,Del", NULL, pBreakKeys, &nBreakCode, items, size);
    if (nBreakCode == -1)
      break;

    wchar_t *name = (wchar_t*)items[res].UserData;
    switch (pBreakKeys[nBreakCode].VirtualKeyCode)
    {
    case VK_F4:
      DeviceNameDialog(name);
      SetItemText(&items[res], GetDeviceCaption(name));
      break;
    case VK_DELETE:
      PluginSettings settings(MainGuid, PsInfo.SettingsControl);
      settings.Set(settings.OpenSubKey(0, L"names"), name, name);
      SetItemText(&items[res], name);
    }

    if (prev_sel >= 0)
      items[prev_sel].Flags &= ~MIF_SELECTED;
    items[res].Flags |= MIF_SELECTED;
    prev_sel = res;
  }

  if (res >= 0)
  {
    currentDevice = (wchar_t*)items[res].UserData;
    currentDeviceName = GetDeviceAliasName(currentDevice);
  }
  for (unsigned i = 0; i < size; i++)
    delete[] items[i].Text;
  free(items);

  return res < 0 ? ABORT : TRUE;
}

void fardroid::Remount(const wchar_t *Mode)
{
  string sRes;
  if (!ADB_mount(L"/system", Mode, sRes)) {
    const wchar_t *msg[]{GetMsg(MError), sRes.CPtr()};
    PsInfo.Message(&MainGuid, &MsgGuid, FMSG_WARNING | FMSG_MB_OK, NULL, msg, _ARRAYSIZE(msg), 0);
  }
}

void fardroid::GetFramebuffer()
{FUNCTION
  fb fb;
  struct fbinfo fbinfo;
  Socket sock(this);
  if (sock) {
    string cmd = L"framebuffer:";
    if (sock.SendADBCommand(cmd) && sock.ReadADBPacket(&fbinfo, sizeof(struct fbinfo)) > 0) {
      fb.bpp = fbinfo.bpp;
      unsigned tmp;
      if ((fbinfo.version != 2 || sock.ReadADBPacket(&tmp, sizeof(tmp)) > 0) && sock.ReadADBPacket(&fb.size, sizeof(struct fb)-4) > 0) {
        fb.data = malloc(fb.size);
        if (fb.data) {
          wchar_t szConsoleTitle[MAX_PATH];
          GetConsoleTitle(szConsoleTitle, MAX_PATH);
          HANDLE hScreen = PsInfo.SaveScreen(0, 0, -1, -1);
          procStruct.pType = PS_FB;
          procStruct.bSilent = false;
          procStruct.title = GetMsg(MScreenshot);
          procStruct.data[PT_ALL].current = 0;
          procStruct.data[PT_ALL].total = fb.size;
          char *p = (char*)fb.data;
          PsInfo.AdvControl(&MainGuid, ACTL_SETPROGRESSSTATE, TBPS_NORMAL, nullptr);
          while (fb.size > 0) {
            int read = sock.ReadADBPacket(p, (fb.size < SYNC_DATA_MAX ? fb.size : SYNC_DATA_MAX));
            if (read == 0)
              break;
            p += read;
            fb.size -= read;
            procStruct.data[PT_ALL].current += read;
            ShowProgressMessage();
          }
          SaveToClipboard(&fb);
          free(fb.data);
          PsInfo.RestoreScreen(hScreen);
          PsInfo.AdvControl(&MainGuid, ACTL_PROGRESSNOTIFY, 0, nullptr);
          PsInfo.AdvControl(&MainGuid, ACTL_SETPROGRESSSTATE, TBPS_NOPROGRESS, nullptr);
          SetConsoleTitle(szConsoleTitle);
        }
      }
    }
  }
}

intptr_t WINAPI PermissionDlgProc(HANDLE hDlg, intptr_t Msg, intptr_t Param1, void *Param2)
{
  if (Msg == DN_BTNCLICK && (Param1 == IDPRM_All || Param1 == IDPRM_None)) {
    prm = (Param1 == IDPRM_All) ? 0777 : 0;
    PsInfo.SendDlgMessage(hDlg, DM_SETTEXTPTR, IDPRM_Octal, (void*)((Param1 == IDPRM_All) ? L"0777" : L"0000"));
    return true;
  }
  else if ((Msg == DN_EDITCHANGE && Param1 == IDPRM_Octal)) {
    prm = StringOctalToMode(((FarDialogItem*)Param2)->Data);
    PsInfo.SendDlgMessage(hDlg, DM_ENABLEREDRAW, FALSE, 0);
    for (unsigned bit = 1, i = 0; bit <= 04000; bit <<= 1, i++) //расстановка галочек по checkbox-ам
      PsInfo.SendDlgMessage(hDlg, DM_SETCHECK, IDPRM_Bit[i], (void*)((prm & bit) != 0));
    PsInfo.SendDlgMessage(hDlg, DM_ENABLEREDRAW, TRUE, 0);
    return true;
  }
  else if (Msg == DN_BTNCLICK && IDPRM_Min <= Param1 && Param1 <= IDPRM_Max) {
    unsigned bit;
    for (bit = 0; bit < _ARRAYSIZE(IDPRM_Bit) && IDPRM_Bit[bit] != Param1; bit++) ;
    if ((intptr_t)Param2 == 1)
      prm |= 1ULL<<bit;
    else
      prm &= ~(1<<bit);
    wchar_t octal[5];
    i2octal(prm, octal, _ARRAYSIZE(octal)-1);
    PsInfo.SendDlgMessage(hDlg, DM_SETTEXTPTR, IDPRM_Octal, octal);
    return true;
  }
  return PsInfo.DefDlgProc(hDlg, Msg, Param1, Param2);
}

bool fardroid::ChangePermissionsDialog(size_t SelectedItemsNumber)
{FUNCTION
  if (!SelectedItemsNumber) //нет выделения и курсор стоит на ".."?
    return false;

  PluginPanelItem *item = GetSelectedPanelItem(0);
  int owner_len = Max(lstrlenW(item->Owner), 32)+1,
      group_len = Max(lstrlenW((wchar_t*)item->UserData.Data), 32)+1;
  wchar_t *owner = new wchar_t[owner_len],
          *group = new wchar_t[group_len];

  if (item->Owner)
    lstrcpyW(owner, item->Owner);
  else
    *owner = L'\0';
  if (item->UserData.Data)
    lstrcpyW(group, (wchar_t*)item->UserData.Data);
  else
    *group = L'\0';

  prm = item->CRC32;
  wchar_t octal[5];
  i2octal(prm & S_ISRWX, octal, _ARRAYSIZE(octal)-1);

  bool perm[3*4];
  for (unsigned bit = 1, i = 0; bit <= 04000; bit <<= 1, i++)
    perm[i] = prm & bit;

  PluginDialogBuilder Builder(PsInfo, MainGuid, DialogGuid, MPermTitle, NULL, PermissionDlgProc);
  Builder.AddText(MPermChange)->Flags |= DIF_CENTERTEXT;
  Builder.AddText(item->FileName)->Flags |= DIF_CENTERTEXT;
  Builder.AddSeparator();

  Builder.StartColumns();
  intptr_t X1 = Builder.AddText(MPermOwner)->X1 + lstrlen(GetMsg(MPermOwner)) + 1;
  Builder.AddText(MPermGroup);
  if ((Opt.WorkMode != WORKMODE_SAFE) && (item->FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
    Builder.AddText(MPermLink);
  else
    Builder.AddText(MPermType);

  Builder.ColumnBreak();
  Builder.AddEditField(owner, owner_len, 40, L"fardroidPermissionOwner")->X1 = X1;
  Builder.AddEditField(group, group_len, 40, L"fardroidPermissionGroup")->X1 = X1;
  if ((Opt.WorkMode != WORKMODE_SAFE) && (item->FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
    Builder.AddReadonlyEditField(item->AlternateFileName, 40)->X1 = X1;
  else
    Builder.AddText(ModeToType(prm))->X1 = X1;

  Builder.AddSeparator(MPermPermissions);
  Builder.StartColumns();
  X1 = Builder.AddText(L"User")->X1 + 8;
  Builder.AddText(L"Group");
  Builder.AddText(L"Others");

  Builder.ColumnBreak();
  Builder.AddCheckbox(L"R", &perm[8])->X1 = X1;
  IDPRM_Min = IDPRM_Bit[8] = Builder.GetLastID();
  Builder.AddCheckbox(L"R", &perm[5])->X1 = X1;
  IDPRM_Bit[5] = Builder.GetLastID();
  Builder.AddCheckbox(L"R", &perm[2])->X1 = X1;
  IDPRM_Bit[2] = Builder.GetLastID();

  Builder.ColumnBreak();
  X1 += 7;
  Builder.AddCheckbox(L"W", &perm[7])->X1 = X1;
  IDPRM_Bit[7] = Builder.GetLastID();
  Builder.AddCheckbox(L"W", &perm[4])->X1 = X1;
  IDPRM_Bit[4] = Builder.GetLastID();
  Builder.AddCheckbox(L"W", &perm[1])->X1 = X1;
  IDPRM_Bit[1] = Builder.GetLastID();

  Builder.ColumnBreak();
  X1 += 7;
  Builder.AddCheckbox(L"X", &perm[6])->X1 = X1;
  IDPRM_Bit[6] = Builder.GetLastID();
  Builder.AddCheckbox(L"X", &perm[3])->X1 = X1;
  IDPRM_Bit[3] = Builder.GetLastID();
  Builder.AddCheckbox(L"X", &perm[0])->X1 = X1;
  IDPRM_Bit[0] = Builder.GetLastID();

  Builder.ColumnBreak();
  X1 += 7;
  Builder.AddCheckbox(L"SUID", &perm[11])->X1 = X1;
  IDPRM_Bit[11] = Builder.GetLastID();
  Builder.AddCheckbox(L"SGID", &perm[10])->X1 = X1;
  IDPRM_Bit[10] = Builder.GetLastID();
  Builder.AddCheckbox(L"Sticky", &perm[9])->X1 = X1;
  IDPRM_Max = IDPRM_Bit[9] = Builder.GetLastID();

  FarDialogItem *di = Builder.AddFixEditField(octal, 5, 4, L"9999");
  IDPRM_Octal = Builder.GetLastID();
  Builder.AddTextBefore(di, L"Octal  ");
  Builder.AddButtonAfter(Builder.AddButtonAfter(di, MPermNone), MPermAll);
  IDPRM_All = Builder.GetLastID();
  IDPRM_None = IDPRM_All - 1;
  Builder.AddOKCancel(MOk, MCancel);

  bool isOk = Builder.ShowDialog();
  free(item);
  if (isOk) {
    string sRes;
    PsInfo.PanelControl(PANEL_ACTIVE, FCTL_BEGINSELECTION, 0, NULL);
    for (unsigned i = 0; i < SelectedItemsNumber; i++) {
      item = GetSelectedPanelItem(0);
      if (item) {
        bool chk = ((!lstrcmp(owner, item->Owner) && !lstrcmp(group, (wchar_t*)item->UserData.Data)) || ADB_chown(item->FileName, owner, group, sRes)) &&
                   (prm == item->CRC32 || ADB_chmod(item->FileName, octal, sRes));
        free(item);
        if (chk)
          PsInfo.PanelControl(PANEL_ACTIVE, FCTL_CLEARSELECTION, i, FALSE);
        else {
          const wchar_t *msg[]{GetMsg(MError), sRes.CPtr()};
          PsInfo.Message(&MainGuid, &MsgGuid, FMSG_WARNING | FMSG_MB_OK, NULL, msg, _ARRAYSIZE(msg), 0);
          break;
        }
      }
      else
        break;
    }
    PsInfo.PanelControl(PANEL_ACTIVE, FCTL_ENDSELECTION, 0, NULL);
  }
  delete[] group;
  delete[] owner;
  return isOk;
}

void fardroid::SetProgress(unsigned pt)
{
  string title;
  title.Format(L"{%u%%} %s - FARDroid", size_t(procStruct.data[pt].total == 0 ? 0 : procStruct.data[pt].current * 100 / procStruct.data[pt].total), procStruct.title);
  SetConsoleTitle(title.CPtr());
  ProgressValue value{sizeof(ProgressValue), procStruct.data[pt].current, procStruct.data[pt].total};
  if (procStruct.pType != PS_SCAN)
    PsInfo.AdvControl(&MainGuid, ACTL_SETPROGRESSVALUE, 0, &value);
}

void fardroid::DrawProgress(wchar_t *buf, unsigned size, unsigned pt)
{
  UINT64 edge = procStruct.data[pt].total == 0 ? 0 : procStruct.data[pt].current * size / procStruct.data[pt].total;
  for (unsigned i = 0; i < size; i++)
    buf[i] = i < edge ? 0x2588 : 0x2591; //'█':'░'
  wsprintf(buf+size, L" %3u%%", size_t(procStruct.data[pt].total == 0 ? 0 : (procStruct.data[pt].current * 100 / procStruct.data[pt].total)));
}

void fardroid::ShowProgressMessage()
{
  ProgressType pt;
  static DWORD dwTicks = 0;
  DWORD dwNewTicks = GetTickCount();
  if (procStruct.bSilent || dwNewTicks - dwTicks < 50)
    return;
  dwTicks = dwNewTicks;

  if (procStruct.pType == PS_SCAN)
  {
    pt = PT_ITEMS;
    size_t elapsed = (dwNewTicks - procStruct.nTotalStartTime) / 1000;
    wchar_t sEla[9];
    wsprintf(sEla, L"%02u:%02u:%02u", elapsed / 3600, (elapsed % 3600) / 60, elapsed % 60);

    wchar_t sFiles1[50], sBytes1[50];
    const wchar_t *mFiles = GetMsg(MFiles), *mBytes = GetMsg(MBytes);
    FSF.FormatFileSize(procStruct.data[PT_ITEMS].total, 50-lstrlen(mFiles), FFFS_COMMAS, sFiles1, _ARRAYSIZE(sFiles1));
    FSF.FormatFileSize(procStruct.data[PT_ALL].total, 50-lstrlen(mFiles), FFFS_COMMAS, sBytes1, _ARRAYSIZE(sBytes1));
    string sFiles, sBytes;
    sFiles.Format(L"%s %s", mFiles, sFiles1);
    sBytes.Format(L"%s %s", mBytes, sBytes1);

    const wchar_t *msg[]{procStruct.title, sFiles.CPtr(), sBytes.CPtr(), sEla};
    PsInfo.Message(&MainGuid, &MsgWaitGuid, FMSG_LEFTALIGN, nullptr, msg, _ARRAYSIZE(msg), 0);
  }
  else if (procStruct.pType == PS_COPY || procStruct.pType == PS_MOVE)
  {
    pt = PT_ALL;
    const wchar_t *mFrom = GetMsg(MFrom), *mTo = GetMsg(MTo);
    string mTotal = GetMsg(MTotal);
    mTotal.Insert(0, L'\x1');

    size_t elapsed = (dwNewTicks - procStruct.nTotalStartTime) / 1000, remain = 0, speed = 0;
    if (elapsed > 0)
      speed = size_t(procStruct.data[PT_ALL].current/ elapsed);
    if (speed > 0)
      remain = size_t((procStruct.data[PT_ALL].total - procStruct.data[PT_ALL].current) / speed);

    wchar_t sEla[9], sRem[9], sSpeed[13];
    wsprintf(sEla, L"%02u:%02u:%02u", elapsed / 3600, (elapsed % 3600) / 60, elapsed % 60);
    wsprintf(sRem, L"%02u:%02u:%02u", remain / 3600, (remain % 3600) / 60, remain % 60);
    FSF.FormatFileSize(speed, 12, FFFS_FLOATSIZE|FFFS_MINSIZEINDEX, sSpeed, _ARRAYSIZE(sSpeed));
    string sInfo;
    sInfo.Format(GetMsg(MProgress), sEla, sRem, sSpeed);

    unsigned size = (unsigned)sInfo.Len();
    string sFrom = procStruct.from, sTo = procStruct.to;
    FSF.TruncPathStr((wchar_t*)sFrom.CPtr(), size);
    FSF.TruncPathStr((wchar_t*)sTo.CPtr(), size);

    wchar_t sFiles1[50], sFiles2[50], sBytes1[50], sBytes2[50];
    const wchar_t *mFiles = GetMsg(MFiles), *mBytes = GetMsg(MBytes);
    size_t len = FSF.FormatFileSize(procStruct.data[PT_ITEMS].total, 0, FFFS_COMMAS, sFiles2, _ARRAYSIZE(sFiles2));
    FSF.FormatFileSize(procStruct.data[PT_ITEMS].current, size-lstrlen(mFiles)-len-3-5, FFFS_COMMAS, sFiles1, _ARRAYSIZE(sFiles1));
    len = FSF.FormatFileSize(procStruct.data[PT_ALL].total, 0, FFFS_COMMAS, sBytes2, _ARRAYSIZE(sBytes2));
    FSF.FormatFileSize(procStruct.data[PT_ALL].current, size-lstrlen(mFiles)-len-3-5, FFFS_COMMAS, sBytes1, _ARRAYSIZE(sBytes1));
    string sFiles, sBytes;
    sFiles.Format(L"%s %s / %s", mFiles, sFiles1, sFiles2);
    sBytes.Format(L"%s %s / %s", mBytes, sBytes1, sBytes2);

    const unsigned PROGRESS_SIZE = 100;
    wchar_t buf1[PROGRESS_SIZE + 6];
    DrawProgress(buf1, Min(size-5, PROGRESS_SIZE), PT_ONE);

    const wchar_t *msg[12]{procStruct.title, mFrom, sFrom.CPtr(), mTo, sTo.CPtr(), buf1, mTotal.CPtr(), sFiles.CPtr(), sBytes.CPtr()};
    unsigned index = 9;
    if (procStruct.data[PT_ITEMS].total > 2) {//0й элемент - служебный, не учитывается
      wchar_t buf2[PROGRESS_SIZE + 6];
      DrawProgress(buf2, Min(size-5, PROGRESS_SIZE), pt);
      msg[index++] = buf2;
    }
    msg[index++] = L"\1";
    msg[index++] = sInfo.CPtr();
    PsInfo.Message(&MainGuid, &MsgWaitGuid, FMSG_LEFTALIGN, nullptr, msg, index, 0);
  }
  else if (procStruct.pType == PS_DELETE)
  {
    pt = PT_ITEMS;
    const unsigned PROGRESS_SIZE = 50;
    wchar_t buf[PROGRESS_SIZE + 6];
    DrawProgress(buf, PROGRESS_SIZE, PT_ITEMS);
    FSF.TruncPathStr((wchar_t*)procStruct.from.CPtr(), PROGRESS_SIZE + 5);
    const wchar_t *msg[]{procStruct.title, procStruct.from.CPtr(), buf};
    PsInfo.Message(&MainGuid, &MsgWaitGuid, FMSG_LEFTALIGN, nullptr, msg, _ARRAYSIZE(msg), 0);
  }
  else if (procStruct.pType == PS_FB)
  {
    pt = PT_ALL;
    const unsigned PROGRESS_SIZE = 50;
    wchar_t buf[PROGRESS_SIZE + 6];
    DrawProgress(buf, PROGRESS_SIZE, PT_ALL);
    const wchar_t *msg[]{procStruct.title, buf};
    PsInfo.Message(&MainGuid, &MsgWaitGuid, FMSG_LEFTALIGN, nullptr, msg, _ARRAYSIZE(msg), 0);
  }
  SetProgress(pt);
}

bool fardroid::DeleteFileFrom(const wchar_t *src, bool bSilent)
{FUNCTION
  string sRes;
deltry:
  if (ADB_rm(src, sRes) || bSilent)
    return true;

  const wchar_t *msg[]{GetMsg(MDelFile), src, sRes.CPtr()};
  switch (PsInfo.Message(&MainGuid, &MsgGuid, FMSG_WARNING | FMSG_MB_ABORTRETRYIGNORE, L"copyerror", msg, _ARRAYSIZE(msg), 0))
  {
  case 1:
    sRes.Clear();
    goto deltry;
  case 2:
    return true;
  default: //-1, 0
    return false;
  }
}

bool fardroid::DeleteFiles(PluginPanelItem *PanelItem, size_t ItemsNumber, OPERATION_MODES OpMode)
{FUNCTION
  bool bSilent = (OpMode & (OPM_SILENT|OPM_FIND)) != 0;

  if ((OpMode & (OPM_SILENT|OPM_FIND|OPM_QUICKVIEW|OPM_VIEW|OPM_EDIT)) == 0) {
    const wchar_t *msg[]{GetMsg(MDeleteTitle), GetMsg(MDeleteWarn)};
    if (PsInfo.Message(&MainGuid, &MsgGuid, FMSG_WARNING | FMSG_MB_YESNO, NULL, msg, _ARRAYSIZE(msg), 0) != 0)
      return false;
  }

  wchar_t szConsoleTitle[MAX_PATH];
  GetConsoleTitle(szConsoleTitle, MAX_PATH);
  procStruct.pType = PS_DELETE;
  procStruct.bSilent = false;
  procStruct.title = GetMsg(MDelFile);
  procStruct.data[PT_ITEMS].total = ItemsNumber;
  PsInfo.AdvControl(&MainGuid, ACTL_SETPROGRESSSTATE, TBPS_NORMAL, nullptr);
  for (procStruct.data[PT_ITEMS].current = 0; procStruct.data[PT_ITEMS].current < ItemsNumber; procStruct.data[PT_ITEMS].current++) {
    if (CheckForEsc())
      break;
    procStruct.from = ConcatPath(currentPath, PanelItem[procStruct.data[PT_ITEMS].current].FileName);
    ShowProgressMessage();
    if (!DeleteFileFrom(procStruct.from.CPtr(), bSilent))
      break;
  }
  PsInfo.AdvControl(&MainGuid, ACTL_PROGRESSNOTIFY, 0, nullptr);
  PsInfo.AdvControl(&MainGuid, ACTL_SETPROGRESSSTATE, TBPS_NOPROGRESS, nullptr);
  SetConsoleTitle(szConsoleTitle);
  return true;
}

int fardroid::CreateDir(const wchar_t **DestPath, OPERATION_MODES OpMode)
{FUNCTION
  string newDir;
  if ((OpMode & (OPM_SILENT|OPM_FIND|OPM_QUICKVIEW|OPM_VIEW|OPM_EDIT)) == 0) {
    wchar_t editbuf[100];
    if (!PsInfo.InputBox(&MainGuid, nullptr, GetMsg(MCreateDir), GetMsg(MDirName), L"CreateDirDialog", *DestPath, editbuf, _ARRAYSIZE(editbuf), NULL, FIB_NONE))
      return ABORT;
    storedPath = editbuf;
    *DestPath = storedPath.CPtr();
    newDir = ConcatPath(currentPath, editbuf);
  }
  else
    newDir = ConcatPath(currentPath, *DestPath);

  string sRes;
  if (ADB_mkdir(newDir.CPtr(), sRes))
    return TRUE;
  else {
    const wchar_t *msg[]{GetMsg(MError), sRes.CPtr()};
    PsInfo.Message(&MainGuid, &MsgGuid, FMSG_WARNING | FMSG_MB_OK, NULL, msg, _ARRAYSIZE(msg), 0);
    return FALSE;
  }
}

bool fardroid::Copy_Rename(bool is_copy)
{FUNCTION
  string trunc_name, name = GetCurrentFileName();
  if (name == L"..")
    return false;

  trunc_name = name;
  wchar_t editbuf[100], *sfmt = (wchar_t*)GetMsg(is_copy ? MCopyFileDest : MRenameFileDest);
  FSF.TruncStr((wchar_t*)trunc_name.CPtr(), 70 - lstrlen(sfmt));
  trunc_name.Format(sfmt, trunc_name.CPtr());
  if (!PsInfo.InputBox(&MainGuid, nullptr, GetMsg(is_copy ? MCopyFile: MRenameFile), trunc_name.CPtr(), nullptr, name.CPtr(), editbuf, _ARRAYSIZE(editbuf), nullptr, FIB_NONE))
    return false;

  string sRes, src = ConcatPath(currentPath, name.CPtr()), dst = ConcatPath(currentPath, editbuf);
  if (is_copy && ADB_copy(src.CPtr(), dst.CPtr(), sRes))
    return true;
  else if (!is_copy && ADB_rename(src.CPtr(), dst.CPtr(), sRes))
    return true;
  else {
    const wchar_t *msg[]{GetMsg(MError), sRes.CPtr()};
    PsInfo.Message(&MainGuid, &MsgGuid, FMSG_WARNING | FMSG_MB_OK, NULL, msg, _ARRAYSIZE(msg), 0);
    return false;
  }
}

bool fardroid::ADBScanDirectory(size_t parent)
{FUNCTION
  CFileRecords recs;
  if (ADB_ls(copy_recs[parent]->src.CPtr(), true, recs)) {
    for (size_t i = 0; i < recs.size(); i++) {
      if (CheckForEsc())
        return false;
      ShowProgressMessage();

      procStruct.data[PT_ITEMS].total++;
      CCopyRecord *rec = new CCopyRecord;
      copy_recs.Add(rec);
      rec->parent = parent;
      rec->ctime = recs[i]->ctime;
      rec->mtime = recs[i]->mtime;
      rec->atime = recs[i]->atime;
      rec->is_dir = ModeToAttr(recs[i]->mode) & FILE_ATTRIBUTE_DIRECTORY;
      if (rec->is_dir) {
        rec->src = ConcatPath(copy_recs[parent]->src, recs[i]->filename.CPtr());
        rec->dst = ConcatPath(copy_recs[parent]->dst, recs[i]->filename.CPtr());
        rec->size = 0;
        if (!ADBScanDirectory(copy_recs.size()-1))
          return false;
      }
      else {
        rec->src = recs[i]->filename;
        rec->size = recs[i]->size;
        procStruct.data[PT_ALL].total += rec->size;
      }
    }
  }
  return true;
}

int fardroid::CopyErrorDialog(const wchar_t *sTitle, string &sRes)
{
  if (Opt.SU || Opt.UseSU) {
    const wchar_t *msg[]{sTitle, GetMsg(MCopyError), procStruct.from.CPtr(), procStruct.to.CPtr(), sRes.CPtr(), GetMsg(MRetry), GetMsg(MSkip), GetMsg(MCancel)};
    switch (PsInfo.Message(&MainGuid, &MsgGuid, FMSG_WARNING, L"copyerror", msg, _ARRAYSIZE(msg), 3)) {
    case 0:
      return RETRY;
    case 1:
      return SKIP;
    default:
      return ABORT;
    }
  }
  else {
    const wchar_t *msg[]{sTitle, GetMsg(MCopyError), procStruct.from.CPtr(), procStruct.to.CPtr(), sRes.CPtr(), GetMsg(MNeedSuperuserPerm), GetMsg(MRetry), GetMsg(MRetryRoot), GetMsg(MSkip), GetMsg(MCancel)};
    switch (PsInfo.Message(&MainGuid, &MsgGuid, FMSG_WARNING, L"copyerror", msg, _ARRAYSIZE(msg), 4)) {
    case 0:
      return RETRY;
    case 1:
      Opt.SU = true;
      return RETRY;
    case 2:
      return SKIP;
    default:
      return ABORT;
    }
  }
}

bool fardroid::ScanDirectory(size_t parent)
{FUNCTION
  WIN32_FIND_DATA fd;
  string sdir = ConcatPath(copy_recs[parent]->src, L"*");
  HANDLE h = FindFirstFile(sdir.CPtr(), &fd);
  if (h == INVALID_HANDLE_VALUE)
    return false;

  do {
    if (CheckForEsc())
      return false;
    ShowProgressMessage();
    if (lstrcmpW(fd.cFileName, L".") && lstrcmpW(fd.cFileName, L"..")) {
      procStruct.data[PT_ITEMS].total++;
      CCopyRecord *rec = new CCopyRecord;
      copy_recs.Add(rec);
      rec->parent = parent;
      rec->ctime = fd.ftCreationTime;
      rec->mtime = fd.ftLastWriteTime;
      rec->atime = fd.ftLastAccessTime;
      rec->is_dir = fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
      if (rec->is_dir) {
        rec->src = ConcatPath(copy_recs[parent]->src, fd.cFileName);
        rec->dst = ConcatPath(copy_recs[parent]->dst, fd.cFileName);
        rec->size = 0;
        if (!ScanDirectory(copy_recs.size()-1))
          return false;
      }
      else {
        rec->src = fd.cFileName;
        rec->size = (UINT64(fd.nFileSizeHigh)<<32) + fd.nFileSizeLow;
        procStruct.data[PT_ALL].total += rec->size;
      }
    }
  } while (FindNextFile(h, &fd) != 0);
  FindClose(h);
  return true;
}

int fardroid::CopyFiles(bool is_get, PluginPanelItem *PanelItem, size_t ItemsNumber, const wchar_t **Path, bool is_move, OPERATION_MODES OpMode)
{FUNCTION
  bool bSilent = (OpMode & (OPM_SILENT|OPM_FIND)) != 0;

  if (is_get && (OpMode & (OPM_SILENT|OPM_FIND|OPM_QUICKVIEW|OPM_VIEW|OPM_EDIT)) == 0) {
    wchar_t editbuf[100];
    if (!PsInfo.InputBox(&MainGuid, nullptr, GetMsg(is_move ? MMoveFile: MGetFile), GetMsg(MCopyDest), nullptr, *Path, editbuf, _ARRAYSIZE(editbuf), L"CopyDialog", FIB_NONE))
      return ABORT;
    storedPath = editbuf;
    *Path = storedPath.CPtr();
  }

  wchar_t szConsoleTitle[MAX_PATH];
  GetConsoleTitle(szConsoleTitle, MAX_PATH);
  procStruct.pType = PS_SCAN;
  procStruct.bSilent = false;
  procStruct.title = GetMsg(MScanDirectory);
  procStruct.data[PT_ITEMS].total = 0;
  procStruct.data[PT_ALL].current = 0;
  procStruct.data[PT_ALL].total = 0;
  procStruct.nTotalStartTime = GetTickCount();

  // Получение полного списка всех файлов с учётом вложенных каталогов
  // Полное имя хранится только у каталогов
  // Файлы берут его по ссылке на родительский каталог
  CCopyRecord *rec;
  bool is_break = false;
  PsInfo.AdvControl(&MainGuid, ACTL_SETPROGRESSSTATE, TBPS_INDETERMINATE, nullptr);
  copy_recs.Add(new CCopyRecord{0, (is_get ? currentPath.CPtr() : *Path), (is_get ? *Path : currentPath.CPtr()), 0, {}, {}, {}, true}); //корневой уровень с полными именами исходных каталогов
  procStruct.data[PT_ITEMS].total++;
  for (size_t i = 0; i < ItemsNumber; i++) {
    if (CheckForEsc()) {
      is_break = true;
      break;
    }
    ShowProgressMessage();

    procStruct.data[PT_ITEMS].total++;
    rec = new CCopyRecord;
    copy_recs.Add(rec);
    rec->parent = 0;
    rec->ctime = PanelItem[i].CreationTime;
    rec->mtime = PanelItem[i].ChangeTime;
    rec->atime = PanelItem[i].LastAccessTime;
    rec->is_dir = PanelItem[i].FileAttributes & FILE_ATTRIBUTE_DIRECTORY;
    if (rec->is_dir) {
      rec->src = ConcatPath(copy_recs[0]->src, PanelItem[i].FileName);
      rec->dst = ConcatPath(copy_recs[0]->dst, PanelItem[i].FileName);
      rec->size = 0;
      if (
        is_get && !ADBScanDirectory(copy_recs.size()-1) ||
        !is_get && !ScanDirectory(copy_recs.size()-1)
      ) {
        is_break = true;
        break;
      }
    }
    else {
      rec->src = PanelItem[i].FileName;
      rec->size = PanelItem[i].FileSize;
      procStruct.data[PT_ALL].total += rec->size;
    }
  }
  #ifdef USE_DEBUG
  string log;
  for (size_t i = 0; i < copy_recs.size(); i++) {
    log.Format(L"%u|%s|%s|%llu|%u\n", copy_recs[i]->parent, copy_recs[i]->src.CPtr(), copy_recs[i]->dst.CPtr(), copy_recs[i]->size, copy_recs[i]->is_dir);
    char *s = log.toUTF8();
    DEBUGBUF(s, (DWORD)log.UTFLen());
  }
  #endif

  if (!is_break) {
    string sd_name, sRes;
    const wchar_t *msg[]{GetMsg(MGetFile), GetMsg(MCopyWarnIfExists), NULL, GetMsg(MYes), GetMsg(MNo), GetMsg(MAlwaysYes), GetMsg(MAlwaysNo), GetMsg(MCancel)};
    intptr_t exResult = bSilent ? 2 : 0;
    procStruct.pType = is_move ? PS_MOVE : PS_COPY;
    procStruct.title = GetMsg(is_move ? MMoveFile: MGetFile);
    procStruct.nTotalStartTime = GetTickCount();
    PsInfo.AdvControl(&MainGuid, ACTL_SETPROGRESSSTATE, TBPS_NORMAL, nullptr);

    for (size_t i = 1; i < copy_recs.size(); i++) { //пропуск корневого элемента
      procStruct.data[PT_ITEMS].current = i;
      if (CheckForEsc()) {
        is_break = true;
        break;
      }

      if (copy_recs[i]->is_dir)
        if (is_get)
          CreateDirectory(copy_recs[i]->dst.CPtr(), NULL);
        else
          ADB_mkdir(copy_recs[i]->dst.CPtr(), sRes);
      else {
        procStruct.from = ConcatPath(copy_recs[copy_recs[i]->parent]->src, copy_recs[i]->src.CPtr());
        procStruct.to = ConcatPath(copy_recs[copy_recs[i]->parent]->dst, copy_recs[i]->src.CPtr());
        procStruct.data[PT_ONE].current = 0;
        procStruct.data[PT_ONE].total = copy_recs[i]->size;
        ShowProgressMessage();

        bool need_copy;
        unsigned mode;
        // Файл не существует?
        if (is_get && GetFileAttributes(procStruct.to.CPtr()) == INVALID_FILE_ATTRIBUTES)
          need_copy = true;
        else if (!is_get && !(mode = ADB_stat(procStruct.to))) {
          mode = 0664;
          need_copy = true;
        }
        else { //Файл существует?
          if (exResult != 2 && exResult != 3) { //не тихий режим и ещё не выбраны "Всегда да" и "Всегда нет"?
            msg[2] = procStruct.to.CPtr();
            exResult = PsInfo.Message(&MainGuid, &MsgGuid, FMSG_WARNING, L"warnifexists", msg, _ARRAYSIZE(msg), 5);
            if (exResult < 0 || exResult > 3) {
              is_break = true;
              break;
            }
          }
          need_copy = (exResult == 0) || (exResult == 2);
        }

        int result;
        if (need_copy) { //Yes
          sd_name = L"/sdcard/";
          sd_name += copy_recs[i]->src;
          sd_name += L".fardroid";
          do {
            sRes.Clear();
            const wchar_t *spath = is_get ? procStruct.from.CPtr() : procStruct.to.CPtr();
            if (Opt.SU && Opt.CopySD && !wcsstr(spath, L"/sdcard") && !wcsstr(spath, L"/emulated")) {//включено предварительное копирование на sd и источник/назначение не на sd-карте?
              if (is_get) {
                result = ADB_copy(procStruct.from.CPtr(), sd_name.CPtr(), sRes);
                if (result)
                  result = ADB_pull(sd_name, procStruct.to.CPtr(), sRes, copy_recs[i]);
                DeleteFileFrom(sd_name.CPtr(), true);
              }
              else {
                result = ADB_push(procStruct.from.CPtr(), sd_name, sRes, mode);
                if (result)
                  result = ADB_rename(sd_name.CPtr(), procStruct.to.CPtr(), sRes);
              }
            }
            else
              if (is_get)
                result = ADB_pull(procStruct.from, procStruct.to.CPtr(), sRes, copy_recs[i]);
              else
                result = ADB_push(procStruct.from.CPtr(), procStruct.to, sRes, mode);

            if (!result)
              if (sRes.IsEmpty())
                result = ABORT;
              else
                result = CopyErrorDialog(GetMsg(MGetFile), sRes);
          } while (result == RETRY);
        }
        else //No == skip
          result = SKIP;

        if (result == ABORT) {
          is_break = true;
          break;
        }
        else {
          if (copy_recs[i]->parent == 0) //элемент с панели (а не из вложенного каталога)?
            PanelItem[i-1].Flags &= ~PPIF_SELECTED;
          if (result == SKIP)
            procStruct.data[PT_ALL].total -= copy_recs[i]->size;
        }
      }//copy file
    }//for
    PsInfo.AdvControl(&MainGuid, ACTL_PROGRESSNOTIFY, 0, nullptr);
  }
  PsInfo.AdvControl(&MainGuid, ACTL_SETPROGRESSSTATE, TBPS_NOPROGRESS, nullptr);
  SetConsoleTitle(szConsoleTitle);
  copy_recs.RemoveAll();
  return is_break ? -1 : 1;
}
