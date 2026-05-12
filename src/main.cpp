

#include <wx/wx.h>
#include <wx/stc/stc.h>
#include <json/json.h>
#include "JsonStyledTextCtrl.h"
#include "FindPanel.h"
#include "EditorMenuBar.h"
#include "FileTab.h"
#include "SidelPanel.h"
#include "FloatingExplorerPanel.h"

class DarkEditor : public wxFrame
{
public:
    DarkEditor()
        : wxFrame(nullptr, wxID_ANY, "DoDev Editor",
                  wxDefaultPosition, wxSize(900, 600))
    {

#if 0
        editor = new JsonStyledTextCtrl(this);
        findPanel = new FindPanel(this);

        auto *s = new wxBoxSizer(wxVERTICAL);
        s->Add(findPanel, 0, wxEXPAND);
        s->Add(editor, 1, wxEXPAND);
        SetSizer(s);

        Bind(wxEVT_CHAR_HOOK, &DarkEditor::OnKey, this);
        findPanel->next->Bind(wxEVT_BUTTON, &DarkEditor::OnFindNext, this);
        findPanel->prev->Bind(wxEVT_BUTTON, &DarkEditor::OnFindPrev, this);
        findPanel->input->Bind(wxEVT_TEXT, &DarkEditor::OnFindUpdate, this);

        editor->ApplyDarkTheme();
        editor->SetText("// JSON themed editor\n");

        EditorMenuBar *menu = new EditorMenuBar(this);

        SetMenuBar(menu);

        menu->AddDefaultEditorMenus();

        menu->AddMenu("Git");

        menu->AddMenuItem(
            "Git",
            "Commit",
            "Ctrl+Shift+C",
            []()
            {
                wxMessageBox("Git Commit");
            });

        menu->AddMenuItem(
            "Git",
            "Push",
            "",
            []()
            {
                wxMessageBox("Git Push");
            });
        //////////////////////////

        FileTabBar *tabs = new FileTabBar(this);

        tabs->SetMinSize(wxSize(-1, 36));

        tabs->SetOnTabSelected(
            [](int index)
            {
                wxLogMessage("Selected tab: %d", index);
            });

        tabs->SetOnTabClosed(
            [](int index)
            {
                wxLogMessage("Closed tab: %d", index);
            });

        tabs->AddTab("main.cpp");
        tabs->AddTab("server.cpp");
        tabs->AddTab("theme.json");
#endif

#if 0 

        editor = new JsonStyledTextCtrl(this);
        findPanel = new FindPanel(this);
        FileTabBar *tabs = new FileTabBar(this);
  



        tabs->SetMinSize(wxSize(-1, 36));

        auto *s = new wxBoxSizer(wxVERTICAL);

        // TOP TAB BAR
        s->Add(tabs, 0, wxEXPAND);

        // FIND PANEL
        s->Add(findPanel, 0, wxEXPAND);

        // EDITOR
        s->Add(editor, 1, wxEXPAND);

        SetSizer(s);

        // FIND EVENTS
        Bind(wxEVT_CHAR_HOOK, &DarkEditor::OnKey, this);

        findPanel->next->Bind(
            wxEVT_BUTTON,
            &DarkEditor::OnFindNext,
            this);

        findPanel->prev->Bind(
            wxEVT_BUTTON,
            &DarkEditor::OnFindPrev,
            this);

        findPanel->input->Bind(
            wxEVT_TEXT,
            &DarkEditor::OnFindUpdate,
            this);

        // EDITOR THEME
        editor->ApplyDarkTheme();
        editor->SetText("// JSON themed editor\n");

        // MENU
        EditorMenuBar *menu = new EditorMenuBar(this);

        SetMenuBar(menu);

        menu->AddDefaultEditorMenus();

        menu->AddMenu("Git");

        menu->AddMenuItem(
            "Git",
            "Commit",
            "Ctrl+Shift+C",
            [this, tabs]()
            {
                wxMessageBox("Git Commit");
                tabs->AddTab("main.cppxss");
            });

        menu->AddMenuItem(
            "Git",
            "Push",
            "Ctrl+Shift+P",
            [ ]()
            {
                wxMessageBox("Git Push");
                
            });

        // TABS CALLBACKS
        tabs->SetOnTabSelected(
            [&](int index)
            {
                /// wxLogMessage("Selected tab: %d", index);
            });

        tabs->SetOnTabClosed(
            [&](int index)
            {
                wxLogMessage("Closed tab: %d", index);
            });

        // SAMPLE TABS
        tabs->AddTab("main.cpp");
        tabs->AddTab("server.cpp");
        tabs->AddTab("theme.json");
#endif

#if 0 
editor = new JsonStyledTextCtrl(this);
findPanel = new FindPanel(this);
FileTabBar* tabs = new FileTabBar(this);

// SIDE PANEL
SidePanel* sidePanel = new SidePanel(this);
sidePanel->SetMinSize(wxSize(260, -1));
sidePanel->SetBackgroundColour(wxColour(37, 37, 38));
sidePanel->SetOnFileSelected(
    [&](const wxString& path)
    {
        wxLogMessage(
            "Selected file: %s",
            path);
    });



// OPTIONAL CONTENT INSIDE SIDE PANEL
auto* sideSizer = new wxBoxSizer(wxVERTICAL);

wxStaticText* explorerTitle =
    new wxStaticText(sidePanel, wxID_ANY, "Explorer");

explorerTitle->SetForegroundColour(*wxWHITE);

sideSizer->Add(
    explorerTitle,
    0,
    wxALL,
    10);

sidePanel->SetSizer(sideSizer);

// ================================
// ROOT HORIZONTAL LAYOUT
// ================================

auto* root = new wxBoxSizer(wxHORIZONTAL);

// LEFT SIDE PANEL
root->Add(sidePanel, 0, wxEXPAND);

// ================================
// RIGHT EDITOR AREA
// ================================

auto* editorLayout = new wxBoxSizer(wxVERTICAL);

tabs->SetMinSize(wxSize(-1, 36));

// TOP TAB BAR
editorLayout->Add(tabs, 0, wxEXPAND);

// FIND PANEL
editorLayout->Add(findPanel, 0, wxEXPAND);

// EDITOR
editorLayout->Add(editor, 1, wxEXPAND);

// ADD EDITOR AREA TO ROOT
root->Add(editorLayout, 1, wxEXPAND);

SetSizer(root);

// ================================
// FIND EVENTS
// ================================

Bind(wxEVT_CHAR_HOOK, &DarkEditor::OnKey, this);

findPanel->next->Bind(
    wxEVT_BUTTON,
    &DarkEditor::OnFindNext,
    this);

findPanel->prev->Bind(
    wxEVT_BUTTON,
    &DarkEditor::OnFindPrev,
    this);

findPanel->input->Bind(
    wxEVT_TEXT,
    &DarkEditor::OnFindUpdate,
    this);

// ================================
// EDITOR THEME
// ================================

editor->ApplyDarkTheme();
editor->SetText("// JSON themed editor\n");

// ================================
// MENU
// ================================

EditorMenuBar* menu =
    new EditorMenuBar(this);

SetMenuBar(menu);

menu->AddDefaultEditorMenus();

menu->AddMenu("Git");

menu->AddMenuItem(
    "Git",
    "Commit",
    "Ctrl+Shift+C",
    [this, tabs]()
    {
        wxMessageBox("Git Commit");

        tabs->AddTab("main.cppxss");
    });

menu->AddMenuItem(
    "Git",
    "Push",
    "Ctrl+Shift+P",
    []()
    {
        wxMessageBox("Git Push");
    });

// ================================
// TAB EVENTS
// ================================

tabs->SetOnTabSelected(
    [&](int index)
    {
    });

tabs->SetOnTabClosed(
    [&](int index)
    {
        wxLogMessage(
            "Closed tab: %d",
            index);
    });

// ================================
// SAMPLE TABS
// ================================

tabs->AddTab("main.cpp");
tabs->AddTab("server.cpp");
tabs->AddTab("theme.json");

#endif

 editor = new JsonStyledTextCtrl(this);
findPanel = new FindPanel(this);
FileTabBar* tabs = new FileTabBar(this);

// ================================
// SIDE PANEL (CORRECT USAGE)
// ================================
SidePanel* sidePanel = new SidePanel(this);

sidePanel->SetMinSize(wxSize(260, -1));

sidePanel->SetOnFileSelected(
    [&](const wxString& path)
    {
        wxLogMessage("Selected file: %s", path);

        // Example: load file into editor
        // editor->LoadFile(path);
    });

// ================================
// ROOT LAYOUT (HORIZONTAL SPLIT)
// ================================
auto* root = new wxBoxSizer(wxHORIZONTAL);

// LEFT: SIDE PANEL
root->Add(sidePanel, 0, wxEXPAND);

// ================================
// RIGHT: EDITOR STACK
// ================================
auto* editorLayout = new wxBoxSizer(wxVERTICAL);

tabs->SetMinSize(wxSize(-1, 36));

// TOP TAB BAR
editorLayout->Add(tabs, 0, wxEXPAND);

// FIND PANEL
editorLayout->Add(findPanel, 0, wxEXPAND);

// EDITOR
editorLayout->Add(editor, 1, wxEXPAND);

// ADD RIGHT SIDE TO ROOT
root->Add(editorLayout, 1, wxEXPAND);

// APPLY LAYOUT
SetSizer(root);
Layout();

// ================================
// EVENTS
// ================================
Bind(wxEVT_CHAR_HOOK, &DarkEditor::OnKey, this);

findPanel->next->Bind(
    wxEVT_BUTTON,
    &DarkEditor::OnFindNext,
    this);

findPanel->prev->Bind(
    wxEVT_BUTTON,
    &DarkEditor::OnFindPrev,
    this);

findPanel->input->Bind(
    wxEVT_TEXT,
    &DarkEditor::OnFindUpdate,
    this);

// ================================
// EDITOR CONFIG
// ================================
editor->ApplyDarkTheme();
editor->SetText("// JSON themed editor\n");

// ================================
// MENU
// ================================
EditorMenuBar* menu = new EditorMenuBar(this);

SetMenuBar(menu);

menu->AddDefaultEditorMenus();

menu->AddMenu("Git");

menu->AddMenuItem(
    "Git",
    "Commit",
    "Ctrl+Shift+C",
    [this, tabs]()
    {
        wxMessageBox("Git Commit");
        tabs->AddTab("main.cppxss");
    });

menu->AddMenuItem(
    "Git",
    "Push",
    "Ctrl+Shift+P",
    []()
    {
        wxMessageBox("Git Push");
    });

// ================================
// TAB EVENTS
// ================================
tabs->SetOnTabSelected(
    [&](int index)
    {
        // handle tab switch
    });

tabs->SetOnTabClosed(
    [&](int index)
    {
       //// wxLogMessage("Closed tab: %d", index);
    });

// ================================
// SAMPLE TABS
// ================================
tabs->AddTab("main.cpp");
tabs->AddTab("server.cpp");
tabs->AddTab("theme.json");





    }

private:
    void setCustomTheme()
    {

        Json::Value theme;
        theme["lexer"] = "cpp";
        theme["background"] = Json::arrayValue;
        theme["background"].append(30);
        theme["background"].append(30);
        theme["background"].append(30);

        theme["foreground"] = Json::arrayValue;
        theme["foreground"].append(220);
        theme["foreground"].append(220);
        theme["foreground"].append(220);

        theme["caret"] = Json::arrayValue;
        theme["caret"].append(255);
        theme["caret"].append(255);
        theme["caret"].append(255);

        theme["keywords"] = "int float double if else return for while";

        Json::Value styles;
        styles["word"] = Json::arrayValue;
        styles["word"].append(86);
        styles["word"].append(156);
        styles["word"].append(214);

        styles["comment"] = Json::arrayValue;
        styles["comment"].append(87);
        styles["comment"].append(166);
        styles["comment"].append(74);

        styles["string"] = Json::arrayValue;
        styles["string"].append(214);
        styles["string"].append(157);
        styles["string"].append(133);

        styles["number"] = Json::arrayValue;
        styles["number"].append(181);
        styles["number"].append(206);
        styles["number"].append(168);

        theme["styles"] = styles;

        editor->ApplyTheme(theme);
    }

