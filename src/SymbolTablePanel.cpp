#include "SymbolTablePanel.h"

#include <wx/filename.h>

#include <map>
#include <set>
#include <utility>
#include <vector>

namespace
{
std::string ToUtf8(const wxString& value)
{
    const wxScopedCharBuffer buffer = value.utf8_str();
    return buffer.data() ? std::string(buffer.data()) : std::string();
}

wxString FromUtf8(const std::string& value)
{
    return wxString::FromUTF8(value.c_str());
}

wxString WordAt(const wxString& text, unsigned requestedLine, unsigned requestedColumn)
{
    if (requestedLine == 0)
        return wxString();

    size_t lineStart = 0;
    unsigned line = 1;
    while (line < requestedLine && lineStart < text.length())
    {
        const size_t next = text.find('\n', lineStart);
        if (next == wxString::npos)
            return wxString();
        lineStart = next + 1;
        ++line;
    }

    size_t lineEnd = text.find('\n', lineStart);
    if (lineEnd == wxString::npos)
        lineEnd = text.length();
    if (lineStart >= lineEnd)
        return wxString();

    size_t position = lineStart;
    if (requestedColumn > 0)
        position += static_cast<size_t>(requestedColumn - 1);
    if (position >= lineEnd)
        position = lineEnd - 1;

    auto isWord = [](wxChar c)
    {
        return wxIsalnum(c) || c == '_' || c == '$';
    };

    if (!isWord(text[position]) && position > lineStart && isWord(text[position - 1]))
        --position;
    if (!isWord(text[position]))
        return wxString();

    size_t begin = position;
    while (begin > lineStart && isWord(text[begin - 1]))
        --begin;
    size_t end = position + 1;
    while (end < lineEnd && isWord(text[end]))
        ++end;
    return text.Mid(begin, end - begin);
}
}

class SymbolTablePanel::LocationData : public wxTreeItemData
{
public:
    LocationData(wxString file, unsigned line, unsigned column)
        : m_file(std::move(file)), m_line(line), m_column(column) {}

    wxString m_file;
    unsigned m_line = 0;
    unsigned m_column = 0;
};

SymbolTablePanel::SymbolTablePanel(wxWindow* parent)
    : wxPanel(parent, wxID_ANY),
      m_manualRuntimeSettings(AppEditorConfig::GetManualSymbolRuntimeConfig())
{
    BuildUI();
    UpdateAvailabilityUI();
}

