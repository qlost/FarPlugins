#pragma once
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <PluginSettings.hpp>
#include "stuff.h"
#include "SimpleVector.hpp"

struct CPanelLine
{
  string  data;
  string  text;
  bool    separator;
};

struct CInfoSize
{
  string path;
  unsigned long long total;
  unsigned long long used;
  unsigned long long free;
};

struct CFileRecord
{
  string    filename;
  string    linkto;
  string    owner;
  string    grp;
  string    desc;
  UINT64    size;
  FILETIME  ctime;
  FILETIME  mtime;
  FILETIME  atime;
  UINT64    mode;
};

struct CCopyRecord
{
  UINT64    parent;
  string    src;
  string    dst;
  UINT64    size;
  FILETIME  ctime;
  FILETIME  mtime;
  FILETIME  atime;
  bool      is_dir;
};

typedef SimpleVector<CPanelLine*> InfoPanelLines;
typedef SimpleVector<CInfoSize*> InfoSize;
typedef SimpleVector<CFileRecord*> CFileRecords;
typedef SimpleVector<CCopyRecord*> CCopyRecords;

class fardroid
{
friend class Socket;
private:
  int             handleAdbServer;
  HANDLE          hConInp;
  string          currentPath, currentDevice, currentDeviceName, panelTitle, storedPath;
  CCopyRecords    copy_recs;
  ProcessStruct   procStruct{};
  InfoPanelLine   *InfoPanelLineArray;
  InfoPanelLines  lines;
  InfoSize        infoSize;

  bool  ReadFileList(string &sFileList, bool bSilent, CFileRecords &recs);
  bool  ADB_ls(const wchar_t *sDir, bool bSilent, CFileRecords &recs);
  bool  ADB_rm(const wchar_t *sDir, string &sRes);
  bool  ADB_chmod(const wchar_t *sSrc, const wchar_t *octal, string &sRes);
  bool  ADB_chown(const wchar_t *sSrc, const wchar_t *user, const wchar_t *group, string &sRes);
  bool  ADB_mount(const wchar_t *sFS, const wchar_t *sMode, string &sRes);
  bool  ADB_mkdir(const wchar_t *sDir, string &sRes);
  bool  ADB_rename(const wchar_t *sSrc, const wchar_t *sDst, string &sRes);
  bool  ADB_copy(const wchar_t *sSrc, const wchar_t *sDst, string &sRes);
  bool  ADB_pull(string &sSrc, const wchar_t *sDst, string &sRes, const CCopyRecord *rec);
  bool  ADB_push(const wchar_t *sSrc, string &sDst, string &sRes, unsigned mode);
  unsigned ADB_stat(string &sDst);

  static string       GetDeviceAliasName(const wchar_t *device);
  static string       GetDeviceCaption(wchar_t *device);
  static void WINAPI  FreeUserData(void *UserData, const struct FarPanelItemFreeInfo *Info);

  bool          CheckForEsc();
  void          DeviceNameDialog(const wchar_t *name);
  void          CheckCapabilities();
  bool          GetDeviceInfo();
  bool          GetMemoryInfo();
  void          ParsePartitionInfo(wchar_t *sLine);
  void          GetPartitionsInfo();
  bool          UpdateInfoLines();
  int           DeviceMenu(string &text);
  void          SetProgress(unsigned type);
  void          DrawProgress(wchar_t *buf, unsigned size, unsigned type);
  void          ShowProgressMessage();
  bool          DeleteFileFrom(const wchar_t *src, bool bSilent);
  bool          ADBScanDirectory(UINT64 parent);
  int           CopyErrorDialog(const wchar_t *sTitle, string &sRes);
  bool          ScanDirectory(UINT64 parent);
public:
  fardroid();
  ~fardroid();

  void                DeviceNameDialog();
  void                ChangeDir(const wchar_t *sDir);
  HANDLE              OpenFromMainMenu();
  HANDLE              OpenFromCommandLine(const wchar_t *cmd);
  void                PreparePanel(struct OpenPanelInfo *Info);
  bool                GetFindData(struct PluginPanelItem **pPanelItem, size_t *pItemsNumber, OPERATION_MODES OpMode);
  void                Remount(const wchar_t *Mode);
  void                GetFramebuffer();
  bool                ChangePermissionsDialog(size_t selected);
  bool                DeleteFiles(PluginPanelItem *PanelItem, size_t ItemsNumber, OPERATION_MODES OpMode);
  int                 CreateDir(const wchar_t **DestPath, OPERATION_MODES OpMode);
  bool                Copy_Rename(bool is_copy);
  int                 GetFiles(PluginPanelItem *PanelItem, size_t ItemsNumber, const wchar_t **DestPath, bool is_move, OPERATION_MODES OpMode);
  int                 PutFiles(PluginPanelItem *PanelItem, size_t ItemsNumber, const wchar_t *SrcPath, bool is_move, OPERATION_MODES OpMode);
};

class Socket
{
private:
  SOCKET    sock;
  fardroid  *android;

  void  CreateADBSocket();
  void  CloseADBSocket();
  void  PrepareADBSocket();
public:
  Socket(fardroid *a);
  ~Socket();
  operator bool() const { return !!sock; }

  bool  SendADBPacket(void *packet, int size);
  int   ReadADBPacket(void *packet, int size);
  bool  SendADBCommand(string &sCMD);
  bool  ADBShellExecute(string &cmd, string &sRes);
  void  ADBSyncQuit();
  void  ReadError(unsigned id, unsigned len, string &sRes);
  bool  ADBPullFile(string &sSrc, const wchar_t *sDst, string &sRes, const CCopyRecord *rec);
  bool  ADBPushFile(const wchar_t *sSrc, string &sDst, string &sRes, unsigned mode);
};
