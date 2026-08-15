#include "SourceControlPanel.h"

#include <wx/artprov.h>
#include <wx/filename.h>
#include <wx/utils.h>
#include <utility>

SourceControlPanel::SourceControlPanel(wxWindow* parent)
    : wxPanel(parent, wxID_ANY)
{
    SetBackgroundColour(wxColour(30, 30, 30));
    auto* rootSizer = new wxBoxSizer(wxVERTICAL);

    auto* header = new wxStaticText(this, wxID_ANY, "SOURCE CONTROL");
    header->SetForegroundColour(wxColour(200, 200, 200));
    wxFont headerFont = header->GetFont();
    headerFont.SetWeight(wxFONTWEIGHT_BOLD);
    headerFont.SetPointSize(9);
    header->SetFont(headerFont);
    rootSizer->Add(header, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 8);

    m_repoLabel = new wxStaticText(this, wxID_ANY, "No repository selected");
    m_repoLabel->SetForegroundColour(wxColour(150, 150, 150));
    rootSizer->Add(m_repoLabel, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP | wxBOTTOM, 8);

    auto* toolbar = new wxPanel(this);
    toolbar->SetBackgroundColour(wxColour(37, 37, 38));
    auto* tbSizer = new wxBoxSizer(wxHORIZONTAL);

    auto* btnRefresh = new wxButton(toolbar, wxID_ANY, "Refresh", wxDefaultPosition, wxSize(70, 26));
    auto* btnStage = new wxButton(toolbar, wxID_ANY, "Stage", wxDefaultPosition, wxSize(62, 26));
    auto* btnUnstage = new wxButton(toolbar, wxID_ANY, "Unstage", wxDefaultPosition, wxSize(72, 26));
    auto* btnDiscard = new wxButton(toolbar, wxID_ANY, "Discard", wxDefaultPosition, wxSize(70, 26));
    auto* btnDiff = new wxButton(toolbar, wxID_ANY, "Diff", wxDefaultPosition, wxSize(56, 26));
    auto* btnOpen = new wxButton(toolbar, wxID_ANY, "Open", wxDefaultPosition, wxSize(56, 26));

    for (auto* button : {btnRefresh, btnStage, btnUnstage, btnDiscard, btnDiff, btnOpen})
    {
        button->SetBackgroundColour(wxColour(45, 45, 48));
        button->SetForegroundColour(wxColour(220, 220, 220));
        tbSizer->Add(button, 0, wxALL, 3);
    }

    toolbar->SetSizer(tbSizer);
    rootSizer->Add(toolbar, 0, wxEXPAND);

    m_filesView = new wxDataViewCtrl(this, wxID_ANY,
                                     wxDefaultPosition, wxDefaultSize,
                                     wxDV_ROW_LINES | wxDV_SINGLE | wxBORDER_NONE);
    m_filesView->SetBackgroundColour(wxColour(30, 30, 30));
    m_filesView->SetForegroundColour(wxColour(220, 220, 220));
    m_filesView->SetRowHeight(22);
    m_filesView->AppendTextColumn("Status", 0, wxDATAVIEW_CELL_INERT, 62);
    m_filesView->AppendTextColumn("File", 1, wxDATAVIEW_CELL_INERT, 260);
    m_filesModel = new wxDataViewListStore();
    m_filesView->AssociateModel(m_filesModel);
    m_filesModel->DecRef();
    rootSizer->Add(m_filesView, 1, wxEXPAND | wxALL, 6);

    auto* commitBox = new wxPanel(this);
    commitBox->SetBackgroundColour(wxColour(37, 37, 38));
    auto* commitSizer = new wxBoxSizer(wxHORIZONTAL);
    m_commitMessage = new wxTextCtrl(commitBox, wxID_ANY, wxEmptyString,
                                     wxDefaultPosition, wxDefaultSize,
                                     wxTE_PROCESS_ENTER);
    m_commitMessage->SetHint("Commit message");
    m_commitMessage->SetBackgroundColour(wxColour(50, 50, 50));
    m_commitMessage->SetForegroundColour(wxColour(230, 230, 230));
    auto* btnCommit = new wxButton(commitBox, wxID_ANY, "Commit", wxDefaultPosition, wxSize(70, 28));
    commitSizer->Add(m_commitMessage, 1, wxEXPAND | wxALL, 4);
    commitSizer->Add(btnCommit, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
    commitBox->SetSizer(commitSizer);
    rootSizer->Add(commitBox, 0, wxEXPAND | wxLEFT | wxRIGHT, 6);

    auto* commitLabel = new wxStaticText(this, wxID_ANY, "RECENT COMMITS");
    commitLabel->SetForegroundColour(wxColour(160, 160, 160));
    rootSizer->Add(commitLabel, 0, wxLEFT | wxRIGHT | wxTOP, 8);

    m_commitView = new wxDataViewCtrl(this, wxID_ANY,
                                      wxDefaultPosition, wxSize(-1, 125),
                                      wxDV_ROW_LINES | wxDV_SINGLE | wxBORDER_NONE);
    m_commitView->SetBackgroundColour(wxColour(30, 30, 30));
    m_commitView->SetForegroundColour(wxColour(200, 200, 200));
    m_commitView->SetRowHeight(20);
    m_commitView->AppendTextColumn("Hash", 0, wxDATAVIEW_CELL_INERT, 76);
    m_commitView->AppendTextColumn("Message", 1, wxDATAVIEW_CELL_INERT, 180);
    m_commitView->AppendTextColumn("Author", 2, wxDATAVIEW_CELL_INERT, 95);
    m_commitView->AppendTextColumn("Date", 3, wxDATAVIEW_CELL_INERT, 96);
    m_commitModel = new wxDataViewListStore();
    m_commitView->AssociateModel(m_commitModel);
    m_commitModel->DecRef();
    rootSizer->Add(m_commitView, 0, wxEXPAND | wxALL, 6);

    m_commitFilesLabel = new wxStaticText(this, wxID_ANY, "COMMIT CHANGES");
    m_commitFilesLabel->SetForegroundColour(wxColour(160, 160, 160));
    rootSizer->Add(m_commitFilesLabel, 0, wxLEFT | wxRIGHT | wxTOP, 8);

    m_commitFilesView = new wxDataViewCtrl(this, wxID_ANY,
                                           wxDefaultPosition, wxSize(-1, 115),
                                           wxDV_ROW_LINES | wxDV_SINGLE | wxBORDER_NONE);
    m_commitFilesView->SetBackgroundColour(wxColour(30, 30, 30));
    m_commitFilesView->SetForegroundColour(wxColour(205, 205, 205));
    m_commitFilesView->SetRowHeight(20);
    m_commitFilesView->AppendTextColumn("Status", 0, wxDATAVIEW_CELL_INERT, 62);
    m_commitFilesView->AppendTextColumn("File", 1, wxDATAVIEW_CELL_INERT, 290);
    m_commitFilesModel = new wxDataViewListStore();
    m_commitFilesView->AssociateModel(m_commitFilesModel);
    m_commitFilesModel->DecRef();
    rootSizer->Add(m_commitFilesView, 0, wxEXPAND | wxALL, 6);

    SetSizer(rootSizer);

    btnRefresh->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { RefreshRepository(); });
    btnStage->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { StageSelected(); });
    btnUnstage->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { UnstageSelected(); });
    btnDiscard->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { DiscardSelected(); });
    btnDiff->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { OpenSelectedDiff(); });
    btnOpen->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { OpenSelectedFile(); });
    btnCommit->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { Commit(); });
    m_commitMessage->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent&) { Commit(); });

    m_filesView->Bind(wxEVT_DATAVIEW_ITEM_ACTIVATED, [this](wxDataViewEvent& event)
    {
        if (!event.GetItem().IsOk())
            return;
        OpenSelectedDiff();
    });

    m_filesView->Bind(wxEVT_DATAVIEW_ITEM_CONTEXT_MENU, [this](wxDataViewEvent& event)
    {
        if (event.GetItem().IsOk())
            m_filesView->Select(event.GetItem());

        wxMenu menu;
        const int openChangesId = wxWindow::NewControlId();
        const int openFileId = wxWindow::NewControlId();
        const int stageId = wxWindow::NewControlId();
        const int unstageId = wxWindow::NewControlId();
        const int discardId = wxWindow::NewControlId();
        menu.Append(openChangesId, "Open Changes (Diff)");
        menu.Append(openFileId, "Open File");
        menu.AppendSeparator();
        menu.Append(stageId, "Stage Changes");
        menu.Append(unstageId, "Unstage Changes");
        menu.AppendSeparator();
        menu.Append(discardId, "Discard Changes");

        menu.Bind(wxEVT_MENU, [this](wxCommandEvent&) { OpenSelectedDiff(); }, openChangesId);
        menu.Bind(wxEVT_MENU, [this](wxCommandEvent&) { OpenSelectedFile(); }, openFileId);
        menu.Bind(wxEVT_MENU, [this](wxCommandEvent&) { StageSelected(); }, stageId);
        menu.Bind(wxEVT_MENU, [this](wxCommandEvent&) { UnstageSelected(); }, unstageId);
        menu.Bind(wxEVT_MENU, [this](wxCommandEvent&) { DiscardSelected(); }, discardId);
        PopupMenu(&menu);
    });

    m_commitView->Bind(wxEVT_DATAVIEW_SELECTION_CHANGED, [this](wxDataViewEvent&)
    {
        const int row = SelectedCommitRow();
        if (row == wxNOT_FOUND)
        {
            m_commitFiles.clear();
            if (m_commitFilesModel)
                m_commitFilesModel->DeleteAllItems();
            return;
        }
        LoadCommitFiles(m_commits[static_cast<size_t>(row)].hash);
    });

    m_commitFilesView->Bind(wxEVT_DATAVIEW_ITEM_ACTIVATED, [this](wxDataViewEvent& event)
    {
        if (event.GetItem().IsOk())
            OpenSelectedCommitDiff();
    });
}

