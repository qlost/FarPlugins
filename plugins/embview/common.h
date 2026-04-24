#ifndef commonH
#define commonH

#include "..\FarHeaders\crt.hpp"
#include "..\FarHeaders\plugin.hpp"

enum {//строки lng-файла
 errOpenFile, errBadSize, errMemAlloc,//Строки сообщений
 lbEditRec, lbCard, lbDate, lbCardStamp, lbTrack1, lbTrack2, lbTrack3, bnOk, bnCancel,//Надписи в диалоге редактирования
 keyF7, keyCtrlF3, keyCtrlF4, keyCtrlF5,//Строка клавиш
 lbDelete,//Запрос удаления строк
 lbJump, lbJumpLine//Переход к строке
};

#pragma pack(push,1)
struct tRec {
 char card[20],
      date[19],
      cardstamp[28],
      rsv1[3],
      tr1[77],
      tr2[38],
      tr3[105],
      rsv2, rsv3[109];
};

struct tUserData {
 unsigned long size, index;
 tRec *rec;
};
#pragma pack(pop)

struct InitDialogItem {
 int Type;
 int X1;
 int Y1;
 int X2;
 int Y2;
 int Focus;
 int Selected;
 unsigned int Flags;
 int DefaultButton;
 char *Data;
};

extern PluginStartupInfo Info;

const char* GetMsg(int MsgId);

#endif
