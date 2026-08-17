#include "HexViewerPage.h"

#if DODEV_ENABLE_HEX_VIEWER

#include <wx/aui/auibook.h>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/file.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/stc/stc.h>
#include <wx/textctrl.h>

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <limits>
#include <sstream>

namespace
{
constexpr int INDICATOR_SEARCH = 8;
constexpr std::size_t SEARCH_CHUNK_SIZE = 1024u * 1024u;

wxColour Background() { return wxColour(30, 30, 30); }
wxColour ToolbarBackground() { return wxColour(37, 37, 38); }
}

HexViewerPage::HexViewerPage(wxWindow* parent, const wxString& filePath)
    : wxPanel(parent, wxID_ANY)
{
    BuildUI();
    if (!filePath.IsEmpty())
    {
        wxString error;
        if (!OpenFile(filePath, &error))
            m_status->SetLabel(error);
    }
}

wxString HexViewerPage::GetTabTitle() const
{
    if (m_filePath.IsEmpty())
        return "Hex Viewer";
    return wxFileName(m_filePath).GetFullName() + " [Hex]";
}

void HexViewerPage::BuildUI()
{
    SetBackgroundColour(Background());
    auto* root = new wxBoxSizer(wxVERTICAL);

    auto* toolbar = new wxPanel(this);
    toolbar->SetBackgroundColour(ToolbarBackground());
    auto* top = new wxBoxSizer(wxHORIZONTAL);

    auto* openButton = new wxButton(toolbar, wxID_ANY, "Open...");
    auto* reloadButton = new wxButton(toolbar, wxID_ANY, "Reload");
    m_prevPage = new wxButton(toolbar, wxID_ANY, "Previous Page");
    m_nextPage = new wxButton(toolbar, wxID_ANY, "Next Page");

    wxArrayString rows;
    rows.Add("8");
    rows.Add("16");
    rows.Add("32");
    m_bytesPerRowChoice = new wxChoice(toolbar, wxID_ANY, wxDefaultPosition, wxSize(70, -1), rows);
    m_bytesPerRowChoice->SetSelection(1);

    top->Add(openButton, 0, wxALL, 4);
    top->Add(reloadButton, 0, wxTOP | wxBOTTOM | wxRIGHT, 4);
    top->Add(m_prevPage, 0, wxTOP | wxBOTTOM | wxRIGHT, 4);
    top->Add(m_nextPage, 0, wxTOP | wxBOTTOM | wxRIGHT, 4);
    top->Add(new wxStaticText(toolbar, wxID_ANY, "Bytes/row:"), 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 4);
    top->Add(m_bytesPerRowChoice, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    top->AddStretchSpacer(1);

    toolbar->SetSizer(top);
    root->Add(toolbar, 0, wxEXPAND);

    auto* nav = new wxPanel(this);
    nav->SetBackgroundColour(ToolbarBackground());
    auto* navSizer = new wxBoxSizer(wxHORIZONTAL);
    m_pathCtrl = new wxTextCtrl(nav, wxID_ANY, wxString(), wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
    m_offsetCtrl = new wxTextCtrl(nav, wxID_ANY, "0x0", wxDefaultPosition, wxSize(130, -1), wxTE_PROCESS_ENTER);
    auto* goButton = new wxButton(nav, wxID_ANY, "Go Offset");
    m_searchCtrl = new wxTextCtrl(nav, wxID_ANY, wxString(), wxDefaultPosition, wxSize(220, -1), wxTE_PROCESS_ENTER);
    m_hexSearch = new wxCheckBox(nav, wxID_ANY, "Hex bytes");
    m_hexSearch->SetValue(true);
    auto* findButton = new wxButton(nav, wxID_ANY, "Find Next");

    navSizer->Add(m_pathCtrl, 1, wxALL | wxEXPAND, 4);
    navSizer->Add(new wxStaticText(nav, wxID_ANY, "Offset:"), 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 4);
    navSizer->Add(m_offsetCtrl, 0, wxALL | wxALIGN_CENTER_VERTICAL, 4);
    navSizer->Add(goButton, 0, wxTOP | wxBOTTOM | wxRIGHT, 4);
    navSizer->Add(new wxStaticText(nav, wxID_ANY, "Find:"), 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 4);
    navSizer->Add(m_searchCtrl, 0, wxALL | wxALIGN_CENTER_VERTICAL, 4);
    navSizer->Add(m_hexSearch, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    navSizer->Add(findButton, 0, wxTOP | wxBOTTOM | wxRIGHT, 4);
    nav->SetSizer(navSizer);
    root->Add(nav, 0, wxEXPAND);

    m_view = new wxStyledTextCtrl(this);
    ConfigureView();
    root->Add(m_view, 1, wxEXPAND);

    auto* statusPanel = new wxPanel(this);
    statusPanel->SetBackgroundColour(ToolbarBackground());
    auto* statusSizer = new wxBoxSizer(wxHORIZONTAL);
    m_status = new wxStaticText(statusPanel, wxID_ANY, "Open a file to inspect its raw bytes.");
    m_status->SetForegroundColour(wxColour(190, 190, 190));
    statusSizer->Add(m_status, 1, wxALL | wxALIGN_CENTER_VERTICAL, 6);
    statusPanel->SetSizer(statusSizer);
    root->Add(statusPanel, 0, wxEXPAND);

    SetSizer(root);

    openButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { BrowseFile(); });
    reloadButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&)
    {
        wxString error;
        if (!Reload(&error) && !error.IsEmpty())
            wxMessageBox(error, "Hex Viewer", wxOK | wxICON_ERROR, this);
    });
    m_prevPage->Bind(wxEVT_BUTTON, [this](wxCommandEvent&)
    {
        if (m_pageOffset == 0)
            return;
        const std::uint64_t offset = m_pageOffset > PAGE_SIZE ? m_pageOffset - PAGE_SIZE : 0;
        wxString error;
        if (!LoadPage(offset, &error))
            wxMessageBox(error, "Hex Viewer", wxOK | wxICON_ERROR, this);
    });
    m_nextPage->Bind(wxEVT_BUTTON, [this](wxCommandEvent&)
    {
        if (m_pageOffset + m_pageBytes.size() >= m_fileSize)
            return;
        wxString error;
        if (!LoadPage(m_pageOffset + PAGE_SIZE, &error))
            wxMessageBox(error, "Hex Viewer", wxOK | wxICON_ERROR, this);
    });
    goButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { GoToOffset(); });
    m_offsetCtrl->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent&) { GoToOffset(); });
    findButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { FindNext(); });
    m_searchCtrl->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent&) { FindNext(); });
    m_bytesPerRowChoice->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) { ChangeBytesPerRow(); });

    UpdateNavigation();
}

