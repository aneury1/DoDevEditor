#include "MainFrame.h"

#include "LogNormalizer.h"

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/dnd.h>
#include <wx/filedlg.h>
#include <wx/filepicker.h>
#include <wx/font.h>
#include <wx/filename.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/splitter.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/statusbr.h>
#include <wx/utils.h>
#include <wx/stc/stc.h>

#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace
{

enum ControlId
{
    IdOpenLeft = wxID_HIGHEST + 1,
    IdOpenRight,
    IdCompare,
    IdSwap,
    IdPreviousDifference,
    IdNextDifference,
    IdIgnoreTimestamp,
    IdIgnoreWhitespace,
    IdIgnoreCase,
    IdAutoCompare,
    IdLeftPicker,
    IdRightPicker
};

enum EditorStyle
{
    StyleEqual = 10,
    StyleRemoved,
    StyleAdded,
    StyleModifiedLeft,
    StyleModifiedRight,
    StylePlaceholder
};

enum EditorMarker
{
    MarkerRemoved = 1,
    MarkerAdded,
    MarkerModified
};

class SideFileDropTarget final : public wxFileDropTarget
{
public:
    SideFileDropTarget(MainFrame& owner, bool leftSide)
        : m_owner(owner),
          m_leftSide(leftSide)
    {
    }

    bool OnDropFiles(wxCoord, wxCoord, const wxArrayString& filenames) override
    {
        if (filenames.empty())
        {
            return false;
        }

        m_owner.SetFileForSide(m_leftSide, filenames[0]);
        return true;
    }

private:
    MainFrame& m_owner;
    bool m_leftSide;
};

std::filesystem::path filesystemPath(const wxString& value)
{
    const auto utf8 = value.ToUTF8();
    if (utf8.data() == nullptr)
    {
        throw std::runtime_error("Unable to convert the selected path to UTF-8");
    }
    return std::filesystem::u8path(utf8.data());
}

std::string formattedLine(const std::optional<jld::SourceLine>& line)
{
    std::ostringstream output;
    if (line)
    {
        output << std::setw(7) << line->number << "  | " << line->text;
    }
    else
    {
        output << "         | ";
    }
    return output.str();
}

int styleFor(const jld::AlignedRow& row, MainFrame::Side side)
{
    const jld::DiffSideKind kind = side == MainFrame::Side::Left ? row.leftKind : row.rightKind;
    switch (kind)
    {
        case jld::DiffSideKind::Equal:
            return StyleEqual;
        case jld::DiffSideKind::Removed:
            return StyleRemoved;
        case jld::DiffSideKind::Added:
            return StyleAdded;
        case jld::DiffSideKind::Modified:
            return side == MainFrame::Side::Left ? StyleModifiedLeft : StyleModifiedRight;
        case jld::DiffSideKind::Placeholder:
            return StylePlaceholder;
    }
    return StyleEqual;
}

int markerFor(const jld::AlignedRow& row, MainFrame::Side side)
{
    const jld::DiffSideKind kind = side == MainFrame::Side::Left ? row.leftKind : row.rightKind;
    switch (kind)
    {
        case jld::DiffSideKind::Removed:
            return MarkerRemoved;
        case jld::DiffSideKind::Added:
            return MarkerAdded;
        case jld::DiffSideKind::Modified:
            return MarkerModified;
        case jld::DiffSideKind::Equal:
        case jld::DiffSideKind::Placeholder:
            return -1;
    }
    return -1;
}

} // namespace

MainFrame::MainFrame(const wxString& leftPath, const wxString& rightPath)
    : wxFrame(
          nullptr,
          wxID_ANY,
          "Journal Log Diff",
          wxDefaultPosition,
          wxSize(1400, 850),
          wxDEFAULT_FRAME_STYLE | wxCLIP_CHILDREN)
{
    SetMinSize(wxSize(900, 600));
    BuildMenu();
    BuildInterface();
    BindEvents();
    CreateStatusBar(1);
    SetStatusText("Choose two journal log files to compare");

    if (!leftPath.empty())
    {
        m_leftPicker->SetPath(leftPath);
    }
    if (!rightPath.empty())
    {
        m_rightPicker->SetPath(rightPath);
    }

    if (CanCompare())
    {
        CompareFiles(false);
    }

    Centre();
}

