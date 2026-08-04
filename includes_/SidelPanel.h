#pragma once

#include <wx/wx.h>
#include <wx/treectrl.h>
#include <wx/dir.h>
#include <wx/dirdlg.h>
#include <functional>

class SidePanel : public wxPanel
{
public:

    using FileSelectedCallback =
        std::function<void(const wxString&)>;

    SidePanel(wxWindow* parent)
        : wxPanel(parent)
    {
        SetMinSize(wxSize(280, -1));
        SetBackgroundColour(wxColour(37, 37, 38));

        auto* root = new wxBoxSizer(wxVERTICAL);

        // =========================
        // TOP BAR
        // =========================
        wxPanel* topBar = new wxPanel(this);
        topBar->SetBackgroundColour(wxColour(45, 45, 48));

        auto* topSizer = new wxBoxSizer(wxHORIZONTAL);

        openBtn = new wxButton(topBar, wxID_ANY, "Open Folder");

        openBtn->SetBackgroundColour(wxColour(60, 60, 60));
        openBtn->SetForegroundColour(*wxWHITE);

        topSizer->Add(openBtn, 1, wxEXPAND | wxALL, 5);
        topBar->SetSizer(topSizer);

        // =========================
        // TREE
        // =========================
        tree = new wxTreeCtrl(
            this,
            wxID_ANY,
            wxDefaultPosition,
            wxDefaultSize,
            wxTR_HAS_BUTTONS |
            wxTR_LINES_AT_ROOT |
            wxTR_HIDE_ROOT);

        root->Add(topBar, 0, wxEXPAND);
        root->Add(tree, 1, wxEXPAND);

        SetSizer(root);

        // =========================
        // EVENTS
        // =========================
        openBtn->Bind(wxEVT_BUTTON, &SidePanel::OnOpenFolder, this);
        tree->Bind(wxEVT_TREE_ITEM_ACTIVATED, &SidePanel::OnItemActivated, this);
    }

    void SetOnFileSelected(FileSelectedCallback cb)
    {
        onFileSelected = cb;
    }

private:

    class PathData : public wxTreeItemData
    {
    public:
        PathData(const wxString& p) : path(p) {}
        wxString path;
    };

    wxButton* openBtn;
    wxTreeCtrl* tree;
    FileSelectedCallback onFileSelected;

    // =========================
    // OPEN FOLDER
    // =========================
    void OnOpenFolder(wxCommandEvent&)
    {
        wxDirDialog dialog(this, "Select Project Folder");

        if (dialog.ShowModal() != wxID_OK)
            return;

        LoadFolder(dialog.GetPath());
    }

    // =========================
    // LOAD TREE (FIXED)
    // =========================
    void LoadFolder(const wxString& folder)
    {
        tree->Freeze();

        tree->DeleteAllItems();

        wxTreeItemId rootId = tree->AddRoot(folder);

        AddFolder(rootId, folder);

        // expand first level only (safe with hidden root)
        wxTreeItemIdValue cookie;
        wxTreeItemId child = tree->GetFirstChild(rootId, cookie);

        while (child.IsOk())
        {
            tree->Expand(child);
            child = tree->GetNextChild(rootId, cookie);
        }

        tree->Thaw();
    }

    // =========================
    // RECURSIVE ADD FOLDER
    // =========================
    void AddFolder(wxTreeItemId parent, const wxString& folder)
    {
        wxDir dir(folder);
        if (!dir.IsOpened())
            return;

        wxString filename;
        bool cont = dir.GetFirst(&filename);

        while (cont)
        {
            wxString fullPath = folder + "/" + filename;

            if (wxDirExists(fullPath))
            {
                wxTreeItemId id =
                    tree->AppendItem(parent, "📁 " + filename);

                tree->SetItemData(id, new PathData(fullPath));

                AddFolder(id, fullPath);

                // IMPORTANT: make folders visible
                tree->Expand(id);
            }
            else
            {
                wxTreeItemId id =
                    tree->AppendItem(parent, filename);

                tree->SetItemData(id, new PathData(fullPath));
            }

            cont = dir.GetNext(&filename);
        }
    }

    // =========================
    // FILE SELECT CALLBACK
    // =========================
    void OnItemActivated(wxTreeEvent& e)
    {
        wxTreeItemId item = e.GetItem();
        if (!item.IsOk()) return;

        PathData* data =
            dynamic_cast<PathData*>(tree->GetItemData(item));

        if (!data) return;

        if (wxFileExists(data->path))
        {
            if (onFileSelected)
                onFileSelected(data->path);
        }
    }
};