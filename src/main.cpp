#include <wx/wx.h>
#include <wx/aui/aui.h>
#include <wx/aui/auibook.h>
#include <wx/stc/stc.h>
#include <wx/treectrl.h>
#include <wx/dir.h>
#include <wx/filename.h>
#include <wx/imaglist.h>
#include <wx/artprov.h>
#include <wx/splitter.h>
#include <wx/statusbr.h>
#include <wx/fontdlg.h>
#include <wx/colordlg.h>
#include <wx/settings.h>
#include <wx/utils.h>
#include <wx/ffile.h>
#include <wx/msgdlg.h>
#include <wx/filedlg.h>
#include <wx/dirdlg.h>
#include <wx/stdpaths.h>
#include <wx/config.h>
#include <wx/log.h>
#include <wx/choicdlg.h>

#include <map>
#include <vector>
#include <string>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <iterator>
#include <cstdint>
#include <unordered_set>
#include "CommandPalette.h"
#include "QuickOpenDialog.h"
#include "constant.h"
#include "EditorPage.h"
#include "PathData.h"
#include "MarkDownViewer.h"
#include "FileTreeCtrl.h"
#include "FolderListView.h"
#include "Findbar.h"
#include "Config.h"
#include "SourceControlPanel.h"
#include "GitDiffPage.h"
#include "GitCommitDiffPage.h"
#include "DockerPanel.h"
#include "SymbolTablePanel.h"
#include "MenuBar.h"
#include "DualNotebookPanel.h"
#include "AIChatPage.h"
#include "GeneralSettingsDialog.h"
#include "WorkspaceManager.h"
#include "LocalHistoryManager.h"
#include "FileHistoryDialog.h"

Json::Value AppEditorConfig::config;

// ─────────────────────────────────────────────────────────────────────────────
// MainFrame
// ─────────────────────────────────────────────────────────────────────────────
static int inex = 0;
class MainFrame : public wxFrame
{
    wxPanel *sidePanel = nullptr;
    wxBoxSizer *edSizer;

    /// @brief Editor Area.
    wxSplitterWindow* m_editorSplitter = nullptr;
    wxPanel *m_bottomPanel = nullptr;
    wxPanel *m_bottomTabBar = nullptr;
    wxPanel *m_bottomContent = nullptr;

    int m_activeBottomTab = 0;
    std::vector<wxPanel *> m_bottomPages;
    std::vector<wxPanel *> m_bottomTabs;

    wxSplitterWindow *m_splitter = nullptr;
    FileTreeCtrl *m_tree = nullptr;
    FolderListView *m_folderList = nullptr;
    wxPanel *m_editorPane = nullptr;
    wxAuiNotebook *m_notebook = nullptr;
    wxAuiNotebook *m_sideNotebook = nullptr;
    FindBar *m_findBar = nullptr;
    SourceControlPanel *m_gitPanel = nullptr;
    DockerPanel *m_dockerPanel = nullptr;
    SymbolTablePanel *m_symbolsPanel = nullptr;
    AIChatPage *m_aiChatPage = nullptr;
    wxWindow *m_lastAIContextPage = nullptr;
    wxStatusBar *m_statusBar = nullptr;

    struct ProjectSearchResult
    {
        wxString filepath;
        int line = 0;
        int column = 0;
        wxString preview;
    };
    std::vector<ProjectSearchResult> m_projectSearchResults;

    WorkspaceManager m_workspace;
    wxString m_activeWorkspaceRoot;

    int m_untitledCount = 0;

public:
    MainFrame()
        : wxFrame(nullptr, wxID_ANY, "DoDevEditor",
                  wxDefaultPosition, wxSize(1280, 780))
    {
        SetBackgroundColour(Colors::BG);
        BuildUI();
        BuildMenuBar();
        BuildStatusBar();
        SetupAccelerators();

        Centre();
        Show();
        AppEditorConfig::LoadOpenedFolder();
        bool restoredWorkspace = false;

        const std::string lastWorkspaceFile = AppEditorConfig::GetLastWorkspaceFile();
        if (!lastWorkspaceFile.empty() && wxFileExists(wxString::FromUTF8(lastWorkspaceFile.c_str())))
        {
            wxString error;
            restoredWorkspace = m_workspace.LoadWorkspace(
                wxString::FromUTF8(lastWorkspaceFile.c_str()), &error);
        }

        if (!restoredWorkspace)
        {
            std::vector<wxString> folders;
            for (const std::string& folder : AppEditorConfig::GetLastWorkspaceFolders())
            {
                const wxString path = wxString::FromUTF8(folder.c_str());
                if (wxDirExists(path))
                    folders.push_back(path);
            }

            if (folders.empty())
            {
                const std::string legacyFolder = AppEditorConfig::GetLastOpenedFolder();
                if (!legacyFolder.empty())
                {
                    const wxString path = wxString::FromUTF8(legacyFolder.c_str());
                    if (wxDirExists(path))
                        folders.push_back(path);
                }
            }

            if (!folders.empty())
                restoredWorkspace = m_workspace.SetFolders(folders);
        }

        if (restoredWorkspace)
            ApplyWorkspaceToUI();
        else
        {
            if (!AppEditorConfig::config.isObject())
                AppEditorConfig::CreateDefaultConfig();
            NewTab();
        }
    }

private:
    void BuildFileTree()
    {

        sidePanel = new wxPanel(m_splitter, wxID_ANY);
        sidePanel->SetBackgroundColour(Colors::BG_PANEL);

        auto *sideSizer = new wxBoxSizer(wxVERTICAL);

        // ── Header ──
        auto *sideHeader = new wxPanel(
            sidePanel,
            wxID_ANY,
            wxDefaultPosition,
            wxSize(-1, 28));

        sideHeader->SetBackgroundColour(wxColour(37, 37, 38));

        auto *hs = new wxBoxSizer(wxHORIZONTAL);

        auto *explorerLabel = new wxStaticText(
            sideHeader,
            wxID_ANY,
            "EXPLORER");

        explorerLabel->SetForegroundColour(wxColour(187, 187, 187));

        wxFont font = explorerLabel->GetFont();
        font.SetPointSize(9);
        explorerLabel->SetFont(font);

        hs->Add(
            explorerLabel,
            1,
            wxALIGN_CENTER_VERTICAL | wxLEFT,
            12);

        sideHeader->SetSizer(hs);

        sideSizer->Add(sideHeader, 0, wxEXPAND);

        // ── MAIN AREA (FILES + TABS) ──
        wxSplitterWindow *splitter =
            new wxSplitterWindow(sidePanel, wxID_ANY);

        splitter->SetSashGravity(0.6);
        splitter->SetMinimumPaneSize(80);
        splitter->SetSashSize(4);

        splitter->SetBackgroundColour(Colors::BG_PANEL);

        // ── TOP: EXPLORER TREE + FLAT FOLDER LIST ──
        auto* explorerViews = new wxSplitterWindow(splitter, wxID_ANY,
                                                   wxDefaultPosition, wxDefaultSize,
                                                   wxSP_LIVE_UPDATE | wxSP_3DSASH);
        explorerViews->SetSashGravity(0.58);
        explorerViews->SetMinimumPaneSize(70);
        explorerViews->SetSashSize(3);
        explorerViews->SetBackgroundColour(Colors::BG_PANEL);

        m_tree = new FileTreeCtrl(explorerViews);
        m_folderList = new FolderListView(explorerViews);
        m_folderList->SetOpenFileCallback([this](const wxString& path)
        {
            if (wxFileExists(path))
                OpenFileInTab(path);
        });
        m_folderList->SetNavigateFolderCallback([this](const wxString& path)
        {
            const wxString root = m_workspace.FindContainingFolder(path);
            if (root.IsEmpty() && m_workspace.HasFolders())
            {
                if (m_folderList && !m_activeWorkspaceRoot.IsEmpty())
                    m_folderList->SetFolder(m_activeWorkspaceRoot);
                return;
            }
            ActivateWorkspaceForPath(path);
        });
        explorerViews->SplitHorizontally(m_tree, m_folderList, 190);

        // ── BOTTOM: NOTEBOOK (3 TABS) ──
        m_sideNotebook =
            new wxAuiNotebook(
                splitter,
                wxID_ANY,
                wxDefaultPosition,
                wxDefaultSize,
                wxAUI_NB_TOP | wxAUI_NB_SCROLL_BUTTONS);
        wxAuiNotebook* bottomTabs = m_sideNotebook;

        // TAB 1: Source Control
        m_gitPanel = new SourceControlPanel(bottomTabs);
        m_gitPanel->SetOpenFileCallback([this](const wxString& path)
        {
            if (wxFileExists(path))
                OpenFileInTab(path);
        });
        m_gitPanel->SetOpenDiffCallback([this](const wxString& repositoryRoot,
                                               const wxString& gitPath,
                                               const wxString& status)
        {
            OpenGitDiff(repositoryRoot, gitPath, status);
        });
        m_gitPanel->SetOpenCommitDiffCallback([this](const wxString& repositoryRoot,
                                                     const wxString& commitHash,
                                                     const wxString& oldPath,
                                                     const wxString& newPath,
                                                     const wxString& status)
        {
            OpenGitCommitDiff(repositoryRoot, commitHash, oldPath, newPath, status);
        });

        // TAB 2: optional LLVM/libclang C/C++ analysis
        m_symbolsPanel = new SymbolTablePanel(bottomTabs);
        m_symbolsPanel->SetOpenLocationCallback([this](const wxString& path, unsigned line, unsigned column)
        {
            OpenLocation(path, line, column);
        });
        m_symbolsPanel->SetSaveCurrentFileCallback([this]()
        {
            return SaveCurrentTab();
        });

        // TAB 3: (optional duplicate view or placeholder)
        wxPanel *emptyPanel = new wxPanel(bottomTabs);
        emptyPanel->SetBackgroundColour(wxColour(30, 30, 30));

        // Add tabs
        bottomTabs->AddPage(m_gitPanel, "GIT", true);
        bottomTabs->AddPage(m_symbolsPanel, "C/C++", false);
        bottomTabs->AddPage(emptyPanel, "Debug", false);

        // ── SPLIT ──
        splitter->SplitHorizontally(
            explorerViews,
            bottomTabs,
            410);

        // ── LAYOUT ──
        sideSizer->Add(splitter, 1, wxEXPAND);

        sidePanel->SetSizer(sideSizer);
        sidePanel->Layout();
    }

    void BuildEditorArea()
    {
 
        // ── Editor area ──
 
        m_editorPane = new wxPanel(m_splitter, wxID_ANY);
        m_editorPane->SetBackgroundColour(Colors::BG);

        edSizer = new wxBoxSizer(wxVERTICAL);
        // Notebook (tabs)
        long nbStyle =
            wxAUI_NB_DEFAULT_STYLE | wxAUI_NB_CLOSE_ON_ALL_TABS |
            wxAUI_NB_TAB_MOVE | wxAUI_NB_SCROLL_BUTTONS;
        DualNotebookPanel *tpanel = new DualNotebookPanel(m_editorPane,  wxID_ANY);
        m_notebook = tpanel->GetTopNotebook();
        edSizer->Add(tpanel, 1, wxEXPAND);
        /// edSizer->Add(m_notebook, 1, wxEXPAND);
    }