void SourceControlPanel::SetOpenFileCallback(std::function<void(const wxString&)> callback)
{
    m_openFileCallback = std::move(callback);
}

void SourceControlPanel::SetOpenDiffCallback(
    std::function<void(const wxString&, const wxString&, const wxString&)> callback)
{
    m_openDiffCallback = std::move(callback);
}

void SourceControlPanel::SetOpenCommitDiffCallback(
    std::function<void(const wxString&, const wxString&, const wxString&, const wxString&, const wxString&)> callback)
{
    m_openCommitDiffCallback = std::move(callback);
}

wxString SourceControlPanel::Quote(const wxString& value) const
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

bool SourceControlPanel::RunGit(const wxString& args,
                                wxArrayString& output,
                                wxArrayString& errors) const
{
    if (m_repositoryPath.IsEmpty())
        return false;

    const wxString workingDirectory = m_repositoryRoot.IsEmpty()
                                          ? m_repositoryPath
                                          : m_repositoryRoot;
    const wxString command = "git -C " + Quote(workingDirectory) + " " + args;
    const long code = wxExecute(command, output, errors, wxEXEC_SYNC);
    return code == 0;
}

void SourceControlPanel::SetRepositoryPath(const wxString& path)
{
    m_repositoryPath = path;
    RefreshRepository();
}