void SymbolTablePanel::BuildUI()
{
    SetBackgroundColour(wxColour(30, 30, 30));
    auto* root = new wxBoxSizer(wxVERTICAL);

    auto* headerRow = new wxBoxSizer(wxHORIZONTAL);
    auto* header = new wxStaticText(this, wxID_ANY, "CODE ANALYSIS");
    header->SetForegroundColour(wxColour(200, 200, 200));
    wxFont headerFont = header->GetFont();
    headerFont.SetWeight(wxFONTWEIGHT_BOLD);
    headerFont.SetPointSize(9);
    header->SetFont(headerFont);
    headerRow->Add(header, 1, wxALIGN_CENTER_VERTICAL);

    m_manualEnableCheck = new wxCheckBox(this, wxID_ANY, "Manual parser");
    m_manualEnableCheck->SetForegroundColour(wxColour(220, 220, 220));
#if DODEV_ENABLE_MANUAL_SYMBOLS
    m_manualEnableCheck->SetValue(m_manualRuntimeSettings.enabled);
#else
    m_manualEnableCheck->SetValue(false);
    m_manualEnableCheck->Enable(false);
#endif
    headerRow->Add(m_manualEnableCheck, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 8);
    root->Add(headerRow, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 8);

    m_status = new wxStaticText(this, wxID_ANY, "");
    m_status->SetForegroundColour(wxColour(150, 150, 150));
    root->Add(m_status, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 8);

    auto* actionRow = new wxBoxSizer(wxHORIZONTAL);
    auto* refreshButton = new wxButton(this, wxID_ANY, "Parse", wxDefaultPosition, wxSize(70, -1));
    auto* callsButton = new wxButton(this, wxID_ANY, "Calls", wxDefaultPosition, wxSize(70, -1));
    actionRow->Add(refreshButton, 0, wxRIGHT, 4);
    actionRow->Add(callsButton, 0);
    root->Add(actionRow, 0, wxEXPAND | wxALL, 8);

    m_notebook = new wxNotebook(this, wxID_ANY);
    m_symbolsTree = new wxTreeCtrl(m_notebook, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                   wxTR_HIDE_ROOT | wxTR_HAS_BUTTONS | wxTR_LINES_AT_ROOT |
                                   wxTR_SINGLE | wxBORDER_NONE);
    m_callsTree = new wxTreeCtrl(m_notebook, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                 wxTR_HIDE_ROOT | wxTR_HAS_BUTTONS | wxTR_LINES_AT_ROOT |
                                 wxTR_SINGLE | wxBORDER_NONE);
    m_output = new wxTextCtrl(m_notebook, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                              wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2 | wxHSCROLL);

    for (wxWindow* window : {static_cast<wxWindow*>(m_symbolsTree),
                             static_cast<wxWindow*>(m_callsTree),
                             static_cast<wxWindow*>(m_output)})
    {
        window->SetBackgroundColour(wxColour(30, 30, 30));
        window->SetForegroundColour(wxColour(220, 220, 220));
    }

    m_notebook->AddPage(m_symbolsTree, "Symbols", true);
    m_notebook->AddPage(m_callsTree, "Call Hierarchy", false);
    m_notebook->AddPage(m_output, "Analysis", false);
    root->Add(m_notebook, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6);
    SetSizer(root);

    refreshButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { RefreshAnalysis(); });
    callsButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { ShowCallHierarchy(); });
    m_manualEnableCheck->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&)
    {
#if DODEV_ENABLE_MANUAL_SYMBOLS
        m_manualRuntimeSettings.enabled = m_manualEnableCheck->GetValue();
        AppEditorConfig::SetManualSymbolRuntimeConfig(m_manualRuntimeSettings);
        RefreshAnalysis();
#else
        m_manualEnableCheck->SetValue(false);
#endif
    });

    m_symbolsTree->Bind(wxEVT_TREE_ITEM_ACTIVATED, [this](wxTreeEvent& event)
    {
        OpenTreeItem(m_symbolsTree, event.GetItem());
    });
    m_callsTree->Bind(wxEVT_TREE_ITEM_ACTIVATED, [this](wxTreeEvent& event)
    {
        OpenTreeItem(m_callsTree, event.GetItem());
    });
}

void SymbolTablePanel::SetOpenLocationCallback(OpenLocationCallback callback)
{
    m_openLocation = std::move(callback);
}

void SymbolTablePanel::SetProjectRoot(const wxString& root)
{
    m_projectRoot = root;
}

void SymbolTablePanel::ReloadRuntimeSettings()
{
    m_manualRuntimeSettings = AppEditorConfig::GetManualSymbolRuntimeConfig();
#if DODEV_ENABLE_MANUAL_SYMBOLS
    if (m_manualEnableCheck)
        m_manualEnableCheck->SetValue(m_manualRuntimeSettings.enabled);
#endif
    if (!m_currentPath.IsEmpty())
        RefreshAnalysis();
    else
        UpdateAvailabilityUI();
}

void SymbolTablePanel::SetCurrentDocument(const wxString& path,
                                          const wxString& contents,
                                          bool autoRefresh)
{
    m_currentPath = path;
    m_currentContents = contents;

#if DODEV_ENABLE_MANUAL_SYMBOLS
    const bool supported = m_manualParser.SupportsFile(ToUtf8(path));
    if (!supported)
    {
        Clear();
        m_status->SetLabel(path.IsEmpty() ? "No document" : "No manual symbol parser registered for this file type");
        return;
    }
    m_status->SetLabel(wxFileName(path).GetFullName());
    if (autoRefresh && IsManualParsingEnabled() && m_manualRuntimeSettings.autoParse)
        RefreshAnalysis();
#else
    (void)autoRefresh;
    Clear();
    m_status->SetLabel("Manual source parser is disabled in this build");
#endif
}

