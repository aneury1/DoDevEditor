#include "GitDiffPage.h"

#include <wx/filename.h>
#include <wx/ffile.h>
#include <wx/utils.h>
#include <algorithm>

namespace
{
constexpr int MARKER_CHANGED = 1;

int ParseCount(const wxString& text)
{
    if (text.IsEmpty())
        return 1;
    long value = 1;
    if (!text.ToLong(&value))
        return 1;
    return std::max(0L, value);
}

bool ParseRange(const wxString& token, int& start, int& count)
{
    // token is -12,3 or +15,2
    if (token.length() < 2)
        return false;

    wxString body = token.Mid(1);
    const int comma = body.Find(',');
    wxString startText = comma == wxNOT_FOUND ? body : body.Left(comma);
    wxString countText = comma == wxNOT_FOUND ? wxString("1") : body.Mid(comma + 1);

    long startLong = 0;
    if (!startText.ToLong(&startLong))
        return false;

    start = static_cast<int>(startLong);
    count = ParseCount(countText);
    return true;
}
}

GitDiffPage::GitDiffPage(wxWindow* parent,
                         const wxString& repositoryRoot,
                         const wxString& gitPath,
                         const wxString& status)
    : wxPanel(parent, wxID_ANY),
      m_repositoryRoot(repositoryRoot),
      m_gitPath(gitPath),
      m_status(status)
{
    SetBackgroundColour(wxColour(30, 30, 30));
    SplitRenamePath(gitPath, m_oldPath, m_newPath);
    SetModeFromStatus();

    auto* root = new wxBoxSizer(wxVERTICAL);

    auto* toolbar = new wxPanel(this);
    toolbar->SetBackgroundColour(wxColour(37, 37, 38));
    auto* toolbarSizer = new wxBoxSizer(wxHORIZONTAL);

    m_title = new wxStaticText(toolbar, wxID_ANY, wxEmptyString);
    m_title->SetForegroundColour(wxColour(230, 230, 230));
    wxFont titleFont = m_title->GetFont();
    titleFont.SetWeight(wxFONTWEIGHT_BOLD);
    m_title->SetFont(titleFont);

    m_modeLabel = new wxStaticText(toolbar, wxID_ANY, wxEmptyString);
    m_modeLabel->SetForegroundColour(wxColour(160, 160, 160));

    auto* refreshButton = new wxButton(toolbar, wxID_ANY, "Refresh", wxDefaultPosition, wxSize(70, 28));
    m_stageButton = new wxButton(toolbar, wxID_ANY, "Stage", wxDefaultPosition, wxSize(62, 28));
    m_unstageButton = new wxButton(toolbar, wxID_ANY, "Unstage", wxDefaultPosition, wxSize(72, 28));
    auto* discardButton = new wxButton(toolbar, wxID_ANY, "Discard", wxDefaultPosition, wxSize(70, 28));
    auto* openButton = new wxButton(toolbar, wxID_ANY, "Open File", wxDefaultPosition, wxSize(78, 28));

    toolbarSizer->Add(m_title, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, 8);
    toolbarSizer->Add(m_modeLabel, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 8);
    for (auto* button : {refreshButton, m_stageButton, m_unstageButton, discardButton, openButton})
        toolbarSizer->Add(button, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
    toolbar->SetSizer(toolbarSizer);
    root->Add(toolbar, 0, wxEXPAND);

    auto* captions = new wxPanel(this);
    captions->SetBackgroundColour(wxColour(30, 30, 30));
    auto* captionSizer = new wxBoxSizer(wxHORIZONTAL);
    auto* leftCaption = new wxStaticText(captions, wxID_ANY, "Original");
    auto* rightCaption = new wxStaticText(captions, wxID_ANY, "Modified");
    leftCaption->SetForegroundColour(wxColour(170, 170, 170));
    rightCaption->SetForegroundColour(wxColour(170, 170, 170));
    captionSizer->Add(leftCaption, 1, wxLEFT | wxTOP | wxBOTTOM, 8);
    captionSizer->Add(rightCaption, 1, wxLEFT | wxTOP | wxBOTTOM, 8);
    captions->SetSizer(captionSizer);
    root->Add(captions, 0, wxEXPAND);

    auto* splitter = new wxSplitterWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                          wxSP_LIVE_UPDATE | wxSP_3DSASH);
    splitter->SetMinimumPaneSize(100);
    splitter->SetSashGravity(0.5);
    m_left = new wxStyledTextCtrl(splitter);
    m_right = new wxStyledTextCtrl(splitter);
    ConfigureEditor(m_left, false);
    ConfigureEditor(m_right, true);
    splitter->SplitVertically(m_left, m_right);
    root->Add(splitter, 1, wxEXPAND);

    SetSizer(root);

    refreshButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { RefreshDiff(); });
    m_stageButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { Stage(); });
    m_unstageButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { Unstage(); });
    discardButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { Discard(); });
    openButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { OpenFile(); });

    RefreshDiff();
}