void HexViewerPage::ConfigureView()
{
    m_view->SetLexer(wxSTC_LEX_NULL);
    m_view->StyleSetBackground(wxSTC_STYLE_DEFAULT, Background());
    m_view->StyleSetForeground(wxSTC_STYLE_DEFAULT, wxColour(220, 220, 220));
    m_view->StyleSetFaceName(wxSTC_STYLE_DEFAULT, "Monospace");
    m_view->StyleSetSize(wxSTC_STYLE_DEFAULT, 10);
    m_view->StyleClearAll();
    m_view->SetReadOnly(true);
    m_view->SetWrapMode(wxSTC_WRAP_NONE);
    m_view->SetMarginWidth(0, 0);
    m_view->SetMarginWidth(1, 0);
    m_view->SetMarginWidth(2, 0);
    m_view->SetCaretLineVisible(true);
    m_view->SetCaretLineBackground(wxColour(42, 42, 44));
    m_view->SetSelBackground(true, wxColour(38, 79, 120));
    m_view->IndicatorSetStyle(INDICATOR_SEARCH, wxSTC_INDIC_ROUNDBOX);
    m_view->IndicatorSetForeground(INDICATOR_SEARCH, wxColour(255, 190, 60));
    m_view->IndicatorSetAlpha(INDICATOR_SEARCH, 80);
    m_view->IndicatorSetOutlineAlpha(INDICATOR_SEARCH, 180);
}

