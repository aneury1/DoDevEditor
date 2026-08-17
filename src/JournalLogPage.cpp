#include "JournalLogPage.h"

#if DODEV_ENABLE_JOURNAL_LOGS

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/clipbrd.h>
#include <wx/dataobj.h>
#include <wx/datetime.h>
#include <wx/filedlg.h>
#include <wx/filepicker.h>
#include <wx/filename.h>
#include <wx/ffile.h>
#include <wx/listctrl.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/process.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/utils.h>
#include <wx/wfstream.h>
#include <wx/stream.h>

#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <memory>
#include <sstream>
#include <utility>

namespace
{
constexpr int kPollIntervalMs = 100;

wxColour PanelBackground()
{
    return wxColour(30, 30, 30);
}

wxColour FieldBackground()
{
    return wxColour(37, 37, 38);
}

wxColour Foreground()
{
    return wxColour(212, 212, 212);
}

wxString FlattenTextForExport(wxString text)
{
    text.Replace("\r", "");
    text.Replace("\n", "\\n");
    text.Replace("\t", "\\t");
    return text;
}

class JournalSshProcessImpl : public wxProcess
{
public:
    explicit JournalSshProcessImpl(JournalLogPage* owner)
        : wxProcess(), m_owner(owner)
    {
        Redirect();
    }

    void ClearOwner()
    {
        m_owner = nullptr;
    }

    void OnTerminate(int pid, int status) override
    {
        JournalLogPage* owner = m_owner;
        m_owner = nullptr;
        if (owner)
            owner->OnSshProcessTerminated(pid, status);
        delete this;
    }

private:
    JournalLogPage* m_owner = nullptr;
};
} // namespace

// Keep the public header free of the private implementation class.
class JournalSshProcess : public JournalSshProcessImpl
{
public:
    explicit JournalSshProcess(JournalLogPage* owner)
        : JournalSshProcessImpl(owner)
    {
    }
};

JournalLogPage::JournalLogPage(wxWindow* parent, Mode mode, const wxString& importedPath)
    : wxPanel(parent),
      m_mode(mode),
      m_importedPath(importedPath),
      m_pollTimer(this)
{
    SetBackgroundColour(PanelBackground());
    BuildUI();

    Bind(wxEVT_TIMER, [this](wxTimerEvent&)
    {
        PollProcessStreams();
    });

    if (m_mode == Mode::Imported && !m_importedPath.IsEmpty())
    {
        wxString error;
        if (!LoadImportFile(m_importedPath, &error))
            SetStatus(error, true);
    }
}

JournalLogPage::~JournalLogPage()
{
    m_pollTimer.Stop();
    if (m_process)
    {
        JournalSshProcess* process = m_process;
        const long pid = m_pid;
        m_process = nullptr;
        m_pid = 0;
        m_running = false;
        process->ClearOwner();
        if (pid > 0)
            wxProcess::Kill(pid, wxSIGTERM, wxKILL_CHILDREN);
    }
}

wxString JournalLogPage::GetTabTitle() const
{
    if (m_mode == Mode::Ssh)
        return "SSH Journal Logs";
    if (!m_importedPath.IsEmpty())
        return wxString("Logs: ") + wxFileName(m_importedPath).GetFullName();
    return "Imported Logs";
}

void JournalLogPage::BuildUI()
{
    auto* root = new wxBoxSizer(wxVERTICAL);

    if (m_mode == Mode::Ssh)
        BuildConnectionControls(root);
    else
    {
        wxString source = "Imported log file";
        if (!m_importedPath.IsEmpty())
            source += wxString(": ") + m_importedPath;
        auto* label = new wxStaticText(this, wxID_ANY, source);
        label->SetForegroundColour(Foreground());
        root->Add(label, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 8);
    }

    BuildFilterControls(root);
    BuildLogList(root);
    BuildExportControls(root);

    SetSizer(root);
    Layout();
    UpdateButtons();
}