void MainFrame::SetFileForSide(bool leftSide, const wxString& path)
{
    wxFilePickerCtrl* picker = leftSide ? m_leftPicker : m_rightPicker;
    picker->SetPath(path);

    if (m_autoCompare->IsChecked() && CanCompare())
    {
        CompareFiles();
    }
}

void MainFrame::BuildMenu()
{
    auto* fileMenu = new wxMenu();
    fileMenu->Append(IdOpenLeft, "Open &reference log...\tCtrl+L");
    fileMenu->Append(IdOpenRight, "Open &new log...\tCtrl+R");
    fileMenu->AppendSeparator();
    fileMenu->Append(IdCompare, "&Compare\tCtrl+Enter");
    fileMenu->AppendSeparator();
    fileMenu->Append(wxID_EXIT, "E&xit\tAlt+F4");

    auto* navigateMenu = new wxMenu();
    navigateMenu->Append(IdPreviousDifference, "Previous difference\tF7");
    navigateMenu->Append(IdNextDifference, "Next difference\tF8");

    auto* helpMenu = new wxMenu();
    helpMenu->Append(wxID_ABOUT, "&About");

    auto* menuBar = new wxMenuBar();
    menuBar->Append(fileMenu, "&File");
    menuBar->Append(navigateMenu, "&Navigate");
    menuBar->Append(helpMenu, "&Help");
    SetMenuBar(menuBar);
}

