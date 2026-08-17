#include "SymbolTablePanel.h"

#include <wx/artprov.h>
#include <wx/filename.h>
#include <wx/filefn.h>

#include <json/json.h>

#include <algorithm>
#include <fstream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <utility>

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
        : m_file(std::move(file)), m_line(line), m_column(column)
    {
    }

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

    m_manualEnableCheck = new wxCheckBox(this, wxID_ANY, "Manual");
    m_manualEnableCheck->SetForegroundColour(wxColour(220, 220, 220));
#if DODEV_ENABLE_MANUAL_SYMBOLS
    m_manualEnableCheck->SetValue(m_manualRuntimeSettings.enabled);
#else
    m_manualEnableCheck->SetValue(false);
    m_manualEnableCheck->Enable(false);
#endif
    headerRow->Add(m_manualEnableCheck, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 8);

    m_enableCheck = new wxCheckBox(this, wxID_ANY, "LLVM");
    m_enableCheck->SetForegroundColour(wxColour(220, 220, 220));
    m_enableCheck->SetValue(CppAnalysisEngine::HasLibClang());
    headerRow->Add(m_enableCheck, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 8);
    root->Add(headerRow, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 8);

    m_status = new wxStaticText(this, wxID_ANY, "");
    m_status->SetForegroundColour(wxColour(150, 150, 150));
    root->Add(m_status, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 8);

    auto* actionRow = new wxBoxSizer(wxHORIZONTAL);
    auto* refreshButton = new wxButton(this, wxID_ANY, "Parse", wxDefaultPosition, wxSize(62, -1));
    auto* syntaxButton = new wxButton(this, wxID_ANY, "Check", wxDefaultPosition, wxSize(62, -1));
    auto* compileButton = new wxButton(this, wxID_ANY, "Compile", wxDefaultPosition, wxSize(70, -1));
    auto* saveConfigButton = new wxButton(this, wxID_ANY, "Save cfg", wxDefaultPosition, wxSize(72, -1));
    actionRow->Add(refreshButton, 0, wxRIGHT, 4);
    actionRow->Add(syntaxButton, 0, wxRIGHT, 4);
    actionRow->Add(compileButton, 0, wxRIGHT, 4);
    actionRow->Add(saveConfigButton, 0);
    root->Add(actionRow, 0, wxEXPAND | wxALL, 8);

    auto* settings = new wxFlexGridSizer(2, 5, 5);
    settings->AddGrowableCol(1, 1);

    auto makeLabel = [this](const wxString& text)
    {
        auto* label = new wxStaticText(this, wxID_ANY, text);
        label->SetForegroundColour(wxColour(180, 180, 180));
        return label;
    };

    settings->Add(makeLabel("Standard"), 0, wxALIGN_CENTER_VERTICAL);
    m_standardChoice = new wxChoice(this, wxID_ANY);
    m_standardChoice->Append("c++17");
    m_standardChoice->Append("c++20");
    m_standardChoice->Append("c++23");
    m_standardChoice->Append("c17");
    m_standardChoice->Append("c11");
    m_standardChoice->SetSelection(0);
    settings->Add(m_standardChoice, 1, wxEXPAND);

    settings->Add(makeLabel("Defines"), 0, wxALIGN_CENTER_VERTICAL);
    m_defines = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
    m_defines->SetHint("DEBUG;PLATFORM_LINUX=1");
    settings->Add(m_defines, 1, wxEXPAND);

    settings->Add(makeLabel("Includes"), 0, wxALIGN_CENTER_VERTICAL);
    m_includeDirs = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
    m_includeDirs->SetHint("include;thirdparty/foo/include");
    settings->Add(m_includeDirs, 1, wxEXPAND);

    settings->Add(makeLabel("Extra args"), 0, wxALIGN_CENTER_VERTICAL);
    m_extraArgs = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
    m_extraArgs->SetHint("-Wall;-Wno-unused-parameter");
    settings->Add(m_extraArgs, 1, wxEXPAND);

    settings->Add(makeLabel("LLVM Auto"), 0, wxALIGN_CENTER_VERTICAL);
    m_autoCheck = new wxCheckBox(this, wxID_ANY, "Parse C/C++ with LLVM on file/tab change");
    m_autoCheck->SetForegroundColour(wxColour(200, 200, 200));
    m_autoCheck->SetValue(true);
    settings->Add(m_autoCheck, 1, wxEXPAND);

    root->Add(settings, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

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
    syntaxButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { CompileCurrent(true); });
    compileButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { CompileCurrent(false); });
    saveConfigButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { SaveProjectConfig(); });

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

    m_enableCheck->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&)
    {
        if (m_enableCheck->GetValue() && !CppAnalysisEngine::HasLibClang())
        {
            m_enableCheck->SetValue(false);
            wxMessageBox("This DoDevEditor build was compiled without libclang.\n\n"
                         "Install LLVM/Clang development files and rebuild with:\n"
                         "  ./scripts/build-linux.sh release --clean --llvm",
                         "LLVM analysis unavailable", wxOK | wxICON_INFORMATION, this);
        }
        UpdateAvailabilityUI();
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

void SymbolTablePanel::SetSaveCurrentFileCallback(SaveCurrentFileCallback callback)
{
    m_saveCurrentFile = std::move(callback);
}

void SymbolTablePanel::SetProjectRoot(const wxString& root)
{
    if (m_projectRoot == root)
        return;
    m_projectRoot = root;
    LoadProjectConfig();
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

    bool manualSupported = false;
#if DODEV_ENABLE_MANUAL_SYMBOLS
    manualSupported = m_manualParser.SupportsFile(ToUtf8(path));
#endif
    const bool llvmSupported = CppAnalysisEngine::IsCOrCppFile(path);
    if (!manualSupported && !llvmSupported)
    {
        Clear();
        m_status->SetLabel(path.IsEmpty() ? "No document" : "No symbol parser registered for this file type");
        return;
    }

    m_status->SetLabel(wxFileName(path).GetFullName());
    const bool manualAuto = manualSupported && IsManualParsingEnabled() && m_manualRuntimeSettings.autoParse;
    const bool llvmAuto = llvmSupported && IsLLVMEnabled() && m_autoCheck->GetValue();
    if (autoRefresh && (manualAuto || llvmAuto))
        RefreshAnalysis();
}

bool SymbolTablePanel::IsLLVMEnabled() const
{
    return CppAnalysisEngine::HasLibClang() && m_enableCheck && m_enableCheck->GetValue();
}

bool SymbolTablePanel::IsManualParsingEnabled() const
{
#if DODEV_ENABLE_MANUAL_SYMBOLS
    return m_manualRuntimeSettings.enabled && m_manualEnableCheck && m_manualEnableCheck->GetValue();
#else
    return false;
#endif
}

wxString SymbolTablePanel::LLVMStatus() const
{
    return CppAnalysisEngine::LibClangVersion();
}

std::vector<wxString> SymbolTablePanel::ParseList(const wxString& value)
{
    std::vector<wxString> result;
    wxString token;
    auto flush = [&]()
    {
        token.Trim(true).Trim(false);
        if (!token.IsEmpty())
            result.push_back(token);
        token.clear();
    };

    for (size_t i = 0; i < value.length(); ++i)
    {
        const wxChar c = value[i];
        if (c == ';' || c == '\n' || c == '\r')
        {
            flush();
            continue;
        }
        token += c;
    }
    flush();
    return result;
}

wxString SymbolTablePanel::JoinList(const std::vector<wxString>& values)
{
    wxString result;
    for (const auto& value : values)
    {
        if (!result.IsEmpty())
            result += ";";
        result += value;
    }
    return result;
}

CppAnalysisOptions SymbolTablePanel::CurrentOptions() const
{
    CppAnalysisOptions options;
    options.projectRoot = m_projectRoot;
    if (m_standardChoice->GetSelection() != wxNOT_FOUND)
        options.standard = m_standardChoice->GetStringSelection();
    options.defines = ParseList(m_defines->GetValue());
    options.includeDirs = ParseList(m_includeDirs->GetValue());
    options.extraArgs = ParseList(m_extraArgs->GetValue());
    return options;
}

wxString SymbolTablePanel::KindName(CppSymbolKind kind)
{
    switch (kind)
    {
    case CppSymbolKind::Function: return "function";
    case CppSymbolKind::Method: return "method";
    case CppSymbolKind::Constructor: return "constructor";
    case CppSymbolKind::Destructor: return "destructor";
    case CppSymbolKind::Class: return "class";
    case CppSymbolKind::Struct: return "struct";
    case CppSymbolKind::Enum: return "enum";
    case CppSymbolKind::Namespace: return "namespace";
    case CppSymbolKind::Variable: return "variable";
    case CppSymbolKind::Field: return "field";
    case CppSymbolKind::Typedef: return "type";
    case CppSymbolKind::Macro: return "macro";
    default: return "symbol";
    }
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
    bool manualRan = false;
    bool llvmRan = false;
    wxString manualStatus;
    wxString llvmStatus;

#if DODEV_ENABLE_MANUAL_SYMBOLS
    if (IsManualParsingEnabled())
    {
        const dodev::symbols::Language language = m_manualParser.LanguageForPath(ToUtf8(m_currentPath));
        const dodev::symbols::RuntimeSettings settings = ManualSettings();
        if (language != dodev::symbols::Language::Unknown &&
            dodev::symbols::ParserRegistry::IsLanguageEnabled(language, settings))
        {
            m_lastManualAnalysis = m_manualParser.Parse(ToUtf8(m_currentPath), ToUtf8(m_currentContents), settings);
            manualRan = m_lastManualAnalysis.success;
            manualStatus = FromUtf8(m_lastManualAnalysis.status);
            if (manualRan)
            {
                if (settings.symbolsEnabled)
                    PopulateManualSymbols(m_lastManualAnalysis);
                if (settings.callsEnabled)
                    PopulateManualCalls(m_lastManualAnalysis);
                m_output->Clear();
                AppendOutput("Manual parser: dependency-free C++17 tokenizer/parser");
                AppendOutput("Language: " + FromUtf8(m_lastManualAnalysis.languageName));
                AppendOutput(wxString::Format("Symbols: %zu", m_lastManualAnalysis.symbols.size()));
                AppendOutput(wxString::Format("Calls: %zu", m_lastManualAnalysis.calls.size()));
            }
        }
        else if (language != dodev::symbols::Language::Unknown)
        {
            manualStatus = FromUtf8(dodev::symbols::LanguageName(language)) + " parser disabled in Settings";
        }
    }
#endif

    if (CppAnalysisEngine::IsCOrCppFile(m_currentPath) && IsLLVMEnabled())
    {
        const CppAnalysisResult result = m_engine.Parse(m_currentPath, m_currentContents, CurrentOptions());
        m_lastAnalysis = result;
        llvmRan = result.success;
        llvmStatus = result.status;
        if (!manualRan)
        {
            PopulateSymbols(result);
            PopulateCalls(result);
        }
        if (!m_output->GetValue().IsEmpty())
        {
            AppendOutput("");
            AppendOutput("LLVM / libclang:");
            AppendOutput(CppAnalysisEngine::LibClangVersion());
            for (const wxString& diagnostic : result.diagnostics)
                AppendOutput(diagnostic);
        }
        else
        {
            ShowDiagnostics(result);
        }
    }

    if (manualRan && llvmRan)
        m_status->SetLabel(manualStatus + " | LLVM ready");
    else if (manualRan)
        m_status->SetLabel(manualStatus);
    else if (llvmRan)
        m_status->SetLabel(llvmStatus);
    else if (!manualStatus.IsEmpty())
        m_status->SetLabel(manualStatus);
    else if (!m_currentPath.IsEmpty())
        m_status->SetLabel("Analysis disabled for this file. Check Settings > Code Analysis.");
    else
        UpdateAvailabilityUI();
}

void SymbolTablePanel::PopulateSymbols(const CppAnalysisResult& result)
{
    m_symbolsTree->DeleteAllItems();
    const wxTreeItemId root = m_symbolsTree->AddRoot("Symbols");
    m_functionsRoot = m_symbolsTree->AppendItem(root, "Functions / Methods");
    m_typesRoot = m_symbolsTree->AppendItem(root, "Types / Namespaces");
    m_variablesRoot = m_symbolsTree->AppendItem(root, "Variables / Fields");
    m_macrosRoot = m_symbolsTree->AppendItem(root, "Macros");
    m_otherRoot = m_symbolsTree->AppendItem(root, "Other");

    for (const auto& symbol : result.symbols)
    {
        wxTreeItemId parent = m_otherRoot;
        switch (symbol.kind)
        {
        case CppSymbolKind::Function:
        case CppSymbolKind::Method:
        case CppSymbolKind::Constructor:
        case CppSymbolKind::Destructor: parent = m_functionsRoot; break;
        case CppSymbolKind::Class:
        case CppSymbolKind::Struct:
        case CppSymbolKind::Enum:
        case CppSymbolKind::Namespace:
        case CppSymbolKind::Typedef: parent = m_typesRoot; break;
        case CppSymbolKind::Variable:
        case CppSymbolKind::Field: parent = m_variablesRoot; break;
        case CppSymbolKind::Macro: parent = m_macrosRoot; break;
        default: break;
        }

        const wxString label = wxString::Format("%s  [%s]  %u:%u",
                                                symbol.displayName.c_str(),
                                                KindName(symbol.kind).c_str(),
                                                symbol.location.line,
                                                symbol.location.column);
        m_symbolsTree->AppendItem(parent, label, -1, -1,
            new LocationData(symbol.location.file, symbol.location.line, symbol.location.column));
    }

    m_symbolsTree->Expand(m_functionsRoot);
    m_symbolsTree->Expand(m_typesRoot);
    m_symbolsTree->Expand(m_variablesRoot);
    m_symbolsTree->Expand(m_macrosRoot);
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

void SymbolTablePanel::PopulateCalls(const CppAnalysisResult& result)
{
    m_callsTree->DeleteAllItems();
    const wxTreeItemId root = m_callsTree->AddRoot("Calls");
    for (const auto& callRoot : result.callTree)
        AppendCallNode(root, callRoot);

    m_callsTree->Expand(root);
    wxTreeItemIdValue cookie;
    wxTreeItemId child = m_callsTree->GetFirstChild(root, cookie);
    while (child.IsOk())
    {
        m_callsTree->Expand(child);
        child = m_callsTree->GetNextChild(root, cookie);
    }
}

void SymbolTablePanel::AppendCallNode(wxTreeItemId parent, const CppCallNode& node)
{
    CppSourceLocation navigation = node.target.IsValid() ? node.target : node.callSite;
    wxString locationText;
    if (navigation.IsValid())
        locationText = wxString::Format("  %s:%u", wxFileName(navigation.file).GetFullName().c_str(), navigation.line);

    const wxTreeItemId item = m_callsTree->AppendItem(parent,
        node.name + locationText, -1, -1,
        navigation.IsValid() ? new LocationData(navigation.file, navigation.line, navigation.column) : nullptr);
    for (const auto& child : node.children)
        AppendCallNode(item, child);
}

void SymbolTablePanel::ShowDiagnostics(const CppAnalysisResult& result)
{
    m_output->Clear();
    AppendOutput("libclang: " + CppAnalysisEngine::LibClangVersion());
    AppendOutput("file: " + m_currentPath);
    if (!result.diagnostics.empty())
    {
        AppendOutput("");
        AppendOutput("Diagnostics:");
        for (const auto& diagnostic : result.diagnostics)
            AppendOutput(diagnostic);
    }
    if (!result.success && !result.status.IsEmpty())
    {
        AppendOutput("");
        AppendOutput(result.status);
    }
}

void SymbolTablePanel::AppendOutput(const wxString& line)
{
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

void SymbolTablePanel::CompileCurrent(bool syntaxOnly)
{
    if (!CppAnalysisEngine::IsCOrCppFile(m_currentPath))
    {
        wxMessageBox("Open a C/C++ source file first.", "C/C++ compile",
                     wxOK | wxICON_INFORMATION, this);
        return;
    }

    if (m_saveCurrentFile && !m_saveCurrentFile())
    {
        AppendOutput("Compilation cancelled because the current file could not be saved.");
        m_notebook->SetSelection(2);
        return;
    }

    SaveProjectConfig();
    const CppCompileResult result = m_engine.Compile(m_currentPath, CurrentOptions(), syntaxOnly);
    m_output->Clear();
    for (const auto& line : result.output)
        AppendOutput(line);
    m_notebook->SetSelection(2);
    m_status->SetLabel(result.success ? "C/C++ compilation succeeded" : "C/C++ compilation failed");
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

    if (IsLLVMEnabled() && !m_lastAnalysis.status.IsEmpty())
    {
        if (!text.IsEmpty())
            text += "\n";
        text += "LLVM/libclang analysis\n";
        text += "Parser: " + CppAnalysisEngine::LibClangVersion() + "\n";
        if (!m_lastAnalysis.diagnostics.empty())
        {
            text += "Diagnostics:\n";
            for (const wxString& diagnostic : m_lastAnalysis.diagnostics)
                text += "- " + diagnostic + "\n";
        }
    }
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
    if (m_lastManualAnalysis.success)
    {
        const wxString word = WordAt(m_currentContents, line, column);
        if (!word.IsEmpty())
        {
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
        }
    }
#endif

    if (!IsLLVMEnabled())
        return false;
    if (!CppAnalysisEngine::IsCOrCppFile(m_currentPath))
        return false;

    CppSourceLocation location;
    wxString diagnostic;
    if (!m_engine.FindDefinition(m_currentPath, m_currentContents, line, column,
                                 CurrentOptions(), location, diagnostic))
    {
        if (!diagnostic.IsEmpty())
            m_status->SetLabel(diagnostic);
        return false;
    }

    if (m_openLocation)
        m_openLocation(location.file, location.line, location.column);
    return true;
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
    const bool llvmAvailable = CppAnalysisEngine::HasLibClang();
    m_enableCheck->Enable(llvmAvailable);
    if (!llvmAvailable)
        m_enableCheck->SetValue(false);

#if DODEV_ENABLE_MANUAL_SYMBOLS
    m_manualEnableCheck->Enable(true);
    m_manualEnableCheck->SetValue(m_manualRuntimeSettings.enabled);
    if (m_manualRuntimeSettings.enabled)
        m_status->SetLabel("Manual C/C++/Kotlin parser ready" +
                           (llvmAvailable ? wxString("; LLVM available") : wxString("; LLVM not linked")));
    else
        m_status->SetLabel("Manual parser disabled in Settings" +
                           (llvmAvailable ? wxString("; LLVM available") : wxString()));
#else
    m_manualEnableCheck->Enable(false);
    m_manualEnableCheck->SetValue(false);
    if (llvmAvailable)
        m_status->SetLabel("Manual parser compiled out; LLVM available for C/C++");
    else
        m_status->SetLabel("Manual parser compiled out and libclang is not linked");
#endif
}

wxString SymbolTablePanel::ConfigPath() const
{
    if (m_projectRoot.IsEmpty())
        return wxString();
    wxString path = m_projectRoot;
    path += wxFileName::GetPathSeparator();
    path += ".dodev";
    path += wxFileName::GetPathSeparator();
    path += "llvm.json";
    return path;
}

void SymbolTablePanel::LoadProjectConfig()
{
    const wxString path = ConfigPath();
    if (path.IsEmpty() || !wxFileExists(path))
    {
        m_defines->Clear();
        m_includeDirs->Clear();
        m_extraArgs->Clear();
        m_standardChoice->SetStringSelection("c++17");
        m_autoCheck->SetValue(true);
        m_enableCheck->SetValue(CppAnalysisEngine::HasLibClang());
        ReloadRuntimeSettings();
        return;
    }

    std::ifstream input(path.ToStdString());
    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errors;
    if (!Json::parseFromStream(builder, input, &root, &errors))
    {
        AppendOutput("Could not parse " + path + ": " + wxString::FromUTF8(errors.c_str()));
        return;
    }

    const wxString standard = wxString::FromUTF8(root.get("standard", "c++17").asString().c_str());
    if (!m_standardChoice->SetStringSelection(standard))
        m_standardChoice->SetStringSelection("c++17");

    auto readArray = [&root](const char* key)
    {
        std::vector<wxString> values;
        for (const auto& item : root[key])
            values.push_back(wxString::FromUTF8(item.asString().c_str()));
        return values;
    };

    m_defines->SetValue(JoinList(readArray("defines")));
    m_includeDirs->SetValue(JoinList(readArray("includeDirs")));
    m_extraArgs->SetValue(JoinList(readArray("extraArgs")));
    if (root.isMember("autoParse"))
        m_autoCheck->SetValue(root["autoParse"].asBool());
    if (root.isMember("enabled") && CppAnalysisEngine::HasLibClang())
        m_enableCheck->SetValue(root["enabled"].asBool());
    m_manualRuntimeSettings = AppEditorConfig::GetManualSymbolRuntimeConfig();
#if DODEV_ENABLE_MANUAL_SYMBOLS
    m_manualEnableCheck->SetValue(m_manualRuntimeSettings.enabled);
#endif
    UpdateAvailabilityUI();
}

void SymbolTablePanel::SaveProjectConfig()
{
    const wxString path = ConfigPath();
    if (path.IsEmpty())
        return;

    wxFileName configFile(path);
    wxFileName::Mkdir(configFile.GetPath(), wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);

    const CppAnalysisOptions options = CurrentOptions();
    Json::Value root;
    root["standard"] = options.standard.ToStdString();
    root["enabled"] = m_enableCheck->GetValue();
    root["autoParse"] = m_autoCheck->GetValue();
    for (const auto& value : options.defines)
        root["defines"].append(value.ToStdString());
    for (const auto& value : options.includeDirs)
        root["includeDirs"].append(value.ToStdString());
    for (const auto& value : options.extraArgs)
        root["extraArgs"].append(value.ToStdString());

    std::ofstream output(path.ToStdString());
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "  ";
    std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
    writer->write(root, &output);
    output << '\n';
    if (output.good())
        m_status->SetLabel("Saved " + path);
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
    m_lastAnalysis = CppAnalysisResult();
#if DODEV_ENABLE_MANUAL_SYMBOLS
    m_lastManualAnalysis = dodev::symbols::AnalysisResult();
#endif
    if (m_symbolsTree)
        m_symbolsTree->DeleteAllItems();
    if (m_callsTree)
        m_callsTree->DeleteAllItems();
    m_functionsRoot = wxTreeItemId();
    m_typesRoot = wxTreeItemId();
    m_variablesRoot = wxTreeItemId();
    m_macrosRoot = wxTreeItemId();
    m_otherRoot = wxTreeItemId();
}