bool HexViewerPage::OpenFile(const wxString& path, wxString* error)
{
    wxFile file(path, wxFile::read);
    if (!file.IsOpened())
    {
        if (error)
            *error = wxString("Unable to open file: ") + path;
        return false;
    }

    const wxFileOffset length = file.Length();
    if (length == wxInvalidOffset || length < 0)
    {
        if (error)
            *error = wxString("Unable to determine file size: ") + path;
        return false;
    }

    m_filePath = path;
    m_fileSize = static_cast<std::uint64_t>(length);
    m_pageOffset = 0;
    m_lastSearchOffset = 0;
    m_pathCtrl->SetValue(path);
    return LoadPage(0, error);
}

bool HexViewerPage::Reload(wxString* error)
{
    if (m_filePath.IsEmpty())
    {
        if (error)
            *error = "No file is open in the Hex Viewer.";
        return false;
    }

    const std::uint64_t wantedOffset = m_pageOffset;
    wxFile file(m_filePath, wxFile::read);
    if (!file.IsOpened())
    {
        if (error)
            *error = wxString("Unable to reopen file: ") + m_filePath;
        return false;
    }
    const wxFileOffset length = file.Length();
    if (length == wxInvalidOffset || length < 0)
    {
        if (error)
            *error = wxString("Unable to determine file size: ") + m_filePath;
        return false;
    }
    m_fileSize = static_cast<std::uint64_t>(length);
    return LoadPage(std::min(wantedOffset, m_fileSize), error);
}

void HexViewerPage::BrowseFile()
{
    wxFileDialog dialog(this, "Open file as hexadecimal", wxString(), wxString(),
                        "All files (*.*)|*.*", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dialog.ShowModal() != wxID_OK)
        return;

    wxString error;
    if (!OpenFile(dialog.GetPath(), &error))
    {
        wxMessageBox(error, "Hex Viewer", wxOK | wxICON_ERROR, this);
        return;
    }
    if (auto* notebook = dynamic_cast<wxAuiNotebook*>(GetParent()))
    {
        const int index = notebook->GetPageIndex(this);
        if (index != wxNOT_FOUND)
            notebook->SetPageText(index, GetTabTitle());
    }
}

bool HexViewerPage::LoadPage(std::uint64_t offset, wxString* error)
{
    if (m_filePath.IsEmpty())
    {
        if (error)
            *error = "No file is open in the Hex Viewer.";
        return false;
    }

    wxFile file(m_filePath, wxFile::read);
    if (!file.IsOpened())
    {
        if (error)
            *error = wxString("Unable to open file: ") + m_filePath;
        return false;
    }

    if (offset > m_fileSize)
        offset = m_fileSize;
    const std::uint64_t pageStart = (offset / PAGE_SIZE) * PAGE_SIZE;
    if (file.Seek(static_cast<wxFileOffset>(pageStart), wxFromStart) == wxInvalidOffset)
    {
        if (error)
            *error = wxString::Format("Unable to seek to offset 0x%llX.", static_cast<unsigned long long>(pageStart));
        return false;
    }

    const std::uint64_t remaining = m_fileSize > pageStart ? m_fileSize - pageStart : 0;
    const std::size_t wanted = static_cast<std::size_t>(std::min<std::uint64_t>(PAGE_SIZE, remaining));
    m_pageBytes.assign(wanted, 0);
    if (wanted > 0)
    {
        const size_t readCount = file.Read(m_pageBytes.data(), wanted);
        m_pageBytes.resize(readCount);
    }

    m_pageOffset = pageStart;
    RenderPage(offset, 0);
    UpdateNavigation();
    UpdateStatus();
    return true;
}

