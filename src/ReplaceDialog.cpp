#include "ReplaceDialog.h"
#include "constant.h"

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

ReplaceDialog::ReplaceDialog(wxWindow* parent,
                             ReplaceRequest::Scope initialScope,
                             const wxString& initialFind)
    : wxDialog(parent, wxID_ANY, "Replace", wxDefaultPosition, wxSize(920, 560),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
    SetBackgroundColour(Colors::BG_PANEL);

    auto* root = new wxBoxSizer(wxVERTICAL);
    auto* grid = new wxFlexGridSizer(2, 8, 8);
    grid->AddGrowableCol(1, 1);

    auto addLabel = [this, grid](const wxString& text)
    {
        auto* label = new wxStaticText(this, wxID_ANY, text);
        label->SetForegroundColour(Colors::FG);
        grid->Add(label, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 4);
    };

    addLabel("Find:");
    m_find = new wxTextCtrl(this, wxID_ANY, initialFind);
    grid->Add(m_find, 1, wxEXPAND | wxRIGHT, 4);

    addLabel("Replace with:");
    m_replace = new wxTextCtrl(this, wxID_ANY);
    grid->Add(m_replace, 1, wxEXPAND | wxRIGHT, 4);

    addLabel("Scope:");
    m_scope = new wxChoice(this, wxID_ANY);
    m_scope->Append("Current Document");
    m_scope->Append("Workspace Folders");
    m_scope->SetSelection(initialScope == ReplaceRequest::Scope::WorkspaceFolders ? 1 : 0);
    grid->Add(m_scope, 0, wxEXPAND | wxRIGHT, 4);

    root->Add(grid, 0, wxEXPAND | wxALL, 10);

    auto* options = new wxBoxSizer(wxHORIZONTAL);
    m_matchCase = new wxCheckBox(this, wxID_ANY, "Match case");
    m_wholeWord = new wxCheckBox(this, wxID_ANY, "Whole word");
    m_regex = new wxCheckBox(this, wxID_ANY, "Regular expression");
    for (auto* box : {m_matchCase, m_wholeWord, m_regex})
    {
        box->SetForegroundColour(Colors::FG);
        box->SetBackgroundColour(Colors::BG_PANEL);
        options->Add(box, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 14);
    }
    options->AddStretchSpacer();
    root->Add(options, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

    auto* buttons = new wxBoxSizer(wxHORIZONTAL);
    auto* preview = new wxButton(this, wxID_ANY, "Preview");
    m_replaceOne = new wxButton(this, wxID_ANY, "Replace One");
    m_replaceAll = new wxButton(this, wxID_ANY, "Replace All");
    auto* close = new wxButton(this, wxID_CANCEL, "Close");
    buttons->Add(preview, 0, wxRIGHT, 6);
    buttons->Add(m_replaceOne, 0, wxRIGHT, 6);
    buttons->Add(m_replaceAll, 0, wxRIGHT, 6);
    buttons->AddStretchSpacer();
    buttons->Add(close, 0);
    root->Add(buttons, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

    m_status = new wxStaticText(this, wxID_ANY, "Enter text and choose Preview.");
    m_status->SetForegroundColour(wxColour(170, 170, 170));
    root->Add(m_status, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

    m_results = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                               wxLC_REPORT | wxLC_SINGLE_SEL | wxBORDER_SIMPLE);
    m_results->SetBackgroundColour(Colors::BG);
    m_results->SetForegroundColour(Colors::FG);
    m_results->InsertColumn(0, "File", wxLIST_FORMAT_LEFT, 300);
    m_results->InsertColumn(1, "Line", wxLIST_FORMAT_LEFT, 85);
    m_results->InsertColumn(2, "Match", wxLIST_FORMAT_LEFT, 480);
    root->Add(m_results, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

    SetSizer(root);

    preview->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { Preview(); });
    m_replaceOne->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { ReplaceOne(); });
    m_replaceAll->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { ReplaceAll(); });
    m_scope->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) { RefreshScopeUI(); });
    m_regex->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&)
    {
        if (m_regex->IsChecked())
            m_wholeWord->SetValue(false);
        m_wholeWord->Enable(!m_regex->IsChecked());
    });
    m_results->Bind(wxEVT_LIST_ITEM_ACTIVATED, [this](wxListEvent& event)
    {
        const long row = event.GetIndex();
        if (row >= 0 && static_cast<size_t>(row) < m_previewItems.size() && m_openResultCallback)
            m_openResultCallback(m_previewItems[static_cast<size_t>(row)]);
    });

    RefreshScopeUI();
    m_find->SetFocus();
    if (!initialFind.IsEmpty())
        m_find->SelectAll();
}

