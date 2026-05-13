#pragma once

#include <wx/wx.h>
#include <wx/treectrl.h>
#include <wx/dir.h>
#include <functional>

class ExplorerDialog : public wxDialog
{
public:

    using FileOpenCallback =
        std::function<void(const wxString&)>;

    ExplorerDialog(
        wxWindow* parent,
        const wxString& folder)
        : wxDialog(
              parent,
              wxID_ANY,
              "Explorer",
              wxDefaultPosition,
              wxSize(350, 600),
              wxDEFAULT_DIALOG_STYLE |
              wxRESIZE_BORDER)
    {
        SetBackgroundColour(
            wxColour(37, 37, 38));

        auto* root =
            new wxBoxSizer(wxVERTICAL);

        // HEADER
        wxPanel* header =
            new wxPanel(this);

        header->SetBackgroundColour(
            wxColour(45, 45, 48));

        auto* headerSizer =
            new wxBoxSizer(wxHORIZONTAL);

        wxStaticText* title =
            new wxStaticText(
                header,
                wxID_ANY,
                "Project Explorer");

        title->SetForegroundColour(*wxWHITE);

        wxButton* closeBtn =
            new wxButton(
                header,
                wxID_CLOSE,
                "×",
                wxDefaultPosition,
                wxSize(30, 30),
                wxBORDER_NONE);

        closeBtn->SetBackgroundColour(
            wxColour(45, 45, 48));

        closeBtn->SetForegroundColour(*wxWHITE);

        headerSizer->Add(
            title,
            1,
            wxALIGN_CENTER_VERTICAL | wxLEFT,
            10);

        headerSizer->Add(
            closeBtn,
            0,
            wxALL,
            2);

        header->SetSizer(headerSizer);

        // TREE
        tree =
            new wxTreeCtrl(
                this,
                wxID_ANY,
                wxDefaultPosition,
                wxDefaultSize,
                wxTR_HAS_BUTTONS |
                wxTR_LINES_AT_ROOT |
                wxTR_HIDE_ROOT);

        root->Add(header, 0, wxEXPAND);
        root->Add(tree, 1, wxEXPAND);

        SetSizer(root);

        // EVENTS
        closeBtn->Bind(
            wxEVT_BUTTON,
            [&](wxCommandEvent&)
            {
                EndModal(wxID_CANCEL);
            });

        tree->Bind(
            wxEVT_TREE_ITEM_ACTIVATED,
            &ExplorerDialog::OnItemActivated,
            this);

        LoadFolder(folder);
    }

    void SetOnFileOpen(FileOpenCallback cb)
    {
        onFileOpen = cb;
    }

private:

    class PathData : public wxTreeItemData
    {
    public:

        PathData(const wxString& p)
            : path(p)
        {}

        wxString path;
    };

    wxTreeCtrl* tree;

    FileOpenCallback onFileOpen;

    void LoadFolder(const wxString& path)
    {
        tree->DeleteAllItems();

        wxTreeItemId rootId =
            tree->AddRoot(path);

        AddFolder(rootId, path);

        tree->Expand(rootId);
    }

    void AddFolder(
        wxTreeItemId parent,
        const wxString& folder)
    {
        wxDir dir(folder);

        if (!dir.IsOpened())
            return;

        wxString filename;

        bool cont =
            dir.GetFirst(&filename);

        while (cont)
        {
            wxString fullPath =
                folder + "/" + filename;

            if (wxDirExists(fullPath))
            {
                wxTreeItemId id =
                    tree->AppendItem(
                        parent,
                        "📁 " + filename);

                tree->SetItemData(
                    id,
                    new PathData(fullPath));

                AddFolder(id, fullPath);
            }
            else
            {
                wxTreeItemId id =
                    tree->AppendItem(
                        parent,
                        filename);

                tree->SetItemData(
                    id,
                    new PathData(fullPath));
            }

            cont =
                dir.GetNext(&filename);
        }
    }

    void OnItemActivated(wxTreeEvent& e)
    {
        wxTreeItemId item =
            e.GetItem();

        if (!item.IsOk())
            return;

        PathData* data =
            dynamic_cast<PathData*>(
                tree->GetItemData(item));

        if (!data)
            return;

        if (wxFileExists(data->path))
        {
            if (onFileOpen)
                onFileOpen(data->path);
        }
    }
};