#include "Dialogs.h"
#include "utils.h" 
#include "constant.h"

#include <iostream>
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

#include <frame.h>
#include <sstream>
OpenExistingFile::OpenExistingFile(wxWindow *parent,std::set<std::string> files):
wxDialog(parent, wxID_ANY, "Open File", wxDefaultPosition, wxSize(400, 72)){
    
    wxPoint position{100,300};
    wxSize size{100,600};
    std::stringstream stream;

    this->panel = new wxPanel(this, wxID_ANY, position, wxDefaultSize);
    wxBoxSizer *sizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* textlabel = new wxStaticText(panel, wxID_ANY, (stream.str().c_str()),wxDefaultPosition, wxDefaultSize);
    

    
    values = new wxTextCtrl(panel, OnEnterClickedForOpenFile, (stream.str().c_str()), wxDefaultPosition, size);
//    if(files.size()>0){
        wxArrayString filesArr;
        for(auto file : files)
        {
           auto autoCompleteText=ExtractFileOnly(file)+"("+file+")";
           std::cout << autoCompleteText<<"\n";
           filesArr.Add(autoCompleteText);

        }   
        
        std::cout <<"Set Autocomplete...";
        values->AutoComplete(filesArr);
   /// }


   // values->Bind(wxEVT_KEY_DOWN, [this](wxKeyEvent ev){
   //    std::cout <<"KEy has been pressed\n";
   //    ev
   // });
    sizer->Add(textlabel, 0, wxALIGN_CENTER_VERTICAL, 0);
    sizer->Add(values, 1, wxALIGN_CENTER_VERTICAL, 0);
    panel->SetSizer(sizer);
    wxBoxSizer *mainSizer = new wxBoxSizer(wxVERTICAL);
    mainSizer->Add(panel, 1, wxEXPAND|wxALL,2);
    SetSizer(mainSizer);
}