void JournalLogPage::BuildConnectionControls(wxSizer* parentSizer)
{
    m_connectionPanel = new wxPanel(this);
    m_connectionPanel->SetBackgroundColour(FieldBackground());

    auto* outer = new wxBoxSizer(wxVERTICAL);
    auto* first = new wxBoxSizer(wxHORIZONTAL);

    auto addLabel = [&](wxSizer* row, const wxString& text)
    {
        auto* label = new wxStaticText(m_connectionPanel, wxID_ANY, text);
        label->SetForegroundColour(Foreground());
        row->Add(label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
    };

    addLabel(first, "Host");
    m_hostCtrl = new wxTextCtrl(m_connectionPanel, wxID_ANY);
    m_hostCtrl->SetHint("server.example.com");
    first->Add(m_hostCtrl, 2, wxRIGHT, 8);

    addLabel(first, "Port");
    m_portCtrl = new wxTextCtrl(m_connectionPanel, wxID_ANY, "22", wxDefaultPosition, wxSize(58, -1));
    first->Add(m_portCtrl, 0, wxRIGHT, 8);

    addLabel(first, "User");
    m_userCtrl = new wxTextCtrl(m_connectionPanel, wxID_ANY);
    first->Add(m_userCtrl, 1, wxRIGHT, 8);

    addLabel(first, "Identity");
    m_identityCtrl = new wxFilePickerCtrl(m_connectionPanel, wxID_ANY, wxString(),
                                          "Choose SSH private key", "*",
                                          wxDefaultPosition, wxSize(210, -1),
                                          wxFLP_OPEN | wxFLP_FILE_MUST_EXIST | wxFLP_USE_TEXTCTRL);
    first->Add(m_identityCtrl, 2, wxRIGHT, 4);
    outer->Add(first, 0, wxEXPAND | wxALL, 6);

    auto* second = new wxBoxSizer(wxHORIZONTAL);
    addLabel(second, "Unit (-u)");
    m_unitCtrl = new wxTextCtrl(m_connectionPanel, wxID_ANY);
    m_unitCtrl->SetHint("optional: my-service.service");
    second->Add(m_unitCtrl, 2, wxRIGHT, 8);

    addLabel(second, "Since");
    m_sinceCtrl = new wxTextCtrl(m_connectionPanel, wxID_ANY);
    m_sinceCtrl->SetHint("2026-08-16 08:00:00 or -2 hours");
    second->Add(m_sinceCtrl, 2, wxRIGHT, 8);

    m_followCheck = new wxCheckBox(m_connectionPanel, wxID_ANY, "Follow (-f)");
    m_followCheck->SetForegroundColour(Foreground());
    m_followCheck->SetValue(true);
    second->Add(m_followCheck, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);

    m_bootCheck = new wxCheckBox(m_connectionPanel, wxID_ANY, "Current boot (-b)");
    m_bootCheck->SetForegroundColour(Foreground());
    m_bootCheck->SetValue(false);
    second->Add(m_bootCheck, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);

    m_connectButton = new wxButton(m_connectionPanel, wxID_ANY, "Connect");
    m_stopButton = new wxButton(m_connectionPanel, wxID_ANY, "Stop");
    second->Add(m_connectButton, 0, wxRIGHT, 4);
    second->Add(m_stopButton, 0);
    outer->Add(second, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6);

    m_connectButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&)
    {
        StartSshCollection();
    });
    m_stopButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&)
    {
        StopSshCollection();
    });

    m_connectionPanel->SetSizer(outer);
    parentSizer->Add(m_connectionPanel, 0, wxEXPAND | wxALL, 6);
}