void MainFrame::BuildInterface()
{
    auto* rootPanel = new wxPanel(this);
    auto* rootSizer = new wxBoxSizer(wxVERTICAL);

    auto* fileRow = new wxBoxSizer(wxHORIZONTAL);

    auto* leftGroup = new wxBoxSizer(wxVERTICAL);
    auto* leftLabel = new wxStaticText(rootPanel, wxID_ANY, "REFERENCE / OLDER LOG");
    auto leftFont = leftLabel->GetFont();
    leftFont.SetWeight(wxFONTWEIGHT_BOLD);
    leftLabel->SetFont(leftFont);
    leftGroup->Add(leftLabel, 0, wxBOTTOM, 4);
    m_leftPicker = new wxFilePickerCtrl(
        rootPanel,
        IdLeftPicker,
        {},
        "Select the reference journal log",
        "Log and text files (*.log;*.txt)|*.log;*.txt|All files (*.*)|*.*",
        wxDefaultPosition,
        wxDefaultSize,
        wxFLP_OPEN | wxFLP_FILE_MUST_EXIST | wxFLP_USE_TEXTCTRL);
    leftGroup->Add(m_leftPicker, 1, wxEXPAND);

    auto* centerButtons = new wxBoxSizer(wxVERTICAL);
    centerButtons->AddStretchSpacer();
    auto* swapButton = new wxButton(rootPanel, IdSwap, "<-> Swap");
    swapButton->SetToolTip("Swap the reference and new log files");
    centerButtons->Add(swapButton, 0, wxLEFT | wxRIGHT, 10);

    auto* rightGroup = new wxBoxSizer(wxVERTICAL);
    auto* rightLabel = new wxStaticText(rootPanel, wxID_ANY, "NEW / CANDIDATE LOG");
    auto rightFont = rightLabel->GetFont();
    rightFont.SetWeight(wxFONTWEIGHT_BOLD);
    rightLabel->SetFont(rightFont);
    rightGroup->Add(rightLabel, 0, wxBOTTOM, 4);
    m_rightPicker = new wxFilePickerCtrl(
        rootPanel,
        IdRightPicker,
        {},
        "Select the new journal log",
        "Log and text files (*.log;*.txt)|*.log;*.txt|All files (*.*)|*.*",
        wxDefaultPosition,
        wxDefaultSize,
        wxFLP_OPEN | wxFLP_FILE_MUST_EXIST | wxFLP_USE_TEXTCTRL);
    rightGroup->Add(m_rightPicker, 1, wxEXPAND);

    fileRow->Add(leftGroup, 1, wxEXPAND);
    fileRow->Add(centerButtons, 0, wxEXPAND);
    fileRow->Add(rightGroup, 1, wxEXPAND);
    rootSizer->Add(fileRow, 0, wxEXPAND | wxALL, 10);

    auto* optionRow = new wxBoxSizer(wxHORIZONTAL);
    m_ignoreTimestamp = new wxCheckBox(rootPanel, IdIgnoreTimestamp, "Ignore journal timestamps");
    m_ignoreTimestamp->SetValue(true);
    m_ignoreTimestamp->SetToolTip("Ignore timestamp prefixes while matching lines; original timestamps remain visible");

    m_ignoreWhitespace = new wxCheckBox(rootPanel, IdIgnoreWhitespace, "Ignore whitespace");
    m_ignoreCase = new wxCheckBox(rootPanel, IdIgnoreCase, "Ignore case");
    m_autoCompare = new wxCheckBox(rootPanel, IdAutoCompare, "Compare automatically");
    m_autoCompare->SetValue(true);

    auto* compareButton = new wxButton(rootPanel, IdCompare, "Compare now");
    auto* previousButton = new wxButton(rootPanel, IdPreviousDifference, "<< Previous");
    auto* nextButton = new wxButton(rootPanel, IdNextDifference, "Next >>");

    optionRow->Add(m_ignoreTimestamp, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 16);
    optionRow->Add(m_ignoreWhitespace, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 16);
    optionRow->Add(m_ignoreCase, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 16);
    optionRow->Add(m_autoCompare, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 16);
    optionRow->AddStretchSpacer();
    optionRow->Add(previousButton, 0, wxRIGHT, 6);
    optionRow->Add(nextButton, 0, wxRIGHT, 10);
    optionRow->Add(compareButton, 0);
    rootSizer->Add(optionRow, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

    rootSizer->Add(new wxStaticLine(rootPanel), 0, wxEXPAND);

    m_splitter = new wxSplitterWindow(
        rootPanel,
        wxID_ANY,
        wxDefaultPosition,
        wxDefaultSize,
        wxSP_LIVE_UPDATE | wxSP_3DSASH | wxCLIP_CHILDREN);
    m_splitter->SetMinimumPaneSize(250);
    m_splitter->SetSashGravity(0.5);

    auto* leftPanel = new wxPanel(m_splitter);
    auto* leftSizer = new wxBoxSizer(wxVERTICAL);
    auto* leftHeader = new wxStaticText(leftPanel, wxID_ANY, "Reference");
    leftHeader->SetBackgroundColour(wxColour(242, 242, 242));
    leftSizer->Add(leftHeader, 0, wxEXPAND | wxALL, 5);
    m_leftEditor = new wxStyledTextCtrl(leftPanel, wxID_ANY);
    ConfigureEditor(m_leftEditor, true);
    leftSizer->Add(m_leftEditor, 1, wxEXPAND);
    leftPanel->SetSizer(leftSizer);

    auto* rightPanel = new wxPanel(m_splitter);
    auto* rightSizer = new wxBoxSizer(wxVERTICAL);
    auto* rightHeader = new wxStaticText(rightPanel, wxID_ANY, "New");
    rightHeader->SetBackgroundColour(wxColour(242, 242, 242));
    rightSizer->Add(rightHeader, 0, wxEXPAND | wxALL, 5);
    m_rightEditor = new wxStyledTextCtrl(rightPanel, wxID_ANY);
    ConfigureEditor(m_rightEditor, false);
    rightSizer->Add(m_rightEditor, 1, wxEXPAND);
    rightPanel->SetSizer(rightSizer);

    m_leftEditor->SetDropTarget(new SideFileDropTarget(*this, true));
    m_rightEditor->SetDropTarget(new SideFileDropTarget(*this, false));

    m_splitter->SplitVertically(leftPanel, rightPanel);
    rootSizer->Add(m_splitter, 1, wxEXPAND);

    m_summaryText = new wxStaticText(rootPanel, wxID_ANY, "No comparison loaded");
    m_summaryText->SetBackgroundColour(wxColour(248, 248, 248));
    rootSizer->Add(m_summaryText, 0, wxEXPAND | wxALL, 8);

    rootPanel->SetSizer(rootSizer);
}

void MainFrame::ConfigureEditor(wxStyledTextCtrl* editor, bool leftSide)
{
    editor->SetReadOnly(false);
    editor->SetLexer(wxSTC_LEX_NULL);
    editor->SetCodePage(wxSTC_CP_UTF8);
    editor->SetWrapMode(wxSTC_WRAP_NONE);
    editor->SetUseTabs(false);
    editor->SetTabWidth(4);
    editor->SetViewEOL(false);
    editor->SetViewWhiteSpace(wxSTC_WS_INVISIBLE);
    editor->SetCaretLineVisible(true);
    editor->SetCaretLineBackground(wxColour(238, 244, 255));
    editor->SetMultipleSelection(false);
    editor->SetExtraAscent(1);
    editor->SetExtraDescent(1);

    editor->StyleSetFont(wxSTC_STYLE_DEFAULT, wxFontInfo(10).Family(wxFONTFAMILY_TELETYPE));
    editor->StyleSetForeground(wxSTC_STYLE_DEFAULT, wxColour(30, 30, 30));
    editor->StyleSetBackground(wxSTC_STYLE_DEFAULT, *wxWHITE);
    editor->StyleClearAll();

    editor->StyleSetForeground(StyleEqual, wxColour(30, 30, 30));
    editor->StyleSetBackground(StyleEqual, *wxWHITE);

    editor->StyleSetForeground(StyleRemoved, wxColour(125, 0, 0));
    editor->StyleSetBackground(StyleRemoved, wxColour(255, 220, 220));

    editor->StyleSetForeground(StyleAdded, wxColour(0, 88, 30));
    editor->StyleSetBackground(StyleAdded, wxColour(216, 247, 222));

    editor->StyleSetForeground(StyleModifiedLeft, wxColour(125, 0, 0));
    editor->StyleSetBackground(StyleModifiedLeft, wxColour(255, 205, 205));

    editor->StyleSetForeground(StyleModifiedRight, wxColour(0, 88, 30));
    editor->StyleSetBackground(StyleModifiedRight, wxColour(196, 241, 205));

    editor->StyleSetForeground(StylePlaceholder, wxColour(145, 145, 145));
    editor->StyleSetBackground(StylePlaceholder, wxColour(245, 245, 245));

    editor->SetMarginType(0, wxSTC_MARGIN_SYMBOL);
    editor->SetMarginWidth(0, 14);
    editor->SetMarginSensitive(0, false);
    editor->MarkerDefine(MarkerRemoved, wxSTC_MARK_BACKGROUND, wxColour(145, 0, 0), wxColour(255, 220, 220));
    editor->MarkerDefine(MarkerAdded, wxSTC_MARK_BACKGROUND, wxColour(0, 100, 40), wxColour(216, 247, 222));
    editor->MarkerDefine(MarkerModified, wxSTC_MARK_BACKGROUND, leftSide ? wxColour(145, 0, 0) : wxColour(0, 100, 40),
                         leftSide ? wxColour(255, 205, 205) : wxColour(196, 241, 205));

    editor->SetReadOnly(true);
}

void MainFrame::BindEvents()
{
    Bind(wxEVT_MENU, &MainFrame::OnOpenLeft, this, IdOpenLeft);
    Bind(wxEVT_MENU, &MainFrame::OnOpenRight, this, IdOpenRight);
    Bind(wxEVT_MENU, &MainFrame::OnCompare, this, IdCompare);
    Bind(wxEVT_MENU, &MainFrame::OnPreviousDifference, this, IdPreviousDifference);
    Bind(wxEVT_MENU, &MainFrame::OnNextDifference, this, IdNextDifference);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { Close(true); }, wxID_EXIT);
    Bind(wxEVT_MENU, &MainFrame::OnAbout, this, wxID_ABOUT);

    Bind(wxEVT_BUTTON, &MainFrame::OnCompare, this, IdCompare);
    Bind(wxEVT_BUTTON, &MainFrame::OnSwap, this, IdSwap);
    Bind(wxEVT_BUTTON, &MainFrame::OnPreviousDifference, this, IdPreviousDifference);
    Bind(wxEVT_BUTTON, &MainFrame::OnNextDifference, this, IdNextDifference);

    Bind(wxEVT_CHECKBOX, &MainFrame::OnOptionChanged, this, IdIgnoreTimestamp);
    Bind(wxEVT_CHECKBOX, &MainFrame::OnOptionChanged, this, IdIgnoreWhitespace);
    Bind(wxEVT_CHECKBOX, &MainFrame::OnOptionChanged, this, IdIgnoreCase);
    Bind(wxEVT_CHECKBOX, &MainFrame::OnOptionChanged, this, IdAutoCompare);

    Bind(wxEVT_FILEPICKER_CHANGED, &MainFrame::OnFileChanged, this, IdLeftPicker);
    Bind(wxEVT_FILEPICKER_CHANGED, &MainFrame::OnFileChanged, this, IdRightPicker);

    m_leftEditor->Bind(wxEVT_STC_UPDATEUI, &MainFrame::OnLeftUpdateUi, this);
    m_rightEditor->Bind(wxEVT_STC_UPDATEUI, &MainFrame::OnRightUpdateUi, this);
}

