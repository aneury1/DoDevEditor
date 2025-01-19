#ifndef __CLANG_FORMAT_H_DEFINED
#define  __CLANG_FORMAT_H_DEFINED
/// TODO: this must be a plugin
///       remember install deps.
///       sudo apt install llvm-dev clang libclang-dev
///       ensure libs are installed if not yoy would need to compile it from source
// #define USING_CLANG_LIBRARY_DEFINED_ON_DEV 0 
#include <wx/wx.h> 


wxString format_cpp(const wxString& original);



#endif // __CLANG_FORMAT_H_DEFINED