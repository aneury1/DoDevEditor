#include "SymbolTablePanel.h"

#include <wx/artprov.h>
#include <wx/filename.h>
#include <wx/filefn.h>

#include <json/json.h>

#include <fstream>
#include <memory>
#include <sstream>
#include <utility>

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
    : wxPanel(parent, wxID_ANY)
{
    BuildUI();
    UpdateAvailabilityUI();
}

void SymbolTablePanel::BuildUI()
{
    SetBackgroundColour(wxColour(30, 30, 30));
    auto* root = new wxBoxSizer(wxVERTICAL);

    auto* headerRow = new wxBoxSizer(wxHORIZONTAL);
    auto* header = new wxStaticText(this, wxID_ANY, "C/C++ ANALYSIS");
    header->SetForegroundColour(wxColour(200, 200, 200));
    wxFont headerFont = header->GetFont();
    headerFont.SetWeight(wxFONTWEIGHT_BOLD);
    headerFont.SetPointSize(9);
    header->SetFont(headerFont);
    headerRow->Add(header, 1, wxALIGN_CENTER_VERTICAL);

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
    m_defines = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                               wxTE_PROCESS_ENTER);
    m_defines->SetHint("DEBUG;PLATFORM_LINUX=1");
    settings->Add(m_defines, 1, wxEXPAND);

    settings->Add(makeLabel("Includes"), 0, wxALIGN_CENTER_VERTICAL);
    m_includeDirs = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                                   wxTE_PROCESS_ENTER);
    m_includeDirs->SetHint("include;thirdparty/foo/include");
    settings->Add(m_includeDirs, 1, wxEXPAND);

    settings->Add(makeLabel("Extra args"), 0, wxALIGN_CENTER_VERTICAL);
    m_extraArgs = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                                 wxTE_PROCESS_ENTER);
    m_extraArgs->SetHint("-Wall;-Wno-unused-parameter");
    settings->Add(m_extraArgs, 1, wxEXPAND);

    settings->Add(makeLabel("Auto"), 0, wxALIGN_CENTER_VERTICAL);
    m_autoCheck = new wxCheckBox(this, wxID_ANY, "Parse on file/tab change");
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
    m_notebook->AddPage(m_callsTree, "Calls", false);
    m_notebook->AddPage(m_output, "LLVM", false);
    root->Add(m_notebook, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6);

    SetSizer(root);

    refreshButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { RefreshAnalysis(); });
    syntaxButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { CompileCurrent(true); });
    compileButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { CompileCurrent(false); });
    saveConfigButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { SaveProjectConfig(); });

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

void SymbolTablePanel::SetCurrentDocument(const wxString& path,
                                          const wxString& contents,
                                          bool autoRefresh)
{
    m_currentPath = path;
    m_currentContents = contents;

    if (!CppAnalysisEngine::IsCOrCppFile(path))
    {
        Clear();
        m_status->SetLabel(path.IsEmpty() ? "No document" : "Current document is not C/C++");
        return;
    }

    m_status->SetLabel(wxFileName(path).GetFullName());
    if (autoRefresh && m_autoCheck->GetValue() && IsLLVMEnabled())
        RefreshAnalysis();
}

bool SymbolTablePanel::IsLLVMEnabled() const
{
    return CppAnalysisEngine::HasLibClang() && m_enableCheck && m_enableCheck->GetValue();
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

void SymbolTablePanel::RefreshAnalysis()
{
    Clear();
    if (!IsLLVMEnabled())
    {
        UpdateAvailabilityUI();
        return;
    }
    if (!CppAnalysisEngine::IsCOrCppFile(m_currentPath))
    {
        m_status->SetLabel("Open a C/C++ file to parse symbols");
        return;
    }

    m_status->SetLabel("Parsing " + wxFileName(m_currentPath).GetFullName() + " ...");
    const CppAnalysisResult result = m_engine.Parse(m_currentPath,
                                                    m_currentContents,
                                                    CurrentOptions());
    m_lastAnalysis = result;
    PopulateSymbols(result);
    PopulateCalls(result);
    ShowDiagnostics(result);
    m_status->SetLabel(result.status);
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
        case CppSymbolKind::Destructor:
            parent = m_functionsRoot;
            break;
        case CppSymbolKind::Class:
        case CppSymbolKind::Struct:
        case CppSymbolKind::Enum:
        case CppSymbolKind::Namespace:
        case CppSymbolKind::Typedef:
            parent = m_typesRoot;
            break;
        case CppSymbolKind::Variable:
        case CppSymbolKind::Field:
            parent = m_variablesRoot;
            break;
        case CppSymbolKind::Macro:
            parent = m_macrosRoot;
            break;
        default:
            break;
        }

        const wxString label = wxString::Format("%s  [%s]  %u:%u",
                                                symbol.displayName.c_str(),
                                                KindName(symbol.kind).c_str(),
                                                symbol.location.line,
                                                symbol.location.column);
        m_symbolsTree->AppendItem(parent, label, -1, -1,
            new LocationData(symbol.location.file,
                             symbol.location.line,
                             symbol.location.column));
    }

    m_symbolsTree->Expand(m_functionsRoot);
    m_symbolsTree->Expand(m_typesRoot);
    m_symbolsTree->Expand(m_variablesRoot);
    m_symbolsTree->Expand(m_macrosRoot);
}

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
        locationText = wxString::Format("  %s:%u", wxFileName(navigation.file).GetFullName().c_str(),
                                        navigation.line);

    const wxTreeItemId item = m_callsTree->AppendItem(parent,
        node.name + locationText,
        -1,
        -1,
        navigation.IsValid()
            ? new LocationData(navigation.file, navigation.line, navigation.column)
            : nullptr);

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

