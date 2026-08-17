#pragma once

#include <wx/dialog.h>
#include <wx/listctrl.h>
#include <wx/string.h>
#include <wx/textctrl.h>
#include <wx/choice.h>
#include <wx/checkbox.h>
#include <wx/button.h>
#include <wx/stattext.h>

#include <functional>
#include <vector>

struct ReplaceRequest
{
    enum class Scope
    {
        CurrentDocument = 0,
        WorkspaceFolders = 1
    };

    wxString findText;
    wxString replaceText;
    Scope scope = Scope::CurrentDocument;
    bool matchCase = false;
    bool wholeWord = false;
    bool useRegex = false;
};

struct ReplacePreviewItem
{
    wxString filepath;
    int line = 0;
    int column = 0;
    wxString preview;
};

class ReplaceDialog final : public wxDialog
{
public:
    using PreviewCallback = std::function<std::vector<ReplacePreviewItem>(const ReplaceRequest&, wxString&)>;
    using ReplaceOneCallback = std::function<bool(const ReplaceRequest&, wxString&)>;
    using ReplaceAllCallback = std::function<size_t(const ReplaceRequest&, wxString&)>;
    using OpenResultCallback = std::function<void(const ReplacePreviewItem&)>;

    ReplaceDialog(wxWindow* parent,
                  ReplaceRequest::Scope initialScope,
                  const wxString& initialFind = wxString());

    void SetPreviewCallback(PreviewCallback callback) { m_previewCallback = std::move(callback); }
    void SetReplaceOneCallback(ReplaceOneCallback callback) { m_replaceOneCallback = std::move(callback); }
    void SetReplaceAllCallback(ReplaceAllCallback callback) { m_replaceAllCallback = std::move(callback); }
    void SetOpenResultCallback(OpenResultCallback callback) { m_openResultCallback = std::move(callback); }

private:
    ReplaceRequest BuildRequest() const;
    void RefreshScopeUI();
    void Preview();
    void ReplaceOne();
    void ReplaceAll();
    void PopulateResults(const std::vector<ReplacePreviewItem>& results);

    wxTextCtrl* m_find = nullptr;
    wxTextCtrl* m_replace = nullptr;
    wxChoice* m_scope = nullptr;
    wxCheckBox* m_matchCase = nullptr;
    wxCheckBox* m_wholeWord = nullptr;
    wxCheckBox* m_regex = nullptr;
    wxButton* m_replaceOne = nullptr;
    wxButton* m_replaceAll = nullptr;
    wxStaticText* m_status = nullptr;
    wxListCtrl* m_results = nullptr;

    std::vector<ReplacePreviewItem> m_previewItems;
    PreviewCallback m_previewCallback;
    ReplaceOneCallback m_replaceOneCallback;
    ReplaceAllCallback m_replaceAllCallback;
    OpenResultCallback m_openResultCallback;
};
