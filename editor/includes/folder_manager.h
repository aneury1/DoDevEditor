#ifndef FOLDER_MANAGER_H_DEFINED
#define FOLDER_MANAGER_H_DEFINED
#include <wx/wx.h>


class folder_manager{
    
   
   private: 
   
   folder_manager();

   public:
   
   static folder_manager * get();

   bool load_folder(const wxString& path);


};


#endif ///FOLDER_MANAGER_H_DEFINED