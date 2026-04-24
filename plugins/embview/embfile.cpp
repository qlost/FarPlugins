#include "..\FarHeaders\crt.hpp"
#include "..\FarHeaders\plugin.hpp"
#pragma hdrstop
#include "embfile.h"
//---------------------------------------------------------------------------
//!!2 байта по смещению 2 (2-е слово, кол-во записей) меняются в GetFiles()
const char header[48]="\xAD\xDE\0\0(C) Union Card 1994";
//---------------------------------------------------------------------------
tEMBfile::tEMBfile(char *_fname) {
 fname=(char*)malloc(lstrlen(_fname)+1);
 lstrcpy(fname, _fname);
}

tEMBfile::~tEMBfile() {
 if (f!=INVALID_HANDLE_VALUE) {
  if (modified && data) {
   tRec *p=data;
   unsigned long size=nrecs*sizeof(tRec);
   asm {
    mov edx, [p]
    mov ecx, size
   l1:
    rol BYTE PTR [edx], 3
    inc edx
    loop l1
   }
   SetFilePointer(f, sizeof(header), NULL, FILE_BEGIN);
   unsigned short count=0;
   for (unsigned long i=0; i<nrecs; i++) {
    if ((deleted[i>>3]&(1<<(i&7)))==0) {//запись не удалена?
     WriteFile(f, &data[i], sizeof(tRec), &size, NULL);
     count++;
    }
   }
   SetEndOfFile(f);
   asm {
    mov ax, count
    rol al, 3
    rol ah, 3
    mov count, ax
   }
   SetFilePointer(f, 2, NULL, FILE_BEGIN);
   WriteFile(f, &count, sizeof(count), &size, NULL);
  }
  CloseHandle(f);
 }
 delete[] deleted;
 delete[] data;
 free(fname);
}