void MainFrame::CompareFiles(bool showErrors)
{
    if (!CanCompare())
    {
        if (showErrors)
        {
            wxMessageBox("Select both the reference log and the new log.", "Missing files", wxOK | wxICON_INFORMATION, this);
        }
        return;
    }

    try
    {
        SetStatusText("Comparing files...");
        wxBusyCursor busy;

        const jld::NormalizationOptions options {
            m_ignoreTimestamp->IsChecked(),
            m_ignoreWhitespace->IsChecked(),
            m_ignoreCase->IsChecked()
        };

        const auto leftLines = jld::LogNormalizer::loadFile(
            filesystemPath(m_leftPicker->GetPath()), options);
        const auto rightLines = jld::LogNormalizer::loadFile(
            filesystemPath(m_rightPicker->GetPath()), options);

        const jld::DiffResult result = jld::DiffEngine::compare(leftLines, rightLines);
        RenderResult(result);
        UpdateSummary(result);

        SetTitle(
            wxString("Journal Log Diff - ") +
            wxFileName(m_leftPicker->GetPath()).GetFullName() +
            " <-> " +
            wxFileName(m_rightPicker->GetPath()).GetFullName());
    }
    catch (const std::exception& error)
    {
        ClearEditors();
        SetStatusText("Comparison failed");
        if (showErrors)
        {
            wxMessageBox(wxString::FromUTF8(error.what()), "Comparison error", wxOK | wxICON_ERROR, this);
        }
    }
}