void HexViewerPage::RenderPage(std::uint64_t highlightOffset, std::size_t highlightLength)
{
    const int rowBytes = BytesPerRow();
    const int offsetWidth = m_fileSize > 0xFFFFFFFFull ? 16 : 8;
    std::ostringstream out;
    out << std::uppercase << std::hex << std::setfill('0');

    for (std::size_t start = 0; start < m_pageBytes.size(); start += static_cast<std::size_t>(rowBytes))
    {
        const std::size_t count = std::min<std::size_t>(static_cast<std::size_t>(rowBytes), m_pageBytes.size() - start);
        out << std::setw(offsetWidth) << (m_pageOffset + start) << "  ";

        for (int i = 0; i < rowBytes; ++i)
        {
            if (i == rowBytes / 2)
                out << ' ';
            if (static_cast<std::size_t>(i) < count)
                out << std::setw(2) << static_cast<unsigned int>(m_pageBytes[start + static_cast<std::size_t>(i)]) << ' ';
            else
                out << "   ";
        }

        out << " |";
        for (std::size_t i = 0; i < count; ++i)
        {
            const unsigned char c = m_pageBytes[start + i];
            out << ((c >= 32 && c <= 126) ? static_cast<char>(c) : '.');
        }
        for (std::size_t i = count; i < static_cast<std::size_t>(rowBytes); ++i)
            out << ' ';
        out << "|\n";
    }

    m_view->SetReadOnly(false);
    m_view->SetText(wxString::FromUTF8(out.str().c_str()));
    m_view->SetReadOnly(true);

    m_view->SetIndicatorCurrent(INDICATOR_SEARCH);
    m_view->IndicatorClearRange(0, m_view->GetTextLength());

    if (highlightOffset != UINT64_MAX && highlightOffset >= m_pageOffset && highlightOffset < m_pageOffset + m_pageBytes.size())
    {
        const std::uint64_t relative = highlightOffset - m_pageOffset;
        const int row = static_cast<int>(relative / static_cast<std::uint64_t>(rowBytes));
        const int lineStart = m_view->PositionFromLine(row);
        const int lineEnd = m_view->GetLineEndPosition(row);
        m_view->SetIndicatorCurrent(INDICATOR_SEARCH);
        m_view->IndicatorFillRange(lineStart, std::max(0, lineEnd - lineStart));
        m_view->GotoLine(row);
        m_view->EnsureVisible(row);
        m_view->SetFirstVisibleLine(std::max(0, row - 3));
        if (highlightLength > 0)
            m_status->SetLabel(wxString::Format("Match at 0x%llX (%llu), %zu byte%s.",
                                               static_cast<unsigned long long>(highlightOffset),
                                               static_cast<unsigned long long>(highlightOffset),
                                               highlightLength,
                                               highlightLength == 1 ? wxT("") : wxT("s")));
    }
}

void HexViewerPage::GoToOffset()
{
    if (m_filePath.IsEmpty())
        return;
    std::uint64_t offset = 0;
    if (!ParseOffset(m_offsetCtrl->GetValue(), offset))
    {
        wxMessageBox("Enter an offset as decimal or hexadecimal (for example 4096 or 0x1000).",
                     "Hex Viewer", wxOK | wxICON_WARNING, this);
        return;
    }
    if (offset >= m_fileSize && m_fileSize > 0)
        offset = m_fileSize - 1;

    wxString error;
    if (!LoadPage(offset, &error))
    {
        wxMessageBox(error, "Hex Viewer", wxOK | wxICON_ERROR, this);
        return;
    }
    RenderPage(offset, 0);
    m_offsetCtrl->SetValue(wxString("0x") + FormatOffset(offset, 1));
}

