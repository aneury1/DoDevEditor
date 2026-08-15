#include "GitCommitDiffPage.h"

#include "EditorPage.h"

#include <wx/filename.h>
#include <wx/splitter.h>
#include <wx/utils.h>
#include <utility>

GitCommitDiffPage::GitCommitDiffPage(wxWindow* parent,
                                     const wxString& repositoryRoot,
                                     const wxString& commitHash,
                                     const wxString& oldPath,
                                     const wxString& newPath,
                                     const wxString& status)
    : wxPanel(parent, wxID_ANY),
      m_repositoryRoot(repositoryRoot),
      m_commitHash(commitHash),
      m_oldPath(oldPath),
      m_newPath(newPath),
      m_status(status)
{
    SetBackgroundColour(wxColour(30, 30, 30));
    ResolveParent();

    auto* root = new wxBoxSizer(wxVERTICAL);
    auto* toolbar = new wxPanel(this);
    toolbar->SetBackgroundColour(wxColour(37, 37, 38));
    auto* toolbarSizer = new wxBoxSizer(wxHORIZONTAL);

    m_title = new wxStaticText(toolbar, wxID_ANY,
        wxFileName(m_newPath).GetFullName() + " — commit changes");
    m_title->SetForegroundColour(wxColour(230, 230, 230));
    wxFont titleFont = m_title->GetFont();
    titleFont.SetWeight(wxFONTWEIGHT_BOLD);
    m_title->SetFont(titleFont);

    m_meta = new wxStaticText(toolbar, wxID_ANY,
        m_commitHash.Left(10) + "  " + m_status);
    m_meta->SetForegroundColour(wxColour(160, 160, 160));

    auto* refreshButton = new wxButton(toolbar, wxID_ANY, "Refresh", wxDefaultPosition, wxSize(70, 28));
    auto* openButton = new wxButton(toolbar, wxID_ANY, "Open File", wxDefaultPosition, wxSize(78, 28));
    toolbarSizer->Add(m_title, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, 8);
    toolbarSizer->Add(m_meta, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 8);
    toolbarSizer->Add(refreshButton, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
    toolbarSizer->Add(openButton, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
    toolbar->SetSizer(toolbarSizer);
    root->Add(toolbar, 0, wxEXPAND);

    auto* captions = new wxPanel(this);
    captions->SetBackgroundColour(wxColour(30, 30, 30));
    auto* captionsSizer = new wxBoxSizer(wxHORIZONTAL);
    const wxString leftName = m_parentHash.IsEmpty() ? wxString("Before (empty/root)")
                                                      : wxString("Parent ") + m_parentHash.Left(8);
    const wxString rightName = wxString("Commit ") + m_commitHash.Left(8);
    auto* leftCaption = new wxStaticText(captions, wxID_ANY, leftName);
    auto* rightCaption = new wxStaticText(captions, wxID_ANY, rightName);
    leftCaption->SetForegroundColour(wxColour(170, 170, 170));
    rightCaption->SetForegroundColour(wxColour(170, 170, 170));
    captionsSizer->Add(leftCaption, 1, wxLEFT | wxTOP | wxBOTTOM, 8);
    captionsSizer->Add(rightCaption, 1, wxLEFT | wxTOP | wxBOTTOM, 8);
    captions->SetSizer(captionsSizer);
    root->Add(captions, 0, wxEXPAND);

    auto* splitter = new wxSplitterWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                          wxSP_LIVE_UPDATE | wxSP_3DSASH);
    splitter->SetMinimumPaneSize(100);
    splitter->SetSashGravity(0.5);
    m_left = new EditorPage(splitter);
    m_right = new EditorPage(splitter);
    m_left->filepath = AbsolutePath(m_oldPath);
    m_right->filepath = AbsolutePath(m_newPath);
    splitter->SplitVertically(m_left, m_right);
    root->Add(splitter, 1, wxEXPAND);
    SetSizer(root);

    refreshButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { RefreshDiff(); });
    openButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { OpenWorkingFile(); });

    RefreshDiff();
}

