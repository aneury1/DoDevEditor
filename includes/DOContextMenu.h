#ifndef VSCODE_CONTEXT_MENU_H
#define VSCODE_CONTEXT_MENU_H

#include <wx/wx.h>
#include <wx/artprov.h>
#include <functional>
#include <unordered_map>

class DOContextMenu : public wxMenu
{
public:
    enum MenuId
    {
        ID_NEW_FILE = wxID_HIGHEST + 500,
        ID_NEW_FOLDER,
        ID_RENAME,
        ID_DELETE,
        ID_COPY_PATH,
        ID_REVEAL,
        ID_OPEN_TERMINAL,
        ID_GIT_STAGE,
        ID_GIT_DISCARD,
        ID_PROPERTIES
    };

public:
    DOContextMenu(wxWindow* owner);
    // ─────────────────────────────────────
    // CALLBACK API
    // ─────────────────────────────────────

    void SetOnNewFile(std::function<void()> fn);
    void SetOnNewFolder(std::function<void()> fn);
    void SetOnRename(std::function<void()> fn);
    void SetOnDelete(std::function<void()> fn);

    void SetOnCopyPath(std::function<void()> fn);
    void SetOnReveal(std::function<void()> fn);

    void SetOnOpenTerminal(std::function<void()> fn);
    void SetOnGitStage(std::function<void()> fn);
    void SetOnGitDiscard(std::function<void()> fn);

    void SetOnProperties(std::function<void()> fn);

private:
    void BuildMenu();

    void BindEvents();

    void OnMenu(wxCommandEvent& evt);

private:
    wxWindow* m_owner = nullptr;

    std::unordered_map<int, std::function<void()>> m_callbacks;
};

#endif