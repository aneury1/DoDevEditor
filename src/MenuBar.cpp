 
#include <wx/wx.h>
#include <functional>
#include <unordered_map>

#include "MenuBar.h"

     DynamicMenuBar::DynamicMenuBar(wxWindow* parent )
    {
        m_parent = parent;
    }

 
    wxMenu*  DynamicMenuBar::AddMenu(const wxString& name)
    {
        wxMenu* menu = new wxMenu();
        Append(menu, name);
        m_menus[name] = menu;
        return menu;
    }
 
    void  DynamicMenuBar::AddItem(const wxString& menuName,
                 const wxString& label,
                 int id,
                 Callback cb,
                 const wxString& shortcut )
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

    void DynamicMenuBar::AddCheckItem(const wxString& menuName,
                                      const wxString& label,
                                      int id,
                                      Callback cb,
                                      bool checked,
                                      const wxString& shortcut)
    {
        wxMenu* menu = GetOrCreate(menuName);

        wxString fullLabel = label;
        if (!shortcut.empty())
            fullLabel += "\t" + shortcut;

        menu->AppendCheckItem(id, fullLabel);
        menu->Check(id, checked);

        Bind(wxEVT_MENU, [cb](wxCommandEvent&)
        {
            cb();
        }, id);

        m_callbacks[id] = cb;
    }

    // ─────────────────────────────
    // Add Separator
    // ─────────────────────────────
    void  DynamicMenuBar::AddSeparator(const wxString& menuName)
    {
        wxMenu* menu = GetOrCreate(menuName);
        menu->AppendSeparator();
    }

 
    wxMenu*  DynamicMenuBar::GetOrCreate(const wxString& name)
    {
        if (m_menus.count(name))
            return m_menus[name];

        return AddMenu(name);
    }
 