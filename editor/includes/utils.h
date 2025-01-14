#ifndef __UTILS_DEFINED_
#define __UTILS_DEFINED_
#include <wx/wx.h>
#include <wx/wfstream.h>
#include <wx/dir.h>
#include <wx/file.h>

static inline bool check_for_subfolder(const wxString &folderPath,  wxString  subfolderName)
{
    if (!wxDir::Exists(folderPath))
    {
       /// wxLogError("The folder does not exist.");
        return false;
    }

    wxDir dir(folderPath);
    if (!dir.IsOpened())
    {
    ///    wxLogError("Failed to open the folder.");
        return false;
    }
 
    bool found = dir.GetFirst(&subfolderName, wxEmptyString, wxDIR_DIRS);
    return found; // Returns true if at least one subfolder is found
}

 

static inline bool save_text_file(const wxString& file, const wxString& text)
{
   std::cout << __LINE__ << "\n";
   wxFileOutputStream output_stream(file);
   if (!output_stream.IsOk())
   {
      wxLogError("Cannot save current contents in file '%s'.", file);
      return false;
   }
   
   //std::cout << __LINE__ << "\n"
   //          << current->get_text() << "\n";
   output_stream.Write(text.c_str(), text.size()); 
 
   output_stream.Close();
   return true;
}


#endif ///__UTILS_DEFINED_