void JournalLogPage::BuildFilterControls(wxSizer* parentSizer)
{
    auto* filterPanel = new wxPanel(this);
    filterPanel->SetBackgroundColour(FieldBackground());
    auto* row = new wxBoxSizer(wxHORIZONTAL);

    auto* appLabel = new wxStaticText(filterPanel, wxID_ANY, "App regex");
    appLabel->SetForegroundColour(Foreground());
    row->Add(appLabel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);

    m_appRegexCtrl = new wxTextCtrl(filterPanel, wxID_ANY, wxString(), wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
    m_appRegexCtrl->SetHint("std::regex, empty = all apps");
    row->Add(m_appRegexCtrl, 1, wxRIGHT, 8);

    auto* payloadLabel = new wxStaticText(filterPanel, wxID_ANY, "Payload regex");
    payloadLabel->SetForegroundColour(Foreground());
    row->Add(payloadLabel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);

    m_payloadRegexCtrl = new wxTextCtrl(filterPanel, wxID_ANY, wxString(), wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
    m_payloadRegexCtrl->SetHint("std::regex, e.g. error|timeout|failed");
    row->Add(m_payloadRegexCtrl, 2, wxRIGHT, 8);

    m_caseSensitiveCheck = new wxCheckBox(filterPanel, wxID_ANY, "Case sensitive");
    m_caseSensitiveCheck->SetForegroundColour(Foreground());
    row->Add(m_caseSensitiveCheck, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);

    auto* apply = new wxButton(filterPanel, wxID_ANY, "Apply");
    auto* clear = new wxButton(filterPanel, wxID_ANY, "Clear");
    row->Add(apply, 0, wxRIGHT, 4);
    row->Add(clear, 0);

    auto applyFilter = [this](wxCommandEvent&)
    {
        ApplyFilters();
    };
    apply->Bind(wxEVT_BUTTON, applyFilter);
    m_appRegexCtrl->Bind(wxEVT_TEXT_ENTER, applyFilter);
    m_payloadRegexCtrl->Bind(wxEVT_TEXT_ENTER, applyFilter);
    clear->Bind(wxEVT_BUTTON, [this](wxCommandEvent&)
    {
        m_appRegexCtrl->Clear();
        m_payloadRegexCtrl->Clear();
        ApplyFilters();
    });

    filterPanel->SetSizer(row);
    parentSizer->Add(filterPanel, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6);
}

void JournalLogPage::BuildLogList(wxSizer* parentSizer)
{
    m_list = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                            wxLC_REPORT | wxLC_HRULES | wxLC_VRULES);
    m_list->SetBackgroundColour(PanelBackground());
    m_list->SetForegroundColour(Foreground());
    m_list->InsertColumn(0, "Time", wxLIST_FORMAT_LEFT, 170);
    m_list->InsertColumn(1, "App / Unit", wxLIST_FORMAT_LEFT, 190);
    m_list->InsertColumn(2, "Payload", wxLIST_FORMAT_LEFT, 760);

    m_list->Bind(wxEVT_CONTEXT_MENU, [this](wxContextMenuEvent& event)
    {
        ShowListContextMenu(event.GetPosition());
    });

    parentSizer->Add(m_list, 1, wxEXPAND | wxLEFT | wxRIGHT, 6);
}

void JournalLogPage::BuildExportControls(wxSizer* parentSizer)
{
    auto* panel = new wxPanel(this);
    panel->SetBackgroundColour(FieldBackground());
    auto* row = new wxBoxSizer(wxHORIZONTAL);

    auto* exportTxt = new wxButton(panel, wxID_ANY, "Export Visible TXT");
    auto* exportJson = new wxButton(panel, wxID_ANY, "Export Visible JSON");
    auto* clearLogs = new wxButton(panel, wxID_ANY, "Clear Logs");
    row->Add(exportTxt, 0, wxRIGHT, 4);
    row->Add(exportJson, 0, wxRIGHT, 4);
    row->Add(clearLogs, 0, wxRIGHT, 12);

    m_statusText = new wxStaticText(panel, wxID_ANY, "Ready");
    m_statusText->SetForegroundColour(Foreground());
    row->Add(m_statusText, 1, wxALIGN_CENTER_VERTICAL);

    exportTxt->Bind(wxEVT_BUTTON, [this](wxCommandEvent&)
    {
        ExportVisible(ExportFormat::Text);
    });
    exportJson->Bind(wxEVT_BUTTON, [this](wxCommandEvent&)
    {
        ExportVisible(ExportFormat::Json);
    });
    clearLogs->Bind(wxEVT_BUTTON, [this](wxCommandEvent&)
    {
        m_entries.clear();
        m_visibleEntries.clear();
        m_list->DeleteAllItems();
        SetStatus("Logs cleared");
    });

    panel->SetSizer(row);
    parentSizer->Add(panel, 0, wxEXPAND | wxALL, 6);
}

void JournalLogPage::StartSshCollection()
{
    if (m_running)
        return;

    if (!m_hostCtrl || m_hostCtrl->GetValue().Trim().IsEmpty())
    {
        SetStatus("SSH host is required", true);
        return;
    }

    long port = 22;
    if (!m_portCtrl->GetValue().ToLong(&port) || port < 1 || port > 65535)
    {
        SetStatus("SSH port must be between 1 and 65535", true);
        return;
    }

    m_stdoutBytes.clear();
    m_stderrBytes.clear();
    m_process = new JournalSshProcess(this);
    const wxString command = BuildSshCommand();

    m_pid = wxExecute(command, wxEXEC_ASYNC | wxEXEC_MAKE_GROUP_LEADER, m_process);
    if (m_pid <= 0)
    {
        m_process->ClearOwner();
        delete m_process;
        m_process = nullptr;
        m_pid = 0;
        SetStatus("Unable to launch ssh. Ensure OpenSSH client is installed and in PATH.", true);
        UpdateButtons();
        return;
    }

    m_running = true;
    m_pollTimer.Start(kPollIntervalMs);
    SetStatus(wxString::Format("Connected process started (PID %ld)", m_pid));
    UpdateButtons();
}

void JournalLogPage::StopSshCollection()
{
    if (!m_running || m_pid <= 0)
        return;

    SetStatus("Stopping SSH journal collection...");
    wxProcess::Kill(m_pid, wxSIGTERM, wxKILL_CHILDREN);
}

wxString JournalLogPage::BuildSshCommand() const
{
    wxString command = "ssh -o BatchMode=yes";
    command += " -p ";
    command += m_portCtrl->GetValue();

    const wxString identity = m_identityCtrl ? m_identityCtrl->GetPath() : wxString();
    if (!identity.IsEmpty())
    {
        command += " -i ";
        command += QuoteLocalArgument(identity);
    }

    wxString destination = m_hostCtrl->GetValue();
    destination.Trim(true).Trim(false);
    wxString user = m_userCtrl ? m_userCtrl->GetValue() : wxString();
    user.Trim(true).Trim(false);
    if (!user.IsEmpty())
        destination = user + "@" + destination;

    command += " ";
    command += QuoteLocalArgument(destination);

    wxString remote = "journalctl -o json --no-pager";
    if (m_followCheck && m_followCheck->GetValue())
        remote += " -f";
    if (m_bootCheck && m_bootCheck->GetValue())
        remote += " -b";

    wxString since = m_sinceCtrl ? m_sinceCtrl->GetValue() : wxString();
    since.Trim(true).Trim(false);
    if (!since.IsEmpty())
    {
        remote += " --since ";
        remote += QuoteRemoteArgument(since);
    }

    wxString unit = m_unitCtrl ? m_unitCtrl->GetValue() : wxString();
    unit.Trim(true).Trim(false);
    if (!unit.IsEmpty())
    {
        remote += " -u ";
        remote += QuoteRemoteArgument(unit);
    }

    command += " ";
    command += QuoteLocalArgument(remote);
    return command;
}

void JournalLogPage::PollProcessStreams()
{
    if (!m_process)
        return;
    ConsumeStream(m_process->GetInputStream(), m_stdoutBytes, false);
    ConsumeStream(m_process->GetErrorStream(), m_stderrBytes, true);
}

void JournalLogPage::ConsumeStream(wxInputStream* stream, std::string& pending, bool stderrStream)
{
    if (!stream)
        return;

    char buffer[4096];
    while (stream->CanRead())
    {
        stream->Read(buffer, sizeof(buffer));
        const size_t count = stream->LastRead();
        if (count == 0)
            break;
        pending.append(buffer, count);
        ConsumeCompleteLines(pending, stderrStream, false);
    }
}

void JournalLogPage::ConsumeCompleteLines(std::string& pending, bool stderrStream, bool flushAll)
{
    size_t pos = std::string::npos;
    while ((pos = pending.find('\n')) != std::string::npos)
    {
        std::string line = pending.substr(0, pos);
        pending.erase(0, pos + 1);
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty())
            continue;
        if (stderrStream)
            SetStatus(wxString("ssh: ") + wxString::FromUTF8(line.c_str()), true);
        else
            AddJournalJsonLine(line);
    }

    if (flushAll && !pending.empty())
    {
        std::string line = pending;
        pending.clear();
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (!line.empty())
        {
            if (stderrStream)
                SetStatus(wxString("ssh: ") + wxString::FromUTF8(line.c_str()), true);
            else
                AddJournalJsonLine(line);
        }
    }
}