    JsonStyledTextCtrl *editor;

    FindPanel *findPanel;

    int lastPos = 0;

    // Ctrl+F
    void OnKey(wxKeyEvent &e)
    {
        if (e.ControlDown() && e.GetKeyCode() == 'F')
        {
            findPanel->Show();
            Layout();
            findPanel->input->SetFocus();
            return;
        }
        e.Skip();
    }

    void OnFindNext(wxCommandEvent &)
    {
        Find(true);
    }

    void OnFindPrev(wxCommandEvent &)
    {
        Find(false);
    }

    void OnFindUpdate(wxCommandEvent &)
    {
        HighlightAll();
    }

    void Find(bool forward)
    {
        wxString text = findPanel->input->GetValue();
        if (text.IsEmpty())
            return;

        int flags = 0x00;
        if (findPanel->matchCase->GetValue())
            flags |= wxSTC_FIND_MATCHCASE;

        int start = editor->GetCurrentPos();
        int end = forward ? editor->GetLength() : 0;

        int pos = editor->FindText(start, end, text, flags);

        if (pos != wxNOT_FOUND)
        {
            editor->SetSelection(pos, pos + text.Length());
            editor->GotoPos(pos);
            lastPos = pos;
        }
    }

    void HighlightAll()
    {
        editor->SetIndicatorCurrent(0);
        editor->IndicatorClearRange(0, editor->GetTextLength());

        wxString text = findPanel->input->GetValue();
        if (text.IsEmpty())
            return;

        int flags = findPanel->matchCase->GetValue() ? wxSTC_FIND_MATCHCASE : 0;

        int pos = 0;
        while (true)
        {
            pos = editor->FindText(pos, editor->GetLength(), text, flags);
            if (pos == wxNOT_FOUND)
                break;

            editor->IndicatorFillRange(pos, text.Length());
            pos += text.Length();
        }
    }
};

class MyApp : public wxApp
{
public:
    bool OnInit() override
    {
        DarkEditor *frame = new DarkEditor();
        frame->Show(true);
        return true;
    }
};

wxIMPLEMENT_APP(MyApp);