void SymbolTablePanel::CompileCurrent(bool syntaxOnly)
{
    if (!CppAnalysisEngine::IsCOrCppFile(m_currentPath))
    {
        wxMessageBox("Open a C/C++ source file first.", "LLVM compile",
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
    m_status->SetLabel(result.success ? "Clang compilation succeeded" : "Clang compilation failed");
}


wxString SymbolTablePanel::BuildAIContext() const
{
    if (!IsLLVMEnabled() || m_currentPath.IsEmpty())
        return wxString();

    wxString text;
    text += "File: " + m_currentPath + "\n";
    text += "Parser: " + CppAnalysisEngine::LibClangVersion() + "\n";
    if (!m_lastAnalysis.status.IsEmpty())
        text += "Status: " + m_lastAnalysis.status + "\n";

    if (!m_lastAnalysis.symbols.empty())
    {
        text += "\nSymbols:\n";
        for (const auto& symbol : m_lastAnalysis.symbols)
        {
            text += "- " + KindName(symbol.kind) + ": " + symbol.displayName;
            if (symbol.location.IsValid())
                text += wxString::Format(" @ %u:%u", symbol.location.line, symbol.location.column);
            text += "\n";
        }
    }

    std::function<void(const CppCallNode&, int)> appendCall;
    appendCall = [&text, &appendCall](const CppCallNode& node, int depth)
    {
        wxString indent;
        indent.Pad(static_cast<size_t>(depth * 2), ' ');
        text += indent;
        text += "- " + node.name;
        const CppSourceLocation location = node.target.IsValid() ? node.target : node.callSite;
        if (location.IsValid())
            text += wxString::Format(" @ %s:%u", wxFileName(location.file).GetFullName().c_str(), location.line);
        text += "\n";
        for (const auto& child : node.children)
            appendCall(child, depth + 1);
    };

    if (!m_lastAnalysis.callTree.empty())
    {
        text += "\nCall tree:\n";
        for (const auto& root : m_lastAnalysis.callTree)
            appendCall(root, 0);
    }

    if (!m_lastAnalysis.diagnostics.empty())
    {
        text += "\nDiagnostics:\n";
        for (const wxString& diagnostic : m_lastAnalysis.diagnostics)
            text += "- " + diagnostic + "\n";
    }
    return text;
}

bool SymbolTablePanel::NavigateToDefinition(unsigned line, unsigned column)
{
    if (!IsLLVMEnabled())
    {
        wxMessageBox("Go to Definition requires optional libclang support.",
                     "LLVM analysis", wxOK | wxICON_INFORMATION, this);
        return false;
    }
    if (!CppAnalysisEngine::IsCOrCppFile(m_currentPath))
        return false;

    CppSourceLocation location;
    wxString diagnostic;
    if (!m_engine.FindDefinition(m_currentPath,
                                 m_currentContents,
                                 line,
                                 column,
                                 CurrentOptions(),
                                 location,
                                 diagnostic))
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
    const bool available = CppAnalysisEngine::HasLibClang();
    m_enableCheck->Enable(available);
    if (!available)
    {
        m_enableCheck->SetValue(false);
        m_status->SetLabel("libclang optional feature: not linked. Build with --llvm to enable parsing/call tree/F12.");
    }
    else if (!m_enableCheck->GetValue())
    {
        m_status->SetLabel("LLVM analysis disabled for this session");
    }
    else
    {
        m_status->SetLabel(CppAnalysisEngine::LibClangVersion());
    }
}

wxString SymbolTablePanel::ConfigPath() const
{
    if (m_projectRoot.IsEmpty())
        return wxString();
    return m_projectRoot + wxFileName::GetPathSeparator() + ".dodev" +
           wxFileName::GetPathSeparator() + "llvm.json";
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
        UpdateAvailabilityUI();
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