bool SymbolTablePanel::IsManualParsingEnabled() const
{
#if DODEV_ENABLE_MANUAL_SYMBOLS
    return m_manualRuntimeSettings.enabled && m_manualEnableCheck && m_manualEnableCheck->GetValue();
#else
    return false;
#endif
}

#if DODEV_ENABLE_MANUAL_SYMBOLS
dodev::symbols::RuntimeSettings SymbolTablePanel::ManualSettings() const
{
    dodev::symbols::RuntimeSettings settings;
    settings.enabled = m_manualRuntimeSettings.enabled;
    settings.cEnabled = m_manualRuntimeSettings.cEnabled;
    settings.cppEnabled = m_manualRuntimeSettings.cppEnabled;
    settings.kotlinEnabled = m_manualRuntimeSettings.kotlinEnabled;
    settings.symbolsEnabled = m_manualRuntimeSettings.symbolsEnabled;
    settings.callsEnabled = m_manualRuntimeSettings.callsEnabled;
    settings.typesEnabled = m_manualRuntimeSettings.typesEnabled;
    settings.functionsEnabled = m_manualRuntimeSettings.functionsEnabled;
    settings.variablesEnabled = m_manualRuntimeSettings.variablesEnabled;
    settings.macrosEnabled = m_manualRuntimeSettings.macrosEnabled;
    settings.autoParse = m_manualRuntimeSettings.autoParse;
    return settings;
}

wxString SymbolTablePanel::ManualKindName(dodev::symbols::SymbolKind kind)
{
    return wxString::FromUTF8(dodev::symbols::SymbolKindName(kind));
}
#endif

void SymbolTablePanel::RefreshAnalysis()
{
    Clear();
#if DODEV_ENABLE_MANUAL_SYMBOLS
    if (!IsManualParsingEnabled())
    {
        m_status->SetLabel("Manual parser disabled in Settings > Code Analysis");
        return;
    }

    const dodev::symbols::Language language = m_manualParser.LanguageForPath(ToUtf8(m_currentPath));
    const dodev::symbols::RuntimeSettings settings = ManualSettings();
    if (language == dodev::symbols::Language::Unknown)
    {
        m_status->SetLabel("No manual symbol parser registered for this file type");
        return;
    }
    if (!dodev::symbols::ParserRegistry::IsLanguageEnabled(language, settings))
    {
        m_status->SetLabel(FromUtf8(dodev::symbols::LanguageName(language)) + " parser disabled in Settings");
        return;
    }

    m_lastManualAnalysis = m_manualParser.Parse(ToUtf8(m_currentPath), ToUtf8(m_currentContents), settings);
    if (!m_lastManualAnalysis.success)
    {
        m_status->SetLabel(FromUtf8(m_lastManualAnalysis.status));
        return;
    }

    if (settings.symbolsEnabled)
        PopulateManualSymbols(m_lastManualAnalysis);
    if (settings.callsEnabled)
        PopulateManualCalls(m_lastManualAnalysis);

    AppendOutput("Parser: dependency-free C++17 tokenizer/parser");
    AppendOutput("Language: " + FromUtf8(m_lastManualAnalysis.languageName));
    AppendOutput(wxString::Format("Symbols: %zu", m_lastManualAnalysis.symbols.size()));
    AppendOutput(wxString::Format("Calls: %zu", m_lastManualAnalysis.calls.size()));
    m_status->SetLabel(FromUtf8(m_lastManualAnalysis.status));
#else
    m_status->SetLabel("Manual source parser is disabled in this build");
#endif
}