    void ShowBottomTab(int index)
    {
        if (index < 0 || index >= (int)m_bottomPages.size())
            return;

        for (int i = 0; i < (int)m_bottomPages.size(); i++)
            m_bottomPages[i]->Hide();

        m_bottomPages[index]->Show();
        m_bottomContent->Layout();

        m_activeBottomTab = index;
    }

    // ── UI construction ──────────────────────────────────────────────────────

    void BuildUI()
    {
        m_splitter = new wxSplitterWindow(this, wxID_ANY,
                                          wxDefaultPosition, wxDefaultSize,
                                          wxSP_3DSASH | wxSP_LIVE_UPDATE);
        m_splitter->SetBackgroundColour(Colors::BG_PANEL);

        // ── Sidebar (tree) ──
        BuildFileTree();

#if 0        
        // ── Editor area ──
        m_editorPane = new wxPanel(m_splitter, wxID_ANY);
        m_editorPane->SetBackgroundColour(Colors::BG);
        auto *edSizer = new wxBoxSizer(wxVERTICAL);

        // Notebook (tabs)
        long nbStyle =
            wxAUI_NB_DEFAULT_STYLE | wxAUI_NB_CLOSE_ON_ALL_TABS |
            wxAUI_NB_TAB_MOVE | wxAUI_NB_SCROLL_BUTTONS;
        m_notebook = new wxAuiNotebook(m_editorPane, ID_NOTEBOOK,
                                       wxDefaultPosition, wxDefaultSize, nbStyle);
        m_notebook->SetBackgroundColour(Colors::BG_ACTIVE);
        edSizer->Add(m_notebook, 1, wxEXPAND);
#endif

        BuildEditorArea();

        // Find bar (hidden initially)
        m_findBar = new FindBar(m_editorPane);
        m_findBar->Hide();
        edSizer->Add(m_findBar, 0, wxEXPAND);

        m_editorPane->SetSizer(edSizer);

        m_splitter->SplitVertically(sidePanel, m_editorPane, 240);
        m_splitter->SetMinimumPaneSize(100);

        auto *frameSizer = new wxBoxSizer(wxVERTICAL);
        frameSizer->Add(m_splitter, 1, wxEXPAND);
        SetSizer(frameSizer);

        // Events
        m_notebook->Bind(wxEVT_AUINOTEBOOK_PAGE_CHANGED, &MainFrame::OnTabChanged, this);
        m_notebook->Bind(wxEVT_AUINOTEBOOK_PAGE_CLOSE, &MainFrame::OnTabClose, this);

        m_tree->Bind(wxEVT_TREE_ITEM_ACTIVATED, &MainFrame::OnTreeItemActivated, this);
        m_tree->Bind(wxEVT_TREE_SEL_CHANGED, [this](wxTreeEvent& event)
        {
            const wxString selectedPath = m_tree->GetSelectedPath();
            if (!selectedPath.IsEmpty())
            {
                ActivateWorkspaceForPath(selectedPath);
                if (m_folderList)
                {
                    if (wxDirExists(selectedPath))
                        m_folderList->SetFolder(selectedPath);
                    else if (wxFileExists(selectedPath))
                        m_folderList->SetFolder(wxFileName(selectedPath).GetPath());
                }
            }
            event.Skip();
        });

        m_findBar->textCtrl->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent &)
                                  { ExecuteFind(true); });
        m_findBar->textCtrl->Bind(wxEVT_TEXT, [this](wxCommandEvent &event)
        {
            m_findBar->resultLabel->SetLabel("");
            m_findBar->textCtrl->SetBackgroundColour(wxColour(60, 60, 60));
            if (m_findBar->GetScope() == FindBar::Scope::AllDocuments)
            {
                m_projectSearchResults.clear();
                m_findBar->resultsList->DeleteAllItems();
                if (!m_findBar->textCtrl->GetValue().IsEmpty())
                    m_findBar->resultLabel->SetLabel("Press Enter to search");
            }
            event.Skip();
        });
        Bind(wxEVT_BUTTON, [this](wxCommandEvent &)
             { ExecuteFind(true); }, ID_FIND_BTN);
        Bind(wxEVT_BUTTON, [this](wxCommandEvent &)
             { ExecuteFind(false); }, ID_FIND_PREV);
        Bind(wxEVT_BUTTON, [this](wxCommandEvent &)
             { CloseFindBar(); }, ID_FIND_CLOSE);

        m_findBar->scopeChoice->Bind(wxEVT_CHOICE, [this](wxCommandEvent &)
        {
            const bool allDocuments = m_findBar->GetScope() == FindBar::Scope::AllDocuments;
            m_findBar->SetResultsVisible(allDocuments);
            m_projectSearchResults.clear();
            m_findBar->ClearResults();
            if (allDocuments && !m_findBar->textCtrl->GetValue().IsEmpty())
                SearchAllDocuments();
            m_editorPane->Layout();
        });

        auto invalidateSearchOptions = [this](wxCommandEvent& event)
        {
            m_projectSearchResults.clear();
            m_findBar->resultsList->DeleteAllItems();
            m_findBar->resultLabel->SetLabel("");
            event.Skip();
        };
        m_findBar->caseChk->Bind(wxEVT_CHECKBOX, invalidateSearchOptions);
        m_findBar->wholeChk->Bind(wxEVT_CHECKBOX, invalidateSearchOptions);

        m_findBar->resultsList->Bind(wxEVT_LIST_ITEM_ACTIVATED, [this](wxListEvent& event)
        {
            OpenProjectSearchResult(event.GetIndex());
        });
    }

    void BuildMenuBar()
    {
        auto *mb = new DynamicMenuBar(this);

        // ───────── FILE ─────────
        mb->AddItem("File", "New", ID_NEW_FILE, [this]()
                    { NewTab(); }, "Ctrl+N");

        mb->AddItem("File", "Open File...", ID_OPEN_FILE, [this]()
                    { OpenFile(); }, "Ctrl+O");

        mb->AddItem("File", "Open Folder...", ID_OPEN_FOLDER, [this]()
                    { OpenFolder(); }, "Ctrl+Shift+O");
        mb->AddItem("File", "Add Folder to Workspace...", ID_ADD_FOLDER_WORKSPACE, [this]()
                    { AddFolderToWorkspace(); });
        mb->AddItem("File", "Remove Folder from Workspace...", ID_REMOVE_FOLDER_WORKSPACE, [this]()
                    { RemoveFolderFromWorkspace(); });

        mb->AddSeparator("File");
        mb->AddItem("File", "Open Workspace...", ID_OPEN_WORKSPACE, [this]()
                    { OpenWorkspace(); });
        mb->AddItem("File", "Save Workspace As...", ID_SAVE_WORKSPACE_AS, [this]()
                    { SaveWorkspaceAs(); });
        mb->AddItem("File", "Close Workspace", ID_CLOSE_WORKSPACE, [this]()
                    { CloseWorkspace(); });

        mb->AddSeparator("File");

        mb->AddItem("File", "Save", ID_SAVE_FILE, [this]()
                    { SaveCurrentTab(); }, "Ctrl+S");

        mb->AddItem("File", "Save As", ID_SAVE_AS, [this]()
                    { SaveAs(); }, "Ctrl+Shift+S");

        mb->AddItem("File", "Save All", ID_SAVE_ALL, [this]()
                    { SaveAll(); }, "Ctrl+Alt+S");
        mb->AddItem("File", "Local History...", ID_FILE_HISTORY, [this]()
                    { ShowLocalHistory(); }, "Ctrl+Alt+H");

        mb->AddSeparator("File");

        mb->AddItem("File", "Exit", wxID_EXIT, [this]()
                    { Close(); }, "Alt+F4");

        // ───────── EDIT ─────────
        mb->AddItem("Edit", "Undo", wxID_UNDO, [this]()
                    { DispatchEdit(wxID_UNDO); }, "Ctrl+Z");

        mb->AddItem("Edit", "Redo", wxID_REDO, [this]()
                    { DispatchEdit(wxID_REDO); }, "Ctrl+Y");

        mb->AddSeparator("Edit");

        mb->AddItem("Edit", "Cut", wxID_CUT, [this]()
                    { DispatchEdit(wxID_CUT); }, "Ctrl+X");

        mb->AddItem("Edit", "Copy", wxID_COPY, [this]()
                    { DispatchEdit(wxID_COPY); }, "Ctrl+C");

        mb->AddItem("Edit", "Paste", wxID_PASTE, [this]()
                    { DispatchEdit(wxID_PASTE); }, "Ctrl+V");

        mb->AddItem("Edit", "Select All", wxID_SELECTALL, [this]()
                    { DispatchEdit(wxID_SELECTALL); }, "Ctrl+A");

        // ───────── SEARCH ─────────
        mb->AddItem("Search", "Find in Current Document", ID_FIND, [this]()
                    { ShowFindBar(FindBar::Scope::CurrentDocument); }, "Ctrl+F");
        mb->AddItem("Search", "Find in All Documents", ID_FIND_ALL, [this]()
                    { ShowFindBar(FindBar::Scope::AllDocuments); }, "Ctrl+Shift+F");
        mb->AddSeparator("Search");
        mb->AddItem("Search", "Find Next", ID_FIND_NEXT, [this]()
                    { ExecuteFind(true); }, "F3");
        mb->AddItem("Search", "Find Previous", ID_FIND_PREV, [this]()
                    { ExecuteFind(false); }, "Shift+F3");

        // ───────── GO / NAVIGATION ─────────
        mb->AddItem("Go", "Quick Open...", ID_QUICK_OPEN, [this]()
                    { ShowQuickOpen(); }, "Ctrl+P");
        mb->AddItem("Go", "Go to Line/Column...", ID_GOTO_LINE, [this]()
                    { GoToLine(); }, "Ctrl+G");

        // ───────── C/C++ / LLVM ─────────
        mb->AddItem("Code", "Go to Definition (LLVM)", ID_GOTO_DEFINITION, [this]()
                    { GoToDefinition(); }, "F12");
        mb->AddSeparator("Code");
        mb->AddItem("Code", "Parse C/C++ Symbols", ID_CPP_PARSE_SYMBOLS, [this]()
                    { RefreshCppAnalysis(); });
        mb->AddItem("Code", "LLVM Syntax Check", ID_CPP_SYNTAX_CHECK, [this]()
                    { CompileCurrentWithLLVM(true); });
        mb->AddItem("Code", "Compile Current Source with LLVM", ID_CPP_COMPILE, [this]()
                    { CompileCurrentWithLLVM(false); }, "Ctrl+F7");

        // ───────── VIEW ─────────
        mb->AddItem("View", "Toggle Sidebar", ID_TOGGLE_SIDEBAR, [this]()
                    { ToggleSidebar(); }, "Ctrl+B");

        mb->AddItem("View", "Word Wrap", ID_TOGGLE_WORDWRAP, [this]()
                    { ToggleWordWrap(); }, "Alt+Z");

        mb->AddSeparator("View");
        mb->AddCheckItem("View", "Docker Inspector", ID_TOGGLE_DOCKER, [this]()
                         { ToggleDockerInspector(); }, false);
        mb->AddItem("View", "AI Chat", ID_OPEN_AI_CHAT, [this]()
                    { OpenAIChat(); }, "Ctrl+Alt+I");

        // ───────── SETTINGS ─────────
        mb->AddItem("Settings", "General Settings...", ID_GENERAL_SETTINGS, [this]()
                    { ShowGeneralSettings(); });

        // ───────── HELP ─────────

        mb->AddItem("Help", "About", ID_ABOUT,
                    [this]()
                    {
                        ShowAbout();
                    });

        SetMenuBar(mb);

        // Bind
        Bind(wxEVT_MENU, [this](wxCommandEvent &)
             { NewTab(); }, ID_NEW_FILE);
        Bind(wxEVT_MENU, [this](wxCommandEvent &)
             { OpenFile(); }, ID_OPEN_FILE);
        Bind(wxEVT_MENU, [this](wxCommandEvent &)
             { OpenFolder(); }, ID_OPEN_FOLDER);
        Bind(wxEVT_MENU, [this](wxCommandEvent &)
             { AddFolderToWorkspace(); }, ID_ADD_FOLDER_WORKSPACE);
        Bind(wxEVT_MENU, [this](wxCommandEvent &)
             { RemoveFolderFromWorkspace(); }, ID_REMOVE_FOLDER_WORKSPACE);
        Bind(wxEVT_MENU, [this](wxCommandEvent &)
             { OpenWorkspace(); }, ID_OPEN_WORKSPACE);
        Bind(wxEVT_MENU, [this](wxCommandEvent &)
             { SaveWorkspaceAs(); }, ID_SAVE_WORKSPACE_AS);
        Bind(wxEVT_MENU, [this](wxCommandEvent &)
             { CloseWorkspace(); }, ID_CLOSE_WORKSPACE);
        Bind(wxEVT_MENU, [this](wxCommandEvent &)
             { ShowQuickOpen(); }, ID_QUICK_OPEN);
        Bind(wxEVT_MENU, [this](wxCommandEvent &)
             { SaveCurrentTab(); }, ID_SAVE_FILE);
        Bind(wxEVT_MENU, [this](wxCommandEvent &)
             { SaveAs(); }, ID_SAVE_AS);
        Bind(wxEVT_MENU, [this](wxCommandEvent &)
             { SaveAll(); }, ID_SAVE_ALL);
        Bind(wxEVT_MENU, [this](wxCommandEvent &)
             { ShowLocalHistory(); }, ID_FILE_HISTORY);
        Bind(wxEVT_MENU, [this](wxCommandEvent &)
             { CloseCurrentTab(); }, ID_CLOSE_TAB);
        Bind(wxEVT_MENU, [this](wxCommandEvent &)
             { CloseAllTabs(); }, ID_CLOSE_ALL_TABS);
        Bind(wxEVT_MENU, [this](wxCommandEvent &)
             { Close(); }, wxID_EXIT);
        Bind(wxEVT_MENU, [this](wxCommandEvent &)
             { DispatchEdit(wxID_UNDO); }, wxID_UNDO);
        Bind(wxEVT_MENU, [this](wxCommandEvent &)
             { DispatchEdit(wxID_REDO); }, wxID_REDO);
        Bind(wxEVT_MENU, [this](wxCommandEvent &)
             { DispatchEdit(wxID_CUT); }, wxID_CUT);
        Bind(wxEVT_MENU, [this](wxCommandEvent &)
             { DispatchEdit(wxID_COPY); }, wxID_COPY);
        Bind(wxEVT_MENU, [this](wxCommandEvent &)
             { DispatchEdit(wxID_PASTE); }, wxID_PASTE);
        Bind(wxEVT_MENU, [this](wxCommandEvent &)
             { DispatchEdit(wxID_SELECTALL); }, wxID_SELECTALL);
        Bind(wxEVT_MENU, [this](wxCommandEvent &)
             { ShowFindBar(FindBar::Scope::CurrentDocument); }, ID_FIND);
        Bind(wxEVT_MENU, [this](wxCommandEvent &)
             { ShowFindBar(FindBar::Scope::AllDocuments); }, ID_FIND_ALL);
        Bind(wxEVT_MENU, [this](wxCommandEvent &)
             { ExecuteFind(true); }, ID_FIND_NEXT);
        Bind(wxEVT_MENU, [this](wxCommandEvent &)
             { ExecuteFind(false); }, ID_FIND_PREV);
        Bind(wxEVT_MENU, [this](wxCommandEvent &)
             { GoToLine(); }, ID_GOTO_LINE);
        Bind(wxEVT_MENU, [this](wxCommandEvent &)
             { GoToDefinition(); }, ID_GOTO_DEFINITION);
        Bind(wxEVT_MENU, [this](wxCommandEvent &)
             { RefreshCppAnalysis(); }, ID_CPP_PARSE_SYMBOLS);
        Bind(wxEVT_MENU, [this](wxCommandEvent &)
             { CompileCurrentWithLLVM(true); }, ID_CPP_SYNTAX_CHECK);
        Bind(wxEVT_MENU, [this](wxCommandEvent &)
             { CompileCurrentWithLLVM(false); }, ID_CPP_COMPILE);
        Bind(wxEVT_MENU, [this](wxCommandEvent &)
             { ToggleSidebar(); }, ID_TOGGLE_SIDEBAR);
        Bind(wxEVT_MENU, [this](wxCommandEvent &)
             { OpenAIChat(); }, ID_OPEN_AI_CHAT);
        Bind(wxEVT_MENU, [this](wxCommandEvent &)
             { ShowGeneralSettings(); }, ID_GENERAL_SETTINGS);
        Bind(wxEVT_MENU, [this](wxCommandEvent &)
             { ToggleWordWrap(); }, ID_TOGGLE_WORDWRAP);
        Bind(wxEVT_MENU, [this](wxCommandEvent &)
             { ToggleWhitespace(); }, ID_TOGGLE_WHITESPACE);
        Bind(wxEVT_MENU, [this](wxCommandEvent &)
             { ZoomEditor(+1); }, ID_ZOOM_IN);
        Bind(wxEVT_MENU, [this](wxCommandEvent &)
             { ZoomEditor(-1); }, ID_ZOOM_OUT);
        Bind(wxEVT_MENU, [this](wxCommandEvent &)
             { ZoomEditor(0); }, ID_ZOOM_RESET);
        Bind(wxEVT_MENU, [this](wxCommandEvent &)
             { ShowAbout(); }, ID_ABOUT);

        Bind(wxEVT_CLOSE_WINDOW, &MainFrame::OnCloseWindow, this);

        Bind(wxEVT_CHAR_HOOK, [this](wxKeyEvent &e)
             {
            if (e.ControlDown() && e.ShiftDown() && e.GetKeyCode() == 'P')
            {
                CommandPalette* cp = new CommandPalette(this /*, commands*/);
                cp->Show();
            }

            e.Skip(); });
    }

    void BuildStatusBar()
    {
        m_statusBar = CreateStatusBar(4);
        m_statusBar->SetBackgroundColour(Colors::STATUSBAR_BG);
        m_statusBar->SetForegroundColour(Colors::STATUSBAR_FG);
        const int widths[] = {-1, 140, 120, 100};
        m_statusBar->SetStatusWidths(4, widths);
        m_statusBar->SetStatusText("Ready", 0);
        m_statusBar->SetStatusText("Ln 1, Col 1", 1);
        m_statusBar->SetStatusText("UTF-8", 2);
        m_statusBar->SetStatusText("Plain Text", 3);
    }

    void SetupAccelerators()
    {
        // Additional accelerators not covered by menu shortcuts
        wxAcceleratorEntry entries[] = {
            wxAcceleratorEntry(wxACCEL_CTRL, (int)'+', ID_ZOOM_IN),
            wxAcceleratorEntry(wxACCEL_CTRL, WXK_NUMPAD_ADD, ID_ZOOM_IN),
            wxAcceleratorEntry(wxACCEL_CTRL, (int)'-', ID_ZOOM_OUT),
            wxAcceleratorEntry(wxACCEL_CTRL, WXK_NUMPAD_SUBTRACT, ID_ZOOM_OUT),
            wxAcceleratorEntry(wxACCEL_CTRL, (int)'F', ID_FIND),
            wxAcceleratorEntry(wxACCEL_CTRL | wxACCEL_SHIFT, (int)'F', ID_FIND_ALL),
            wxAcceleratorEntry(wxACCEL_CTRL, (int)'P', ID_QUICK_OPEN),
            wxAcceleratorEntry(wxACCEL_CTRL, (int)'G', ID_GOTO_LINE),
            wxAcceleratorEntry(wxACCEL_NORMAL, WXK_F3, ID_FIND_NEXT),
            wxAcceleratorEntry(wxACCEL_SHIFT, WXK_F3, ID_FIND_PREV),
            wxAcceleratorEntry(wxACCEL_NORMAL, WXK_F12, ID_GOTO_DEFINITION),
            wxAcceleratorEntry(wxACCEL_CTRL, WXK_F7, ID_CPP_COMPILE),
            wxAcceleratorEntry(wxACCEL_CTRL | wxACCEL_ALT, (int)'I', ID_OPEN_AI_CHAT),
            wxAcceleratorEntry(wxACCEL_CTRL | wxACCEL_ALT, (int)'H', ID_FILE_HISTORY),
        };
        SetAcceleratorTable(wxAcceleratorTable(static_cast<int>(std::size(entries)), entries));
    }

    // ── Tab helpers ──────────────────────────────────────────────────────────

    EditorPage *CurrentPage()
    {
        int sel = m_notebook->GetSelection();
        if (sel == wxNOT_FOUND)
            return nullptr;
        return dynamic_cast<EditorPage *>(m_notebook->GetPage(sel));
    }

    void NewTab(const wxString &filepath = wxEmptyString)
    {
        auto *page = new EditorPage(m_notebook, filepath);
        wxString title = filepath.IsEmpty()
                             ? wxString::Format("Untitled-%d", ++m_untitledCount)
                             : wxFileName(filepath).GetFullName();
        m_notebook->AddPage(page, title, true);
        page->SetFocus();
        UpdateStatusBar(page);
        page->Bind(wxEVT_STC_UPDATEUI, [this, page](wxStyledTextEvent &e)
                   {
            UpdateStatusBar(page);
            e.Skip(); });
        page->Bind(wxEVT_STC_CHANGE, [this, page](wxStyledTextEvent &e)
                   {
            int idx = m_notebook->GetPageIndex(page);
            if (idx != wxNOT_FOUND)
                m_notebook->SetPageText(idx, page->GetTitle());
            if (m_symbolsPanel && CurrentPage() == page)
                m_symbolsPanel->SetCurrentDocument(page->filepath, page->GetText(), false);
            e.Skip(); });

        if (!page->filepath.IsEmpty())
            ActivateWorkspaceForPath(page->filepath);
        if (m_symbolsPanel)
            m_symbolsPanel->SetCurrentDocument(page->filepath, page->GetText(), true);
    }

    void OpenFileInTab(const wxString &path)
    {
        // Check if already open
        for (size_t i = 0; i < m_notebook->GetPageCount(); ++i)
        {
            auto *p = dynamic_cast<EditorPage *>(m_notebook->GetPage(i));
            if (p && p->filepath == path)
            {
                m_notebook->SetSelection(i);
                return;
            }
        }
        NewTab(path);
    }

    void OpenGitDiff(const wxString& repositoryRoot,
                     const wxString& gitPath,
                     const wxString& status)
    {
        for (size_t i = 0; i < m_notebook->GetPageCount(); ++i)
        {
            auto* diff = dynamic_cast<GitDiffPage*>(m_notebook->GetPage(i));
            if (diff && diff->GetRepositoryRoot() == repositoryRoot && diff->GetGitPath() == gitPath)
            {
                diff->UpdateStatus(status);
                m_notebook->SetSelection(i);
                return;
            }
        }

        auto* diff = new GitDiffPage(m_notebook, repositoryRoot, gitPath, status);
        diff->SetOpenFileCallback([this](const wxString& path)
        {
            OpenFileInTab(path);
        });
        diff->SetRepositoryChangedCallback([this]()
        {
            if (m_gitPanel)
                m_gitPanel->RefreshRepository();
        });
        m_notebook->AddPage(diff, diff->GetTabTitle(), true);
        if (m_statusBar)
            m_statusBar->SetStatusText(wxString("Git diff: ") + gitPath, 0);
    }

    void OpenGitCommitDiff(const wxString& repositoryRoot,
                           const wxString& commitHash,
                           const wxString& oldPath,
                           const wxString& newPath,
                           const wxString& status)
    {
        for (size_t i = 0; i < m_notebook->GetPageCount(); ++i)
        {
            auto* diff = dynamic_cast<GitCommitDiffPage*>(m_notebook->GetPage(i));
            if (diff && diff->GetRepositoryRoot() == repositoryRoot &&
                diff->GetCommitHash() == commitHash && diff->GetGitPath() == newPath)
            {
                diff->RefreshDiff();
                m_notebook->SetSelection(i);
                return;
            }
        }

        auto* diff = new GitCommitDiffPage(m_notebook, repositoryRoot, commitHash,
                                           oldPath, newPath, status);
        diff->SetOpenFileCallback([this](const wxString& path)
        {
            OpenFileInTab(path);
        });
        m_notebook->AddPage(diff, diff->GetTabTitle(), true);
        if (m_statusBar)
            m_statusBar->SetStatusText("Commit " + commitHash.Left(8) + ": " + newPath, 0);
    }

    AIEditorContext BuildAIEditorContext() const
    {
        AIEditorContext context;
        wxWindow* source = m_lastAIContextPage;
        if (!source && m_notebook)
        {
            const int selection = m_notebook->GetSelection();
            if (selection != wxNOT_FOUND)
                source = m_notebook->GetPage(selection);
        }

        if (auto* page = dynamic_cast<EditorPage*>(source))
        {
            context.filePath = page->filepath;
            context.currentFileText = page->GetText();
            context.selection = page->GetSelectedText();
        }
        else if (auto* diff = dynamic_cast<GitDiffPage*>(source))
        {
            context.gitDiff = diff->GetPatchForAI();
            wxString path = diff->GetRepositoryRoot();
            path += wxFileName::GetPathSeparator();
            path += diff->GetGitPath();
            context.filePath = path;
        }
        else if (auto* diff = dynamic_cast<GitCommitDiffPage*>(source))
        {
            context.gitDiff = diff->GetPatchForAI();
            wxString path = diff->GetRepositoryRoot();
            path += wxFileName::GetPathSeparator();
            path += diff->GetGitPath();
            context.filePath = path;
        }
        if (m_symbolsPanel)
            context.llvmContext = m_symbolsPanel->BuildAIContext();
        return context;
    }

    void OpenAIChat()
    {
        if (!m_notebook)
            return;

        const int current = m_notebook->GetSelection();
        if (current != wxNOT_FOUND)
        {
            wxWindow* page = m_notebook->GetPage(current);
            if (dynamic_cast<EditorPage*>(page) || dynamic_cast<GitDiffPage*>(page) ||
                dynamic_cast<GitCommitDiffPage*>(page))
                m_lastAIContextPage = page;
        }

        if (m_aiChatPage)
        {
            const int index = m_notebook->GetPageIndex(m_aiChatPage);
            if (index != wxNOT_FOUND)
            {
                m_aiChatPage->ReloadSettings();
                m_notebook->SetSelection(index);
                return;
            }
            m_aiChatPage = nullptr;
        }

        m_aiChatPage = new AIChatPage(m_notebook);
        m_aiChatPage->SetContextProvider([this]()
        {
            return BuildAIEditorContext();
        });
        m_aiChatPage->SetSettingsCallback([this]()
        {
            ShowGeneralSettings();
        });
        m_notebook->AddPage(m_aiChatPage, "AI Chat", true);
        if (m_statusBar)
            m_statusBar->SetStatusText("AI Chat", 0);
    }

    void ApplyEditorThemeToOpenPages()
    {
        if (!m_notebook)
            return;
        for (size_t i = 0; i < m_notebook->GetPageCount(); ++i)
        {
            wxWindow* window = m_notebook->GetPage(i);
            if (auto* page = dynamic_cast<EditorPage*>(window))
                page->ApplyTheme();
            else if (auto* commitDiff = dynamic_cast<GitCommitDiffPage*>(window))
                commitDiff->ApplyTheme();
        }
        if (auto* page = CurrentPage())
            UpdateStatusBar(page);
    }

    void ShowGeneralSettings()
    {
        GeneralSettingsDialog dialog(this);
        if (dialog.ShowModal() == wxID_OK && dialog.SettingsChanged())
        {
            ApplyEditorThemeToOpenPages();
            if (m_aiChatPage)
                m_aiChatPage->ReloadSettings();
            if (m_statusBar)
                m_statusBar->SetStatusText("Settings saved", 0);
        }
    }

    void OpenLocation(const wxString& path, unsigned line, unsigned column)
    {
        if (path.IsEmpty() || !wxFileExists(path))
            return;

        OpenFileInTab(path);
        auto* page = CurrentPage();
        if (!page)
            return;

        const int lineIndex = line > 0 ? static_cast<int>(line - 1) : 0;
        page->GotoLine(lineIndex);
        int position = page->PositionFromLine(lineIndex);
        if (column > 0)
            position += static_cast<int>(column - 1);
        position = std::min(position, page->GetLength());
        page->SetCurrentPos(position);
        page->SetSelection(position, position);
        page->EnsureCaretVisible();
        page->SetFocus();
    }

    void RefreshCppAnalysis()
    {
        auto* page = CurrentPage();
        if (!page || !m_symbolsPanel)
            return;
        if (m_sideNotebook && m_sideNotebook->GetPageCount() > 1)
            m_sideNotebook->SetSelection(1);
        m_symbolsPanel->SetCurrentDocument(page->filepath, page->GetText(), false);
        m_symbolsPanel->RefreshAnalysis();
    }

    void CompileCurrentWithLLVM(bool syntaxOnly)
    {
        auto* page = CurrentPage();
        if (!page || !m_symbolsPanel)
            return;
        if (m_sideNotebook && m_sideNotebook->GetPageCount() > 1)
            m_sideNotebook->SetSelection(1);
        m_symbolsPanel->SetCurrentDocument(page->filepath, page->GetText(), false);
        m_symbolsPanel->CompileCurrent(syntaxOnly);
    }

    void GoToDefinition()
    {
        auto* page = CurrentPage();
        if (!page || !m_symbolsPanel)
            return;
        if (!CppAnalysisEngine::IsCOrCppFile(page->filepath))
            return;

        m_symbolsPanel->SetCurrentDocument(page->filepath, page->GetText(), false);
        const int caret = page->GetCurrentPos();
        const int lineIndex = page->LineFromPosition(caret);
        const unsigned line = static_cast<unsigned>(lineIndex + 1);
        const unsigned column = static_cast<unsigned>(caret - page->PositionFromLine(lineIndex) + 1);
        if (!m_symbolsPanel->NavigateToDefinition(line, column) && m_statusBar)
            m_statusBar->SetStatusText("Definition not found", 0);
    }

    void UpdateStatusBar(EditorPage *page)
    {
        if (!page)
            return;
        int line = page->GetCurrentLine() + 1;
        int col = page->GetColumn(page->GetCurrentPos()) + 1;
        m_statusBar->SetStatusText(wxString::Format("Ln %d, Col %d", line, col), 1);
        m_statusBar->SetStatusText("UTF-8", 2);
        m_statusBar->SetStatusText(page->GetLanguageName(), 3);
        wxString title = page->filepath.IsEmpty() ? "Untitled" : page->filepath;
        m_statusBar->SetStatusText(title, 0);
    }

    void UpdateTabTitle(EditorPage *page)
    {
        int idx = m_notebook->GetPageIndex(page);
        if (idx != wxNOT_FOUND)
            m_notebook->SetPageText(idx, page->GetTitle());
    }

    // ── File operations ──────────────────────────────────────────────────────

    void NewFile() { NewTab(); }

    void OpenFile()
    {
        wxFileDialog dlg(this, "Open File", wxEmptyString, wxEmptyString,
                         "All files (*.*)|*.*|"
                         "C/C++ (*.c;*.cpp;*.h;*.hpp)|*.c;*.cpp;*.h;*.hpp|"
                         "Python (*.py)|*.py|"
                         "JavaScript/TypeScript (*.js;*.ts;*.jsx;*.tsx)|*.js;*.ts;*.jsx;*.tsx|"
                         "Rust (*.rs)|*.rs|"
                         "HTML (*.html;*.htm)|*.html;*.htm|"
                         "CSS (*.css;*.scss)|*.css;*.scss|"
                         "JSON (*.json)|*.json|"
                         "Markdown (*.md)|*.md|"
                         "Shell (*.sh;*.bash)|*.sh;*.bash|"
                         "YAML (*.yml;*.yaml)|*.yml;*.yaml",
                         wxFD_OPEN | wxFD_FILE_MUST_EXIST | wxFD_MULTIPLE);

        if (dlg.ShowModal() == wxID_OK)
        {
            wxArrayString paths;
            dlg.GetPaths(paths);
            for (const auto &p : paths)
                OpenFileInTab(p);
        }
    }

    std::vector<wxString> WorkspaceFolderPaths() const
    {
        return m_workspace.GetFolderPaths();
    }

    void PersistWorkspaceSession()
    {
        std::vector<std::string> folders;
        for (const wxString& folder : m_workspace.GetFolderPaths())
        {
            const wxScopedCharBuffer utf8 = folder.utf8_str();
            folders.push_back(utf8.data() ? std::string(utf8.data()) : std::string());
        }
        const wxScopedCharBuffer workspaceUtf8 = m_workspace.GetWorkspaceFile().utf8_str();
        AppEditorConfig::SaveWorkspaceSession(
            folders,
            workspaceUtf8.data() ? std::string(workspaceUtf8.data()) : std::string());
    }

    void SetActiveWorkspaceRoot(const wxString& root)
    {
        if (m_activeWorkspaceRoot == root)
            return;

        m_activeWorkspaceRoot = root;
        if (m_gitPanel)
            m_gitPanel->SetRepositoryPath(root);
        if (m_symbolsPanel)
            m_symbolsPanel->SetProjectRoot(root);
    }

    void ActivateWorkspaceForPath(const wxString& path)
    {
        const wxString root = m_workspace.FindContainingFolder(path);
        if (!root.IsEmpty())
            SetActiveWorkspaceRoot(root);
    }

    wxString WorkspaceDisplayPath(const wxString& absolutePath) const
    {
        const wxString root = m_workspace.FindContainingFolder(absolutePath);
        if (root.IsEmpty())
            return absolutePath;

        wxFileName relative(absolutePath);
        wxString display = absolutePath;
        if (relative.MakeRelativeTo(root))
            display = relative.GetFullPath();

        if (m_workspace.GetFolders().size() > 1)
        {
            wxString prefixed = m_workspace.GetFolderLabel(root);
            prefixed += "/";
            prefixed += display;
            display = prefixed;
        }
        return display;
    }

    void ApplyWorkspaceToUI()
    {
        const std::vector<wxString> folders = m_workspace.GetFolderPaths();
        if (m_tree)
        {
            if (folders.empty())
                m_tree->ClearFolders();
            else if (folders.size() == 1 && !m_workspace.IsWorkspaceMode())
                m_tree->LoadFolder(folders.front());
            else
                m_tree->LoadFolders(folders, true);
        }

        wxString preferredRoot;
        wxString preferredListFolder;
        if (auto* page = CurrentPage())
        {
            preferredRoot = m_workspace.FindContainingFolder(page->filepath);
            if (!page->filepath.IsEmpty())
                preferredListFolder = wxFileName(page->filepath).GetPath();
        }
        if (preferredRoot.IsEmpty() && !folders.empty())
            preferredRoot = folders.front();
        if (preferredListFolder.IsEmpty())
            preferredListFolder = preferredRoot;
        if (m_folderList)
            m_folderList->SetFolder(preferredListFolder);

        // Force panels to refresh even when a workspace was replaced with a
        // folder using the same textual path.
        if (preferredRoot != m_activeWorkspaceRoot)
            SetActiveWorkspaceRoot(preferredRoot);
        else
        {
            if (m_gitPanel)
                m_gitPanel->SetRepositoryPath(preferredRoot);
            if (m_symbolsPanel)
                m_symbolsPanel->SetProjectRoot(preferredRoot);
        }

        m_projectSearchResults.clear();
        if (m_findBar)
            m_findBar->ClearResults();

        const wxString name = m_workspace.GetDisplayName();
        if (name == "DoDevEditor")
            SetTitle("DoDevEditor");
        else
            SetTitle(wxString("DoDevEditor — ") + name);
        if (m_statusBar)
        {
            if (folders.empty())
                m_statusBar->SetStatusText("No folder open", 0);
            else if (folders.size() == 1)
                m_statusBar->SetStatusText(wxString("Folder: ") + folders.front(), 0);
            else
                m_statusBar->SetStatusText(wxString::Format("Workspace: %zu folders", folders.size()), 0);
        }
    }

    void AddFolderToWorkspace()
    {
        wxDirDialog dlg(this, "Add Folder to Workspace", wxEmptyString,
                        wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);
        if (dlg.ShowModal() != wxID_OK)
            return;

        if (!m_workspace.AddFolder(dlg.GetPath()))
        {
            wxMessageBox("Could not add the selected folder.", "Workspace", wxOK | wxICON_ERROR, this);
            return;
        }

        const wxString workspaceFile = m_workspace.GetWorkspaceFile();
        if (!workspaceFile.IsEmpty())
        {
            wxString error;
            if (!m_workspace.SaveWorkspace(workspaceFile, &error))
                wxMessageBox(error, "Workspace", wxOK | wxICON_WARNING, this);
        }
        ApplyWorkspaceToUI();
        PersistWorkspaceSession();
    }

    void RemoveFolderFromWorkspace()
    {
        const auto& folders = m_workspace.GetFolders();
        if (folders.empty())
            return;

        wxArrayString choices;
        for (const auto& folder : folders)
        {
            wxString label = folder.name;
            label += " — ";
            label += folder.path;
            choices.Add(label);
        }

        wxSingleChoiceDialog dlg(this,
                                 "Choose a folder to remove from the workspace.",
                                 "Remove Folder from Workspace",
                                 choices);
        if (dlg.ShowModal() != wxID_OK)
            return;

        const int selection = dlg.GetSelection();
        if (selection < 0 || static_cast<size_t>(selection) >= folders.size())
            return;
        const wxString path = folders[static_cast<size_t>(selection)].path;
        if (!m_workspace.RemoveFolder(path))
            return;

        const wxString workspaceFile = m_workspace.GetWorkspaceFile();
        if (!m_workspace.HasFolders())
        {
            m_workspace.Clear();
        }
        else if (!workspaceFile.IsEmpty())
        {
            wxString error;
            if (!m_workspace.SaveWorkspace(workspaceFile, &error))
                wxMessageBox(error, "Workspace", wxOK | wxICON_WARNING, this);
        }
        ApplyWorkspaceToUI();
        PersistWorkspaceSession();
    }

    void OpenWorkspace()
    {
        wxFileDialog dlg(this,
                         "Open Workspace",
                         wxEmptyString,
                         wxEmptyString,
                         "DoDevEditor workspace (*.dodev-workspace)|*.dodev-workspace|JSON files (*.json)|*.json|All files (*.*)|*.*",
                         wxFD_OPEN | wxFD_FILE_MUST_EXIST);
        if (dlg.ShowModal() != wxID_OK)
            return;

        wxString error;
        if (!m_workspace.LoadWorkspace(dlg.GetPath(), &error))
        {
            wxMessageBox(error, "Open Workspace", wxOK | wxICON_ERROR, this);
            return;
        }
        ApplyWorkspaceToUI();
        PersistWorkspaceSession();
    }

    void SaveWorkspaceAs()
    {
        if (!m_workspace.HasFolders())
        {
            wxMessageBox("Open or add at least one folder before saving a workspace.",
                         "Save Workspace", wxOK | wxICON_INFORMATION, this);
            return;
        }

        wxString directory;
        const auto folders = m_workspace.GetFolderPaths();
        if (!folders.empty())
            directory = folders.front();

        wxFileDialog dlg(this,
                         "Save Workspace As",
                         directory,
                         "workspace.dodev-workspace",
                         "DoDevEditor workspace (*.dodev-workspace)|*.dodev-workspace",
                         wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
        if (dlg.ShowModal() != wxID_OK)
            return;

        wxString path = dlg.GetPath();
        if (!path.Lower().EndsWith(".dodev-workspace"))
            path += ".dodev-workspace";

        wxString error;
        if (!m_workspace.SaveWorkspace(path, &error))
        {
            wxMessageBox(error, "Save Workspace", wxOK | wxICON_ERROR, this);
            return;
        }
        ApplyWorkspaceToUI();
        PersistWorkspaceSession();
        if (m_statusBar)
            m_statusBar->SetStatusText(wxString("Workspace saved: ") + path, 0);
    }

    void CloseWorkspace()
    {
        m_workspace.Clear();
        m_activeWorkspaceRoot.clear();
        ApplyWorkspaceToUI();
        PersistWorkspaceSession();
    }

    void OpenFolder()
    {
        wxDirDialog dlg(this, "Open Folder", wxEmptyString,
                        wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);
        if (dlg.ShowModal() == wxID_OK)
            loadPath(dlg.GetPath());
    }

    void loadPath(const wxString &path)
    {
        if (wxDirExists(path))
        {
            if (!m_workspace.OpenFolder(path))
            {
                wxMessageBox("Could not open the specified folder.", "Error", wxOK | wxICON_ERROR, this);
                return;
            }
            ApplyWorkspaceToUI();
            PersistWorkspaceSession();
        }
        else if (wxFileExists(path))
        {
            OpenFileInTab(path);
        }
        else
        {
            wxMessageBox("The specified path does not exist.", "Error", wxOK | wxICON_ERROR, this);
        }
    }

    std::vector<QuickOpenItem> CollectQuickOpenItems() const
    {
        std::vector<QuickOpenItem> items;
        std::unordered_set<std::string> seen;

        auto addFile = [&items, &seen](const wxString& absolutePath,
                                       const wxString& displayPath)
        {
            if (absolutePath.IsEmpty() || !wxFileExists(absolutePath))
                return;

            const wxScopedCharBuffer utf8 = absolutePath.utf8_str();
            const std::string key = utf8.data() ? std::string(utf8.data()) : std::string();
            if (!seen.insert(key).second)
                return;

            QuickOpenItem item;
            item.absolutePath = absolutePath;
            item.displayPath = displayPath.IsEmpty() ? absolutePath : displayPath;
            items.push_back(item);
        };

        namespace fs = std::filesystem;
        for (const wxString& root : m_workspace.GetFolderPaths())
        {
            if (root.IsEmpty() || !wxDirExists(root))
                continue;

            const wxScopedCharBuffer rootUtf8 = root.utf8_str();
            const fs::path rootPath = fs::u8path(rootUtf8.data() ? rootUtf8.data() : "");
            std::error_code ec;
            fs::recursive_directory_iterator it(
                rootPath,
                fs::directory_options::skip_permission_denied,
                ec);
            const fs::recursive_directory_iterator end;

            for (; it != end; it.increment(ec))
            {
                if (ec)
                {
                    ec.clear();
                    continue;
                }

                const fs::directory_entry& entry = *it;
                const std::string name = entry.path().filename().string();

                if (entry.is_directory(ec))
                {
                    if (name == ".git" || name == ".dodev" || name == "node_modules" ||
                        name == "build" || name == "out" || name == "dist" ||
                        name == ".cache" || name == "_deps")
                    {
                        it.disable_recursion_pending();
                    }
                    continue;
                }

                if (!entry.is_regular_file(ec))
                    continue;

                const wxString absolute = wxString::FromUTF8(entry.path().u8string().c_str());
                addFile(absolute, WorkspaceDisplayPath(absolute));
            }
        }

        // Keep already-open files discoverable even when no folder is open and
        // also include files outside every workspace root.
        if (m_notebook)
        {
            for (size_t i = 0; i < m_notebook->GetPageCount(); ++i)
            {
                auto* page = dynamic_cast<EditorPage*>(m_notebook->GetPage(i));
                if (!page || page->filepath.IsEmpty())
                    continue;

                wxString display = WorkspaceDisplayPath(page->filepath);
                if (m_workspace.FindContainingFolder(page->filepath).IsEmpty())
                    display = wxFileName(page->filepath).GetFullName();
                addFile(page->filepath, display);
            }
        }

        std::sort(items.begin(), items.end(), [](const QuickOpenItem& a, const QuickOpenItem& b)
        {
            return a.displayPath.CmpNoCase(b.displayPath) < 0;
        });
        return items;
    }

    void ShowQuickOpen()
    {
        const std::vector<QuickOpenItem> items = CollectQuickOpenItems();
        if (items.empty())
        {
            if (m_statusBar)
                m_statusBar->SetStatusText("Quick Open: no workspace files available", 0);
            return;
        }

        QuickOpenDialog dialog(this, items);
        if (dialog.ShowModal() != wxID_OK)
            return;

        const wxString path = dialog.GetSelectedPath();
        if (!path.IsEmpty())
        {
            OpenFileInTab(path);
            if (auto* page = CurrentPage())
                page->SetFocus();
        }
    }

    wxString HistoryRootForPath(const wxString& path) const
    {
        wxString root = m_workspace.FindContainingFolder(path);
        if (!root.IsEmpty())
            return root;
        return wxFileName(path).GetPath();
    }

    void AfterSuccessfulSave(EditorPage* page)
    {
        UpdateTabTitle(page);
        ActivateWorkspaceForPath(page->filepath);
        if (m_gitPanel)
            m_gitPanel->RefreshRepository();
        if (m_symbolsPanel)
        {
            m_symbolsPanel->SetCurrentDocument(page->filepath, page->GetText(), false);
            if (m_symbolsPanel->IsLLVMEnabled())
                m_symbolsPanel->RefreshAnalysis();
        }
    }

    bool SavePageWithHistory(EditorPage* page, const wxString& targetPath = wxEmptyString)
    {
        if (!page)
            return false;

        const wxString target = targetPath.IsEmpty() ? page->filepath : targetPath;
        if (target.IsEmpty())
            return false;

        const wxString root = HistoryRootForPath(target);
        wxString historyWarning;
        if (wxFileExists(target))
            LocalHistoryManager::EnsureBaseline(target, root, &historyWarning);

        const bool ok = page->SaveFile(targetPath);
        if (!ok)
            return false;

        wxString recordWarning;
        if (!LocalHistoryManager::RecordSave(page->filepath,
                                             HistoryRootForPath(page->filepath),
                                             page->GetText(),
                                             "Save",
                                             &recordWarning))
        {
            if (m_statusBar)
                m_statusBar->SetStatusText("File saved, but local history failed: " + recordWarning, 0);
        }
        else if (!historyWarning.IsEmpty() && m_statusBar)
        {
            m_statusBar->SetStatusText("File saved; baseline warning: " + historyWarning, 0);
        }

        AfterSuccessfulSave(page);
        return true;
    }

    bool SaveCurrentTab()
    {
        auto *page = CurrentPage();
        if (!page)
            return false;
        if (page->filepath.IsEmpty())
            return SaveAs();
        return SavePageWithHistory(page);
    }

    bool SaveAs()
    {
        auto *page = CurrentPage();
        if (!page)
            return false;
        wxFileDialog dlg(this, "Save As", wxEmptyString,
                         page->filepath.IsEmpty() ? "Untitled.txt"
                                                  : wxFileName(page->filepath).GetFullName(),
                         "All files (*.*)|*.*",
                         wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
        if (dlg.ShowModal() != wxID_OK)
            return false;
        return SavePageWithHistory(page, dlg.GetPath());
    }

    void SaveAll()
    {
        for (size_t i = 0; i < m_notebook->GetPageCount(); ++i)
        {
            auto *p = dynamic_cast<EditorPage *>(m_notebook->GetPage(i));
            if (!p || !p->modified)
                continue;

            if (p->filepath.IsEmpty())
            {
                m_notebook->SetSelection(i);
                SaveAs();
            }
            else
            {
                SavePageWithHistory(p);
            }
        }
        if (m_gitPanel)
            m_gitPanel->RefreshRepository();
    }

    void ShowLocalHistory()
    {
        EditorPage* page = CurrentPage();
        if (!page || page->filepath.IsEmpty())
        {
            wxMessageBox("Local History is available for files that have been saved to disk.",
                         "Local History", wxOK | wxICON_INFORMATION, this);
            return;
        }

        const wxString root = HistoryRootForPath(page->filepath);
        const std::vector<LocalHistoryEntry> entries =
            LocalHistoryManager::List(page->filepath, root);
        if (entries.empty())
        {
            wxMessageBox("No local history exists for this file yet.\n\nSave the file to create a revision.",
                         "Local History", wxOK | wxICON_INFORMATION, this);
            return;
        }

        FileHistoryDialog dialog(this, page->filepath, root);
        if (dialog.ShowModal() != wxID_OK)
            return;

        wxString content;
        wxString error;
        if (!dialog.GetSelectedSnapshot(content, &error))
        {
            wxMessageBox(error, "Local History", wxOK | wxICON_ERROR, this);
            return;
        }

        const LocalHistoryEntry* selected = dialog.GetSelectedEntry();
        const wxString label = selected ? selected->timestamp : wxString("selected revision");
        const int answer = wxMessageBox(
            "Restore local-history revision " + label +
            "?\n\nThe selected content will be loaded into the editor as an unsaved change. "
            "The current file on disk will not be overwritten until you save.",
            "Restore Local History", wxYES_NO | wxICON_QUESTION, this);
        if (answer != wxYES)
            return;

        page->BeginUndoAction();
        page->SetTargetStart(0);
        page->SetTargetEnd(page->GetTextLength());
        page->ReplaceTarget(content);
        page->EndUndoAction();
        page->modified = true;
        page->ApplySyntax();
        UpdateTabTitle(page);
        page->SetFocus();
        if (m_statusBar)
            m_statusBar->SetStatusText("Restored local-history revision " + label + " (not saved)", 0);
    }

    bool ConfirmClose(EditorPage *page)
    {
        if (!page->modified)
            return true;
        wxString name = page->filepath.IsEmpty() ? "Untitled" : wxFileName(page->filepath).GetFullName();
        int answer = wxMessageBox(
            wxString::Format("'%s' has unsaved changes.\nSave before closing?", name),
            "Unsaved Changes",
            wxYES_NO | wxCANCEL | wxICON_WARNING, this);
        if (answer == wxCANCEL)
            return false;
        if (answer == wxYES)
        {
            if (page->filepath.IsEmpty())
            {
                const int pageIndex = m_notebook->GetPageIndex(page);
                if (pageIndex != wxNOT_FOUND)
                    m_notebook->SetSelection(pageIndex);
                return SaveAs();
            }
            return SavePageWithHistory(page);
        }
        return true;
    }

    void CloseCurrentTab()
    {
        int sel = m_notebook->GetSelection();
        if (sel == wxNOT_FOUND)
            return;
        auto *page = dynamic_cast<EditorPage *>(m_notebook->GetPage(sel));
        if (!page || ConfirmClose(page))
            m_notebook->DeletePage(sel);
    }

    void CloseAllTabs()
    {
        for (int i = (int)m_notebook->GetPageCount() - 1; i >= 0; --i)
        {
            auto *page = dynamic_cast<EditorPage *>(m_notebook->GetPage(i));
            if (page && !ConfirmClose(page))
                return;
            m_notebook->DeletePage(i);
        }
    }

    // ── Edit dispatch ────────────────────────────────────────────────────────

    void DispatchEdit(int id)
    {
        auto *page = CurrentPage();
        if (!page)
            return;
        switch (id)
        {
        case wxID_UNDO:
            page->Undo();
            break;
        case wxID_REDO:
            page->Redo();
            break;
        case wxID_CUT:
            page->Cut();
            break;
        case wxID_COPY:
            page->Copy();
            break;
        case wxID_PASTE:
            page->Paste();
            break;
        case wxID_SELECTALL:
            page->SelectAll();
            break;
        }
    }

    // ── Find ────────────────────────────────────────────────────────────────

    void ShowFindBar(FindBar::Scope scope = FindBar::Scope::CurrentDocument)
    {
        m_findBar->SetScope(scope);
        if (scope == FindBar::Scope::CurrentDocument)
            m_findBar->resultLabel->SetLabel("");
        m_findBar->Show();
        m_editorPane->Layout();
        m_findBar->textCtrl->SetFocus();

        auto* page = CurrentPage();
        if (scope == FindBar::Scope::CurrentDocument &&
            page && !page->GetSelectedText().IsEmpty())
        {
            m_findBar->textCtrl->SetValue(page->GetSelectedText());
            m_findBar->textCtrl->SelectAll();
        }

        if (scope == FindBar::Scope::AllDocuments &&
            !m_findBar->textCtrl->GetValue().IsEmpty())
        {
            SearchAllDocuments();
        }
    }

    void CloseFindBar()
    {
        m_findBar->Hide();
        m_editorPane->Layout();
        if (auto* page = CurrentPage())
            page->SetFocus();
    }

    void ExecuteFind(bool forward)
    {
        if (m_findBar->GetScope() == FindBar::Scope::AllDocuments)
        {
            if (m_projectSearchResults.empty())
                SearchAllDocuments();

            const long count = m_findBar->resultsList->GetItemCount();
            if (count <= 0)
                return;

            long selected = m_findBar->resultsList->GetNextItem(
                -1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
            if (selected == -1)
                selected = forward ? 0 : count - 1;
            else
                selected = forward ? (selected + 1) % count
                                   : (selected + count - 1) % count;

            m_findBar->resultsList->SetItemState(
                selected, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED,
                wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
            m_findBar->resultsList->EnsureVisible(selected);
            OpenProjectSearchResult(selected);
            return;
        }

        FindNextInCurrentDocument(forward);
    }

    void FindNextInCurrentDocument(bool forward)
    {
        auto* page = CurrentPage();
        if (!page)
            return;

        const wxString text = m_findBar->textCtrl->GetValue();
        if (text.IsEmpty())
            return;

        int flags = 0;
        if (m_findBar->caseChk->IsChecked())
            flags |= wxSTC_FIND_MATCHCASE;
        if (m_findBar->wholeChk->IsChecked())
            flags |= wxSTC_FIND_WHOLEWORD;

        page->SearchAnchor();
        int result = forward ? page->SearchNext(flags, text)
                             : page->SearchPrev(flags, text);
        if (result == wxSTC_INVALID_POSITION)
        {
            page->SetCurrentPos(forward ? 0 : page->GetLength());
            page->SearchAnchor();
            result = forward ? page->SearchNext(flags, text)
                             : page->SearchPrev(flags, text);
        }

        if (result != wxSTC_INVALID_POSITION)
        {
            page->EnsureCaretVisible();
            m_findBar->textCtrl->SetBackgroundColour(wxColour(60, 60, 60));
            m_findBar->resultLabel->SetLabel("Match found");
        }
        else
        {
            m_findBar->textCtrl->SetBackgroundColour(wxColour(100, 40, 40));
            m_findBar->resultLabel->SetLabel("No matches");
        }
        m_findBar->textCtrl->Refresh();
    }

    static bool IsWordCharacter(unsigned char c)
    {
        return std::isalnum(c) != 0 || c == '_';
    }

    bool MatchWholeWord(const std::string& line, size_t pos, size_t length) const
    {
        const bool leftOk = pos == 0 || !IsWordCharacter(static_cast<unsigned char>(line[pos - 1]));
        const size_t end = pos + length;
        const bool rightOk = end >= line.size() || !IsWordCharacter(static_cast<unsigned char>(line[end]));
        return leftOk && rightOk;
    }

    bool ShouldSkipSearchDirectory(const std::filesystem::path& path) const
    {
        const std::string name = path.filename().u8string();
        return name == ".git" || name == ".svn" || name == ".hg" ||
               name == ".dodev" || name == "_deps" ||
               name == "build" || name == "dist" || name == "out" ||
               name == "target" || name == "node_modules" ||
               name == ".cache" || name.rfind("cmake-build-", 0) == 0;
    }

    void SearchAllDocuments()
    {
        m_projectSearchResults.clear();
        m_findBar->ClearResults();
        m_findBar->SetResultsVisible(true);
        m_editorPane->Layout();

        const wxString queryWx = m_findBar->textCtrl->GetValue();
        if (queryWx.IsEmpty())
            return;

        const std::vector<wxString> roots = m_workspace.GetFolderPaths();
        if (roots.empty())
        {
            m_findBar->resultLabel->SetLabel("Open a folder or workspace to search all documents");
            return;
        }

        const wxCharBuffer queryBuffer = queryWx.ToUTF8();
        if (!queryBuffer.data())
            return;

        std::string query(queryBuffer.data());
        std::string needle = query;
        const bool matchCase = m_findBar->caseChk->IsChecked();
        const bool wholeWord = m_findBar->wholeChk->IsChecked();
        if (!matchCase)
        {
            std::transform(needle.begin(), needle.end(), needle.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        }

        constexpr std::uintmax_t kMaxFileSize = 4u * 1024u * 1024u;
        constexpr size_t kMaxResults = 5000;
        std::unordered_set<std::string> searchedFiles;

        for (const wxString& rootWx : roots)
        {
            if (m_projectSearchResults.size() >= kMaxResults || !wxDirExists(rootWx))
                break;

            const wxCharBuffer rootBuffer = rootWx.ToUTF8();
            if (!rootBuffer.data())
                continue;
            const std::filesystem::path root = std::filesystem::u8path(rootBuffer.data());

            std::error_code ec;
            std::filesystem::recursive_directory_iterator it(
                root, std::filesystem::directory_options::skip_permission_denied, ec);
            const std::filesystem::recursive_directory_iterator end;

            for (; it != end && m_projectSearchResults.size() < kMaxResults; it.increment(ec))
            {
                if (ec)
                {
                    ec.clear();
                    continue;
                }

                const std::filesystem::directory_entry& entry = *it;
                if (entry.is_directory(ec))
                {
                    if (ShouldSkipSearchDirectory(entry.path()))
                        it.disable_recursion_pending();
                    continue;
                }
                if (!entry.is_regular_file(ec))
                    continue;

                const std::string fileKey = entry.path().u8string();
                if (!searchedFiles.insert(fileKey).second)
                    continue;

                const std::uintmax_t size = entry.file_size(ec);
                if (ec || size > kMaxFileSize)
                {
                    ec.clear();
                    continue;
                }

                std::ifstream input(entry.path(), std::ios::binary);
                if (!input)
                    continue;

                std::string line;
                int lineNumber = 0;
                bool binaryFile = false;
                while (std::getline(input, line))
                {
                    ++lineNumber;
                    if (line.find('\0') != std::string::npos)
                    {
                        binaryFile = true;
                        break;
                    }

                    std::string haystack = line;
                    if (!matchCase)
                    {
                        std::transform(haystack.begin(), haystack.end(), haystack.begin(),
                                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                    }

                    size_t from = 0;
                    while (from <= haystack.size())
                    {
                        const size_t pos = haystack.find(needle, from);
                        if (pos == std::string::npos)
                            break;

                        if (!wholeWord || MatchWholeWord(haystack, pos, needle.size()))
                        {
                            ProjectSearchResult result;
                            result.filepath = wxString::FromUTF8(entry.path().u8string().c_str());
                            result.line = lineNumber;
                            result.column = static_cast<int>(pos);
                            result.preview = wxString::FromUTF8(line.c_str(), line.size());
                            if (result.preview.IsEmpty() && !line.empty())
                                result.preview = wxString::From8BitData(line.c_str(), line.size());
                            result.preview.Trim(true).Trim(false);
                            m_projectSearchResults.push_back(result);
                            if (m_projectSearchResults.size() >= kMaxResults)
                                break;
                        }

                        from = pos + std::max<size_t>(needle.size(), 1);
                    }

                    if (m_projectSearchResults.size() >= kMaxResults)
                        break;
                }

                if (binaryFile)
                    continue;
            }
        }

        long row = 0;
        for (const auto& result : m_projectSearchResults)
        {
            const long item = m_findBar->resultsList->InsertItem(
                row, WorkspaceDisplayPath(result.filepath));
            m_findBar->resultsList->SetItem(item, 1,
                wxString::Format("%d:%d", result.line, result.column + 1));
            m_findBar->resultsList->SetItem(item, 2, result.preview);
            ++row;
        }

        const size_t count = m_projectSearchResults.size();
        if (count == 0)
        {
            m_findBar->textCtrl->SetBackgroundColour(wxColour(100, 40, 40));
            m_findBar->resultLabel->SetLabel("No matches");
        }
        else
        {
            m_findBar->textCtrl->SetBackgroundColour(wxColour(60, 60, 60));
            m_findBar->resultLabel->SetLabel(
                wxString::Format("%zu match%s", count, count == 1 ? "" : "es"));
        }
        m_findBar->textCtrl->Refresh();
    }

    void OpenProjectSearchResult(long index)
    {
        if (index < 0 || static_cast<size_t>(index) >= m_projectSearchResults.size())
            return;

        const ProjectSearchResult& result = m_projectSearchResults[static_cast<size_t>(index)];
        OpenFileInTab(result.filepath);
        auto* page = CurrentPage();
        if (!page)
            return;

        const int lineIndex = std::max(0, result.line - 1);
        page->GotoLine(lineIndex);
        const int lineStart = page->PositionFromLine(lineIndex);
        const int start = lineStart + result.column;
        const int length = static_cast<int>(m_findBar->textCtrl->GetValue().ToUTF8().length());
        page->SetSelection(start, start + std::max(length, 1));
        page->EnsureCaretVisible();
        page->SetFocus();
    }

    // ── View ────────────────────────────────────────────────────────────────

    void ToggleSidebar()
    {
        wxWindow *left = m_splitter->GetWindow1();
        if (left && left->IsShown())
        {
            m_splitter->Unsplit(left);
            left->Hide();
        }
        else if (left)
        {
            left->Show();
            m_splitter->SplitVertically(left, m_splitter->GetWindow2(), 240);
        }
    }

    int FindSidePage(wxWindow* page) const
    {
        if (!m_sideNotebook || !page)
            return wxNOT_FOUND;
        for (size_t i = 0; i < m_sideNotebook->GetPageCount(); ++i)
        {
            if (m_sideNotebook->GetPage(i) == page)
                return static_cast<int>(i);
        }
        return wxNOT_FOUND;
    }

    void ToggleDockerInspector()
    {
        if (!m_sideNotebook)
            return;

        if (m_dockerPanel)
        {
            const int index = FindSidePage(m_dockerPanel);
            if (index != wxNOT_FOUND)
                m_sideNotebook->DeletePage(static_cast<size_t>(index));
            m_dockerPanel = nullptr;
            if (GetMenuBar())
                GetMenuBar()->Check(ID_TOGGLE_DOCKER, false);
            if (m_statusBar)
                m_statusBar->SetStatusText("Docker inspector disabled", 0);
            return;
        }

        m_dockerPanel = new DockerPanel(m_sideNotebook);
        m_dockerPanel->SetDisableCallback([this]()
        {
            // Defer deletion until the DockerPanel button event has unwound.
            CallAfter([this]() { ToggleDockerInspector(); });
        });
        m_sideNotebook->AddPage(m_dockerPanel, "DOCKER", true);
        if (GetMenuBar())
            GetMenuBar()->Check(ID_TOGGLE_DOCKER, true);
        if (m_statusBar)
            m_statusBar->SetStatusText("Docker inspector enabled", 0);
    }

    void ToggleWordWrap()
    {
        for (size_t i = 0; i < m_notebook->GetPageCount(); ++i)
        {
            auto *p = dynamic_cast<EditorPage *>(m_notebook->GetPage(i));
            if (!p)
                continue;
            p->SetWrapMode(p->GetWrapMode() == wxSTC_WRAP_NONE
                               ? wxSTC_WRAP_WORD
                               : wxSTC_WRAP_NONE);
        }
    }

    void ToggleWhitespace()
    {
        for (size_t i = 0; i < m_notebook->GetPageCount(); ++i)
        {
            auto *p = dynamic_cast<EditorPage *>(m_notebook->GetPage(i));
            if (!p)
                continue;
            p->SetViewWhiteSpace(p->GetViewWhiteSpace() == wxSTC_WS_INVISIBLE
                                     ? wxSTC_WS_VISIBLEALWAYS
                                     : wxSTC_WS_INVISIBLE);
        }
    }

    void ZoomEditor(int delta)
    {
        auto *page = CurrentPage();
        if (!page)
            return;
        if (delta == 0)
            page->SetZoom(0);
        else
            page->SetZoom(page->GetZoom() + delta);
    }

    void GoToLine()
    {
        auto* page = CurrentPage();
        if (!page)
            return;

        const int maxLine = page->GetLineCount();
        const int currentLine = page->GetCurrentLine() + 1;
        const int currentColumn = page->GetColumn(page->GetCurrentPos()) + 1;

        wxString value = wxGetTextFromUser(
            wxString::Format("Go to line or line:column (1-%d):", maxLine),
            "Go to Line/Column",
            wxString::Format("%d:%d", currentLine, currentColumn),
            this);
        value.Trim(true).Trim(false);
        if (value.IsEmpty())
            return;

        wxString lineText = value;
        wxString columnText;
        const int colon = value.Find(':');
        if (colon != wxNOT_FOUND)
        {
            lineText = value.Left(static_cast<size_t>(colon));
            columnText = value.Mid(static_cast<size_t>(colon + 1));
        }

        lineText.Trim(true).Trim(false);
        columnText.Trim(true).Trim(false);

        long line = 0;
        long column = 1;
        if (!lineText.ToLong(&line) || line < 1 || line > maxLine)
        {
            if (m_statusBar)
                m_statusBar->SetStatusText("Go to Line: invalid line number", 0);
            return;
        }

        if (!columnText.IsEmpty() && (!columnText.ToLong(&column) || column < 1))
        {
            if (m_statusBar)
                m_statusBar->SetStatusText("Go to Line: invalid column number", 0);
            return;
        }

        const int lineIndex = static_cast<int>(line - 1);
        const int lineStart = page->PositionFromLine(lineIndex);
        const int lineEnd = page->GetLineEndPosition(lineIndex);
        const int target = std::min(lineStart + static_cast<int>(column - 1), lineEnd);

        page->GotoLine(lineIndex);
        page->SetCurrentPos(target);
        page->SetSelection(target, target);
        page->EnsureCaretVisible();
        page->SetFocus();
    }

    void ShowAbout()
    {
        wxMessageBox(
            "DoDevEditor  v1.0\n\n"
            "A VSCode-inspired multi-tab code editor\n"
            "built with wxWidgets + wxStyledTextCtrl.\n\n"
            "Features:\n"
            "  • Multi-tab editing (drag to reorder)\n"
            "  • Syntax highlighting for 15+ languages\n"
            "  • Single-folder and multi-root workspace explorer\n"
            "  • Saved .dodev-workspace files with session restore\n"
            "  • Code folding & brace matching\n"
            "  • Inline find bar\n"
            "  • Ctrl+P Quick Open with fuzzy project-file navigation\n"
            "  • Ctrl+G Go to line or line:column\n"
            "  • Git stage/unstage, commit file lists and side-by-side diff editors\n"
            "  • JSON syntax highlighting plus Dark/Light/VS Code-like editor themes\n"
            "  • Optional LLVM/libclang C/C++ symbols, call tree and F12 definition\n"
            "  • Clang syntax check / object compilation with project defines/includes\n"
            "  • Runtime-toggleable Docker inspector (containers, images, logs, stats, inspect)\n"
            "  • AI Chat tab with OpenAI, Claude, OpenAI-compatible, GitHub Copilot CLI and Ollama providers\n"
            "  • General AI settings with environment, session-memory and OS-keyring secrets\n"
            "  • Auto-indent & auto-close brackets\n"
            "  • Word-wrap, whitespace display\n"
            "  • Zoom in/out\n",
            "About DoDevEditor", wxOK | wxICON_INFORMATION, this);
    }

    // ── Events ───────────────────────────────────────────────────────────────

    void OnTabChanged(wxAuiNotebookEvent &evt)
    {
        auto* page = CurrentPage();
        if (page)
        {
            m_lastAIContextPage = page;
            ActivateWorkspaceForPath(page->filepath);
            if (m_folderList && !page->filepath.IsEmpty())
                m_folderList->SetFolder(wxFileName(page->filepath).GetPath());
            UpdateStatusBar(page);
            if (m_symbolsPanel)
                m_symbolsPanel->SetCurrentDocument(page->filepath, page->GetText(), true);
        }
        else if (m_statusBar && evt.GetSelection() != wxNOT_FOUND)
        {
            wxWindow* selected = m_notebook->GetPage(evt.GetSelection());
            if (auto* diff = dynamic_cast<GitDiffPage*>(selected))
            {
                m_lastAIContextPage = diff;
                ActivateWorkspaceForPath(diff->GetRepositoryRoot());
                m_statusBar->SetStatusText(wxString("Git diff: ") + diff->GetGitPath(), 0);
            }
            else if (auto* commitDiff = dynamic_cast<GitCommitDiffPage*>(selected))
            {
                m_lastAIContextPage = commitDiff;
                ActivateWorkspaceForPath(commitDiff->GetRepositoryRoot());
                m_statusBar->SetStatusText("Commit " + commitDiff->GetCommitHash().Left(8) +
                                           ": " + commitDiff->GetGitPath(), 0);
            }
            else if (selected == m_aiChatPage)
            {
                m_statusBar->SetStatusText("AI Chat", 0);
            }
        }
        evt.Skip();
    }

    void OnTabClose(wxAuiNotebookEvent &evt)
    {
        wxWindow* closing = m_notebook->GetPage(evt.GetSelection());
        auto *page = dynamic_cast<EditorPage *>(closing);
        if (page && !ConfirmClose(page))
        {
            evt.Veto();
            return;
        }
        if (closing == m_lastAIContextPage)
            m_lastAIContextPage = nullptr;
        if (closing == m_aiChatPage)
            m_aiChatPage = nullptr;
    }

    void OnTreeItemActivated(wxTreeEvent &evt)
    {
        wxTreeItemId item = evt.GetItem();
        if (!item.IsOk())
            return;
        if (m_tree->ItemHasChildren(item))
        {
            evt.Skip();
            return;
        }
        PathData *data = dynamic_cast<PathData *>(m_tree->GetItemData(item));
        if (!data)
            return;
        wxString path = data->GetPath();
        if (wxFileName::FileExists(path))
            OpenFileInTab(path);
    }

    void OnCloseWindow(wxCloseEvent &evt)
    {
        for (size_t i = 0; i < m_notebook->GetPageCount(); ++i)
        {
            auto *p = dynamic_cast<EditorPage *>(m_notebook->GetPage(i));
            if (!p)
                continue;
            if (p->modified)
            {
                if (!ConfirmClose(p))
                {
                    evt.Veto();
                    return;
                }
            }
        }
        evt.Skip();
    }

    void AddBottomTab(const wxString &title, wxPanel *page)
    {
        // tab button
        wxPanel *tab = new wxPanel(m_bottomTabBar, wxID_ANY);
        tab->SetBackgroundColour(wxColour(40, 40, 40));

        wxBoxSizer *s = new wxBoxSizer(wxVERTICAL);
        wxStaticText *txt = new wxStaticText(tab, wxID_ANY, title);
        txt->SetForegroundColour(wxColour(220, 220, 220));

        s->Add(txt, 1, wxALIGN_CENTER | wxALL, 6);
        tab->SetSizer(s);

        int index = m_bottomPages.size();

        tab->Bind(wxEVT_LEFT_DOWN, [this, index](wxMouseEvent &)
                  { ShowBottomTab(index); });

        m_bottomTabBar->GetSizer()->Add(tab, 0, wxEXPAND | wxALL, 2);

        // store
        m_bottomTabs.push_back(tab);
        m_bottomPages.push_back(page);

        m_bottomContent->GetSizer()->Add(page, 1, wxEXPAND);

        page->Hide();
    }
    wxPanel *CreateProblemsPanel(wxWindow *parent)
    {
        wxPanel *panel = new wxPanel(parent, wxID_ANY);
        panel->SetBackgroundColour(wxColour(35, 20, 20));

        wxBoxSizer *s = new wxBoxSizer(wxVERTICAL);

        wxStaticText *title = new wxStaticText(panel, wxID_ANY, "Problems");
        title->SetForegroundColour(wxColour(255, 200, 200));

        wxListBox *list = new wxListBox(panel, wxID_ANY);

        list->Append("Error: Undefined reference in main.cpp:120");
        list->Append("Warning: unused variable 'tmp'");
        list->Append("Error: segmentation fault possible in parser");

        s->Add(title, 0, wxALL, 5);
        s->Add(list, 1, wxEXPAND | wxALL, 5);

        panel->SetSizer(s);
        return panel;
    }

    wxPanel *CreateOutputPanel(wxWindow *parent)
    {
        wxPanel *panel = new wxPanel(parent, wxID_ANY);
        panel->SetBackgroundColour(wxColour(20, 35, 20));

        wxBoxSizer *s = new wxBoxSizer(wxVERTICAL);

        wxStaticText *title = new wxStaticText(panel, wxID_ANY, "Output");
        title->SetForegroundColour(wxColour(200, 255, 200));

        wxTextCtrl *log = new wxTextCtrl(
            panel,
            wxID_ANY,
            "",
            wxDefaultPosition,
            wxDefaultSize,
            wxTE_MULTILINE | wxTE_READONLY);

        log->SetBackgroundColour(wxColour(15, 15, 15));
        log->SetForegroundColour(wxColour(180, 180, 180));

        log->AppendText("[build] compiling project...\n");
        log->AppendText("[build] linking...\n");
        log->AppendText("[build] done.\n");

        s->Add(title, 0, wxALL, 5);
        s->Add(log, 1, wxEXPAND | wxALL, 5);

        panel->SetSizer(s);
        return panel;
    }
    wxPanel *CreateDebugPanel(wxWindow *parent)
    {
        wxPanel *panel = new wxPanel(parent, wxID_ANY);
        panel->SetBackgroundColour(wxColour(20, 20, 40));

        wxBoxSizer *s = new wxBoxSizer(wxVERTICAL);

        wxStaticText *title = new wxStaticText(panel, wxID_ANY, "Debug Console");
        title->SetForegroundColour(wxColour(200, 200, 255));

        wxTextCtrl *console = new wxTextCtrl(
            panel,
            wxID_ANY,
            "",
            wxDefaultPosition,
            wxDefaultSize,
            wxTE_MULTILINE | wxTE_READONLY);

        console->SetBackgroundColour(wxColour(10, 10, 25));
        console->SetForegroundColour(wxColour(160, 160, 255));

        console->AppendText("Debugger attached...\n");
        console->AppendText("Breakpoint hit at main()\n");

        s->Add(title, 0, wxALL, 5);
        s->Add(console, 1, wxEXPAND | wxALL, 5);

        panel->SetSizer(s);
        return panel;
    }

    wxPanel *CreateTerminalPanel(wxWindow *parent)
    {
        wxPanel *panel = new wxPanel(parent, wxID_ANY);
        panel->SetBackgroundColour(wxColour(30, 30, 30));

        wxBoxSizer *s = new wxBoxSizer(wxVERTICAL);

        wxStaticText *title = new wxStaticText(panel, wxID_ANY, "Terminal");
        title->SetForegroundColour(wxColour(220, 220, 220));

        wxTextCtrl *terminal = new wxTextCtrl(
            panel,
            wxID_ANY,
            "$ ",
            wxDefaultPosition,
            wxDefaultSize,
            wxTE_MULTILINE);

        terminal->SetBackgroundColour(wxColour(10, 10, 10));
        terminal->SetForegroundColour(wxColour(0, 255, 0));

        s->Add(title, 0, wxALL, 5);
        s->Add(terminal, 1, wxEXPAND | wxALL, 5);

        panel->SetSizer(s);
        return panel;
    }

    wxPanel *CreatePortsPanel(wxWindow *parent)
    {
        wxPanel *panel = new wxPanel(parent, wxID_ANY);
        panel->SetBackgroundColour(wxColour(25, 25, 35));

        wxBoxSizer *s = new wxBoxSizer(wxVERTICAL);

        wxStaticText *title = new wxStaticText(panel, wxID_ANY, "Ports");
        title->SetForegroundColour(wxColour(180, 220, 255));

        wxListBox *list = new wxListBox(panel, wxID_ANY);

        list->Append("8080 - HTTP Server");
        list->Append("3000 - Dev Server");
        list->Append("5432 - PostgreSQL");

        s->Add(title, 0, wxALL, 5);
        s->Add(list, 1, wxEXPAND | wxALL, 5);

        panel->SetSizer(s);
        return panel;
    }
    wxPanel *CreateExecutablesPanel(wxWindow *parent)
    {
        wxPanel *panel = new wxPanel(parent, wxID_ANY);
        panel->SetBackgroundColour(wxColour(35, 25, 35));

        wxBoxSizer *s = new wxBoxSizer(wxVERTICAL);

        wxStaticText *title = new wxStaticText(panel, wxID_ANY, "Executables");
        title->SetForegroundColour(wxColour(255, 200, 255));

        wxListBox *list = new wxListBox(panel, wxID_ANY);

        list->Append("./build/app");
        list->Append("./build/server");
        list->Append("./bin/tool");

        s->Add(title, 0, wxALL, 5);
        s->Add(list, 1, wxEXPAND | wxALL, 5);

        panel->SetSizer(s);
        return panel;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// App
// ─────────────────────────────────────────────────────────────────────────────

class EditorApp : public wxApp
{
public:
    bool OnInit() override
    {
        SetAppName("DoDevEditor");
        wxInitAllImageHandlers();
        auto *frame = new MainFrame();

        // Open file(s) passed on command line
        for (int i = 1; i < argc; ++i)
        {
            wxString arg = argv[i];
            if (wxFileName::FileExists(arg))
                frame->GetChildren(); // tab already created in ctor; reopen
        }

        return true;
    }
};

wxIMPLEMENT_APP(EditorApp);

#if 0 

#include <iostream>
#include "ShellProcess.h"

int main()
{
    ShellProcess shell;

    shell.write("echo Hello World");
    shell.write("cd ~/Documents/project/DoDevEditor/build && cmake --build .");

    while (true)
    {
        std::string out = shell.readOutput();

        if (!out.empty())
            std::cout << out << std::flush;

        usleep(50 * 1000);
    }
}

#endif