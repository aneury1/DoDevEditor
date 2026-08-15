#pragma once

#include <wx/dialog.h>
#include <wx/listbox.h>
#include <wx/textctrl.h>
#include <wx/string.h>

#include <vector>

struct QuickOpenItem
{
    wxString absolutePath;
    wxString displayPath;
};

class QuickOpenDialog : public wxDialog
{
public:
    QuickOpenDialog(wxWindow* parent, const std::vector<QuickOpenItem>& items);

    wxString GetSelectedPath() const;

private:
    struct Match
    {
        size_t itemIndex = 0;
        int score = 0;
    };

    static int FuzzyScore(const wxString& text, const wxString& query);
    void RefreshResults();
    void AcceptSelection();
    void MoveSelection(int delta);

    void OnText(wxCommandEvent& event);
    void OnListDoubleClick(wxCommandEvent& event);
    void OnCharHook(wxKeyEvent& event);

private:
    std::vector<QuickOpenItem> m_items;
    std::vector<size_t> m_visibleItems;
    wxTextCtrl* m_input = nullptr;
    wxListBox* m_list = nullptr;
    wxString m_selectedPath;
};