ReplaceRequest ReplaceDialog::BuildRequest() const
{
    ReplaceRequest request;
    request.findText = m_find->GetValue();
    request.replaceText = m_replace->GetValue();
    request.scope = m_scope->GetSelection() == 1
                        ? ReplaceRequest::Scope::WorkspaceFolders
                        : ReplaceRequest::Scope::CurrentDocument;
    request.matchCase = m_matchCase->IsChecked();
    request.wholeWord = m_wholeWord->IsChecked();
    request.useRegex = m_regex->IsChecked();
    return request;
}

void ReplaceDialog::RefreshScopeUI()
{
    const bool current = m_scope->GetSelection() == 0;
    m_replaceOne->Enable(current);
    m_results->DeleteAllItems();
    m_previewItems.clear();
    m_status->SetLabel(current
        ? "Current document: replacements stay in the editor buffer until you save."
        : "Workspace folders: Preview first. Closed files are backed up under .dodev/replace-backups before Replace All.");
}

void ReplaceDialog::Preview()
{
    if (!m_previewCallback)
        return;
    const ReplaceRequest request = BuildRequest();
    if (request.findText.IsEmpty())
    {
        m_status->SetLabel("Find text cannot be empty.");
        return;
    }

    wxString error;
    const auto results = m_previewCallback(request, error);
    if (!error.IsEmpty())
    {
        m_status->SetLabel(error);
        PopulateResults({});
        return;
    }
    PopulateResults(results);
    m_status->SetLabel(wxString::Format("%zu match%s", results.size(), results.size() == 1 ? "" : "es"));
}

void ReplaceDialog::ReplaceOne()
{
    if (!m_replaceOneCallback)
        return;
    const ReplaceRequest request = BuildRequest();
    if (request.findText.IsEmpty())
    {
        m_status->SetLabel("Find text cannot be empty.");
        return;
    }

    wxString error;
    const bool replaced = m_replaceOneCallback(request, error);
    if (!error.IsEmpty())
    {
        m_status->SetLabel(error);
        return;
    }
    if (replaced)
    {
        Preview();
        m_status->SetLabel("Replaced one occurrence. Preview refreshed.");
    }
    else
    {
        m_status->SetLabel("No match found.");
    }
}

void ReplaceDialog::ReplaceAll()
{
    if (!m_replaceAllCallback)
        return;
    const ReplaceRequest request = BuildRequest();
    if (request.findText.IsEmpty())
    {
        m_status->SetLabel("Find text cannot be empty.");
        return;
    }

    if (request.scope == ReplaceRequest::Scope::WorkspaceFolders)
    {
        const int answer = wxMessageBox(
            "Replace all matching occurrences across the open workspace folders?\n\n"
            "Closed files will be modified on disk after a backup is created. Open files are changed in their editor buffers and remain unsaved.",
            "Confirm Replace in Folders", wxYES_NO | wxNO_DEFAULT | wxICON_WARNING, this);
        if (answer != wxYES)
            return;
    }

    wxString error;
    const size_t count = m_replaceAllCallback(request, error);
    if (!error.IsEmpty())
    {
        m_status->SetLabel(error);
        return;
    }
    Preview();
    m_status->SetLabel(wxString::Format("Replaced %zu occurrence%s. Preview refreshed.",
                                        count, count == 1 ? "" : "s"));
}

void ReplaceDialog::PopulateResults(const std::vector<ReplacePreviewItem>& results)
{
    m_previewItems = results;
    m_results->Freeze();
    m_results->DeleteAllItems();
    long row = 0;
    for (const auto& item : results)
    {
        const long index = m_results->InsertItem(row, item.filepath);
        m_results->SetItem(index, 1, wxString::Format("%d:%d", item.line, item.column));
        m_results->SetItem(index, 2, item.preview);
        ++row;
    }
    m_results->Thaw();
}
