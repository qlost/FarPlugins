#ifndef embfileH
#define embfileH

#include "common.h"

//---------------------------------------------------------------------------
class tEMBfile {
private:
 char *fname;
 HANDLE f;
 tRec *data;
 unsigned char *deleted;//биты присутствия записей. бит==0 - запись НЕ удалена
 unsigned long nrecs;//screen width, height, кол-во записей
 bool modified;
public:
 tEMBfile(char*);
 ~tEMBfile();
 bool PrepareData();
 const char *GetFName() const {return fname;}
 bool GetFindData(struct PluginPanelItem**, int*);
 void FreeFindData(struct PluginPanelItem*, int);
 bool DeleteFiles(struct PluginPanelItem*, int);
 void CleanItems(struct PluginPanelItem*, int);
 int GetFiles(struct PluginPanelItem*, int, char*);
 int PutFiles(struct PluginPanelItem*, int);
 bool EditItem(unsigned long, FarDialogItem*, int);
};
//---------------------------------------------------------------------------
#endif