void MainFrame::RenderResult(const jld::DiffResult& result)
{
    m_differenceRows.clear();
    for (std::size_t row = 0; row < result.rows.size(); ++row)
    {
        if (result.rows[row].isDifferent())
        {
            m_differenceRows.push_back(row);
        }
    }

    m_hasCurrentDifference = false;
    m_currentDifference = 0;

    RenderSide(m_leftEditor, result, Side::Left);
    RenderSide(m_rightEditor, result, Side::Right);

    m_leftEditor->ScrollToLine(0);
    m_rightEditor->ScrollToLine(0);

    if (!m_differenceRows.empty())
    {
        m_currentDifference = 0;
        m_hasCurrentDifference = true;
    }
}

void MainFrame::RenderSide(wxStyledTextCtrl* editor, const jld::DiffResult& result, Side side)
{
    std::ostringstream output;
    for (std::size_t index = 0; index < result.rows.size(); ++index)
    {
        const auto& row = result.rows[index];
        output << formattedLine(side == Side::Left ? row.left : row.right);
        if (index + 1 < result.rows.size())
        {
            output << '\n';
        }
    }

    editor->SetReadOnly(false);
    const std::string rendered = output.str();
    editor->SetText(wxString::FromUTF8(rendered.c_str(), rendered.size()));
    editor->MarkerDeleteAll(-1);

    for (std::size_t index = 0; index < result.rows.size(); ++index)
    {
        const int line = static_cast<int>(index);
        const int start = editor->PositionFromLine(line);
        const int end = index + 1 < result.rows.size()
            ? editor->PositionFromLine(line + 1)
            : editor->GetTextLength();

        editor->StartStyling(start);
        editor->SetStyling(std::max(0, end - start), styleFor(result.rows[index], side));

        const int marker = markerFor(result.rows[index], side);
        if (marker >= 0)
        {
            editor->MarkerAdd(line, marker);
        }
    }

    editor->EmptyUndoBuffer();
    editor->SetSavePoint();
    editor->SetReadOnly(true);
}

