#ifndef DYNAMIC_MENU_BAR_H
#define DYNAMIC_MENU_BAR_H

#include <wx/wx.h>
#include <functional>
#include <unordered_map>

class DynamicMenuBar : public wxMenuBar
{
public:
    using Callback = std::function<void()>;

    DynamicMenuBar(wxWindow* parent = nullptr);

    // ─────────────────────────────
    // Add Menu (File, Edit, View...)
    // ─────────────────────────────
    wxMenu* AddMenu(const wxString& name);

    // ─────────────────────────────
    // Add Item with Callback
    // ─────────────────────────────
    void AddItem(const wxString& menuName,
                 const wxString& label,
                 int id,
                 Callback cb,
                 const wxString& shortcut = "");

    // ─────────────────────────────
    // Add runtime-toggleable check item
    // ─────────────────────────────
    void AddCheckItem(const wxString& menuName,
                      const wxString& label,
                      int id,
                      Callback cb,
                      bool checked = false,
                      const wxString& shortcut = "");

    // ─────────────────────────────
    // Add Submenu
    // ─────────────────────────────
    void AddSubMenu(const wxString& menuName,
                    const wxString& label,
                    wxMenu* subMenu);

    // Dynamic/plugin item. The caller supplies a unique id. Unlike AddItem,
    // the event trampoline looks up the current callback at dispatch time so
    // the menu item can be removed safely while the application remains open.
    void AddRuntimeItem(const wxString& menuName,
                        const wxString& label,
                        int id,
                        Callback cb,
                        const wxString& shortcut = "");
    bool RemoveRuntimeItem(int id);
    wxMenu* FindMenuByName(const wxString& name) const;

    // ─────────────────────────────
    // Add Separator
    // ─────────────────────────────
    void AddSeparator(const wxString& menuName);

private:
    wxMenu* GetOrCreate(const wxString& name);

private:
    wxWindow* m_parent = nullptr;
    std::unordered_map<wxString, wxMenu*> m_menus;
    std::unordered_map<int, Callback> m_callbacks;
};

#endif