void GitDiffPage::SetOpenFileCallback(std::function<void(const wxString&)> callback)
{
    m_openFileCallback = std::move(callback);
}

void GitDiffPage::SetRepositoryChangedCallback(std::function<void()> callback)
{
    m_repositoryChangedCallback = std::move(callback);
}

wxString GitDiffPage::GetTabTitle() const
{
    return wxFileName(m_newPath).GetFullName() + " (Diff)";
}

wxString GitDiffPage::GetPatchForAI() const
{
    wxString patch = DiffPatch();
    if (!patch.IsEmpty())
        return patch;
    if (m_status == "??")
        return wxString("Untracked file: ") + m_newPath + "\n\n" + ReadWorkingTreeFile();
    return patch;
}

wxString GitDiffPage::Quote(const wxString& value) const
{
#ifdef __WXMSW__
    wxString escaped = value;
    escaped.Replace("\"", "\\\"");
    return "\"" + escaped + "\"";
#else
    wxString escaped = value;
    escaped.Replace("'", "'\"'\"'");
    return "'" + escaped + "'";
#endif
}

bool GitDiffPage::RunGit(const wxString& args, wxArrayString& output, wxArrayString& errors) const
{
    if (m_repositoryRoot.IsEmpty())
        return false;
    const wxString command = "git -C " + Quote(m_repositoryRoot) + " " + args;
    return wxExecute(command, output, errors, wxEXEC_SYNC) == 0;
}

wxString GitDiffPage::RunGitText(const wxString& args, bool* ok) const
{
    wxArrayString output;
    wxArrayString errors;
    const bool success = RunGit(args, output, errors);
    if (ok)
        *ok = success;
    if (!success)
        return wxString();

    wxString text;
    for (size_t i = 0; i < output.size(); ++i)
    {
        text += output[i];
        if (i + 1 < output.size())
            text += "\n";
    }
    if (!output.IsEmpty())
        text += "\n";
    return text;
}

wxString GitDiffPage::AbsolutePath(const wxString& relative) const
{
    wxString fullPath = m_repositoryRoot;
    fullPath += wxFileName::GetPathSeparator();
    fullPath += relative;
    wxFileName file(fullPath);
    file.Normalize();
    return file.GetFullPath();
}

wxString GitDiffPage::ReadWorkingTreeFile() const
{
    const wxString path = AbsolutePath(m_newPath);
    if (!wxFileExists(path))
        return wxString();

    wxFFile file(path, "rb");
    if (!file.IsOpened())
        return wxString();
    wxString text;
    file.ReadAll(&text, wxConvUTF8);
    return text;
}

wxString GitDiffPage::ReadIndexFile() const
{
    bool ok = false;
    wxString text = RunGitText("show --textconv :" + Quote(m_newPath), &ok);
    if (!ok && m_oldPath != m_newPath)
        text = RunGitText("show --textconv :" + Quote(m_oldPath), &ok);
    return ok ? text : wxString();
}

wxString GitDiffPage::ReadHeadFile() const
{
    bool ok = false;
    wxString text = RunGitText("show --textconv HEAD:" + Quote(m_oldPath), &ok);
    if (!ok && m_oldPath != m_newPath)
        text = RunGitText("show --textconv HEAD:" + Quote(m_newPath), &ok);
    return ok ? text : wxString();
}

wxString GitDiffPage::DiffPatch() const
{
    if (m_status == "??")
        return wxString();

    wxString cached;
    if (m_mode == DiffMode::Staged)
        cached = "--cached ";
    return RunGitText("diff " + cached + "--no-ext-diff --unified=0 -- " + Quote(m_newPath));
}