void JournalLogPage::OnSshProcessTerminated(int pid, int status)
{
    if (pid != m_pid)
        return;

    PollProcessStreams();
    ConsumeCompleteLines(m_stdoutBytes, false, true);
    ConsumeCompleteLines(m_stderrBytes, true, true);

    m_pollTimer.Stop();
    m_running = false;
    m_pid = 0;
    m_process = nullptr;
    SetStatus(wxString::Format("SSH journal process exited with status %d", status), status != 0);
    UpdateButtons();
}

void JournalLogPage::AddJournalJsonLine(const std::string& line)
{
    Json::CharReaderBuilder builder;
    builder["collectComments"] = false;
    Json::Value root;
    std::string errors;
    std::istringstream input(line);
    if (!Json::parseFromStream(builder, input, &root, &errors) || !root.isObject())
    {
        AddPlainTextLine(wxString::FromUTF8(line.c_str()));
        return;
    }

    AppendEntryUsingCurrentFilters(EntryFromJournalJson(root));
}

JournalLogEntry JournalLogPage::EntryFromJournalJson(const Json::Value& value) const
{
    JournalLogEntry entry;
    entry.raw = value;
    entry.timestamp = FormatJournalTimestamp(value);

    const char* appKeys[] = {"SYSLOG_IDENTIFIER", "_SYSTEMD_UNIT", "_COMM", "_EXE"};
    for (const char* key : appKeys)
    {
        if (value.isMember(key) && value[key].isString() && !value[key].asString().empty())
        {
            entry.app = wxString::FromUTF8(value[key].asCString());
            break;
        }
    }

    if (value.isMember("MESSAGE"))
        entry.payload = JsonValueToString(value["MESSAGE"]);
    else
        entry.payload = JsonValueToString(value);

    return entry;
}

