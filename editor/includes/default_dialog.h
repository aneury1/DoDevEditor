#ifndef __DEFAULT_DIALOG_H
#define __DEFAULT_DIALOG_H

#include <wx/wx.h>
#include <wx/textctrl.h>

// Custom Find Dialog
class find_dialog_text : public wxDialog
{
public:
    explicit find_dialog_text(wxWindow* parent)
        : wxDialog(parent, wxID_ANY, "Find Text", wxDefaultPosition, wxSize(300, 150))
    {
        // Create a vertical box sizer for layout
        wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

        // Add a label
        wxStaticText* label = new wxStaticText(this, wxID_ANY, "Enter text to find:");
        sizer->Add(label, 0, wxALL | wxALIGN_LEFT, 10);

        // Add a text input field
        m_textCtrl = new wxTextCtrl(this, wxID_ANY, "");
        sizer->Add(m_textCtrl, 0, wxALL | wxEXPAND, 10);

        // Add a Find and Cancel button
        wxSizer* buttonSizer = CreateButtonSizer(wxOK | wxCANCEL);
        sizer->Add(buttonSizer, 0, wxALL | wxALIGN_CENTER, 10);

        // Set the sizer for the dialog
        SetSizerAndFit(sizer);

        // Bind the OK button event
        Bind(wxEVT_BUTTON, &find_dialog_text::on_find, this, wxID_OK);
    }

    wxString GetSearchText() const
    {
        return m_textCtrl->GetValue();
    }

private:
    wxTextCtrl* m_textCtrl;

    void on_find(wxCommandEvent& event)
    {
        if (m_textCtrl->GetValue().IsEmpty())
        {
            wxMessageBox("Please enter text to search.", "Error", wxICON_ERROR);
            return;
        }

        EndModal(wxID_OK);  // Close the dialog with OK result
    }
};




#endif