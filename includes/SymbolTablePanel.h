#ifndef SYMBOL_TABLE_PANEL_H
#define SYMBOL_TABLE_PANEL_H

#include <wx/wx.h>
#include <wx/notebook.h>
#include <wx/treectrl.h>
#include <wx/checkbox.h>

#include <functional>
#include <string>

#include "Config.h"

#ifndef DODEV_ENABLE_MANUAL_SYMBOLS
#define DODEV_ENABLE_MANUAL_SYMBOLS 0
#endif
#if DODEV_ENABLE_MANUAL_SYMBOLS
#include "symbols/ManualSymbolParser.h"
#endif

class SymbolTablePanel : public wxPanel
{
public:
    using OpenLocationCallback = std::function<void(const wxString&, unsigned, unsigned)>;

    explicit SymbolTablePanel(wxWindow* parent);

    void SetOpenLocationCallback(OpenLocationCallback callback);
    void SetProjectRoot(const wxString& root);
    void SetCurrentDocument(const wxString& path, const wxString& contents, bool autoRefresh = true);

    void RefreshAnalysis();
    void ShowCallHierarchy();
    bool NavigateToDefinition(unsigned line, unsigned column);

    bool IsManualParsingEnabled() const;
    wxString BuildAIContext() const;
    void ReloadRuntimeSettings();

    // Compatibility with the original lightweight symbol panel API.
    void AddFunction(const wxString& name);
    void AddVariable(const wxString& name);
    void AddMacro(const wxString& name);
    void Clear();

private:
    class LocationData;

    void BuildUI();
#if DODEV_ENABLE_MANUAL_SYMBOLS
    void PopulateManualSymbols(const dodev::symbols::AnalysisResult& result);
    void PopulateManualCalls(const dodev::symbols::AnalysisResult& result);
    dodev::symbols::RuntimeSettings ManualSettings() const;
    static wxString ManualKindName(dodev::symbols::SymbolKind kind);
#endif
    void AppendOutput(const wxString& line);
    void OpenTreeItem(wxTreeCtrl* tree, const wxTreeItemId& item);
    void UpdateAvailabilityUI();

    wxString m_projectRoot;
    wxString m_currentPath;
    wxString m_currentContents;

#if DODEV_ENABLE_MANUAL_SYMBOLS
    dodev::symbols::ParserRegistry m_manualParser;
    dodev::symbols::AnalysisResult m_lastManualAnalysis;
#endif
    ManualSymbolRuntimeConfig m_manualRuntimeSettings;
    OpenLocationCallback m_openLocation;

    wxStaticText* m_status = nullptr;
    wxCheckBox* m_manualEnableCheck = nullptr;
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