void JournalLogPage::AddPlainTextLine(const wxString& line)
{
    JournalLogEntry entry;
    entry.payload = line;

    // First understand DoDevEditor exported TXT: timestamp<TAB>app<TAB>payload.
    const int firstTab = line.Find('\t');
    if (firstTab != wxNOT_FOUND)
    {
        const wxString tail = line.Mid(firstTab + 1);
        const int secondRelative = tail.Find('\t');
        if (secondRelative != wxNOT_FOUND)
        {
            entry.timestamp = line.Left(firstTab);
            entry.app = tail.Left(secondRelative);
            entry.payload = tail.Mid(secondRelative + 1);
            entry.payload.Replace("\\n", "\n");
            entry.payload.Replace("\\t", "\t");
        }
    }

    if (entry.app.IsEmpty())
    {
        // Light heuristic for common syslog text: "... app[123]: payload".
        const std::string utf8 = ToUtf8(line);
        try
        {
            static const std::regex syslogPattern(
                R"(^.*\s([A-Za-z0-9_.@/:-]+)(?:\[[0-9]+\])?:\s+(.*)$)");
            std::smatch match;
            if (std::regex_match(utf8, match, syslogPattern) && match.size() >= 3)
            {
                entry.app = wxString::FromUTF8(match[1].str().c_str());
                entry.payload = wxString::FromUTF8(match[2].str().c_str());
            }
        }
        catch (const std::regex_error&)
        {
        }
    }

    if (m_mode == Mode::Ssh)
        AppendEntryUsingCurrentFilters(entry);
    else
        m_entries.push_back(entry);
}

void JournalLogPage::AppendEntryUsingCurrentFilters(JournalLogEntry entry)
{
    const size_t index = m_entries.size();
    m_entries.push_back(std::move(entry));

    const std::string appPattern = ToUtf8(m_appRegexCtrl ? m_appRegexCtrl->GetValue() : wxString());
    const std::string payloadPattern = ToUtf8(m_payloadRegexCtrl ? m_payloadRegexCtrl->GetValue() : wxString());
    const bool caseSensitive = m_caseSensitiveCheck && m_caseSensitiveCheck->GetValue();
    const std::regex::flag_type flags = caseSensitive
        ? std::regex::ECMAScript
        : static_cast<std::regex::flag_type>(std::regex::ECMAScript | std::regex::icase);

    try
    {
        std::unique_ptr<std::regex> appRegex;
        std::unique_ptr<std::regex> payloadRegex;
        if (!appPattern.empty())
            appRegex = std::make_unique<std::regex>(appPattern, flags);
        if (!payloadPattern.empty())
            payloadRegex = std::make_unique<std::regex>(payloadPattern, flags);

        if (EntryMatches(m_entries[index], appRegex.get(), payloadRegex.get()))
        {
            m_visibleEntries.push_back(index);
            AppendVisibleRow(index);
        }
        SetStatus(wxString::Format("%zu visible / %zu total", m_visibleEntries.size(), m_entries.size()));
    }
    catch (const std::regex_error& ex)
    {
        SetStatus(wxString("Invalid std::regex: ") + wxString::FromUTF8(ex.what()), true);
    }
}

