#pragma once
#include <CRT\crt.hpp>
#include <shellapi.h>
#include <winsock.h>
#include <plugin.hpp>
#include <SimpleString.hpp>
#include "version.hpp"

#define SYNC_DATA_MAX (64*1024)
#define TICKS_PER_SECOND 10000000LL
#define EPOCH_DIFFERENCE 11644473600LL
#define ABORT -1
#define SKIP   2
#define RETRY  3
#define IS_FLAG(val, flag) (((val)&(flag))==(flag))

#define MKID(a,b,c,d) ((a) | ((b) << 8) | ((c) << 16) | ((d) << 24))
#define ID_STAT MKID('S','T','A','T')
#define ID_LIST MKID('L','I','S','T')
#define ID_LIS2 MKID('L','I','S','2')
#define ID_ULNK MKID('U','L','N','K')
#define ID_SEND MKID('S','E','N','D')
#define ID_RECV MKID('R','E','C','V')
#define ID_DENT MKID('D','E','N','T')
#define ID_DNT2 MKID('D','N','T','2')
#define ID_DONE MKID('D','O','N','E')
#define ID_DATA MKID('D','A','T','A')
#define ID_OKAY MKID('O','K','A','Y')
#define ID_FAIL MKID('F','A','I','L')
#define ID_QUIT MKID('Q','U','I','T')
#define ID_0000 MKID('0','0','0','0')

// Traditional mask definitions for st_mode
#define S_IFLNK  0120000  // symbolic link
#define S_IFSOCK 0140000  // socket
#define S_IFBLK  0060000  // block special
#define S_IFIFO  0010000  // this is a FIFO
// POSIX masks for st_mode
#define S_IRUSR   00400   // owner:  r--------
#define S_IWUSR   00200   // owner:  -w-------
#define S_IXUSR   00100   // owner:  --x------
#define S_IRWXU   (S_IRUSR | S_IWUSR | S_IXUSR)

#define S_IRGRP   00040    // group:  ---r-----
#define S_IWGRP   00020    // group:  ----w----
#define S_IXGRP   00010    // group:  -----x---
#define S_IRWXG   (S_IRGRP | S_IWGRP | S_IXGRP)

#define S_IROTH   00004    // others: ------r--
#define S_IWOTH   00002    // others: -------w-
#define S_IXOTH   00001    // others: --------x
#define S_IRWXO   (S_IROTH | S_IWOTH | S_IXOTH)

#define S_ISUID  0004000  // set user id on execution
#define S_ISGID  0002000  // set group id on execution
#define S_ISVTX  0001000  // save swapped text even after use

#define S_ISRWX  (S_IRWXU | S_IRWXG | S_IRWXO | S_ISUID | S_ISGID | S_ISVTX)

enum { WORKMODE_SAFE, WORKMODE_NATIVE, WORKMODE_BUSYBOX };
const enum ProcessType { PS_SCAN, PS_COPY, PS_MOVE, PS_DELETE, PS_FB };
const enum ProgressType { PT_ITEMS, PT_ONE, PT_ALL };

struct Options {
  //plugin
  wchar_t Prefix[16];
  wchar_t ADBPath[512];
  //interface
  int     PanelMode;
  int     WorkMode;
  int     SortMode;
  bool    SortOrder;
  bool    AddToDiskMenu;
  bool    ShowLinksAsDir;
  bool    KillServer;
  bool    SU;
  bool    SU0;
  bool    UseSU;
  bool    CopySD;
  bool    RemountSystem;
  bool    UseLS_N;
  bool    UseLIS2;
};

#pragma pack(push, 1)
typedef union {
  unsigned id;
  struct {
    unsigned id;
    unsigned namelen;
  } req;
  struct {
    unsigned id;
    unsigned mode;
    unsigned size;
    unsigned time;
  } stat;
  struct {
    unsigned id;
    unsigned mode;
    unsigned size;
    unsigned time;
    unsigned namelen;
  } dent;
  struct {
    UINT64 id;
    UINT64 unk0;
    UINT64 unk1;
    UINT64 mode;
    UINT64 unk2;
    UINT64 size;
    UINT64 ctime;
    UINT64 mtime;
    UINT64 atime;
    unsigned namelen;
  } dnt2;
  struct {
    unsigned id;
    unsigned size;
  } data;
  struct {
    unsigned id;
    unsigned msglen;
  } status;
} syncmsg;

struct syncsendbuf
{
  unsigned id;
  unsigned size;
  char data[SYNC_DATA_MAX];
};
#pragma pack(pop)

struct ProcessStruct
{
  ProcessType   pType;
  bool          bSilent;
  const wchar_t *title;
  string        from, to;
  DWORD         nTotalStartTime;
  struct { size_t current, total; } data[3];//files, filesize, totalsize
};

extern  Options Opt;
extern  PluginStartupInfo PsInfo;
extern  FarStandardFunctions FSF;
extern  HANDLE hRegexpSize, hRegexpMem, hRegexpPart1, hRegexpPart2, hRegexpFile;

BOOL    ExecuteCommandLine(const wchar_t *command, const wchar_t *path, const wchar_t *parameters, bool wait);
string& EscapeCommand(string &cmd, bool quoted = false);
string  ConcatPath(string &left, const wchar_t *right);

HANDLE  RegexpMake(const wchar_t *regex);
void    RegexpFree(HANDLE hRegex);
int     RegExTokenize(wchar_t *str, HANDLE hRegex, RegExpMatch **match, bool set_end);
UINT64  ParseSizeInfo(wchar_t *s);
void    CopyMatch(string &dst, wchar_t *src, RegExpMatch &m);

wchar_t*        GetDeviceName(wchar_t *device);
unsigned short  GetMonth(const wchar_t *sMonth);

void            i2octal(uintptr_t v, wchar_t *buf, unsigned maxlen);
uintptr_t       StringToMode(const wchar_t *sAttr);
unsigned        StringOctalToMode(const wchar_t *octal);
uintptr_t       ModeToAttr(uintptr_t mode);
const wchar_t*  ModeToType(const UINT64 p);
FILETIME        UnixTimeToFileTime(time_t time);
time_t          FileTimeToUnixTime(FILETIME ft);

const wchar_t*    GetMsg(int MsgId);
void              SetItemText(FarMenuItem *item, const string &text);
PluginPanelItem*  GetSelectedPanelItem(unsigned i);
PluginPanelItem*  GetCurrentPanelItem();
string            GetCurrentFileName();

string  GetSysError(bool is_sock);
void    DeleteLog();
void    DebugLog(const wchar_t *str);
void    DebugBuf(void *s, DWORD size = 0);

class CDbgPrint
{
  char *fl, *fn;
public:
  CDbgPrint(char *file, char *func) {
    wchar_t log[1024];
    wsprintfW(log, L"  ->%hs/%hs", fl=file, fn=func);
    DebugLog(log);
  }
  ~CDbgPrint() {
    wchar_t log[1024];
    wsprintfW(log, L"<-  %hs/%hs", fl, fn);
    DebugLog(log);
  }
};
#define __FILENAME__ (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)
#ifdef USE_DEBUG
#define FUNCTION CDbgPrint __function((char*)__FILENAME__,(char*)__FUNCTION__);//__FUNCSIG__
#define DELETELOG() DeleteLog()
#define DEBUGLOG(str) DebugLog(str)
#define DEBUGBUF(s, size) DebugBuf(s, size)
#define DEBUGNL() DebugBuf((void*)"\n")
#else
#define FUNCTION
#define DELETELOG()
#define DEBUGLOG(str)
#define DEBUGBUF(s, size)
#define DEBUGNL()
#endif