void HexViewerPage::FindNext()
{
    if (m_filePath.IsEmpty())
        return;

    std::vector<unsigned char> pattern;
    wxString error;
    if (!BuildSearchPattern(pattern, &error))
    {
        wxMessageBox(error, "Hex Search", wxOK | wxICON_WARNING, this);
        return;
    }

    std::uint64_t found = 0;
    std::uint64_t start = m_lastSearchOffset;
    if (start >= m_fileSize)
        start = 0;

    if (!FindPattern(pattern, start, found, &error))
    {
        if (!error.IsEmpty())
            wxMessageBox(error, "Hex Search", wxOK | wxICON_ERROR, this);
        else
            wxMessageBox("Pattern not found.", "Hex Search", wxOK | wxICON_INFORMATION, this);
        return;
    }

    m_lastSearchOffset = found + std::max<std::size_t>(1, pattern.size());
    if (found < m_pageOffset || found >= m_pageOffset + m_pageBytes.size())
    {
        if (!LoadPage(found, &error))
        {
            wxMessageBox(error, "Hex Search", wxOK | wxICON_ERROR, this);
            return;
        }
    }
    RenderPage(found, pattern.size());
    m_offsetCtrl->SetValue(wxString("0x") + FormatOffset(found, 1));
}

void HexViewerPage::ChangeBytesPerRow()
{
    RenderPage();
    UpdateStatus();
}

int HexViewerPage::BytesPerRow() const
{
    const int selection = m_bytesPerRowChoice ? m_bytesPerRowChoice->GetSelection() : 1;
    if (selection == 0)
        return 8;
    if (selection == 2)
        return 32;
    return 16;
}

bool HexViewerPage::ParseOffset(const wxString& text, std::uint64_t& value) const
{
    wxString trimmed = text;
    trimmed.Trim(true).Trim(false);
    if (trimmed.IsEmpty())
        return false;

    unsigned long long parsed = 0;
    int base = 10;
    if (trimmed.StartsWith("0x") || trimmed.StartsWith("0X"))
    {
        trimmed = trimmed.Mid(2);
        base = 16;
    }
    if (trimmed.IsEmpty())
        return false;

    const std::string s = trimmed.ToStdString();
    try
    {
        std::size_t used = 0;
        parsed = std::stoull(s, &used, base);
        if (used != s.size())
            return false;
    }
    catch (...)
    {
        return false;
    }
    value = static_cast<std::uint64_t>(parsed);
    return true;
}

bool HexViewerPage::BuildSearchPattern(std::vector<unsigned char>& pattern, wxString* error) const
{
    pattern.clear();
    wxString input = m_searchCtrl->GetValue();
    if (input.IsEmpty())
    {
        if (error)
            *error = "Enter bytes or text to search for.";
        return false;
    }

    if (!m_hexSearch->GetValue())
    {
        const wxScopedCharBuffer utf8 = input.utf8_str();
        if (!utf8.data())
        {
            if (error)
                *error = "Unable to convert search text to UTF-8 bytes.";
            return false;
        }
        const char* data = utf8.data();
        pattern.assign(reinterpret_cast<const unsigned char*>(data),
                       reinterpret_cast<const unsigned char*>(data) + utf8.length());
        return !pattern.empty();
    }

    std::string compact;
    for (char c : input.ToStdString())
    {
        if (std::isspace(static_cast<unsigned char>(c)) || c == ':' || c == '-')
            continue;
        if (!std::isxdigit(static_cast<unsigned char>(c)))
        {
            if (error)
                *error = "Hex search accepts hexadecimal digits with optional spaces, ':' or '-' separators.";
            return false;
        }
        compact.push_back(c);
    }
    if (compact.empty() || (compact.size() % 2) != 0)
    {
        if (error)
            *error = "Hex search requires complete bytes, for example: DE AD BE EF.";
        return false;
    }

    for (std::size_t i = 0; i < compact.size(); i += 2)
    {
        const std::string byteText = compact.substr(i, 2);
        pattern.push_back(static_cast<unsigned char>(std::stoul(byteText, nullptr, 16)));
    }
    return true;
}