bool JournalLogPage::LoadImportFile(const wxString& path, wxString* error)
{
    wxFFile file(path, "rb");
    if (!file.IsOpened())
    {
        if (error)
            *error = wxString("Unable to open log file: ") + path;
        return false;
    }

    wxString content;
    if (!file.ReadAll(&content, wxConvUTF8))
    {
        if (error)
            *error = wxString("Unable to read log file as UTF-8: ") + path;
        return false;
    }

    m_importedPath = path;
    m_entries.clear();
    m_visibleEntries.clear();

    const wxString ext = wxFileName(path).GetExt().Lower();
    if (ext == "json")
    {
        const std::string utf8 = ToUtf8(content);
        Json::CharReaderBuilder builder;
        builder["collectComments"] = false;
        Json::Value root;
        std::string errors;
        std::istringstream input(utf8);

        if (Json::parseFromStream(builder, input, &root, &errors))
        {
            if (root.isArray())
            {
                for (const Json::Value& item : root)
                {
                    if (item.isObject() && item.isMember("payload"))
                    {
                        JournalLogEntry entry;
                        if (item["timestamp"].isString())
                            entry.timestamp = wxString::FromUTF8(item["timestamp"].asCString());
                        if (item["app"].isString())
                            entry.app = wxString::FromUTF8(item["app"].asCString());
                        entry.payload = JsonValueToString(item["payload"]);
                        if (item.isMember("journal"))
                            entry.raw = item["journal"];
                        m_entries.push_back(entry);
                    }
                    else if (item.isObject())
                    {
                        m_entries.push_back(EntryFromJournalJson(item));
                    }
                }
            }
            else if (root.isObject())
            {
                m_entries.push_back(EntryFromJournalJson(root));
            }
        }
        else
        {
            // journalctl can be stored as JSON Lines rather than one JSON document.
            std::istringstream lines(utf8);
            std::string line;
            while (std::getline(lines, line))
            {
                if (line.empty())
                    continue;
                Json::Value item;
                std::string lineErrors;
                std::istringstream lineStream(line);
                if (Json::parseFromStream(builder, lineStream, &item, &lineErrors) && item.isObject())
                    m_entries.push_back(EntryFromJournalJson(item));
                else
                    AddPlainTextLine(wxString::FromUTF8(line.c_str()));
            }
        }
    }
    else
    {
        std::istringstream lines(ToUtf8(content));
        std::string line;
        while (std::getline(lines, line))
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            AddPlainTextLine(wxString::FromUTF8(line.c_str()));
        }
    }

    ApplyFilters();
    SetStatus(wxString::Format("Loaded %zu log entries from %s", m_entries.size(), path));
    return true;
}

void JournalLogPage::ApplyFilters()
{
    const std::string appPattern = ToUtf8(m_appRegexCtrl ? m_appRegexCtrl->GetValue() : wxString());
    const std::string payloadPattern = ToUtf8(m_payloadRegexCtrl ? m_payloadRegexCtrl->GetValue() : wxString());
    const bool caseSensitive = m_caseSensitiveCheck && m_caseSensitiveCheck->GetValue();
    const std::regex::flag_type flags = caseSensitive
        ? std::regex::ECMAScript
        : static_cast<std::regex::flag_type>(std::regex::ECMAScript | std::regex::icase);

    try
    {
        std::unique_ptr<std::regex> appRegex;
        std::unique_ptr<std::regex> payloadRegex;
        if (!appPattern.empty())
            appRegex = std::make_unique<std::regex>(appPattern, flags);
        if (!payloadPattern.empty())
            payloadRegex = std::make_unique<std::regex>(payloadPattern, flags);

        m_visibleEntries.clear();
        for (size_t i = 0; i < m_entries.size(); ++i)
        {
            if (EntryMatches(m_entries[i], appRegex.get(), payloadRegex.get()))
                m_visibleEntries.push_back(i);
        }
        RefreshList();
        SetStatus(wxString::Format("%zu visible / %zu total", m_visibleEntries.size(), m_entries.size()));
    }
    catch (const std::regex_error& ex)
    {
        SetStatus(wxString("Invalid std::regex: ") + wxString::FromUTF8(ex.what()), true);
    }
}

bool JournalLogPage::EntryMatches(const JournalLogEntry& entry,
                                  const std::regex* appRegex,
                                  const std::regex* payloadRegex) const
{
    if (appRegex && !std::regex_search(ToUtf8(entry.app), *appRegex))
        return false;
    if (payloadRegex && !std::regex_search(ToUtf8(entry.payload), *payloadRegex))
        return false;
    return true;
}

void JournalLogPage::RefreshList()
{
    if (!m_list)
        return;
    m_list->Freeze();
    m_list->DeleteAllItems();
    for (size_t index : m_visibleEntries)
        AppendVisibleRow(index);
    m_list->Thaw();
}

