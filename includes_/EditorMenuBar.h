#pragma once

#pragma once

#include <wx/wx.h>
#include <functional>
#include <unordered_map>
#include <vector>

class EditorMenuBar : public wxMenuBar
{
public:

    using Callback = std::function<void()>;

    struct MenuItemDef
    {
        wxString label;
        wxString shortcut;
        Callback callback;
        bool separator = false;
    };

    EditorMenuBar(wxFrame* owner)
        : frame(owner)
    {
    }

    void AddMenu(const wxString& menuName)
    {
        if (menus.count(menuName))
            return;

        wxMenu* menu = new wxMenu();

        menus[menuName] = menu;

        Append(menu, menuName);
    }

    int AddMenuItem(
        const wxString& menuName,
        const wxString& label,
        const wxString& shortcut,
        Callback callback)
    {
        if (!menus.count(menuName))
            AddMenu(menuName);

        int id = GenerateID();

        wxString finalLabel = label;

        if (!shortcut.IsEmpty())
            finalLabel += "\t" + shortcut;

        menus[menuName]->Append(id, finalLabel);

        callbacks[id] = callback;

        frame->Bind(
            wxEVT_MENU,
            [=](wxCommandEvent&)
            {
                if (callbacks.count(id))
                    callbacks[id]();
            },
            id);

        return id;
    }

    void AddSeparator(const wxString& menuName)
    {
        if (!menus.count(menuName))
            AddMenu(menuName);

        menus[menuName]->AppendSeparator();
    }

    void AddDefaultEditorMenus()
    {
        // FILE
        AddMenu("File");

        AddMenuItem("File", "New", "Ctrl+N", [](){});
        AddMenuItem("File", "Open", "Ctrl+O", [](){});
        AddMenuItem("File", "Save", "Ctrl+S", [](){});
        AddMenuItem("File", "Save As", "", [](){});

        AddSeparator("File");

        AddMenuItem("File", "Exit", "Alt+F4",
            [=]()
            {
                frame->Close();
            });

        // EDIT
        AddMenu("Edit");

        AddMenuItem("Edit", "Undo", "Ctrl+Z", [](){});
        AddMenuItem("Edit", "Redo", "Ctrl+Y", [](){});

        AddSeparator("Edit");

        AddMenuItem("Edit", "Cut", "Ctrl+X", [](){});
        AddMenuItem("Edit", "Copy", "Ctrl+C", [](){});
        AddMenuItem("Edit", "Paste", "Ctrl+V", [](){});

        // SEARCH
        AddMenu("Search");

        AddMenuItem("Search", "Find", "Ctrl+F", [](){});
        AddMenuItem("Search", "Replace", "Ctrl+H", [](){});

        // VIEW
        AddMenu("View");

        AddMenuItem("View", "Toggle Sidebar", "", [](){});
        AddMenuItem("View", "Toggle Terminal", "", [](){});

        // HELP
        AddMenu("Help");

        AddMenuItem("Help", "About", "",
            [=]()
            {
                wxMessageBox("DoDev Editor");
            });
    }

private:

    wxFrame* frame;

    std::unordered_map<wxString, wxMenu*> menus;
    std::unordered_map<int, Callback> callbacks;

    int GenerateID()
    {
        static int current = wxID_HIGHEST + 500;
        return current++;
    }
};