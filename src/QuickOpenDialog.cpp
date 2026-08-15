#include "QuickOpenDialog.h"
#include "constant.h"

#include <wx/sizer.h>
#include <wx/filename.h>
#include <wx/stattext.h>

#include <algorithm>

namespace
{
constexpr size_t kMaxVisibleResults = 250;
}

QuickOpenDialog::QuickOpenDialog(wxWindow* parent,
                                 const std::vector<QuickOpenItem>& items)
    : wxDialog(parent,
               wxID_ANY,
               "Quick Open",
               wxDefaultPosition,
               wxSize(720, 460),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      m_items(items)
{
    SetBackgroundColour(Colors::BG_PANEL);

    auto* root = new wxBoxSizer(wxVERTICAL);

    auto* hint = new wxStaticText(this, wxID_ANY,
                                  "Type a file name or path  •  Enter to open  •  Esc to cancel");
    hint->SetForegroundColour(Colors::SIDEBAR_TEXT);
    root->Add(hint, 0, wxLEFT | wxRIGHT | wxTOP, 10);

    m_input = new wxTextCtrl(this, wxID_ANY, wxEmptyString,
                             wxDefaultPosition, wxDefaultSize,
                             wxTE_PROCESS_ENTER);
    m_input->SetBackgroundColour(Colors::BG_ACTIVE);
    m_input->SetForegroundColour(Colors::FG);
    root->Add(m_input, 0, wxEXPAND | wxALL, 10);

    m_list = new wxListBox(this, wxID_ANY);
    m_list->SetBackgroundColour(Colors::BG);
    m_list->SetForegroundColour(Colors::FG);
    root->Add(m_list, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

    SetSizer(root);

    m_input->Bind(wxEVT_TEXT, &QuickOpenDialog::OnText, this);
    m_input->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent&) { AcceptSelection(); });
    m_list->Bind(wxEVT_LISTBOX_DCLICK, &QuickOpenDialog::OnListDoubleClick, this);
    Bind(wxEVT_CHAR_HOOK, &QuickOpenDialog::OnCharHook, this);

    RefreshResults();
    CentreOnParent();
    m_input->SetFocus();
}

wxString QuickOpenDialog::GetSelectedPath() const
{
    return m_selectedPath;
}

int QuickOpenDialog::FuzzyScore(const wxString& text, const wxString& query)
{
    if (query.IsEmpty())
        return 1;

    const wxString haystack = text.Lower();
    const wxString needle = query.Lower();

    const int direct = haystack.Find(needle);
    int score = 0;
    if (direct != wxNOT_FOUND)
    {
        score += 1000;
        if (direct == 0)
            score += 500;
        score -= std::min(direct, 200);
    }

    size_t searchFrom = 0;
    int lastPos = -1;
    int consecutive = 0;

    for (size_t i = 0; i < needle.length(); ++i)
    {
        const wxUniChar wanted = needle[i];
        bool found = false;

        for (size_t p = searchFrom; p < haystack.length(); ++p)
        {
            if (haystack[p] != wanted)
                continue;

            found = true;
            score += 20;

            if (lastPos >= 0 && static_cast<int>(p) == lastPos + 1)
            {
                ++consecutive;
                score += 20 + consecutive * 2;
            }
            else
            {
                consecutive = 0;
            }

            if (p == 0 || haystack[p - 1] == '/' || haystack[p - 1] == '\\' ||
                haystack[p - 1] == '_' || haystack[p - 1] == '-' || haystack[p - 1] == '.')
            {
                score += 25;
            }

            lastPos = static_cast<int>(p);
            searchFrom = p + 1;
            break;
        }

        if (!found)
            return 0;
    }

    score -= std::min(static_cast<int>(haystack.length()), 300);
    return std::max(score, 1);
}

void QuickOpenDialog::RefreshResults()
{
    wxString query;
    if (m_input)
        query = m_input->GetValue();

    std::vector<Match> matches;
    matches.reserve(m_items.size());

    for (size_t i = 0; i < m_items.size(); ++i)
    {
        const QuickOpenItem& item = m_items[i];
        const wxString fileName = wxFileName(item.displayPath).GetFullName();

        int score = FuzzyScore(item.displayPath, query);
        const int filenameScore = FuzzyScore(fileName, query);
        if (filenameScore > 0)
            score = std::max(score, filenameScore + 350);

        if (score > 0)
            matches.push_back({i, score});
    }

    std::sort(matches.begin(), matches.end(), [this](const Match& a, const Match& b)
    {
        if (a.score != b.score)
            return a.score > b.score;
        return m_items[a.itemIndex].displayPath.CmpNoCase(
                   m_items[b.itemIndex].displayPath) < 0;
    });

    m_list->Freeze();
    m_list->Clear();
    m_visibleItems.clear();

    const size_t count = std::min(matches.size(), kMaxVisibleResults);
    for (size_t i = 0; i < count; ++i)
    {
        const size_t itemIndex = matches[i].itemIndex;
        m_visibleItems.push_back(itemIndex);
        m_list->Append(m_items[itemIndex].displayPath);
    }

    if (!m_visibleItems.empty())
        m_list->SetSelection(0);

    m_list->Thaw();
}

void QuickOpenDialog::AcceptSelection()
{
    if (m_visibleItems.empty())
        return;

    int selection = m_list->GetSelection();
    if (selection == wxNOT_FOUND)
        selection = 0;

    if (selection < 0 || static_cast<size_t>(selection) >= m_visibleItems.size())
        return;

    m_selectedPath = m_items[m_visibleItems[static_cast<size_t>(selection)]].absolutePath;
    EndModal(wxID_OK);
}

void QuickOpenDialog::MoveSelection(int delta)
{
    if (m_visibleItems.empty())
        return;

    int selection = m_list->GetSelection();
    if (selection == wxNOT_FOUND)
        selection = 0;

    selection = std::clamp(selection + delta, 0,
                           static_cast<int>(m_visibleItems.size()) - 1);
    m_list->SetSelection(selection);
    m_list->EnsureVisible(selection);
}

void QuickOpenDialog::OnText(wxCommandEvent&)
{
    RefreshResults();
}

void QuickOpenDialog::OnListDoubleClick(wxCommandEvent&)
{
    AcceptSelection();
}

void QuickOpenDialog::OnCharHook(wxKeyEvent& event)
{
    switch (event.GetKeyCode())
    {
        case WXK_ESCAPE:
            EndModal(wxID_CANCEL);
            return;
        case WXK_RETURN:
        case WXK_NUMPAD_ENTER:
            AcceptSelection();
            return;
        case WXK_DOWN:
            MoveSelection(+1);
            return;
        case WXK_UP:
            MoveSelection(-1);
            return;
        default:
            event.Skip();
            return;
    }
}