#if DODEV_ENABLE_MANUAL_SYMBOLS
void SymbolTablePanel::PopulateManualSymbols(const dodev::symbols::AnalysisResult& result)
{
    using dodev::symbols::SymbolKind;
    m_symbolsTree->DeleteAllItems();
    const wxTreeItemId root = m_symbolsTree->AddRoot("Symbols");
    m_functionsRoot = m_symbolsTree->AppendItem(root, "Functions / Methods");
    m_typesRoot = m_symbolsTree->AppendItem(root, "Types / Namespaces / Packages");
    m_variablesRoot = m_symbolsTree->AppendItem(root, "Variables / Fields / Properties");
    m_macrosRoot = m_symbolsTree->AppendItem(root, "Macros");
    m_otherRoot = m_symbolsTree->AppendItem(root, "Other");

    for (const auto& symbol : result.symbols)
    {
        wxTreeItemId parent = m_otherRoot;
        switch (symbol.kind)
        {
        case SymbolKind::Function:
        case SymbolKind::Method:
        case SymbolKind::Constructor:
        case SymbolKind::Destructor: parent = m_functionsRoot; break;
        case SymbolKind::Class:
        case SymbolKind::Struct:
        case SymbolKind::Union:
        case SymbolKind::Interface:
        case SymbolKind::Object:
        case SymbolKind::Enum:
        case SymbolKind::Namespace:
        case SymbolKind::Package:
        case SymbolKind::TypeAlias: parent = m_typesRoot; break;
        case SymbolKind::Variable:
        case SymbolKind::Field:
        case SymbolKind::Property: parent = m_variablesRoot; break;
        case SymbolKind::Macro: parent = m_macrosRoot; break;
        default: break;
        }

        wxString display = FromUtf8(symbol.qualifiedName.empty() ? symbol.name : symbol.qualifiedName);
        const wxString label = wxString::Format("%s  [%s]  %u:%u",
                                                display.c_str(),
                                                ManualKindName(symbol.kind).c_str(),
                                                symbol.location.line,
                                                symbol.location.column);
        m_symbolsTree->AppendItem(parent, label, -1, -1,
            new LocationData(FromUtf8(symbol.location.file), symbol.location.line, symbol.location.column));
    }

    for (const wxTreeItemId& item : {m_functionsRoot, m_typesRoot, m_variablesRoot, m_macrosRoot})
        if (item.IsOk()) m_symbolsTree->Expand(item);
}

void SymbolTablePanel::PopulateManualCalls(const dodev::symbols::AnalysisResult& result)
{
    using dodev::symbols::SymbolKind;
    m_callsTree->DeleteAllItems();
    const wxTreeItemId root = m_callsTree->AddRoot("Call Hierarchy");

    std::map<std::string, const dodev::symbols::Symbol*> callableSymbols;
    for (const auto& symbol : result.symbols)
    {
        if (symbol.kind == SymbolKind::Function || symbol.kind == SymbolKind::Method ||
            symbol.kind == SymbolKind::Constructor || symbol.kind == SymbolKind::Destructor)
            callableSymbols[symbol.qualifiedName] = &symbol;
    }

    std::set<std::string> callableNames;
    for (const auto& entry : callableSymbols)
        callableNames.insert(entry.first);
    for (const auto& call : result.calls)
    {
        callableNames.insert(call.caller);
        if (call.target.IsValid() && !call.displayCallee.empty())
            callableNames.insert(call.displayCallee);
    }

    auto shortName = [](const std::string& name)
    {
        const std::size_t cpp = name.rfind("::");
        const std::size_t dot = name.rfind('.');
        if (cpp != std::string::npos && (dot == std::string::npos || cpp > dot))
            return name.substr(cpp + 2);
        if (dot != std::string::npos)
            return name.substr(dot + 1);
        return name;
    };

    for (const std::string& callableName : callableNames)
    {
        const dodev::symbols::Symbol* callableSymbol = nullptr;
        auto symbolIt = callableSymbols.find(callableName);
        if (symbolIt != callableSymbols.end())
            callableSymbol = symbolIt->second;

        LocationData* rootData = nullptr;
        if (callableSymbol && callableSymbol->location.IsValid())
            rootData = new LocationData(FromUtf8(callableSymbol->location.file), callableSymbol->location.line, callableSymbol->location.column);
        const wxTreeItemId callableItem = m_callsTree->AppendItem(root, FromUtf8(callableName), -1, -1, rootData);

        std::vector<const dodev::symbols::CallReference*> outgoing;
        std::vector<const dodev::symbols::CallReference*> incoming;
        for (const auto& call : result.calls)
        {
            if (call.caller == callableName)
                outgoing.push_back(&call);
            const std::string targetName = call.displayCallee.empty() ? call.callee : call.displayCallee;
            if (targetName == callableName || call.callee == shortName(callableName))
                incoming.push_back(&call);
        }

        const wxTreeItemId callees = m_callsTree->AppendItem(callableItem,
            wxString::Format("Callees (%zu)", outgoing.size()));
        for (const auto* call : outgoing)
        {
            const auto& nav = call->target.IsValid() ? call->target : call->callSite;
            wxString label = FromUtf8(call->displayCallee.empty() ? call->callee : call->displayCallee);
            if (!call->target.IsValid())
                label += "  [unresolved]";
            m_callsTree->AppendItem(callees, label, -1, -1,
                nav.IsValid() ? new LocationData(FromUtf8(nav.file), nav.line, nav.column) : nullptr);
        }

        const wxTreeItemId callers = m_callsTree->AppendItem(callableItem,
            wxString::Format("Callers (%zu)", incoming.size()));
        for (const auto* call : incoming)
        {
            const dodev::symbols::Symbol* callerSymbol = nullptr;
            auto callerIt = callableSymbols.find(call->caller);
            if (callerIt != callableSymbols.end())
                callerSymbol = callerIt->second;
            const auto& nav = callerSymbol ? callerSymbol->location : call->callSite;
            m_callsTree->AppendItem(callers, FromUtf8(call->caller), -1, -1,
                nav.IsValid() ? new LocationData(FromUtf8(nav.file), nav.line, nav.column) : nullptr);
        }

        m_callsTree->Expand(callableItem);
        if (!outgoing.empty()) m_callsTree->Expand(callees);
        if (!incoming.empty()) m_callsTree->Expand(callers);
    }
    m_callsTree->Expand(root);
}
#endif