bool SourceControlPanel::ResolveRepositoryRoot()
{
    wxArrayString output;
    wxArrayString errors;
    if (!RunGit("rev-parse --show-toplevel", output, errors) || output.IsEmpty())
    {
        m_repositoryRoot.clear();
        m_repoLabel->SetLabel("Not a Git repository");
        return false;
    }

    m_repositoryRoot = output[0];

    wxArrayString branchOutput;
    wxArrayString branchErrors;
    if (RunGit("branch --show-current", branchOutput, branchErrors) && !branchOutput.IsEmpty())
        m_repoLabel->SetLabel(m_repositoryRoot + "  [" + branchOutput[0] + "]");
    else
        m_repoLabel->SetLabel(m_repositoryRoot);
    return true;
}

void SourceControlPanel::RefreshRepository()
{
    m_files.clear();
    m_commits.clear();
    m_commitFiles.clear();
    if (m_filesModel)
        m_filesModel->DeleteAllItems();
    if (m_commitModel)
        m_commitModel->DeleteAllItems();
    if (m_commitFilesModel)
        m_commitFilesModel->DeleteAllItems();
    if (m_commitFilesLabel)
        m_commitFilesLabel->SetLabel("COMMIT CHANGES");

    if (m_repositoryPath.IsEmpty())
    {
        m_repoLabel->SetLabel("No repository selected");
        return;
    }

    if (!ResolveRepositoryRoot())
        return;

    LoadStatus();
    LoadHistory();
}

void SourceControlPanel::LoadStatus()
{
    wxArrayString output;
    wxArrayString errors;
    if (!RunGit("-c core.quotePath=false status --short --untracked-files=all", output, errors))
        return;

    for (const wxString& line : output)
    {
        if (line.length() < 3)
            continue;

        GitFileInfo info;
        info.status = line.Left(2);
        info.file = line.Mid(3);
        m_files.push_back(info);

        wxVector<wxVariant> row;
        row.push_back(info.status);
        row.push_back(info.file);
        m_filesModel->AppendItem(row);
    }
}