void GitDiffPage::ConfigureEditor(wxStyledTextCtrl* editor, bool addedSide)
{
    editor->SetReadOnly(false);
    editor->StyleClearAll();
    editor->StyleSetBackground(wxSTC_STYLE_DEFAULT, wxColour(30, 30, 30));
    editor->StyleSetForeground(wxSTC_STYLE_DEFAULT, wxColour(220, 220, 220));
    editor->StyleSetFaceName(wxSTC_STYLE_DEFAULT, "Monospace");
    editor->StyleSetSize(wxSTC_STYLE_DEFAULT, 10);
    editor->StyleClearAll();
    editor->SetMarginType(0, wxSTC_MARGIN_NUMBER);
    editor->SetMarginWidth(0, 48);
    editor->SetMarginBackground(0, wxColour(37, 37, 38));
    editor->StyleSetBackground(wxSTC_STYLE_LINENUMBER, wxColour(37, 37, 38));
    editor->StyleSetForeground(wxSTC_STYLE_LINENUMBER, wxColour(130, 130, 130));
    editor->SetWrapMode(wxSTC_WRAP_NONE);
    editor->MarkerDefine(MARKER_CHANGED, wxSTC_MARK_BACKGROUND);
    editor->MarkerSetBackground(MARKER_CHANGED,
                                addedSide ? wxColour(32, 64, 40) : wxColour(78, 38, 38));
    editor->SetReadOnly(true);
}

void GitDiffPage::SetModeFromStatus()
{
    wxChar indexStatus = ' ';
    wxChar workStatus = ' ';
    if (m_status.length() > 0)
        indexStatus = m_status[0];
    if (m_status.length() > 1)
        workStatus = m_status[1];

    if (workStatus != ' ' && workStatus != '?')
        m_mode = DiffMode::Unstaged;
    else if (m_status == "??")
        m_mode = DiffMode::Unstaged;
    else if (indexStatus != ' ' && indexStatus != '?')
        m_mode = DiffMode::Staged;
    else
        m_mode = DiffMode::Unstaged;
}

void GitDiffPage::UpdateHeader()
{
    m_title->SetLabel(m_newPath);
    if (m_mode == DiffMode::Staged)
        m_modeLabel->SetLabel("HEAD  ↔  INDEX (staged)");
    else if (m_status == "??")
        m_modeLabel->SetLabel("UNTRACKED");
    else
        m_modeLabel->SetLabel("INDEX  ↔  WORKING TREE");

    const bool hasStaged = m_status.length() > 0 && m_status[0] != ' ' && m_status[0] != '?';
    const bool hasUnstaged = m_status == "??" ||
                             (m_status.length() > 1 && m_status[1] != ' ' && m_status[1] != '?');
    m_stageButton->Enable(hasUnstaged);
    m_unstageButton->Enable(hasStaged);
}

void GitDiffPage::LoadTexts()
{
    wxString leftText;
    wxString rightText;

    if (m_mode == DiffMode::Staged)
    {
        leftText = ReadHeadFile();
        rightText = ReadIndexFile();
    }
    else
    {
        leftText = m_status == "??" ? wxString() : ReadIndexFile();
        rightText = ReadWorkingTreeFile();
    }

    m_left->SetReadOnly(false);
    m_right->SetReadOnly(false);
    m_left->SetText(leftText);
    m_right->SetText(rightText);
    m_left->EmptyUndoBuffer();
    m_right->EmptyUndoBuffer();
    m_left->SetSavePoint();
    m_right->SetSavePoint();
    m_left->SetReadOnly(true);
    m_right->SetReadOnly(true);

    ClearMarkers();
    const wxString patch = DiffPatch();
    if (patch.IsEmpty() && m_status == "??")
    {
        for (int line = 0; line < m_right->GetLineCount(); ++line)
            m_right->MarkerAdd(line, MARKER_CHANGED);
    }
    else
    {
        ApplyHunkMarkers(patch);
    }
}

void GitDiffPage::ClearMarkers()
{
    m_left->MarkerDeleteAll(MARKER_CHANGED);
    m_right->MarkerDeleteAll(MARKER_CHANGED);
}

void GitDiffPage::ApplyHunkMarkers(const wxString& patch)
{
    wxArrayString lines = wxSplit(patch, '\n');
    for (const wxString& line : lines)
    {
        if (!line.StartsWith("@@"))
            continue;

        // Typical header: @@ -oldStart,oldCount +newStart,newCount @@
        wxArrayString fields = wxSplit(line, ' ');
        wxString oldToken;
        wxString newToken;
        for (const wxString& field : fields)
        {
            if (field.StartsWith("-") && oldToken.IsEmpty())
                oldToken = field;
            else if (field.StartsWith("+") && newToken.IsEmpty())
                newToken = field;
        }
        if (oldToken.IsEmpty() || newToken.IsEmpty())
            continue;

        int oldStart = 0, oldCount = 0, newStart = 0, newCount = 0;
        if (!ParseRange(oldToken, oldStart, oldCount) || !ParseRange(newToken, newStart, newCount))
            continue;

        for (int i = 0; i < oldCount; ++i)
        {
            const int editorLine = std::max(0, oldStart - 1 + i);
            if (editorLine < m_left->GetLineCount())
                m_left->MarkerAdd(editorLine, MARKER_CHANGED);
        }
        for (int i = 0; i < newCount; ++i)
        {
            const int editorLine = std::max(0, newStart - 1 + i);
            if (editorLine < m_right->GetLineCount())
                m_right->MarkerAdd(editorLine, MARKER_CHANGED);
        }
    }
}