void SymbolTablePanel::AppendOutput(const wxString& line)
{
    if (!m_output)
        return;
    if (!m_output->GetValue().IsEmpty())
        m_output->AppendText("\n");
    m_output->AppendText(line);
}

void SymbolTablePanel::ShowCallHierarchy()
{
    RefreshAnalysis();
    if (m_notebook && m_notebook->GetPageCount() > 1)
        m_notebook->SetSelection(1);
}

wxString SymbolTablePanel::BuildAIContext() const
{
    wxString text;
#if DODEV_ENABLE_MANUAL_SYMBOLS
    if (m_lastManualAnalysis.success)
    {
        text += "Manual source analysis\n";
        text += "File: " + m_currentPath + "\n";
        text += "Language: " + FromUtf8(m_lastManualAnalysis.languageName) + "\n";
        if (!m_lastManualAnalysis.symbols.empty())
        {
            text += "\nSymbols:\n";
            for (const auto& symbol : m_lastManualAnalysis.symbols)
            {
                text += "- " + ManualKindName(symbol.kind) + ": " +
                        FromUtf8(symbol.qualifiedName.empty() ? symbol.name : symbol.qualifiedName);
                if (symbol.location.IsValid())
                    text += wxString::Format(" @ %u:%u", symbol.location.line, symbol.location.column);
                text += "\n";
            }
        }
        if (!m_lastManualAnalysis.calls.empty())
        {
            text += "\nCalls:\n";
            for (const auto& call : m_lastManualAnalysis.calls)
            {
                text += "- " + FromUtf8(call.caller) + " -> " +
                        FromUtf8(call.displayCallee.empty() ? call.callee : call.displayCallee);
                if (!call.target.IsValid())
                    text += " [unresolved]";
                text += "\n";
            }
        }
    }
#endif
    return text;
}

