#ifndef __ID_HANDLER_DEFINED
#define __ID_HANDLER_DEFINED
#include <wx/wx.h>
#include <stdint.h>
 
 

enum FixedID{
   OpenFolder = wxID_LAST+1,
   CloseFolder,
   ViewFileExplorer,
   HideFileExplorer,
   OpenBinaryFileReadOnly,
   SaveAs,
   CloseTab,
   ExplorerItemTaken,
   ActionMenuCommands,
   CreateFileInCurrentSelection,
   CreateFolderInCurrentSelection,
   DeleteCurrentFile,
   RenameFile,
   OpenFile,
   CloneFile,
   ExpandSubFolder,
   LastFixedId,
};


enum AcceleratorFixedId{
   prevCtrlI = wxID_HIGHEST + 1
};

static inline int64_t get_next_id(){
   static int64_t current_id = LastFixedId+1;
   return ++current_id;
}

static inline int64_t get_accelerator_next_id(){
   static int64_t current_id = prevCtrlI;
   return ++current_id;
}

#endif