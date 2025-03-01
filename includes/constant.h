#ifndef CONSTANT_H_DEFINED
#define CONSTANT_H_DEFINED

#include <stdint.h>
 
static const char *tabcontainer ="tabcontainer";
static const char *explorercontainer="explorercontainer";



#ifdef __linux__
static const char *FileSeparator ="/";
#elif __WIN32__
static const char *FileSeparator ="\\";
#else
static const char *FileSeparator ="/";
#endif

enum class Response{
  Success,
  Error,
  Cancel,
};
 
static const char *untitled ="Untitled";

static const char* file_filter = 
    "All Files (*.*)|*.*|"
    "C++ Source Files (*.cpp)|*.cpp|"
    "C Header Files (*.h)|*.h|"
    "C++ Header Files (*.hpp)|*.hpp|"
    "C++ Source Files (*.cc)|*.cc|"
    "C++ Source Files (*.cxx)|*.cxx|"
    "JavaScript Files (*.js)|*.js|"
    "JSON Files (*.json)|*.json|"
    "Text Files (*.txt)|*.txt|"
    "Java Files (*.java)|*.java|"
    "Assembly Files (*.asm;*.s)|*.asm;*.s|"
    "COBOL Files (*.cob)|*.cob|"
    "Makefiles (*.Make)|*.Make|"
    "Binary File (*.bin)|*.bin"
    ;

static const char *automotive_filter= "Covesa DLT File (*.dlt)|*.dlt|Binary File (*.bin)|*.bin";
static const char *csv_filter= "Comma Separate files (*.csv)|*.dlt|Binary File (*.bin)|*.bin";
 


enum FixedID{
   First = wxID_LAST+3,
   OpenFolder ,
   CloseFolder,
   ViewFileExplorer,
   ViewGitExplorer,
   ViewExecPanel,
   HideFileExplorer,
   OpenBinaryFileReadOnly,
   SaveAs,
   newEmptyTab,
   CloseTab,
   ExplorerItemTaken,
   ActionMenuCommands,
   CreateFileInCurrentSelection,
   CreateFolderInCurrentSelection,
   DeleteCurrentFile,
   RenameFile,
   OpenFileOn,
   CloneFile,
   ExpandSubFolder,
   AddDLTViewer,
   OpenDLTFile,
   AddCSVViewer,
   OpenCSVFile,
   AddDataServerEditor,
   OnEnterClickedForOpenFile,


   //// Accelerator must be place bellow to control it.
   OpenFileAccelerator,   
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
