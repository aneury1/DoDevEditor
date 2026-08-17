#include "FileComparePage.h"

#if DODEV_ENABLE_FILE_COMPARE

#include <wx/aui/auibook.h>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/ffile.h>
#include <wx/sizer.h>
#include <wx/splitter.h>
#include <wx/stattext.h>
#include <wx/stc/stc.h>
#include <wx/textctrl.h>

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <limits>
#include <sstream>
#include <utility>

namespace
{
constexpr int MARK_CHANGED = 1;
constexpr int MARK_SIDE_ONLY = 2;
constexpr size_t BINARY_ROW_BYTES = 16;
constexpr size_t MAX_LCS_CELLS = 4000000;

wxColour EditorBackground()
{
    return wxColour(30, 30, 30);
}

wxColour ToolbarBackground()
{
    return wxColour(37, 37, 38);
}
}

FileComparePage::FileComparePage(wxWindow* parent,
                                 const wxString& leftPath,
                                 const wxString& rightPath)
    : wxPanel(parent, wxID_ANY),
      m_leftPath(leftPath),
      m_rightPath(rightPath)
{
    BuildUI();
    UpdateCaptions();
    if (!m_leftPath.IsEmpty() && !m_rightPath.IsEmpty())
        CompareFiles();
}

wxString FileComparePage::GetTabTitle() const
{
    if (m_leftPath.IsEmpty() && m_rightPath.IsEmpty())
        return wxString("File Compare");

    wxString leftName = m_leftPath.IsEmpty() ? wxString("Left") : wxFileName(m_leftPath).GetFullName();
    wxString rightName = m_rightPath.IsEmpty() ? wxString("Right") : wxFileName(m_rightPath).GetFullName();
    return leftName + wxString(" ↔ ") + rightName;
}