void MainFrame::UpdateSummary(const jld::DiffResult& result)
{
    const auto& summary = result.summary;
    m_summaryText->SetLabel(wxString::Format(
        "Equal: %llu    Changed: %llu    Removed from reference: %llu    New in candidate: %llu    Total difference rows: %llu",
        static_cast<unsigned long long>(summary.equalRows),
        static_cast<unsigned long long>(summary.modifiedRows),
        static_cast<unsigned long long>(summary.removedRows),
        static_cast<unsigned long long>(summary.addedRows),
        static_cast<unsigned long long>(summary.differenceRows())));

    if (summary.differenceRows() == 0)
    {
        SetStatusText("Files are equivalent using the selected comparison options");
    }
    else
    {
        SetStatusText(wxString::Format("Comparison complete: %llu difference rows", static_cast<unsigned long long>(summary.differenceRows())));
    }
}

void MainFrame::ClearEditors()
{
    for (wxStyledTextCtrl* editor : {m_leftEditor, m_rightEditor})
    {
        editor->SetReadOnly(false);
        editor->ClearAll();
        editor->SetReadOnly(true);
    }
    m_differenceRows.clear();
    m_hasCurrentDifference = false;
    m_summaryText->SetLabel("No comparison loaded");
}

void MainFrame::OpenFile(Side side)
{
    wxFileDialog dialog(
        this,
        side == Side::Left ? "Open reference journal log" : "Open new journal log",
        {},
        {},
        "Log and text files (*.log;*.txt)|*.log;*.txt|All files (*.*)|*.*",
        wxFD_OPEN | wxFD_FILE_MUST_EXIST);

    if (dialog.ShowModal() != wxID_OK)
    {
        return;
    }

    SetFileForSide(side == Side::Left, dialog.GetPath());
}

