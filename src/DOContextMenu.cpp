 
#include <wx/wx.h>
#include <wx/artprov.h>
#include <functional>
#include <unordered_map>
#include "DOContextMenu.h"

DOContextMenu::DOContextMenu(wxWindow *owner)
    : wxMenu(),
      m_owner(owner)
{
    BuildMenu();
    BindEvents();
}

// ─────────────────────────────────────
// CALLBACK API
// ─────────────────────────────────────

void DOContextMenu::SetOnNewFile(std::function<void()> fn)
{
    m_callbacks[ID_NEW_FILE] = fn;
}

void DOContextMenu::SetOnNewFolder(std::function<void()> fn)
{
    m_callbacks[ID_NEW_FOLDER] = fn;
}

void DOContextMenu::SetOnRename(std::function<void()> fn)
{
    m_callbacks[ID_RENAME] = fn;
}

void DOContextMenu::SetOnDelete(std::function<void()> fn)
{
    m_callbacks[ID_DELETE] = fn;
}

void DOContextMenu::SetOnCopyPath(std::function<void()> fn)
{
    m_callbacks[ID_COPY_PATH] = fn;
}

void DOContextMenu::SetOnReveal(std::function<void()> fn)
{
    m_callbacks[ID_REVEAL] = fn;
}

void DOContextMenu::SetOnOpenTerminal(std::function<void()> fn)
{
    m_callbacks[ID_OPEN_TERMINAL] = fn;
}

void DOContextMenu::SetOnGitStage(std::function<void()> fn)
{
    m_callbacks[ID_GIT_STAGE] = fn;
}

void DOContextMenu::SetOnGitDiscard(std::function<void()> fn)
{
    m_callbacks[ID_GIT_DISCARD] = fn;
}

void DOContextMenu::SetOnProperties(std::function<void()> fn)
{
    m_callbacks[ID_PROPERTIES] = fn;
}

void DOContextMenu::BuildMenu()
{
    // ─────────────────────────
    // FILE ACTIONS
    // ─────────────────────────

    wxMenuItem *newFileItem = Append(
        ID_NEW_FILE,
        "New File\tCtrl+N",
        "Create a new file");

    newFileItem->SetBitmap(
        wxArtProvider::GetBitmap(
            wxART_NEW,
            wxART_MENU,
            wxSize(16, 16)));

    wxMenuItem *newFolderItem = Append(
        ID_NEW_FOLDER,
        "New Folder",
        "Create a new folder");

    newFolderItem->SetBitmap(
        wxArtProvider::GetBitmap(
            wxART_FOLDER,
            wxART_MENU,
            wxSize(16, 16)));

    AppendSeparator();

    wxMenuItem *renameItem = Append(
        ID_RENAME,
        "Rename\tF2");

    renameItem->SetBitmap(
        wxArtProvider::GetBitmap(
            wxART_EDIT,
            wxART_MENU,
            wxSize(16, 16)));

    wxMenuItem *deleteItem = Append(
        ID_DELETE,
        "Delete\tDel");

    deleteItem->SetBitmap(
        wxArtProvider::GetBitmap(
            wxART_DELETE,
            wxART_MENU,
            wxSize(16, 16)));

    AppendSeparator();

    Append(
        ID_COPY_PATH,
        "Copy Path");

    Append(
        ID_REVEAL,
        "Reveal in File Manager");

    Append(
        ID_OPEN_TERMINAL,
        "Open in Integrated Terminal");

    AppendSeparator();

    // ─────────────────────────
    // GIT
    // ─────────────────────────

    Append(
        ID_GIT_STAGE,
        "Git: Stage Changes");

    Append(
        ID_GIT_DISCARD,
        "Git: Discard Changes");

    AppendSeparator();

    // ─────────────────────────
    // PROPERTIES
    // ─────────────────────────

    wxMenuItem *propertiesItem = Append(
        ID_PROPERTIES,
        "Properties");

    propertiesItem->SetBitmap(
        wxArtProvider::GetBitmap(
            wxART_INFORMATION,
            wxART_MENU,
            wxSize(16, 16)));
}

void DOContextMenu::BindEvents()
{
    Bind(
        wxEVT_MENU,
        &DOContextMenu::OnMenu,
        this);
}

void DOContextMenu::OnMenu(wxCommandEvent &evt)
{
    auto it = m_callbacks.find(evt.GetId());

    if (it != m_callbacks.end())
    {
        it->second();
    }
}
