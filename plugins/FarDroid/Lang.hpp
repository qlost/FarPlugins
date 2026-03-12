#ifndef lang_hpp
#define lang_hpp
enum {
  MTitle,

  MOk,
  MCancel,
  MYes,
  MNo,
  MAlwaysYes,
  MAlwaysNo,
  MRetry,
  MRetryRoot,
  MSkip,

  MConfTitle,
  MConfAddToDisk,
  MConfPrefix,
  MConfSafeMode,
  MConfNative,
  MConfBusybox,
  MConfShowLinksAsDirs,
  MConfUseSU,
  MConfCopySD,
  MConfCopySDWarning,
  MConfRemountSystem,
  MConfADBPath,
  MConfKillServer,
  MConfKillServerWarning,

  MError,
  MDeviceNotFound,
  MSelectDevice,
  MRenameDeviceName,

  MFrom,
  MTo,
  MProgress,
  MTotal,
  MFiles,
  MBytes,

  MGetFile,
  MDelFile,
  MCreateDir,
  MMoveFile,
  MRenameFile,
  MCopyFile,
  MScanDirectory,
  MScreenshot,
  MScreenshotComplete,

  MBreakWarn,

  MDirName,
  MCopyDest,
  MRenameFileDest,
  MCopyFileDest,
  MCopyWarnIfExists,
  MCopyError,
  MDeleteTitle,
  MDeleteWarn,

  MMemoryInfo,
  MPartitionsInfo,

  MPermTitle,
  MPermChange,
  MPermPermissions,
  MPermOwner,
  MPermGroup,
  MPermType,
  MPermLink,
  MPermAll,
  MPermNone,

  MNeedSuperuserPerm,
};

#endif