void FileComparePage::BuildUI()
{
    SetBackgroundColour(EditorBackground());
    auto* root = new wxBoxSizer(wxVERTICAL);

    auto* toolbar = new wxPanel(this);
    toolbar->SetBackgroundColour(ToolbarBackground());
    auto* toolbarSizer = new wxBoxSizer(wxHORIZONTAL);

    auto* leftButton = new wxButton(toolbar, wxID_ANY, "Load Left...");
    auto* rightButton = new wxButton(toolbar, wxID_ANY, "Load Right...");
    auto* swapButton = new wxButton(toolbar, wxID_ANY, "Swap");
    auto* refreshButton = new wxButton(toolbar, wxID_ANY, "Compare");

    wxArrayString modes;
    modes.Add("Auto");
    modes.Add("Text");
    modes.Add("Binary");
    m_modeChoice = new wxChoice(toolbar, wxID_ANY, wxDefaultPosition, wxSize(92, -1), modes);
    m_modeChoice->SetSelection(0);

    m_ignoreWhitespace = new wxCheckBox(toolbar, wxID_ANY, "Ignore whitespace");
    m_ignoreCase = new wxCheckBox(toolbar, wxID_ANY, "Ignore case");
    m_prevButton = new wxButton(toolbar, wxID_ANY, "Previous Diff");
    m_nextButton = new wxButton(toolbar, wxID_ANY, "Next Diff");

    toolbarSizer->Add(leftButton, 0, wxALL, 4);
    toolbarSizer->Add(rightButton, 0, wxTOP | wxBOTTOM | wxRIGHT, 4);
    toolbarSizer->Add(swapButton, 0, wxTOP | wxBOTTOM | wxRIGHT, 4);
    toolbarSizer->Add(refreshButton, 0, wxTOP | wxBOTTOM | wxRIGHT, 4);
    toolbarSizer->Add(new wxStaticText(toolbar, wxID_ANY, "Mode:"), 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 4);
    toolbarSizer->Add(m_modeChoice, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    toolbarSizer->Add(m_ignoreWhitespace, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    toolbarSizer->Add(m_ignoreCase, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    toolbarSizer->AddStretchSpacer(1);
    toolbarSizer->Add(m_prevButton, 0, wxTOP | wxBOTTOM | wxRIGHT, 4);
    toolbarSizer->Add(m_nextButton, 0, wxTOP | wxBOTTOM | wxRIGHT, 4);
    toolbar->SetSizer(toolbarSizer);
    root->Add(toolbar, 0, wxEXPAND);

    auto* paths = new wxPanel(this);
    paths->SetBackgroundColour(EditorBackground());
    auto* pathsSizer = new wxBoxSizer(wxHORIZONTAL);
    m_leftPathCtrl = new wxTextCtrl(paths, wxID_ANY, wxString(), wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
    m_rightPathCtrl = new wxTextCtrl(paths, wxID_ANY, wxString(), wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
    pathsSizer->Add(m_leftPathCtrl, 1, wxALL | wxEXPAND, 4);
    pathsSizer->Add(m_rightPathCtrl, 1, wxTOP | wxBOTTOM | wxRIGHT | wxEXPAND, 4);
    paths->SetSizer(pathsSizer);
    root->Add(paths, 0, wxEXPAND);

    auto* splitter = new wxSplitterWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                          wxSP_LIVE_UPDATE | wxSP_3DSASH);
    splitter->SetMinimumPaneSize(120);
    splitter->SetSashGravity(0.5);
    m_leftEditor = new wxStyledTextCtrl(splitter);
    m_rightEditor = new wxStyledTextCtrl(splitter);
    ConfigureEditor(m_leftEditor, false);
    ConfigureEditor(m_rightEditor, true);
    splitter->SplitVertically(m_leftEditor, m_rightEditor);
    root->Add(splitter, 1, wxEXPAND);

    auto* statusPanel = new wxPanel(this);
    statusPanel->SetBackgroundColour(ToolbarBackground());
    auto* statusSizer = new wxBoxSizer(wxHORIZONTAL);
    m_status = new wxStaticText(statusPanel, wxID_ANY, "Load two files to compare.");
    m_status->SetForegroundColour(wxColour(190, 190, 190));
    statusSizer->Add(m_status, 1, wxALL | wxALIGN_CENTER_VERTICAL, 6);
    statusPanel->SetSizer(statusSizer);
    root->Add(statusPanel, 0, wxEXPAND);

    SetSizer(root);

    leftButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { BrowseLeft(); });
    rightButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { BrowseRight(); });
    swapButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { SwapFiles(); });
    refreshButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { CompareFiles(); });
    m_modeChoice->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) { CompareFiles(); });
    m_ignoreWhitespace->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) { CompareFiles(); });
    m_ignoreCase->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) { CompareFiles(); });
    m_prevButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { NavigateDifference(-1); });
    m_nextButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { NavigateDifference(+1); });

    m_leftEditor->Bind(wxEVT_STC_UPDATEUI, [this](wxStyledTextEvent& event)
    {
        SyncScroll(m_leftEditor, m_rightEditor);
        event.Skip();
    });
    m_rightEditor->Bind(wxEVT_STC_UPDATEUI, [this](wxStyledTextEvent& event)
    {
        SyncScroll(m_rightEditor, m_leftEditor);
        event.Skip();
    });

    UpdateNavigation();
}

void FileComparePage::ConfigureEditor(wxStyledTextCtrl* editor, bool rightSide)
{
    editor->SetReadOnly(false);
    editor->SetLexer(wxSTC_LEX_NULL);
    editor->StyleSetBackground(wxSTC_STYLE_DEFAULT, EditorBackground());
    editor->StyleSetForeground(wxSTC_STYLE_DEFAULT, wxColour(220, 220, 220));
    editor->StyleSetFaceName(wxSTC_STYLE_DEFAULT, "Monospace");
    editor->StyleSetSize(wxSTC_STYLE_DEFAULT, 10);
    editor->StyleClearAll();
    editor->SetWrapMode(wxSTC_WRAP_NONE);
    editor->SetMarginWidth(0, 0);
    editor->SetCaretLineVisible(false);

    editor->MarkerDefine(MARK_CHANGED, wxSTC_MARK_BACKGROUND);
    editor->MarkerSetBackground(MARK_CHANGED, wxColour(92, 76, 26));
    editor->MarkerDefine(MARK_SIDE_ONLY, wxSTC_MARK_BACKGROUND);
    editor->MarkerSetBackground(MARK_SIDE_ONLY,
                                rightSide ? wxColour(31, 72, 42) : wxColour(86, 38, 38));
    editor->SetReadOnly(true);
}