void SourceControlPanel::LoadHistory()
{
    wxArrayString output;
    wxArrayString errors;
    if (!RunGit("log -n 30 --date=short --pretty=format:\"%H%x1f%s%x1f%an%x1f%ad\"", output, errors))
        return;

    for (const wxString& line : output)
    {
        wxArrayString fields = wxSplit(line, static_cast<wxChar>(0x1f));
        if (fields.size() < 4)
            continue;

        CommitInfo commit{fields[0], fields[1], fields[2], fields[3]};
        m_commits.push_back(commit);

        wxVector<wxVariant> row;
        row.push_back(commit.hash.Left(8));
        row.push_back(commit.message);
        row.push_back(commit.author);
        row.push_back(commit.date);
        m_commitModel->AppendItem(row);
    }

    if (!m_commits.empty() && m_commitView && m_commitModel)
    {
        const wxDataViewItem first = m_commitModel->GetItem(0);
        if (first.IsOk())
        {
            m_commitView->Select(first);
            LoadCommitFiles(m_commits.front().hash);
        }
    }
}

int SourceControlPanel::SelectedCommitRow() const
{
    if (!m_commitView || !m_commitModel)
        return wxNOT_FOUND;
    const wxDataViewItem item = m_commitView->GetSelection();
    if (!item.IsOk())
        return wxNOT_FOUND;
    const unsigned int row = m_commitModel->GetRow(item);
    return row < m_commits.size() ? static_cast<int>(row) : wxNOT_FOUND;
}

void SourceControlPanel::LoadCommitFiles(const wxString& commitHash)
{
    m_commitFiles.clear();
    if (m_commitFilesModel)
        m_commitFilesModel->DeleteAllItems();

    wxArrayString output;
    wxArrayString errors;
    if (!RunGit("-c core.quotePath=false diff-tree --root --no-commit-id --name-status -r -M " + Quote(commitHash),
                output, errors))
        return;

    for (const wxString& line : output)
    {
        wxArrayString fields = wxSplit(line, static_cast<wxChar>('\t'));
        if (fields.size() < 2)
            continue;

        GitCommitFileInfo info;
        info.status = fields[0];
        if (info.status.StartsWith("R") || info.status.StartsWith("C"))
        {
            if (fields.size() < 3)
                continue;
            info.oldFile = fields[1];
            info.file = fields[2];
        }
        else
        {
            info.oldFile = fields[1];
            info.file = fields[1];
        }
        m_commitFiles.push_back(info);

        wxVector<wxVariant> row;
        row.push_back(info.status);
        wxString display = info.file;
        if (info.oldFile != info.file)
            display = info.oldFile + " -> " + info.file;
        row.push_back(display);
        m_commitFilesModel->AppendItem(row);
    }

    const int commitRow = SelectedCommitRow();
    if (m_commitFilesLabel && commitRow != wxNOT_FOUND)
    {
        const CommitInfo& commit = m_commits[static_cast<size_t>(commitRow)];
        m_commitFilesLabel->SetLabel(
            wxString::Format("COMMIT CHANGES  %s  (%lu files)", commit.hash.Left(8).c_str(), static_cast<unsigned long>(m_commitFiles.size())));
    }
}

int SourceControlPanel::SelectedCommitFileRow() const
{
    if (!m_commitFilesView || !m_commitFilesModel)
        return wxNOT_FOUND;
    const wxDataViewItem item = m_commitFilesView->GetSelection();
    if (!item.IsOk())
        return wxNOT_FOUND;
    const unsigned int row = m_commitFilesModel->GetRow(item);
    return row < m_commitFiles.size() ? static_cast<int>(row) : wxNOT_FOUND;
}

void SourceControlPanel::OpenSelectedCommitDiff()
{
    if (!m_openCommitDiffCallback)
        return;
    const int commitRow = SelectedCommitRow();
    const int fileRow = SelectedCommitFileRow();
    if (commitRow == wxNOT_FOUND || fileRow == wxNOT_FOUND)
        return;

    const CommitInfo& commit = m_commits[static_cast<size_t>(commitRow)];
    const GitCommitFileInfo& file = m_commitFiles[static_cast<size_t>(fileRow)];
    m_openCommitDiffCallback(m_repositoryRoot, commit.hash, file.oldFile, file.file, file.status);
}

int SourceControlPanel::SelectedFileRow() const
{
    if (!m_filesView || !m_filesModel)
        return wxNOT_FOUND;
    const wxDataViewItem item = m_filesView->GetSelection();
    if (!item.IsOk())
        return wxNOT_FOUND;
    const unsigned int row = m_filesModel->GetRow(item);
    return row < m_files.size() ? static_cast<int>(row) : wxNOT_FOUND;
}

wxString SourceControlPanel::SelectedFilePath() const
{
    const int row = SelectedFileRow();
    if (row == wxNOT_FOUND)
        return wxEmptyString;

    wxString path = m_files[static_cast<size_t>(row)].file;
    const int renamePos = path.Find(" -> ");
    if (renamePos != wxNOT_FOUND)
        path = path.Mid(renamePos + 4);
    return DecodeGitPath(path);
}

