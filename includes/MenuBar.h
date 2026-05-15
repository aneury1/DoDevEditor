#ifndef DYNAMIC_MENU_BAR_H
#define DYNAMIC_MENU_BAR_H

#include <wx/wx.h>
#include <functional>
#include <unordered_map>

class DynamicMenuBar : public wxMenuBar
{
public:
    using Callback = std::function<void()>;

    DynamicMenuBar(wxWindow* parent = nullptr)
    {
        m_parent = parent;
    }

    // ─────────────────────────────
    // Add Menu (File, Edit, View...)
    // ─────────────────────────────
    wxMenu* AddMenu(const wxString& name)
    {
        wxMenu* menu = new wxMenu();
        Append(menu, name);
        m_menus[name] = menu;
        return menu;
    }

    // ─────────────────────────────
    // Add Item with Callback
    // ─────────────────────────────
    void AddItem(const wxString& menuName,
                 const wxString& label,
                 int id,
                 Callback cb,
                 const wxString& shortcut = "")
    {
        wxMenu* menu = GetOrCreate(menuName);

        wxString fullLabel = label;
        if (!shortcut.empty())
            fullLabel += "\t" + shortcut;

        menu->Append(id, fullLabel);

        Bind(wxEVT_MENU, [cb](wxCommandEvent&)
        {
            cb();
        }, id);

        m_callbacks[id] = cb;
    }

    // ─────────────────────────────
    // Add Separator
    // ─────────────────────────────
    void AddSeparator(const wxString& menuName)
    {
        wxMenu* menu = GetOrCreate(menuName);
        menu->AppendSeparator();
    }

private:
    wxMenu* GetOrCreate(const wxString& name)
    {
        if (m_menus.count(name))
            return m_menus[name];

        return AddMenu(name);
    }

private:
    wxWindow* m_parent = nullptr;
    std::unordered_map<wxString, wxMenu*> m_menus;
    std::unordered_map<int, Callback> m_callbacks;
};

#endif