#include "folder_manager.h"


   
folder_manager::folder_manager(){

}

   
folder_manager * folder_manager::get(){
       static folder_manager * ptr;
       if(!ptr){
         ptr = new folder_manager();
       }
       return ptr;
}

bool folder_manager::load_folder(const wxString& path){

    return true;
}