wxString SourceControlPanel::DecodeGitPath(const wxString& path) const
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
        case '"':  decoded += '"'; break;
        case 't':  decoded += '\t'; break;
        case 'n':  decoded += '\n'; break;
        case 'r':  decoded += '\r'; break;
        default:
            if (next >= '0' && next <= '7')
            {
                int value = next - '0';
                int count = 1;
                while (count < 3 && i + 1 < body.length() &&
                       body[i + 1] >= '0' && body[i + 1] <= '7')
                {
                    value = value * 8 + (body[++i] - '0');
                    ++count;
                }
                decoded += static_cast<wxChar>(value);
            }
            else
            {
                decoded += next;
            }
            break;
        }
    }
    return decoded;
}

void SourceControlPanel::OpenSelectedFile()
{
    if (!m_openFileCallback)
        return;
    const wxString relative = SelectedFilePath();
    if (relative.IsEmpty())
        return;

    wxString fullPath = m_repositoryRoot;
    fullPath += wxFileName::GetPathSeparator();
    fullPath += relative;
    wxFileName absolutePath(fullPath);
    absolutePath.Normalize();
    if (wxFileExists(absolutePath.GetFullPath()))
        m_openFileCallback(absolutePath.GetFullPath());
}

void SourceControlPanel::OpenSelectedDiff()
{
    if (!m_openDiffCallback)
    {
        OpenSelectedFile();
        return;
    }

    const int row = SelectedFileRow();
    if (row == wxNOT_FOUND)
        return;
    const GitFileInfo& info = m_files[static_cast<size_t>(row)];
    m_openDiffCallback(m_repositoryRoot, info.file, info.status);
}

void SourceControlPanel::StageSelected()
{
    const wxString file = SelectedFilePath();
    if (file.IsEmpty())
        return;

    wxArrayString output, errors;
    if (!RunGit("add -- " + Quote(file), output, errors))
        ShowGitError("Git stage failed", errors);
    RefreshRepository();
}

void SourceControlPanel::UnstageSelected()
{
    const wxString file = SelectedFilePath();
    if (file.IsEmpty())
        return;

    wxArrayString output, errors;
    if (!RunGit("restore --staged -- " + Quote(file), output, errors))
    {
        errors.Clear();
        output.Clear();
        if (!RunGit("reset HEAD -- " + Quote(file), output, errors))
            ShowGitError("Git unstage failed", errors);
    }
    RefreshRepository();
}

void SourceControlPanel::DiscardSelected()
{
    const wxString file = SelectedFilePath();
    if (file.IsEmpty())
        return;

    if (wxMessageBox("Discard local changes in:\n" + file + "?",
                     "Discard Changes", wxYES_NO | wxICON_WARNING, this) != wxYES)
        return;

    const int row = SelectedFileRow();
    if (row != wxNOT_FOUND && m_files[static_cast<size_t>(row)].status == "??")
    {
        wxFileName absoluteFile(m_repositoryRoot + wxFileName::GetPathSeparator() + file);
        absoluteFile.Normalize();
        const wxString absolutePath = absoluteFile.GetFullPath();
        if (!wxRemoveFile(absolutePath))
            wxMessageBox("Could not delete the untracked file.", "Git discard failed",
                         wxOK | wxICON_ERROR, this);
        RefreshRepository();
        return;
    }

    wxArrayString output, errors;
    if (!RunGit("restore --source=HEAD --staged --worktree -- " + Quote(file), output, errors))
        ShowGitError("Git discard failed", errors);
    RefreshRepository();
}

void SourceControlPanel::Commit()
{
    const wxString message = m_commitMessage->GetValue().Trim(true).Trim(false);
    if (message.IsEmpty())
    {
        wxMessageBox("Enter a commit message first.", "Git Commit",
                     wxOK | wxICON_INFORMATION, this);
        return;
    }

    wxArrayString output, errors;
    if (!RunGit("commit -m " + Quote(message), output, errors))
    {
        ShowGitError("Git commit failed", errors);
        return;
    }

    m_commitMessage->Clear();
    RefreshRepository();
}

void SourceControlPanel::ShowGitError(const wxString& title,
                                      const wxArrayString& errors) const
{
    wxString message;
    for (const wxString& line : errors)
        message += line + "\n";
    if (message.IsEmpty())
        message = "Git command failed.";
    wxMessageBox(message, title, wxOK | wxICON_ERROR,
                 const_cast<SourceControlPanel*>(this));
}