void GitCommitDiffPage::SetOpenFileCallback(std::function<void(const wxString&)> callback)
{
    m_openFileCallback = std::move(callback);
}

wxString GitCommitDiffPage::GetTabTitle() const
{
    return wxFileName(m_newPath).GetFullName() + " @ " + m_commitHash.Left(8);
}

wxString GitCommitDiffPage::Quote(const wxString& value) const
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

bool GitCommitDiffPage::RunGit(const wxString& args,
                               wxArrayString& output,
                               wxArrayString& errors) const
{
    if (m_repositoryRoot.IsEmpty())
        return false;
    const wxString command = "git -C " + Quote(m_repositoryRoot) + " " + args;
    return wxExecute(command, output, errors, wxEXEC_SYNC) == 0;
}

wxString GitCommitDiffPage::RunGitText(const wxString& args, bool* ok) const
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

void GitCommitDiffPage::ResolveParent()
{
    wxArrayString output;
    wxArrayString errors;
    if (RunGit("rev-parse " + Quote(m_commitHash + "^"), output, errors) && !output.IsEmpty())
        m_parentHash = output[0];
    else
        m_parentHash.clear();
}

wxString GitCommitDiffPage::RevisionText(const wxString& revision, const wxString& path) const
{
    if (revision.IsEmpty() || path.IsEmpty())
        return wxString();
    bool ok = false;
    const wxString text = RunGitText("show --textconv " + Quote(revision + ":" + path), &ok);
    return ok ? text : wxString();
}

wxString GitCommitDiffPage::AbsolutePath(const wxString& path) const
{
    wxString full = m_repositoryRoot;
    full += wxFileName::GetPathSeparator();
    full += path;
    wxFileName file(full);
    file.Normalize();
    return file.GetFullPath();
}

void GitCommitDiffPage::LoadTexts()
{
    const bool added = m_status.StartsWith("A");
    const bool deleted = m_status.StartsWith("D");

    const wxString leftText = added ? wxString() : RevisionText(m_parentHash, m_oldPath);
    const wxString rightText = deleted ? wxString() : RevisionText(m_commitHash, m_newPath);

    m_left->SetReadOnly(false);
    m_right->SetReadOnly(false);
    m_left->filepath = AbsolutePath(m_oldPath);
    m_right->filepath = AbsolutePath(m_newPath);
    m_left->SetText(leftText);
    m_right->SetText(rightText);
    m_left->ApplyTheme();
    m_right->ApplyTheme();
    m_left->EmptyUndoBuffer();
    m_right->EmptyUndoBuffer();
    m_left->SetSavePoint();
    m_right->SetSavePoint();
    m_left->modified = false;
    m_right->modified = false;
    m_left->SetReadOnly(true);
    m_right->SetReadOnly(true);
}

void GitCommitDiffPage::RefreshDiff()
{
    ResolveParent();
    LoadTexts();
    if (m_meta)
        m_meta->SetLabel(m_commitHash.Left(10) + "  " + m_status);
}

void GitCommitDiffPage::ApplyTheme()
{
    if (m_left)
        m_left->ApplyTheme();
    if (m_right)
        m_right->ApplyTheme();
}

wxString GitCommitDiffPage::GetPatchForAI() const
{
    wxString paths = Quote(m_newPath);
    if (m_oldPath != m_newPath)
        paths = Quote(m_oldPath) + " " + Quote(m_newPath);
    return RunGitText("show --format= --no-ext-diff --unified=3 " + Quote(m_commitHash) +
                      " -- " + paths);
}

void GitCommitDiffPage::OpenWorkingFile()
{
    if (!m_openFileCallback)
        return;
    const wxString path = AbsolutePath(m_newPath);
    if (wxFileExists(path))
        m_openFileCallback(path);
}
