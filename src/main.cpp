
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

#include <map>
#include <vector>
#include <string>
#include "CommanPalette.h"
#include "constant.h"
#include "EditorPage.h"
#include "PathData.h"
#include "MarkDownViewer.h"
#include "FileTreeCtrl.h"
#include "Findbar.h"
#include "Config.h"
#include "SourceControlPanel.h"
#include "SymbolTablePanel.h"
#include "MenuBar.h"
#include "DualNotebookPanel.h"

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
    wxPanel *m_editorPane = nullptr;
    wxAuiNotebook *m_notebook = nullptr;
    FindBar *m_findBar = nullptr;
    wxStatusBar *m_statusBar = nullptr;

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
        if (!AppEditorConfig::AppEditorConfig::GetLastOpenedFolder().empty())
        {
            m_tree->LoadFolder(AppEditorConfig::AppEditorConfig::GetLastOpenedFolder());
        }
        else
        {
            AppEditorConfig::AppEditorConfig::CreateDefaultConfig();
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

        // ── TOP: FILE TREE (UNCHANGED) ──
        m_tree = new FileTreeCtrl(splitter);

        // ── BOTTOM: NOTEBOOK (3 TABS) ──
        wxAuiNotebook *bottomTabs =
            new wxAuiNotebook(
                splitter,
                wxID_ANY,
                wxDefaultPosition,
                wxDefaultSize,
                wxAUI_NB_TOP | wxAUI_NB_SCROLL_BUTTONS);

        // TAB 1: Source Control
        SourceControlPanel *gitPanel =
            new SourceControlPanel(bottomTabs);

        // TAB 2: Symbols
        SymbolTablePanel *symbolsPanel =
            new SymbolTablePanel(bottomTabs);

        // TAB 3: (optional duplicate view or placeholder)
        wxPanel *emptyPanel = new wxPanel(bottomTabs);
        emptyPanel->SetBackgroundColour(wxColour(30, 30, 30));

        // Add tabs
        bottomTabs->AddPage(gitPanel, "GIT", true);
        bottomTabs->AddPage(symbolsPanel, "Symbols", false);
        bottomTabs->AddPage(emptyPanel, "Debug", false);

        // ── SPLIT ──
        splitter->SplitHorizontally(
            m_tree,
            bottomTabs,
            300);

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

        m_findBar->textCtrl->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent &)
                                  { FindNext(true); });
        Bind(wxEVT_BUTTON, [this](wxCommandEvent &)
             { FindNext(true); }, ID_FIND_BTN);
        Bind(wxEVT_BUTTON, [this](wxCommandEvent &)
             { FindNext(false); }, ID_FIND_PREV);
        Bind(wxEVT_BUTTON, [this](wxCommandEvent &)
             { CloseFindBar(); }, ID_FIND_CLOSE);
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

        mb->AddSeparator("File");

        mb->AddItem("File", "Save", ID_SAVE_FILE, [this]()
                    { SaveCurrentTab(); }, "Ctrl+S");

        mb->AddItem("File", "Save As", ID_SAVE_AS, [this]()
                    { SaveAs(); }, "Ctrl+Shift+S");

        mb->AddItem("File", "Save All", ID_SAVE_ALL, [this]()
                    { SaveAll(); }, "Ctrl+Alt+S");

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

        // ───────── VIEW ─────────
        mb->AddItem("View", "Toggle Sidebar", ID_TOGGLE_SIDEBAR, [this]()
                    { ToggleSidebar(); }, "Ctrl+B");

        mb->AddItem("View", "Word Wrap", ID_TOGGLE_WORDWRAP, [this]()
                    { ToggleWordWrap(); }, "Alt+Z");

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
             { SaveCurrentTab(); }, ID_SAVE_FILE);
        Bind(wxEVT_MENU, [this](wxCommandEvent &)
             { SaveAs(); }, ID_SAVE_AS);
        Bind(wxEVT_MENU, [this](wxCommandEvent &)
             { SaveAll(); }, ID_SAVE_ALL);
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
             { ShowFindBar(); }, ID_FIND);
        Bind(wxEVT_MENU, [this](wxCommandEvent &)
             { FindNext(true); }, ID_FIND_NEXT);
        Bind(wxEVT_MENU, [this](wxCommandEvent &)
             { FindNext(false); }, ID_FIND_PREV);
        Bind(wxEVT_MENU, [this](wxCommandEvent &)
             { GoToLine(); }, ID_GOTO_LINE);
        Bind(wxEVT_MENU, [this](wxCommandEvent &)
             { ToggleSidebar(); }, ID_TOGGLE_SIDEBAR);
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
        };
        SetAcceleratorTable(wxAcceleratorTable(4, entries));
    }

    // ── Tab helpers ──────────────────────────────────────────────────────────

    EditorPage *CurrentPage()
    {
        int sel = m_notebook->GetSelection();
        if (sel == wxNOT_FOUND)
            return nullptr;
        return static_cast<EditorPage *>(m_notebook->GetPage(sel));
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
            e.Skip(); });
    }

    void OpenFileInTab(const wxString &path)
    {
        // Check if already open
        for (size_t i = 0; i < m_notebook->GetPageCount(); ++i)
        {
            auto *p = static_cast<EditorPage *>(m_notebook->GetPage(i));
            if (p->filepath == path)
            {
                m_notebook->SetSelection(i);
                return;
            }
        }
        NewTab(path);
    }

    void UpdateStatusBar(EditorPage *page)
    {
        if (!page)
            return;
        int line = page->GetCurrentLine() + 1;
        int col = page->GetColumn(page->GetCurrentPos()) + 1;
        m_statusBar->SetStatusText(wxString::Format("Ln %d, Col %d", line, col), 1);
        m_statusBar->SetStatusText("UTF-8", 2);
        LangInfo lang = DetectLanguage(page->filepath);
        m_statusBar->SetStatusText(lang.name, 3);
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

    void OpenFolder()
    {
        wxDirDialog dlg(this, "Open Folder", wxEmptyString,
                        wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);
        if (dlg.ShowModal() == wxID_OK)
            loadPath(dlg.GetPath());
        /// m_tree->LoadFolder(dlg.GetPath());
    }

    void loadPath(const wxString &path)
    {
        if (wxDirExists(path))
        {
            m_tree->LoadFolder(path);
        }
        else if (wxFileExists(path))
        {
            OpenFileInTab(path);
        }
        else
        {
            wxMessageBox("The specified path does not exist.", "Error", wxICON_ERROR);
        }
    }

    bool SaveCurrentTab()
    {
        auto *page = CurrentPage();
        if (!page)
            return false;
        if (page->filepath.IsEmpty())
            return SaveAs();
        bool ok = page->SaveFile();
        if (ok)
            UpdateTabTitle(page);
        return ok;
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
        bool ok = page->SaveFile(dlg.GetPath());
        if (ok)
            UpdateTabTitle(page);
        return ok;
    }

    void SaveAll()
    {
        for (size_t i = 0; i < m_notebook->GetPageCount(); ++i)
        {
            auto *p = static_cast<EditorPage *>(m_notebook->GetPage(i));
            if (p->modified)
            {
                if (p->filepath.IsEmpty())
                {
                    m_notebook->SetSelection(i);
                    SaveAs();
                }
                else
                {
                    p->SaveFile();
                    m_notebook->SetPageText(i, p->GetTitle());
                }
            }
        }
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
                return SaveAs();
            return page->SaveFile();
        }
        return true;
    }

    void CloseCurrentTab()
    {
        int sel = m_notebook->GetSelection();
        if (sel == wxNOT_FOUND)
            return;
        auto *page = static_cast<EditorPage *>(m_notebook->GetPage(sel));
        if (ConfirmClose(page))
            m_notebook->DeletePage(sel);
    }

    void CloseAllTabs()
    {
        for (int i = (int)m_notebook->GetPageCount() - 1; i >= 0; --i)
        {
            auto *page = static_cast<EditorPage *>(m_notebook->GetPage(i));
            if (!ConfirmClose(page))
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

    void ShowFindBar()
    {
        m_findBar->Show();
        m_editorPane->Layout();
        m_findBar->textCtrl->SetFocus();
        auto *page = CurrentPage();
        if (page && page->GetSelectedText().length() > 0)
            m_findBar->textCtrl->SetValue(page->GetSelectedText());
    }

    void CloseFindBar()
    {
        m_findBar->Hide();
        m_editorPane->Layout();
        if (auto *p = CurrentPage())
            p->SetFocus();
    }

    void FindNext(bool forward)
    {
        auto *page = CurrentPage();
        if (!page)
            return;
        wxString text = m_findBar->textCtrl->GetValue();
        if (text.IsEmpty())
            return;

        int flags = 0;
        if (m_findBar->caseChk->IsChecked())
            flags |= wxSTC_FIND_MATCHCASE;
        if (m_findBar->wholeChk->IsChecked())
            flags |= wxSTC_FIND_WHOLEWORD;

        page->SearchAnchor();
        int res = forward ? page->SearchNext(flags, text)
                          : page->SearchPrev(flags, text);
        if (res == wxSTC_INVALID_POSITION)
        {
            // Wrap
            page->SetCurrentPos(forward ? 0 : page->GetLength());
            page->SearchAnchor();
            res = forward ? page->SearchNext(flags, text)
                          : page->SearchPrev(flags, text);
        }
        if (res != wxSTC_INVALID_POSITION)
        {
            page->EnsureCaretVisible();
            m_findBar->textCtrl->SetBackgroundColour(wxColour(60, 60, 60));
        }
        else
        {
            m_findBar->textCtrl->SetBackgroundColour(wxColour(100, 40, 40));
        }
        m_findBar->textCtrl->Refresh();
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

    void ToggleWordWrap()
    {
        for (size_t i = 0; i < m_notebook->GetPageCount(); ++i)
        {
            auto *p = static_cast<EditorPage *>(m_notebook->GetPage(i));
            p->SetWrapMode(p->GetWrapMode() == wxSTC_WRAP_NONE
                               ? wxSTC_WRAP_WORD
                               : wxSTC_WRAP_NONE);
        }
    }

    void ToggleWhitespace()
    {
        for (size_t i = 0; i < m_notebook->GetPageCount(); ++i)
        {
            auto *p = static_cast<EditorPage *>(m_notebook->GetPage(i));
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
        auto *page = CurrentPage();
        if (!page)
            return;
        int maxLine = page->GetLineCount();
        wxString val = wxGetTextFromUser(
            wxString::Format("Go to line (1 – %d):", maxLine),
            "Go to Line", wxEmptyString, this);
        if (val.IsEmpty())
            return;
        long line;
        if (val.ToLong(&line) && line >= 1 && line <= maxLine)
        {
            page->GotoLine(line - 1);
            page->SetFocus();
        }
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
            "  • Folder explorer with lazy loading\n"
            "  • Code folding & brace matching\n"
            "  • Inline find bar\n"
            "  • Auto-indent & auto-close brackets\n"
            "  • Word-wrap, whitespace display\n"
            "  • Zoom in/out\n",
            "About DoDevEditor", wxOK | wxICON_INFORMATION, this);
    }

    // ── Events ───────────────────────────────────────────────────────────────

    void OnTabChanged(wxAuiNotebookEvent &evt)
    {
        UpdateStatusBar(CurrentPage());
        evt.Skip();
    }

    void OnTabClose(wxAuiNotebookEvent &evt)
    {
        auto *page = static_cast<EditorPage *>(m_notebook->GetPage(evt.GetSelection()));
        if (!ConfirmClose(page))
            evt.Veto();
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
            auto *p = static_cast<EditorPage *>(m_notebook->GetPage(i));
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