bool HexViewerPage::FindPattern(const std::vector<unsigned char>& pattern,
                                std::uint64_t startOffset,
                                std::uint64_t& foundOffset,
                                wxString* error) const
{
    if (pattern.empty() || m_filePath.IsEmpty())
        return false;

    auto searchRange = [&](std::uint64_t rangeStart, std::uint64_t rangeEnd, std::uint64_t& result) -> bool
    {
        if (rangeStart >= rangeEnd)
            return false;

        wxFile file(m_filePath, wxFile::read);
        if (!file.IsOpened())
        {
            if (error)
                *error = wxString("Unable to open file: ") + m_filePath;
            return false;
        }
        if (file.Seek(static_cast<wxFileOffset>(rangeStart), wxFromStart) == wxInvalidOffset)
        {
            if (error)
                *error = "Unable to seek while searching the file.";
            return false;
        }

        const std::size_t overlap = pattern.size() > 1 ? pattern.size() - 1 : 0;
        std::vector<unsigned char> buffer(SEARCH_CHUNK_SIZE + overlap);
        std::size_t carry = 0;
        std::uint64_t absolute = rangeStart;

        while (absolute < rangeEnd)
        {
            const std::uint64_t remaining = rangeEnd - absolute;
            const std::size_t wanted = static_cast<std::size_t>(std::min<std::uint64_t>(SEARCH_CHUNK_SIZE, remaining));
            const size_t got = file.Read(buffer.data() + carry, wanted);
            if (got == 0)
                break;

            const std::size_t total = carry + got;
            const auto begin = buffer.begin();
            const auto end = begin + static_cast<std::ptrdiff_t>(total);
            const auto it = std::search(begin, end, pattern.begin(), pattern.end());
            if (it != end)
            {
                const std::size_t index = static_cast<std::size_t>(std::distance(begin, it));
                result = absolute - carry + index;
                if (result + pattern.size() <= rangeEnd)
                    return true;
            }

            absolute += static_cast<std::uint64_t>(got);
            carry = std::min(overlap, total);
            if (carry > 0)
                std::copy(buffer.begin() + static_cast<std::ptrdiff_t>(total - carry), end, buffer.begin());
        }
        return false;
    };

    if (searchRange(startOffset, m_fileSize, foundOffset))
        return true;
    if (startOffset > 0 && searchRange(0, startOffset, foundOffset))
        return true;
    return false;
}

void HexViewerPage::UpdateStatus()
{
    if (m_filePath.IsEmpty())
    {
        m_status->SetLabel("Open a file to inspect its raw bytes.");
        return;
    }

    const std::uint64_t pageEnd = m_pageOffset + static_cast<std::uint64_t>(m_pageBytes.size());
    m_status->SetLabel(wxString::Format("%s  |  %s (%llu bytes)  |  page 0x%llX–0x%llX  |  %d bytes/row  |  read-only",
                                       m_filePath.c_str(),
                                       HumanSize(m_fileSize).c_str(),
                                       static_cast<unsigned long long>(m_fileSize),
                                       static_cast<unsigned long long>(m_pageOffset),
                                       static_cast<unsigned long long>(pageEnd),
                                       BytesPerRow()));
}

void HexViewerPage::UpdateNavigation()
{
    const bool hasFile = !m_filePath.IsEmpty();
    m_prevPage->Enable(hasFile && m_pageOffset > 0);
    m_nextPage->Enable(hasFile && m_pageOffset + m_pageBytes.size() < m_fileSize);
    m_offsetCtrl->Enable(hasFile);
    m_searchCtrl->Enable(hasFile);
    m_hexSearch->Enable(hasFile);
}

wxString HexViewerPage::FormatOffset(std::uint64_t value, int width)
{
    std::ostringstream out;
    out << std::uppercase << std::hex << std::setfill('0') << std::setw(width) << value;
    return wxString::FromUTF8(out.str().c_str());
}

wxString HexViewerPage::HumanSize(std::uint64_t bytes)
{
    static const char* suffix[] = {"B", "KiB", "MiB", "GiB", "TiB"};
    double value = static_cast<double>(bytes);
    int index = 0;
    while (value >= 1024.0 && index < 4)
    {
        value /= 1024.0;
        ++index;
    }
    if (index == 0)
        return wxString::Format("%llu B", static_cast<unsigned long long>(bytes));
    return wxString::Format("%.2f %s", value, wxString::FromUTF8(suffix[index]).c_str());
}

#endif // DODEV_ENABLE_HEX_VIEWER