void GitDiffPage::UpdateStatus(const wxString& status)
{
    m_status = status;
    SetModeFromStatus();
    RefreshDiff();
}

void GitDiffPage::ReloadStatus()
{
    wxArrayString output, errors;
    if (!RunGit("-c core.quotePath=false status --short --untracked-files=all -- " + Quote(m_newPath),
                output, errors) || output.IsEmpty())
    {
        m_status = "  ";
        return;
    }
    if (output[0].length() >= 2)
        m_status = output[0].Left(2);
}

void GitDiffPage::RefreshDiff()
{
    UpdateHeader();
    LoadTexts();
}

void GitDiffPage::Stage()
{
    wxArrayString output, errors;
    if (!RunGit("add -- " + Quote(m_newPath), output, errors))
    {
        ShowError("Git stage failed", errors);
        return;
    }
    ReloadStatus();
    SetModeFromStatus();
    if (m_repositoryChangedCallback)
        m_repositoryChangedCallback();
    RefreshDiff();
}

void GitDiffPage::Unstage()
{
    wxArrayString output, errors;
    if (!RunGit("restore --staged -- " + Quote(m_newPath), output, errors))
    {
        output.Clear();
        errors.Clear();
        if (!RunGit("reset HEAD -- " + Quote(m_newPath), output, errors))
        {
            ShowError("Git unstage failed", errors);
            return;
        }
    }
    ReloadStatus();
    SetModeFromStatus();
    if (m_repositoryChangedCallback)
        m_repositoryChangedCallback();
    RefreshDiff();
}

void GitDiffPage::Discard()
{
    if (wxMessageBox("Discard changes in:\n" + m_newPath + "?",
                     "Discard Changes", wxYES_NO | wxICON_WARNING, this) != wxYES)
        return;

    if (m_status == "??")
    {
        const wxString path = AbsolutePath(m_newPath);
        if (!wxRemoveFile(path))
        {
            wxMessageBox("Could not delete the untracked file.", "Git discard failed",
                         wxOK | wxICON_ERROR, this);
            return;
        }
    }
    else
    {
        wxArrayString output, errors;
        if (!RunGit("restore --source=HEAD --staged --worktree -- " + Quote(m_newPath), output, errors))
        {
            ShowError("Git discard failed", errors);
            return;
        }
    }

    if (m_repositoryChangedCallback)
        m_repositoryChangedCallback();
    m_status = "  ";
    RefreshDiff();
}

void GitDiffPage::OpenFile()
{
    if (!m_openFileCallback)
        return;
    const wxString path = AbsolutePath(m_newPath);
    if (wxFileExists(path))
        m_openFileCallback(path);
}

void GitDiffPage::ShowError(const wxString& title, const wxArrayString& errors) const
{
    wxString message;
    for (const wxString& line : errors)
        message += line + "\n";
    if (message.IsEmpty())
        message = "Git command failed.";
    wxMessageBox(message, title, wxOK | wxICON_ERROR,
                 const_cast<GitDiffPage*>(this));
}

wxString GitDiffPage::DecodeGitPath(const wxString& path)
{
    if (path.length() < 2 || path[0] != '"' || path[path.length() - 1] != '"')
        return path;

    const wxString body = path.Mid(1, path.length() - 2);
    wxString decoded;
    for (size_t i = 0; i < body.length(); ++i)
    {
        wxChar c = body[i];
        if (c != '\\' || i + 1 >= body.length())
        {
            decoded += c;
            continue;
        }
        const wxChar next = body[++i];
        switch (next)
        {
        case '\\': decoded += '\\'; break;
        case '"': decoded += '"'; break;
        case 't': decoded += '\t'; break;
        case 'n': decoded += '\n'; break;
        case 'r': decoded += '\r'; break;
        default: decoded += next; break;
        }
    }
    return decoded;
}

void GitDiffPage::SplitRenamePath(const wxString& gitPath, wxString& oldPath, wxString& newPath)
{
    const int renamePos = gitPath.Find(" -> ");
    if (renamePos == wxNOT_FOUND)
    {
        oldPath = DecodeGitPath(gitPath);
        newPath = oldPath;
        return;
    }

    oldPath = DecodeGitPath(gitPath.Left(renamePos));
    newPath = DecodeGitPath(gitPath.Mid(renamePos + 4));
}
