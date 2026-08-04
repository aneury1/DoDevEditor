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