bool tEMBfile::PrepareData() {
 const char *msg[2]={"EMBView", NULL};

 f=CreateFile(fname, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
 if (f==INVALID_HANDLE_VALUE) {
  msg[1]=GetMsg(errOpenFile);
  Info.Message(Info.ModuleNumber, FMSG_WARNING|FMSG_MB_OK, NULL, msg, 2, 0);
  return false;
 }

 unsigned long size;
 size=SetFilePointer(f, 0, NULL, FILE_END)-sizeof(header);//размер данных
 if (size<=0 || size%sizeof(tRec)!=0) {
  msg[1]=GetMsg(errBadSize);
  Info.Message(Info.ModuleNumber, FMSG_WARNING|FMSG_MB_OK, NULL, msg, 2, 0);
  return false;
 }

 nrecs=size/400uL;//количество записей
 data=new tRec[nrecs]; deleted=new unsigned char[(nrecs>>3)+1];
 if (!data) {
  msg[1]=GetMsg(errMemAlloc);
  Info.Message(Info.ModuleNumber, FMSG_WARNING|FMSG_MB_OK, NULL, msg, 2, 0);
  return false;
 }
 SetFilePointer(f, sizeof(header), NULL, FILE_BEGIN);
 ReadFile(f, data, size, &size, NULL);//читаем все записи
 tRec *p=data;
 asm {
  mov edx, [p]
  mov ecx, size
 l1:
  ror BYTE PTR [edx], 3
  inc edx
  loop l1
 }
 return true;
}
//---------------------------------------------------------------------------
bool tEMBfile::GetFindData(struct PluginPanelItem **pPanelItem, int *pItemsNumber) {
 PluginPanelItem* ppi=(PluginPanelItem*)malloc(sizeof(PluginPanelItem)*nrecs);

 if (!ppi)
  return false;

 int count=0;
 for (unsigned long i=0; i<nrecs; i++) {
  if ((deleted[i>>3]&(1<<(i&7)))==0) {//запись не удалена?
   lstrcpy(ppi[count].FindData.cFileName, data[i].card);
   ppi[count].Description=data[i].cardstamp;

   ppi[count].CustomColumnData=(char**)new (char*);//1 custom-колонка
   ppi[count].CustomColumnNumber=1;
   ppi[count].CustomColumnData[0]=data[i].date;

   ppi[count].FindData.nFileSizeLow=400L;

   tUserData *p=new tUserData;//чтобы позже узнать порядковый номер записи при произведении операций
   p->size=sizeof(tUserData); p->index=i; p->rec=&data[i];
   ppi[count].UserData=(unsigned long)p;
   ppi[count].Flags=PPIF_USERDATA;
   ppi[count].CRC32=0x424D45;//идентификация файла как "EMB"
   count++;
  }
 }
 *pPanelItem=ppi; *pItemsNumber=count;
 return true;
}

void tEMBfile::FreeFindData(struct PluginPanelItem *PanelItem, int ItemsNumber) {
 for (int i=0; i<ItemsNumber; i++) {
  delete PanelItem[i].CustomColumnData;
  delete (tUserData*)PanelItem[i].UserData;
 }
 free(PanelItem);
}

bool tEMBfile::DeleteFiles(struct PluginPanelItem *PanelItem, int ItemsNumber) {
 for (int j=0; j<ItemsNumber; j++) {
  unsigned long i=((tUserData*)PanelItem[j].UserData)->index;
  deleted[i>>3]|=(unsigned char)(1<<(i&7));
 }
 modified=true;
 return true;
}

void tEMBfile::CleanItems(struct PluginPanelItem *PanelItem, int ItemsNumber) {
 for (int j=0; j<ItemsNumber; j++) {
  unsigned long i=((tUserData*)PanelItem[j].UserData)->index;
  memset(data[i].card, ' ', 19);      data[i].card[19]='\0';
  memset(data[i].date, ' ', 18);      data[i].date[18]='\0';
  memset(data[i].cardstamp, ' ', 27); data[i].cardstamp[27]='\0';
 }
 modified=ItemsNumber>0;
}

int tEMBfile::GetFiles(struct PluginPanelItem *PanelItem, int ItemsNumber, char *DestPath) {
 char *s=(char*)malloc(lstrlen(DestPath)+16);

 if (!s)
  return 0;
 GetTempFileName(DestPath, "emb", 0, s);
 HANDLE f=CreateFile(s, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
 free(s);
 if (f==INVALID_HANDLE_VALUE)
  return 0;

 unsigned long size;
 asm {
  mov ax, WORD PTR ItemsNumber
  rol al, 3
  rol ah, 3
  mov WORD PTR [header+2], ax
 }
 WriteFile(f, header, sizeof(header), &size, NULL);
 for (int j=0; j<ItemsNumber; j++) {
  tRec rec=data[((tUserData*)PanelItem[j].UserData)->index];
  asm {
   lea edx, rec
   mov ecx, 400
  l1:
   rol BYTE PTR [edx], 3
   inc edx
   loop l1
  }
  WriteFile(f, &rec, sizeof(tRec), &size, NULL);
 }
 CloseHandle(f);
 return 1;
}

int tEMBfile::PutFiles(struct PluginPanelItem *PanelItem, int ItemsNumber) {
 int nEMBItems=0;

 for (int j=0; j<ItemsNumber; j++)
  if (PanelItem[j].CRC32==0x424D45)//"EMB"-файл?
   nEMBItems++;
 if (nEMBItems==0)
  return 0;

 data=(tRec*)realloc(data, (nrecs+nEMBItems)*sizeof(tRec));
 deleted=(unsigned char*)realloc(deleted, ((nrecs+nEMBItems)>>3)+1);
 if (!data || !deleted)
  return 0;

 for (int j=0; j<ItemsNumber; j++)
  if (PanelItem[j].CRC32==0x424D45)//"EMB"-файл?
   memcpy(&data[nrecs+j], ((tUserData*)PanelItem[j].UserData)->rec, sizeof(tRec));
 nrecs+=nEMBItems; modified=true;
 return 2;
}

bool tEMBfile::EditItem(unsigned long Index, FarDialogItem *diEdit, int DialogItems) {
 bool ret;

 wsprintf(diEdit[0].Data, GetMsg(lbEditRec), Index+1);
 lstrcpy(diEdit[2].Data, data[Index].card);
 lstrcpy(diEdit[4].Data, data[Index].date);
 lstrcpy(diEdit[6].Data, data[Index].cardstamp);
 lstrcpy(diEdit[8].Data, data[Index].tr1);
 lstrcpy(diEdit[10].Data, data[Index].tr2);
 lstrcpy(diEdit[12].Data, data[Index].tr3);
 if (Info.Dialog(Info.ModuleNumber, -1, -1, 80, 12, NULL, diEdit, DialogItems)==14) {
  wsprintf(data[Index].card, "%-19.19s", diEdit[2].Data);
  wsprintf(data[Index].date, "%-18.18s", diEdit[4].Data);
  wsprintf(data[Index].cardstamp, "%-27.27s", diEdit[6].Data);
  wsprintf(data[Index].tr1, "%-76.76s", diEdit[8].Data);
  wsprintf(data[Index].tr2, "%-37.37s", diEdit[10].Data);
  wsprintf(data[Index].tr3, "%-104.104s", diEdit[12].Data);
  modified=true; ret=true;
 }
 else
  ret=false;
 diEdit[2].Focus=true;
 return ret;
}
