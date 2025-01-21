#ifndef SAVE_FOLDER_DIALOG_H_DEFINED
#define SAVE_FOLDER_DIALOG_H_DEFINED
#include <wx/wx.h>

enum class save_dialog_folder_of_file_type{
   File, 
   Folder,
   Other // this means not implemented. 
};

struct save_dialog_folder_or_file_in_current_dir : public wxFrame{
   wxStaticText *label;
   save_dialog_folder_or_file_in_current_dir(wxWindow *window);
   ~save_dialog_folder_or_file_in_current_dir();



};



#endif