#include <wx/wx.h>
#include <wx/file.h>
#include <wx/dnd.h>
#include <wx/textfile.h>
#include "text_editor.h"
 
class file_drop_target : public wxFileDropTarget
{
public:
    explicit file_drop_target(text_editor* owner) : m_owner(owner) {}

    // Called when a file is dropped onto the control
    bool OnDropFiles(wxCoord x, wxCoord y, const wxArrayString& filenames) override
    {
        if (!filenames.IsEmpty())
        {
            //// todo check for more than one file.
            return m_owner->load_text_file(filenames[0]);
        }
        return false;
    }

private:
    text_editor* m_owner;
};
