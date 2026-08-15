#include <wx/treectrl.h>
#include <wx/dir.h>
#include <wx/filename.h>
#include <wx/imaglist.h>
#include <wx/artprov.h>
#include <string>
#include <vector>
#include "constant.h"
#include "PathData.h"
#include "DOContextMenu.h"
#include "FileTreeCtrl.h"

namespace
{
wxString FolderLabel(const wxString& path)
{
    wxFileName folder(path, wxEmptyString);
    const wxArrayString dirs = folder.GetDirs();
    if (!dirs.IsEmpty())
        return dirs[dirs.size() - 1];
    return path;
}
}

FileTreeCtrl::FileTreeCtrl(wxWindow* parent)
    : wxTreeCtrl(parent, ID_TREE_CTRL, wxDefaultPosition, wxDefaultSize,
                 wxTR_HAS_BUTTONS | wxTR_LINES_AT_ROOT | wxTR_HIDE_ROOT |
                     wxTR_SINGLE | wxNO_BORDER)
{
    SetBackgroundColour(Colors::BG_PANEL);
    SetForegroundColour(Colors::SIDEBAR_TEXT);

    wxImageList* il = new wxImageList(16, 16, true);
    il->Add(wxArtProvider::GetIcon(wxART_FOLDER, wxART_OTHER, wxSize(16, 16)));
    il->Add(wxArtProvider::GetIcon(wxART_FOLDER_OPEN, wxART_OTHER, wxSize(16, 16)));
    il->Add(wxArtProvider::GetIcon(wxART_NORMAL_FILE, wxART_OTHER, wxSize(16, 16)));
    AssignImageList(il);

    Bind(wxEVT_CONTEXT_MENU, [&](wxContextMenuEvent&)
    {
        auto* menu = new DOContextMenu(this);
        menu->SetOnNewFile([&]() { wxLogMessage("New File"); });
        menu->SetOnDelete([&]() { wxLogMessage("Delete"); });
        PopupMenu(menu);
        delete menu;
    });

    Bind(wxEVT_TREE_ITEM_EXPANDING,
         &FileTreeCtrl::OnItemExpanding,
         this,
         ID_TREE_CTRL);
}

void FileTreeCtrl::ClearFolders()
{
    DeleteAllItems();
    m_rootPaths.clear();
    files.clear();
    rootPath.clear();
    AddRoot("WORKSPACE", 0, 1);
}

void FileTreeCtrl::LoadFolder(const wxString& path)
{
    LoadFolders({path}, false);
}

void FileTreeCtrl::LoadFolders(const std::vector<wxString>& paths, bool workspaceMode)
{
    DeleteAllItems();
    m_rootPaths.clear();
    files.clear();
    rootPath.clear();

    for (const wxString& path : paths)
    {
        if (!path.IsEmpty() && wxDirExists(path))
            m_rootPaths.push_back(path);
    }

    if (!m_rootPaths.empty())
        rootPath = m_rootPaths.front();

    if (m_rootPaths.empty())
    {
        AddRoot("WORKSPACE", 0, 1);
        return;
    }

    if (m_rootPaths.size() == 1 && !workspaceMode)
    {
        wxTreeItemId root = AddRoot(m_rootPaths.front(), 0, 1,
                                    new PathData(m_rootPaths.front()));
        PopulateDir(root, m_rootPaths.front());
        return;
    }

    // In workspace mode the hidden root is synthetic and every workspace
    // folder remains individually visible, like VS Code multi-root workspaces.
    wxTreeItemId workspaceRoot = AddRoot("WORKSPACE", 0, 1);
    for (const wxString& path : m_rootPaths)
    {
        wxTreeItemId folderItem = AppendItem(workspaceRoot,
                                             FolderLabel(path),
                                             0,
                                             1,
                                             new PathData(path));
        SetItemTextColour(folderItem, Colors::SIDEBAR_TEXT);
        PopulateDir(folderItem, path);
        Expand(folderItem);
    }
}

wxString FileTreeCtrl::GetSelectedPath()
{
    const wxTreeItemId selected = GetSelection();
    if (!selected.IsOk())
        return wxString();
    PathData* data = dynamic_cast<PathData*>(GetItemData(selected));
    return data ? data->GetPath() : wxString();
}

wxString FileTreeCtrl::GetSelectedFilePath()
{
    const wxString path = GetSelectedPath();
    return !path.IsEmpty() && wxFileExists(path) ? path : wxString();
}

wxString FileTreeCtrl::GetContainingRoot(const wxString& path) const
{
    if (path.IsEmpty())
        return wxString();

    wxFileName candidate(path);
    candidate.Normalize(wxPATH_NORM_DOTS | wxPATH_NORM_ABSOLUTE);
    const wxString full = candidate.GetFullPath();

    wxString best;
    for (const wxString& root : m_rootPaths)
    {
        wxString prefix = root;
        if (!prefix.IsEmpty() && prefix.Last() != wxFileName::GetPathSeparator())
            prefix += wxFileName::GetPathSeparator();
#ifdef __WXMSW__
        const bool exact = full.CmpNoCase(root) == 0;
        const bool inside = full.Lower().StartsWith(prefix.Lower());
#else
        const bool exact = full == root;
        const bool inside = full.StartsWith(prefix);
#endif
        if ((exact || inside) && root.length() > best.length())
            best = root;
    }
    return best;
}

void FileTreeCtrl::PopulateDir(wxTreeItemId parent, const wxString& path)
{
    wxDir dir(path);
    if (!dir.IsOpened())
        return;

    wxString name;
    if (dir.GetFirst(&name, wxEmptyString, wxDIR_DIRS))
    {
        do
        {
            if (name.StartsWith("."))
                continue;
            wxString full = path;
            full += wxFileName::GetPathSeparator();
            full += name;
            wxTreeItemId child = AppendItem(parent, name, 0, 1, new PathData(full));
            SetItemTextColour(child, Colors::SIDEBAR_TEXT);
            AppendItem(child, "<loading>");
        } while (dir.GetNext(&name));
    }

    if (dir.GetFirst(&name, wxEmptyString, wxDIR_FILES))
    {
        do
        {
            if (name.StartsWith("."))
                continue;
            wxString full = path;
            full += wxFileName::GetPathSeparator();
            full += name;
            wxTreeItemId child = AppendItem(parent, name, 2, 2, new PathData(full));
            SetItemTextColour(child, Colors::SIDEBAR_TEXT);
            files.emplace_back(full.ToStdString());
        } while (dir.GetNext(&name));
    }
}

void FileTreeCtrl::OnItemExpanding(wxTreeEvent& evt)
{
    wxTreeItemId item = evt.GetItem();
    wxTreeItemIdValue cookie;
    wxTreeItemId first = GetFirstChild(item, cookie);
    if (first.IsOk() && GetItemText(first) == "<loading>")
    {
        Delete(first);
        PathData* data = dynamic_cast<PathData*>(GetItemData(item));
        if (data)
            PopulateDir(item, data->GetPath());
    }
}
