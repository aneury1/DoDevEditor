#ifndef SYMBOL_TABLE_PANEL_H
#define SYMBOL_TABLE_PANEL_H

#include <wx/wx.h>
#include <wx/notebook.h>
#include <wx/treectrl.h>
#include <wx/choice.h>
#include <wx/checkbox.h>
#include <wx/textctrl.h>

#include <functional>
#include <vector>

#include "CppAnalysisEngine.h"

class SymbolTablePanel : public wxPanel
{
public:
    using OpenLocationCallback = std::function<void(const wxString&, unsigned, unsigned)>;
    using SaveCurrentFileCallback = std::function<bool()>;

    explicit SymbolTablePanel(wxWindow* parent);

    void SetOpenLocationCallback(OpenLocationCallback callback);
    void SetSaveCurrentFileCallback(SaveCurrentFileCallback callback);
    void SetProjectRoot(const wxString& root);
    void SetCurrentDocument(const wxString& path, const wxString& contents, bool autoRefresh = true);

    void RefreshAnalysis();
    void CompileCurrent(bool syntaxOnly = false);
    bool NavigateToDefinition(unsigned line, unsigned column);

    bool IsLLVMEnabled() const;
    wxString LLVMStatus() const;
    wxString BuildAIContext() const;

    // Compatibility with the original lightweight symbol panel API.
    void AddFunction(const wxString& name);
    void AddVariable(const wxString& name);
    void AddMacro(const wxString& name);
    void Clear();

private:
    class LocationData;

    CppAnalysisOptions CurrentOptions() const;
    static std::vector<wxString> ParseList(const wxString& value);
    static wxString JoinList(const std::vector<wxString>& values);
    static wxString KindName(CppSymbolKind kind);

    void BuildUI();
    void PopulateSymbols(const CppAnalysisResult& result);
    void PopulateCalls(const CppAnalysisResult& result);
    void AppendCallNode(wxTreeItemId parent, const CppCallNode& node);
    void ShowDiagnostics(const CppAnalysisResult& result);
    void AppendOutput(const wxString& line);
    void OpenTreeItem(wxTreeCtrl* tree, const wxTreeItemId& item);
    void UpdateAvailabilityUI();

    void LoadProjectConfig();
    void SaveProjectConfig();
    wxString ConfigPath() const;

    wxString m_projectRoot;
    wxString m_currentPath;
    wxString m_currentContents;

    CppAnalysisEngine m_engine;
    CppAnalysisResult m_lastAnalysis;
    OpenLocationCallback m_openLocation;
    SaveCurrentFileCallback m_saveCurrentFile;

    wxStaticText* m_status = nullptr;
    wxCheckBox* m_enableCheck = nullptr;
    wxCheckBox* m_autoCheck = nullptr;
    wxChoice* m_standardChoice = nullptr;
    wxTextCtrl* m_defines = nullptr;
    wxTextCtrl* m_includeDirs = nullptr;
    wxTextCtrl* m_extraArgs = nullptr;

    wxNotebook* m_notebook = nullptr;
    wxTreeCtrl* m_symbolsTree = nullptr;
    wxTreeCtrl* m_callsTree = nullptr;
    wxTextCtrl* m_output = nullptr;

    wxTreeItemId m_functionsRoot;
    wxTreeItemId m_typesRoot;
    wxTreeItemId m_variablesRoot;
    wxTreeItemId m_macrosRoot;
    wxTreeItemId m_otherRoot;
};

#endif