void FileComparePage::BrowseLeft()
{
    wxFileDialog dialog(this, "Select left file", wxString(), wxString(),
                        "All files (*.*)|*.*", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dialog.ShowModal() != wxID_OK)
        return;
    m_leftPath = dialog.GetPath();
    UpdateCaptions();
    CompareFiles();
}

void FileComparePage::BrowseRight()
{
    wxFileDialog dialog(this, "Select right file", wxString(), wxString(),
                        "All files (*.*)|*.*", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dialog.ShowModal() != wxID_OK)
        return;
    m_rightPath = dialog.GetPath();
    UpdateCaptions();
    CompareFiles();
}

void FileComparePage::SwapFiles()
{
    std::swap(m_leftPath, m_rightPath);
    UpdateCaptions();
    CompareFiles();
}

FileComparePage::CompareMode FileComparePage::SelectedMode() const
{
    if (!m_modeChoice)
        return CompareMode::Auto;
    const int selection = m_modeChoice->GetSelection();
    if (selection == 1)
        return CompareMode::Text;
    if (selection == 2)
        return CompareMode::Binary;
    return CompareMode::Auto;
}

bool FileComparePage::ReadFileBytes(const wxString& path,
                                    std::vector<unsigned char>& bytes,
                                    wxString* error) const
{
    bytes.clear();
    wxFFile file(path, "rb");
    if (!file.IsOpened())
    {
        if (error)
            *error = wxString("Unable to open: ") + path;
        return false;
    }

    const wxFileOffset length = file.Length();
    if (length == wxInvalidOffset || length < 0)
    {
        if (error)
            *error = wxString("Unable to determine file size: ") + path;
        return false;
    }

    if (static_cast<unsigned long long>(length) >
        static_cast<unsigned long long>(std::numeric_limits<size_t>::max()))
    {
        if (error)
            *error = wxString("File is too large to compare: ") + path;
        return false;
    }

    bytes.resize(static_cast<size_t>(length));
    if (!bytes.empty())
    {
        const size_t count = file.Read(bytes.data(), bytes.size());
        if (count != bytes.size())
        {
            if (error)
                *error = wxString("Unable to read complete file: ") + path;
            return false;
        }
    }
    return true;
}

bool FileComparePage::LooksBinary(const std::vector<unsigned char>& bytes) const
{
    if (bytes.empty())
        return false;

    const size_t sample = std::min<size_t>(bytes.size(), 65536);
    size_t controls = 0;
    for (size_t i = 0; i < sample; ++i)
    {
        const unsigned char value = bytes[i];
        if (value == 0)
            return true;
        if (value < 0x20 && value != '\n' && value != '\r' && value != '\t' && value != '\f')
            ++controls;
    }
    return controls * 100 > sample * 3;
}