void MainFrame::NavigateDifference(int direction)
{
    if (m_differenceRows.empty())
    {
        SetStatusText("No differences to navigate");
        return;
    }

    if (!m_hasCurrentDifference)
    {
        m_currentDifference = direction >= 0 ? 0 : m_differenceRows.size() - 1;
        m_hasCurrentDifference = true;
    }
    else if (direction >= 0)
    {
        m_currentDifference = (m_currentDifference + 1) % m_differenceRows.size();
    }
    else
    {
        m_currentDifference = m_currentDifference == 0
            ? m_differenceRows.size() - 1
            : m_currentDifference - 1;
    }

    ScrollBothToRow(m_differenceRows[m_currentDifference]);
    SetStatusText(wxString::Format(
        "Difference %llu of %llu - aligned row %llu",
        static_cast<unsigned long long>(m_currentDifference + 1),
        static_cast<unsigned long long>(m_differenceRows.size()),
        static_cast<unsigned long long>(m_differenceRows[m_currentDifference] + 1)));
}

void MainFrame::ScrollBothToRow(std::size_t row)
{
    const int line = static_cast<int>(row);
    m_synchronizingScroll = true;
    m_leftEditor->GotoLine(line);
    m_rightEditor->GotoLine(line);
    m_leftEditor->EnsureVisibleEnforcePolicy(line);
    m_rightEditor->EnsureVisibleEnforcePolicy(line);
    m_leftEditor->ScrollToLine(std::max(0, line - 3));
    m_rightEditor->ScrollToLine(std::max(0, line - 3));
    m_synchronizingScroll = false;
}

void MainFrame::SynchronizeScroll(wxStyledTextCtrl* source, wxStyledTextCtrl* target)
{
    if (m_synchronizingScroll)
    {
        return;
    }

    const int sourceFirstLine = source->GetFirstVisibleLine();
    if (target->GetFirstVisibleLine() == sourceFirstLine)
    {
        return;
    }

    m_synchronizingScroll = true;
    target->ScrollToLine(sourceFirstLine);
    target->SetXOffset(source->GetXOffset());
    m_synchronizingScroll = false;
}

bool MainFrame::CanCompare() const
{
    return !m_leftPicker->GetPath().empty() && !m_rightPicker->GetPath().empty();
}

void MainFrame::OnOpenLeft(wxCommandEvent&)
{
    OpenFile(Side::Left);
}

void MainFrame::OnOpenRight(wxCommandEvent&)
{
    OpenFile(Side::Right);
}

void MainFrame::OnCompare(wxCommandEvent&)
{
    CompareFiles();
}

void MainFrame::OnSwap(wxCommandEvent&)
{
    const wxString left = m_leftPicker->GetPath();
    m_leftPicker->SetPath(m_rightPicker->GetPath());
    m_rightPicker->SetPath(left);

    if (CanCompare())
    {
        CompareFiles();
    }
}

void MainFrame::OnOptionChanged(wxCommandEvent& event)
{
    if (event.GetId() == IdAutoCompare)
    {
        if (m_autoCompare->IsChecked() && CanCompare())
        {
            CompareFiles();
        }
        return;
    }

    if (m_autoCompare->IsChecked() && CanCompare())
    {
        CompareFiles();
    }
}

void MainFrame::OnFileChanged(wxFileDirPickerEvent&)
{
    if (m_autoCompare->IsChecked() && CanCompare())
    {
        CompareFiles();
    }
}

void MainFrame::OnPreviousDifference(wxCommandEvent&)
{
    NavigateDifference(-1);
}

void MainFrame::OnNextDifference(wxCommandEvent&)
{
    NavigateDifference(1);
}

void MainFrame::OnLeftUpdateUi(wxStyledTextEvent& event)
{
    SynchronizeScroll(m_leftEditor, m_rightEditor);
    event.Skip();
}

void MainFrame::OnRightUpdateUi(wxStyledTextEvent& event)
{
    SynchronizeScroll(m_rightEditor, m_leftEditor);
    event.Skip();
}

void MainFrame::OnAbout(wxCommandEvent&)
{
    wxMessageBox(
        "Journal Log Diff 1.0\n\n"
        "Side-by-side comparison for journalctl and syslog exports.\n"
        "Red marks removed or changed reference lines.\n"
        "Green marks new or changed candidate lines.",
        "About Journal Log Diff",
        wxOK | wxICON_INFORMATION,
        this);
}
