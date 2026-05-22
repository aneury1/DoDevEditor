
#include <wx/treectrl.h>
#include <wx/dir.h>
#include <wx/filename.h>
#include <string>
#include <vector>
#include "constant.h"
#include "PathData.h"
#include "Config.h"
#include "DOContextMenu.h"
#include "FileTreeCtrl.h"

FileTreeCtrl::FileTreeCtrl(wxWindow *parent)
    : wxTreeCtrl(parent, ID_TREE_CTRL, wxDefaultPosition, wxDefaultSize,
                 wxTR_HAS_BUTTONS | wxTR_LINES_AT_ROOT | wxTR_HIDE_ROOT |
                     wxTR_SINGLE | wxNO_BORDER)
{
    SetBackgroundColour(Colors::BG_PANEL);
    SetForegroundColour(Colors::SIDEBAR_TEXT);

    // Build image list
    wxImageList *il = new wxImageList(16, 16, true);
    il->Add(wxArtProvider::GetIcon(wxART_FOLDER, wxART_OTHER, wxSize(16, 16)));
    il->Add(wxArtProvider::GetIcon(wxART_FOLDER_OPEN, wxART_OTHER, wxSize(16, 16)));
    il->Add(wxArtProvider::GetIcon(wxART_NORMAL_FILE, wxART_OTHER, wxSize(16, 16)));
    AssignImageList(il);

    Bind(
        wxEVT_CONTEXT_MENU,
        [&](wxContextMenuEvent &)
        {
            auto *menu = new DOContextMenu(this);

            menu->SetOnNewFile([&]()
                               { wxLogMessage("New File"); });

            menu->SetOnDelete([&]()
                              { wxLogMessage("Delete"); });

            PopupMenu(menu);

            delete menu;
        });

        // In constructor
Bind(wxEVT_TREE_ITEM_EXPANDING,
     &FileTreeCtrl::OnItemExpanding,
     this,
     ID_TREE_CTRL);
}

void FileTreeCtrl::LoadFolder(const wxString &path)
{
    DeleteAllItems();
    rootPath = path;
    wxTreeItemId root = AddRoot(path, 0, 1);
    PopulateDir(root, path);
    AppEditorConfig::AppEditorConfig::SaveOpenedFolder(path.ToStdString());
    /// Expand(root);
}

// Return full path of selected file item (empty string if it's a directory)
wxString FileTreeCtrl::GetSelectedFilePath()
{
    wxTreeItemId sel = GetSelection();
    if (!sel.IsOk())
        return wxEmptyString;
    if (ItemHasChildren(sel))
        return wxEmptyString; // directory
    PathData *data = dynamic_cast<PathData *>(GetItemData(sel));
    return data ? data->GetPath() : wxString();
}

inline std::vector<std::string> FileTreeCtrl::getFiles()
{
    return files;
}

void FileTreeCtrl::PopulateDir(wxTreeItemId parent, const wxString &path)
{
    wxDir dir(path);
    if (!dir.IsOpened())
        return;
    files.clear();

    // First: subdirectories
    wxString name;
    if (dir.GetFirst(&name, wxEmptyString, wxDIR_DIRS))
    {
        do
        {
            if (name.StartsWith("."))
                continue; // skip hidden
            wxString full = path + wxFileName::GetPathSeparator() + name;
            wxTreeItemId child = AppendItem(parent, name, 0, 1,
                                            new PathData(full));
            SetItemTextColour(child, Colors::SIDEBAR_TEXT);
            AppendItem(child, "<loading>"); // placeholder so expand arrow shows
        } while (dir.GetNext(&name));
    }

    // Then: files
    if (dir.GetFirst(&name, wxEmptyString, wxDIR_FILES))
    {
        do
        {
            if (name.StartsWith("."))
                continue;
            wxString full = path + wxFileName::GetPathSeparator() + name;
            wxTreeItemId child = AppendItem(parent, name, 2, 2,
                                            new PathData(full));
            SetItemTextColour(child, Colors::SIDEBAR_TEXT);
            files.emplace_back(full);
        } while (dir.GetNext(&name));
    }
}

void FileTreeCtrl::OnItemExpanding(wxTreeEvent &evt)
{
    wxTreeItemId item = evt.GetItem();
    wxTreeItemIdValue cookie;
    wxTreeItemId first = GetFirstChild(item, cookie);
    if (first.IsOk() && GetItemText(first) == "<loading>")
    {
        Delete(first);
        PathData *data = dynamic_cast<PathData *>(GetItemData(item));
        if (data)
            PopulateDir(item, data->GetPath());
    }
}