wxString FileComparePage::DecodeText(const std::vector<unsigned char>& bytes) const
{
    if (bytes.empty())
        return wxString();

    wxString text = wxString::FromUTF8(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    if (!text.IsEmpty())
        return text;
    return wxString::From8BitData(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

void FileComparePage::CompareFiles()
{
    UpdateCaptions();
    m_differenceRows.clear();
    m_currentDifference = -1;
    ClearMarkers();

    if (m_leftPath.IsEmpty() || m_rightPath.IsEmpty())
    {
        SetEditorsText(wxString(), wxString());
        m_status->SetLabel("Load both left and right files to compare.");
        UpdateNavigation();
        return;
    }

    std::vector<unsigned char> leftBytes;
    std::vector<unsigned char> rightBytes;
    wxString error;
    if (!ReadFileBytes(m_leftPath, leftBytes, &error) ||
        !ReadFileBytes(m_rightPath, rightBytes, &error))
    {
        SetEditorsText(wxString(), wxString());
        m_status->SetLabel(error);
        UpdateNavigation();
        return;
    }

    CompareMode mode = SelectedMode();
    if (mode == CompareMode::Auto)
        mode = (LooksBinary(leftBytes) || LooksBinary(rightBytes)) ? CompareMode::Binary : CompareMode::Text;

    const bool textMode = mode == CompareMode::Text;
    m_ignoreWhitespace->Enable(textMode);
    m_ignoreCase->Enable(textMode);

    if (textMode)
        CompareText(leftBytes, rightBytes);
    else
        CompareBinary(leftBytes, rightBytes);

    UpdateNavigation();
}

std::vector<wxString> FileComparePage::SplitLines(const wxString& text) const
{
    std::vector<wxString> result;
    size_t start = 0;
    while (start <= text.length())
    {
        const size_t pos = text.find('\n', start);
        wxString line;
        if (pos == wxString::npos)
            line = text.Mid(start);
        else
            line = text.Mid(start, pos - start);
        if (!line.IsEmpty() && line.Last() == '\r')
            line.RemoveLast();
        result.push_back(line);
        if (pos == wxString::npos)
            break;
        start = pos + 1;
    }
    return result;
}

wxString FileComparePage::ComparisonKey(const wxString& value) const
{
    wxString key = value;
    if (m_ignoreWhitespace && m_ignoreWhitespace->GetValue())
    {
        wxString compact;
        bool previousSpace = false;
        for (wxUniChar ch : key)
        {
            const bool isSpace = ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
            if (isSpace)
            {
                if (!previousSpace && !compact.IsEmpty())
                    compact += ' ';
                previousSpace = true;
            }
            else
            {
                compact += ch;
                previousSpace = false;
            }
        }
        key = compact;
        key.Trim(true);
        key.Trim(false);
    }
    if (m_ignoreCase && m_ignoreCase->GetValue())
        key.MakeLower();
    return key;
}

std::vector<FileComparePage::TextRow> FileComparePage::BuildAlignedTextRows(const wxString& leftText,
                                                                            const wxString& rightText) const
{
    const std::vector<wxString> left = SplitLines(leftText);
    const std::vector<wxString> right = SplitLines(rightText);
    std::vector<wxString> leftKeys(left.size());
    std::vector<wxString> rightKeys(right.size());
    for (size_t i = 0; i < left.size(); ++i)
        leftKeys[i] = ComparisonKey(left[i]);
    for (size_t i = 0; i < right.size(); ++i)
        rightKeys[i] = ComparisonKey(right[i]);

    struct Op
    {
        int side = 0; // 0 equal, -1 left only, +1 right only
        size_t index = 0;
        size_t other = 0;
    };
    std::vector<Op> ops;

    const size_t width = right.size() + 1;
    const size_t leftHeight = left.size() + 1;
    const bool canUseLcs = width > 0 && leftHeight <= (MAX_LCS_CELLS / width);
    if (canUseLcs)
    {
        const size_t cells = leftHeight * width;
        std::vector<uint32_t> dp(cells, 0);
        for (size_t ii = left.size(); ii-- > 0;)
        {
            for (size_t jj = right.size(); jj-- > 0;)
            {
                const size_t at = ii * width + jj;
                if (leftKeys[ii] == rightKeys[jj])
                    dp[at] = dp[(ii + 1) * width + (jj + 1)] + 1;
                else
                    dp[at] = std::max(dp[(ii + 1) * width + jj], dp[ii * width + (jj + 1)]);
            }
        }

        size_t i = 0;
        size_t j = 0;
        while (i < left.size() && j < right.size())
        {
            if (leftKeys[i] == rightKeys[j])
            {
                ops.push_back({0, i, j});
                ++i;
                ++j;
            }
            else if (dp[(i + 1) * width + j] >= dp[i * width + (j + 1)])
            {
                ops.push_back({-1, i, 0});
                ++i;
            }
            else
            {
                ops.push_back({+1, j, 0});
                ++j;
            }
        }
        while (i < left.size())
        {
            ops.push_back({-1, i, 0});
            ++i;
        }
        while (j < right.size())
        {
            ops.push_back({+1, j, 0});
            ++j;
        }
    }
    else
    {
        const size_t common = std::min(left.size(), right.size());
        for (size_t i = 0; i < common; ++i)
        {
            if (leftKeys[i] == rightKeys[i])
                ops.push_back({0, i, i});
            else
            {
                ops.push_back({-1, i, 0});
                ops.push_back({+1, i, 0});
            }
        }
        for (size_t i = common; i < left.size(); ++i)
            ops.push_back({-1, i, 0});
        for (size_t i = common; i < right.size(); ++i)
            ops.push_back({+1, i, 0});
    }

    std::vector<TextRow> rows;
    std::vector<size_t> pendingLeft;
    std::vector<size_t> pendingRight;

    auto flushPending = [&]()
    {
        const size_t count = std::max(pendingLeft.size(), pendingRight.size());
        for (size_t k = 0; k < count; ++k)
        {
            TextRow row;
            const bool haveLeft = k < pendingLeft.size();
            const bool haveRight = k < pendingRight.size();
            if (haveLeft)
            {
                row.left = left[pendingLeft[k]];
                row.leftLine = static_cast<int>(pendingLeft[k] + 1);
            }
            if (haveRight)
            {
                row.right = right[pendingRight[k]];
                row.rightLine = static_cast<int>(pendingRight[k] + 1);
            }
            if (haveLeft && haveRight)
                row.kind = RowKind::Changed;
            else if (haveLeft)
                row.kind = RowKind::LeftOnly;
            else
                row.kind = RowKind::RightOnly;
            rows.push_back(std::move(row));
        }
        pendingLeft.clear();
        pendingRight.clear();
    };

    for (const Op& op : ops)
    {
        if (op.side == 0)
        {
            flushPending();
            TextRow row;
            row.left = left[op.index];
            row.right = right[op.other];
            row.leftLine = static_cast<int>(op.index + 1);
            row.rightLine = static_cast<int>(op.other + 1);
            row.kind = RowKind::Equal;
            rows.push_back(std::move(row));
        }
        else if (op.side < 0)
        {
            pendingLeft.push_back(op.index);
        }
        else
        {
            pendingRight.push_back(op.index);
        }
    }
    flushPending();
    return rows;
}

wxString FileComparePage::FormatTextLine(int sourceLine, const wxString& text)
{
    wxString result;
    if (sourceLine > 0)
        result = wxString::Format("%6d | ", sourceLine);
    else
        result = "       | ";
    result += text;
    return result;
}

void FileComparePage::CompareText(const std::vector<unsigned char>& leftBytes,
                                  const std::vector<unsigned char>& rightBytes)
{
    const wxString leftText = DecodeText(leftBytes);
    const wxString rightText = DecodeText(rightBytes);
    const std::vector<TextRow> rows = BuildAlignedTextRows(leftText, rightText);

    wxString leftDisplay;
    wxString rightDisplay;
    int added = 0;
    int deleted = 0;
    int changed = 0;

    for (size_t i = 0; i < rows.size(); ++i)
    {
        if (i > 0)
        {
            leftDisplay += '\n';
            rightDisplay += '\n';
        }
        leftDisplay += FormatTextLine(rows[i].leftLine, rows[i].left);
        rightDisplay += FormatTextLine(rows[i].rightLine, rows[i].right);

        if (rows[i].kind != RowKind::Equal)
            m_differenceRows.push_back(static_cast<int>(i));
        if (rows[i].kind == RowKind::Changed)
            ++changed;
        else if (rows[i].kind == RowKind::LeftOnly)
            ++deleted;
        else if (rows[i].kind == RowKind::RightOnly)
            ++added;
    }

    SetEditorsText(leftDisplay, rightDisplay);
    for (size_t i = 0; i < rows.size(); ++i)
        MarkRow(static_cast<int>(i), rows[i].kind);

    if (m_differenceRows.empty())
    {
        m_status->SetLabel(wxString::Format("Text files are identical (%llu aligned lines).",
                                            static_cast<unsigned long long>(rows.size())));
    }
    else
    {
        m_status->SetLabel(wxString::Format("Text comparison: %llu difference rows — %d changed, %d left-only, %d right-only.",
                                            static_cast<unsigned long long>(m_differenceRows.size()),
                                            changed, deleted, added));
    }
}

wxString FileComparePage::FormatHexLine(size_t offset,
                                        const std::vector<unsigned char>& bytes,
                                        size_t start,
                                        size_t count)
{
    std::ostringstream out;
    out << std::uppercase << std::hex << std::setfill('0') << std::setw(8) << offset << "  ";
    for (size_t i = 0; i < BINARY_ROW_BYTES; ++i)
    {
        if (i < count)
            out << std::setw(2) << static_cast<unsigned int>(bytes[start + i]) << ' ';
        else
            out << "   ";
        if (i == 7)
            out << ' ';
    }
    out << " |";
    for (size_t i = 0; i < count; ++i)
    {
        const unsigned char value = bytes[start + i];
        out << ((value >= 32 && value <= 126) ? static_cast<char>(value) : '.');
    }
    for (size_t i = count; i < BINARY_ROW_BYTES; ++i)
        out << ' ';
    out << '|';
    return wxString::FromUTF8(out.str().c_str());
}

void FileComparePage::CompareBinary(const std::vector<unsigned char>& leftBytes,
                                    const std::vector<unsigned char>& rightBytes)
{
    const size_t maxSize = std::max(leftBytes.size(), rightBytes.size());
    const size_t rows = (maxSize + BINARY_ROW_BYTES - 1) / BINARY_ROW_BYTES;
    wxString leftDisplay;
    wxString rightDisplay;
    size_t differingBytes = 0;
    size_t firstDifference = std::numeric_limits<size_t>::max();

    for (size_t offset = 0; offset < maxSize; ++offset)
    {
        const bool haveLeft = offset < leftBytes.size();
        const bool haveRight = offset < rightBytes.size();
        const bool different = !haveLeft || !haveRight || leftBytes[offset] != rightBytes[offset];
        if (different)
        {
            ++differingBytes;
            if (firstDifference == std::numeric_limits<size_t>::max())
                firstDifference = offset;
        }
    }

    for (size_t row = 0; row < rows; ++row)
    {
        const size_t start = row * BINARY_ROW_BYTES;
        const size_t leftCount = start < leftBytes.size()
                                     ? std::min(BINARY_ROW_BYTES, leftBytes.size() - start)
                                     : 0;
        const size_t rightCount = start < rightBytes.size()
                                      ? std::min(BINARY_ROW_BYTES, rightBytes.size() - start)
                                      : 0;
        if (row > 0)
        {
            leftDisplay += '\n';
            rightDisplay += '\n';
        }
        leftDisplay += FormatHexLine(start, leftBytes, start, leftCount);
        rightDisplay += FormatHexLine(start, rightBytes, start, rightCount);

        bool rowDifferent = leftCount != rightCount;
        const size_t common = std::min(leftCount, rightCount);
        for (size_t i = 0; i < common && !rowDifferent; ++i)
            rowDifferent = leftBytes[start + i] != rightBytes[start + i];
        if (rowDifferent)
            m_differenceRows.push_back(static_cast<int>(row));
    }

    SetEditorsText(leftDisplay, rightDisplay);
    for (int row : m_differenceRows)
    {
        m_leftEditor->MarkerAdd(row, MARK_SIDE_ONLY);
        m_rightEditor->MarkerAdd(row, MARK_SIDE_ONLY);
    }

    if (differingBytes == 0)
    {
        m_status->SetLabel(wxString::Format("Binary files are identical (%llu bytes).",
                                            static_cast<unsigned long long>(leftBytes.size())));
    }
    else
    {
        m_status->SetLabel(wxString::Format("Binary comparison: %llu differing bytes, %llu difference rows; first difference at 0x%llX. Left=%llu bytes, Right=%llu bytes.",
                                            static_cast<unsigned long long>(differingBytes),
                                            static_cast<unsigned long long>(m_differenceRows.size()),
                                            static_cast<unsigned long long>(firstDifference),
                                            static_cast<unsigned long long>(leftBytes.size()),
                                            static_cast<unsigned long long>(rightBytes.size())));
    }
}

void FileComparePage::SetEditorsText(const wxString& leftText, const wxString& rightText)
{
    m_leftEditor->SetReadOnly(false);
    m_rightEditor->SetReadOnly(false);
    m_leftEditor->SetText(leftText);
    m_rightEditor->SetText(rightText);
    m_leftEditor->EmptyUndoBuffer();
    m_rightEditor->EmptyUndoBuffer();
    m_leftEditor->SetSavePoint();
    m_rightEditor->SetSavePoint();
    m_leftEditor->SetReadOnly(true);
    m_rightEditor->SetReadOnly(true);
    ClearMarkers();
}

void FileComparePage::ClearMarkers()
{
    if (m_leftEditor)
    {
        m_leftEditor->MarkerDeleteAll(MARK_CHANGED);
        m_leftEditor->MarkerDeleteAll(MARK_SIDE_ONLY);
    }
    if (m_rightEditor)
    {
        m_rightEditor->MarkerDeleteAll(MARK_CHANGED);
        m_rightEditor->MarkerDeleteAll(MARK_SIDE_ONLY);
    }
}

void FileComparePage::MarkRow(int row, RowKind kind)
{
    if (kind == RowKind::Equal)
        return;
    if (kind == RowKind::Changed)
    {
        m_leftEditor->MarkerAdd(row, MARK_CHANGED);
        m_rightEditor->MarkerAdd(row, MARK_CHANGED);
    }
    else if (kind == RowKind::LeftOnly)
    {
        m_leftEditor->MarkerAdd(row, MARK_SIDE_ONLY);
    }
    else if (kind == RowKind::RightOnly)
    {
        m_rightEditor->MarkerAdd(row, MARK_SIDE_ONLY);
    }
}

void FileComparePage::NavigateDifference(int delta)
{
    if (m_differenceRows.empty())
        return;

    if (m_currentDifference < 0)
        m_currentDifference = delta < 0 ? static_cast<int>(m_differenceRows.size()) - 1 : 0;
    else
    {
        m_currentDifference += delta;
        if (m_currentDifference < 0)
            m_currentDifference = static_cast<int>(m_differenceRows.size()) - 1;
        if (m_currentDifference >= static_cast<int>(m_differenceRows.size()))
            m_currentDifference = 0;
    }
    ScrollToDifference(m_differenceRows[static_cast<size_t>(m_currentDifference)]);
    UpdateNavigation();
}

void FileComparePage::ScrollToDifference(int row)
{
    for (wxStyledTextCtrl* editor : {m_leftEditor, m_rightEditor})
    {
        editor->GotoLine(row);
        const int first = std::max(0, row - 3);
        const int delta = first - editor->GetFirstVisibleLine();
        if (delta != 0)
            editor->LineScroll(0, delta);
    }
}

void FileComparePage::SyncScroll(wxStyledTextCtrl* source, wxStyledTextCtrl* target)
{
    if (m_syncingScroll || !source || !target)
        return;
    const int sourceFirst = source->GetFirstVisibleLine();
    const int targetFirst = target->GetFirstVisibleLine();
    const int delta = sourceFirst - targetFirst;
    const int sourceX = source->GetXOffset();
    const int targetX = target->GetXOffset();
    if (delta == 0 && sourceX == targetX)
        return;
    m_syncingScroll = true;
    if (delta != 0)
        target->LineScroll(0, delta);
    if (sourceX != targetX)
        target->SetXOffset(sourceX);
    m_syncingScroll = false;
}

void FileComparePage::UpdateNavigation()
{
    const bool enabled = !m_differenceRows.empty();
    if (m_prevButton)
        m_prevButton->Enable(enabled);
    if (m_nextButton)
        m_nextButton->Enable(enabled);
}

void FileComparePage::UpdateCaptions()
{
    if (m_leftPathCtrl)
        m_leftPathCtrl->SetValue(m_leftPath);
    if (m_rightPathCtrl)
        m_rightPathCtrl->SetValue(m_rightPath);

    wxWindow* parent = GetParent();
    if (!parent)
        return;
    auto* notebook = dynamic_cast<wxAuiNotebook*>(parent);
    if (!notebook)
        return;
    const int index = notebook->GetPageIndex(this);
    if (index != wxNOT_FOUND)
        notebook->SetPageText(index, GetTabTitle());
}

#endif // DODEV_ENABLE_FILE_COMPARE