void JournalLogPage::AppendVisibleRow(size_t entryIndex)
{
    if (!m_list || entryIndex >= m_entries.size())
        return;
    const JournalLogEntry& entry = m_entries[entryIndex];
    const long row = m_list->InsertItem(m_list->GetItemCount(), entry.timestamp);
    m_list->SetItem(row, 1, entry.app);
    wxString singleLine = entry.payload;
    singleLine.Replace("\r", " ");
    singleLine.Replace("\n", " ");
    m_list->SetItem(row, 2, singleLine);
}

std::vector<size_t> JournalLogPage::GetSelectedEntryIndices() const
{
    std::vector<size_t> result;
    if (!m_list)
        return result;
    long item = -1;
    for (;;)
    {
        item = m_list->GetNextItem(item, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
        if (item == -1)
            break;
        if (static_cast<size_t>(item) < m_visibleEntries.size())
            result.push_back(m_visibleEntries[static_cast<size_t>(item)]);
    }
    return result;
}

std::vector<size_t> JournalLogPage::GetVisibleEntryIndices() const
{
    return m_visibleEntries;
}

void JournalLogPage::ShowListContextMenu(const wxPoint& screenPosition)
{
    if (GetSelectedEntryIndices().empty())
        return;

    wxMenu menu;
    wxMenuItem* copy = menu.Append(wxID_ANY, "Copy Selected to Clipboard");
    menu.AppendSeparator();
    wxMenuItem* saveTxt = menu.Append(wxID_ANY, "Save Selected as TXT...");
    wxMenuItem* saveJson = menu.Append(wxID_ANY, "Save Selected as JSON...");

    menu.Bind(wxEVT_MENU, [this](wxCommandEvent&)
    {
        CopySelectedToClipboard();
    }, copy->GetId());
    menu.Bind(wxEVT_MENU, [this](wxCommandEvent&)
    {
        ExportSelected(ExportFormat::Text);
    }, saveTxt->GetId());
    menu.Bind(wxEVT_MENU, [this](wxCommandEvent&)
    {
        ExportSelected(ExportFormat::Json);
    }, saveJson->GetId());

    wxPoint client = screenPosition;
    if (client.x == -1 && client.y == -1)
        client = wxPoint(20, 20);
    else
        client = m_list->ScreenToClient(client);
    m_list->PopupMenu(&menu, client);
}

void JournalLogPage::CopySelectedToClipboard()
{
    const std::vector<size_t> selected = GetSelectedEntryIndices();
    if (selected.empty())
        return;

    wxString text;
    for (size_t index : selected)
    {
        if (index >= m_entries.size())
            continue;
        const JournalLogEntry& entry = m_entries[index];
        if (!text.IsEmpty())
            text += "\n";
        text += entry.timestamp;
        text += "\t";
        text += entry.app;
        text += "\t";
        text += entry.payload;
    }

    if (wxTheClipboard->Open())
    {
        wxTheClipboard->SetData(new wxTextDataObject(text));
        wxTheClipboard->Close();
        SetStatus(wxString::Format("Copied %zu selected rows", selected.size()));
    }
    else
    {
        SetStatus("Unable to open clipboard", true);
    }
}

void JournalLogPage::ExportVisible(ExportFormat format)
{
    ExportEntries(GetVisibleEntryIndices(), format);
}

void JournalLogPage::ExportSelected(ExportFormat format)
{
    ExportEntries(GetSelectedEntryIndices(), format);
}

void JournalLogPage::ExportEntries(const std::vector<size_t>& indices, ExportFormat format)
{
    if (indices.empty())
    {
        SetStatus("There are no log rows to export", true);
        return;
    }

    const bool json = format == ExportFormat::Json;
    wxFileDialog dialog(this,
                        json ? "Export logs as JSON" : "Export logs as text",
                        wxString(),
                        json ? "journal-logs.json" : "journal-logs.txt",
                        json ? "JSON files (*.json)|*.json" : "Text files (*.txt)|*.txt",
                        wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dialog.ShowModal() != wxID_OK)
        return;

    wxString error;
    if (!WriteEntries(dialog.GetPath(), indices, format, &error))
    {
        SetStatus(error, true);
        return;
    }
    SetStatus(wxString::Format("Exported %zu rows to %s", indices.size(), dialog.GetPath()));
}

bool JournalLogPage::WriteEntries(const wxString& path,
                                  const std::vector<size_t>& indices,
                                  ExportFormat format,
                                  wxString* error) const
{
    wxFFile file(path, "wb");
    if (!file.IsOpened())
    {
        if (error)
            *error = wxString("Unable to create export file: ") + path;
        return false;
    }

    wxString output;
    if (format == ExportFormat::Text)
    {
        for (size_t index : indices)
        {
            if (index >= m_entries.size())
                continue;
            const JournalLogEntry& entry = m_entries[index];
            output += FlattenTextForExport(entry.timestamp);
            output += "\t";
            output += FlattenTextForExport(entry.app);
            output += "\t";
            output += FlattenTextForExport(entry.payload);
            output += "\n";
        }
    }
    else
    {
        Json::Value root(Json::arrayValue);
        for (size_t index : indices)
        {
            if (index >= m_entries.size())
                continue;
            const JournalLogEntry& entry = m_entries[index];
            Json::Value item(Json::objectValue);
            item["timestamp"] = ToUtf8(entry.timestamp);
            item["app"] = ToUtf8(entry.app);
            item["payload"] = ToUtf8(entry.payload);
            if (!entry.raw.isNull())
                item["journal"] = entry.raw;
            root.append(item);
        }
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "  ";
        output = wxString::FromUTF8(Json::writeString(builder, root).c_str());
        output += "\n";
    }

    if (!file.Write(output, wxConvUTF8))
    {
        if (error)
            *error = wxString("Failed while writing export file: ") + path;
        return false;
    }
    return true;
}

void JournalLogPage::SetStatus(const wxString& text, bool error)
{
    if (!m_statusText)
        return;
    m_statusText->SetLabel(text);
    m_statusText->SetForegroundColour(error ? wxColour(244, 71, 71) : Foreground());
    m_statusText->GetParent()->Layout();
}

void JournalLogPage::UpdateButtons()
{
    if (m_connectButton)
        m_connectButton->Enable(!m_running);
    if (m_stopButton)
        m_stopButton->Enable(m_running);
}

wxString JournalLogPage::FormatJournalTimestamp(const Json::Value& value)
{
    const char* keys[] = {"_SOURCE_REALTIME_TIMESTAMP", "__REALTIME_TIMESTAMP"};
    for (const char* key : keys)
    {
        if (!value.isMember(key))
            continue;

        std::string raw;
        if (value[key].isString())
            raw = value[key].asString();
        else if (value[key].isUInt64())
            raw = std::to_string(value[key].asUInt64());
        else if (value[key].isInt64())
            raw = std::to_string(value[key].asInt64());
        if (raw.empty())
            continue;

        try
        {
            const long long micros = std::stoll(raw);
            const std::time_t seconds = static_cast<std::time_t>(micros / 1000000LL);
            const wxDateTime dateTime(seconds);
            if (dateTime.IsValid())
                return dateTime.FormatISOCombined(' ');
            return wxString::FromUTF8(raw.c_str());
        }
        catch (const std::exception&)
        {
            return wxString::FromUTF8(raw.c_str());
        }
    }

    if (value.isMember("timestamp") && value["timestamp"].isString())
        return wxString::FromUTF8(value["timestamp"].asCString());
    return wxString();
}

wxString JournalLogPage::JsonValueToString(const Json::Value& value)
{
    if (value.isString())
        return wxString::FromUTF8(value.asCString());
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    return wxString::FromUTF8(Json::writeString(builder, value).c_str());
}

std::string JournalLogPage::ToUtf8(const wxString& value)
{
    const wxScopedCharBuffer bytes = value.ToUTF8();
    return bytes.data() ? std::string(bytes.data(), bytes.length()) : std::string();
}

wxString JournalLogPage::QuoteLocalArgument(const wxString& value)
{
#ifdef _WIN32
    wxString escaped = value;
    escaped.Replace("\"", "\\\"");
    return wxString("\"") + escaped + "\"";
#else
    wxString escaped = value;
    escaped.Replace("'", "'\"'\"'");
    return wxString("'") + escaped + "'";
#endif
}

wxString JournalLogPage::QuoteRemoteArgument(const wxString& value)
{
    // The remote side is expected to be a Linux/systemd host, so quote for a
    // POSIX shell independently of the local Windows/Linux command line.
    wxString escaped = value;
    escaped.Replace("'", "'\"'\"'");
    return wxString("'") + escaped + "'";
}

#endif // DODEV_ENABLE_JOURNAL_LOGS
