#include "Dialogs.h"

FindDialogInCurrentTextEditor::FindDialogInCurrentTextEditor(wxWindow *parent) : 
wxDialog(parent, wxID_ANY, "Custom Dialog", wxDefaultPosition, wxSize(400, 72))
{
    if (parent)
    {
        wxPoint parentPos = parent->GetScreenPosition();
        wxSize parentSize = parent->GetSize();
        wxSize dialogSize = GetSize();

        // Calculate desired position (near center of parent)
        int x = parentPos.x + (parentSize.x - dialogSize.x) / 2;
        int y = parentPos.y + (parentSize.y - dialogSize.y) / 2;

        // Ensure dialog is within parent bounds
        if (x < parentPos.x)
            x = parentPos.x;
        if (y < parentPos.y)
            y = parentPos.y;
        if (x + dialogSize.x > parentPos.x + parentSize.x)
            x = parentPos.x + parentSize.x - dialogSize.x;
        if (y + dialogSize.y > parentPos.y + parentSize.y)
            y = parentPos.y + parentSize.y - dialogSize.y;

        SetPosition(wxPoint(x, y)); // Set clamped position
    }

    wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

    wxBoxSizer *f1sizer = new wxBoxSizer(wxHORIZONTAL);
    wxTextCtrl *entry = new wxTextCtrl(this, wxID_ANY);
    wxStaticText *label = new wxStaticText(this, wxID_ANY, wxT("Enter Text"));
    wxStaticText *result = new wxStaticText(this, wxID_ANY, wxT("No Results"));
    f1sizer->Add(label  ,0, wxALIGN_CENTER_VERTICAL|wxALL,6);    
    f1sizer->Add(entry  ,0, wxALIGN_CENTER_VERTICAL|wxALL,0);    
    f1sizer->Add(result ,0, wxALIGN_CENTER_VERTICAL|wxALL,1);    
    ///f1sizer->Add(label,0,wxEXPAND,1);    
    ///wxBoxSizer *f2zer = new wxBoxSizer(wxVERTICAL);
    SetSizer(f1sizer);
}