bool SymbolTablePanel::NavigateToDefinition(unsigned line, unsigned column)
{
#if DODEV_ENABLE_MANUAL_SYMBOLS
    if (IsManualParsingEnabled() && m_manualParser.SupportsFile(ToUtf8(m_currentPath)))
    {
        const dodev::symbols::RuntimeSettings settings = ManualSettings();
        const dodev::symbols::Language language = m_manualParser.LanguageForPath(ToUtf8(m_currentPath));
        if (dodev::symbols::ParserRegistry::IsLanguageEnabled(language, settings))
            m_lastManualAnalysis = m_manualParser.Parse(ToUtf8(m_currentPath), ToUtf8(m_currentContents), settings);
    }

    if (!m_lastManualAnalysis.success)
        return false;

    const wxString word = WordAt(m_currentContents, line, column);
    if (word.IsEmpty())
        return false;

    const std::string utf8Word = ToUtf8(word);
    for (const auto& call : m_lastManualAnalysis.calls)
    {
        if (call.callSite.line == line && call.callee == utf8Word && call.target.IsValid())
        {
            if (m_openLocation)
                m_openLocation(FromUtf8(call.target.file), call.target.line, call.target.column);
            return true;
        }
    }

    const dodev::symbols::Symbol* selected = nullptr;
    for (const auto& symbol : m_lastManualAnalysis.symbols)
    {
        if (symbol.name != utf8Word)
            continue;
        if (!selected || (symbol.definition && !selected->definition))
            selected = &symbol;
    }
    if (selected && selected->location.IsValid())
    {
        if (m_openLocation)
            m_openLocation(FromUtf8(selected->location.file), selected->location.line, selected->location.column);
        return true;
    }
#else
    (void)line;
    (void)column;
#endif
    return false;
}

void SymbolTablePanel::OpenTreeItem(wxTreeCtrl* tree, const wxTreeItemId& item)
{
    if (!item.IsOk() || !m_openLocation)
        return;
    auto* data = dynamic_cast<LocationData*>(tree->GetItemData(item));
    if (!data || data->m_file.IsEmpty())
        return;
    m_openLocation(data->m_file, data->m_line, data->m_column);
}

void SymbolTablePanel::UpdateAvailabilityUI()
{
#if DODEV_ENABLE_MANUAL_SYMBOLS
    m_manualEnableCheck->Enable(true);
    m_manualEnableCheck->SetValue(m_manualRuntimeSettings.enabled);
    m_status->SetLabel(m_manualRuntimeSettings.enabled
        ? "Manual C/C++/Kotlin parser ready"
        : "Manual parser disabled in Settings");
#else
    m_manualEnableCheck->Enable(false);
    m_manualEnableCheck->SetValue(false);
    m_status->SetLabel("Manual source parser is disabled in this build");
#endif
}

void SymbolTablePanel::AddFunction(const wxString& name)
{
    if (!m_functionsRoot.IsOk())
    {
        m_symbolsTree->DeleteAllItems();
        const wxTreeItemId root = m_symbolsTree->AddRoot("Symbols");
        m_functionsRoot = m_symbolsTree->AppendItem(root, "Functions / Methods");
        m_typesRoot = m_symbolsTree->AppendItem(root, "Types / Namespaces");
        m_variablesRoot = m_symbolsTree->AppendItem(root, "Variables / Fields");
        m_macrosRoot = m_symbolsTree->AppendItem(root, "Macros");
        m_otherRoot = m_symbolsTree->AppendItem(root, "Other");
    }
    if (!name.IsEmpty())
        m_symbolsTree->AppendItem(m_functionsRoot, name);
}

void SymbolTablePanel::AddVariable(const wxString& name)
{
    if (!m_variablesRoot.IsOk())
        AddFunction(wxString());
    if (!name.IsEmpty())
        m_symbolsTree->AppendItem(m_variablesRoot, name);
}

void SymbolTablePanel::AddMacro(const wxString& name)
{
    if (!m_macrosRoot.IsOk())
        AddFunction(wxString());
    if (!name.IsEmpty())
        m_symbolsTree->AppendItem(m_macrosRoot, name);
}

void SymbolTablePanel::Clear()
{
#if DODEV_ENABLE_MANUAL_SYMBOLS
    m_lastManualAnalysis = dodev::symbols::AnalysisResult();
#endif
    if (m_symbolsTree)
        m_symbolsTree->DeleteAllItems();
    if (m_callsTree)
        m_callsTree->DeleteAllItems();
    if (m_output)
        m_output->Clear();
    m_functionsRoot = wxTreeItemId();
    m_typesRoot = wxTreeItemId();
    m_variablesRoot = wxTreeItemId();
    m_macrosRoot = wxTreeItemId();
    m_otherRoot = wxTreeItemId();
}
