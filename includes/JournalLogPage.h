#pragma once

#if DODEV_ENABLE_JOURNAL_LOGS

#include <wx/panel.h>
#include <wx/string.h>
#include <wx/timer.h>

#include <json/json.h>

#include <regex>
#include <string>
#include <vector>

class wxButton;
class wxCheckBox;
class wxFilePickerCtrl;
class wxInputStream;
class wxListCtrl;
class wxStaticText;
class wxSizer;
class wxTextCtrl;
class JournalSshProcess;

struct JournalLogEntry
{
    wxString timestamp;
    wxString app;
    wxString payload;
    Json::Value raw;
};

class JournalLogPage : public wxPanel
{
public:
    enum class Mode
    {
        Ssh,
        Imported
    };

    JournalLogPage(wxWindow* parent, Mode mode, const wxString& importedPath = wxString());
    ~JournalLogPage() override;

    wxString GetTabTitle() const;
    bool LoadImportFile(const wxString& path, wxString* error = nullptr);
    bool IsLive() const { return m_mode == Mode::Ssh; }
    bool IsRunning() const { return m_running; }

    // Called by the wxProcess wrapper when the SSH process exits.
    void OnSshProcessTerminated(int pid, int status);

private:
    enum class ExportFormat
    {
        Text,
        Json
    };

    Mode m_mode;
    wxString m_importedPath;

    wxPanel* m_connectionPanel = nullptr;
    wxTextCtrl* m_hostCtrl = nullptr;
    wxTextCtrl* m_portCtrl = nullptr;
    wxTextCtrl* m_userCtrl = nullptr;
    wxFilePickerCtrl* m_identityCtrl = nullptr;
    wxTextCtrl* m_unitCtrl = nullptr;
    wxTextCtrl* m_sinceCtrl = nullptr;
    wxCheckBox* m_followCheck = nullptr;
    wxCheckBox* m_bootCheck = nullptr;
    wxButton* m_connectButton = nullptr;
    wxButton* m_stopButton = nullptr;

    wxTextCtrl* m_appRegexCtrl = nullptr;
    wxTextCtrl* m_payloadRegexCtrl = nullptr;
    wxCheckBox* m_caseSensitiveCheck = nullptr;
    wxListCtrl* m_list = nullptr;
    wxStaticText* m_statusText = nullptr;

    std::vector<JournalLogEntry> m_entries;
    std::vector<size_t> m_visibleEntries;

    JournalSshProcess* m_process = nullptr;
    long m_pid = 0;
    bool m_running = false;
    wxTimer m_pollTimer;
    std::string m_stdoutBytes;
    std::string m_stderrBytes;

    void BuildUI();
    void BuildConnectionControls(wxSizer* parentSizer);
    void BuildFilterControls(wxSizer* parentSizer);
    void BuildLogList(wxSizer* parentSizer);
    void BuildExportControls(wxSizer* parentSizer);

    void StartSshCollection();
    void StopSshCollection();
    wxString BuildSshCommand() const;
    void PollProcessStreams();
    void ConsumeStream(wxInputStream* stream, std::string& pending, bool stderrStream);
    void ConsumeCompleteLines(std::string& pending, bool stderrStream, bool flushAll = false);

    void AddJournalJsonLine(const std::string& line);
    void AddPlainTextLine(const wxString& line);
    void AppendEntryUsingCurrentFilters(JournalLogEntry entry);
    JournalLogEntry EntryFromJournalJson(const Json::Value& value) const;

    void ApplyFilters();
    bool EntryMatches(const JournalLogEntry& entry,
                      const std::regex* appRegex,
                      const std::regex* payloadRegex) const;
    void RefreshList();
    void AppendVisibleRow(size_t entryIndex);

    std::vector<size_t> GetSelectedEntryIndices() const;
    std::vector<size_t> GetVisibleEntryIndices() const;
    void ShowListContextMenu(const wxPoint& screenPosition);
    void CopySelectedToClipboard();

    void ExportVisible(ExportFormat format);
    void ExportSelected(ExportFormat format);
    void ExportEntries(const std::vector<size_t>& indices, ExportFormat format);
    bool WriteEntries(const wxString& path,
                      const std::vector<size_t>& indices,
                      ExportFormat format,
                      wxString* error) const;

    void SetStatus(const wxString& text, bool error = false);
    void UpdateButtons();

    static wxString FormatJournalTimestamp(const Json::Value& value);
    static wxString JsonValueToString(const Json::Value& value);
    static std::string ToUtf8(const wxString& value);
    static wxString QuoteLocalArgument(const wxString& value);
    static wxString QuoteRemoteArgument(const wxString& value);
};

#endif // DODEV_ENABLE_JOURNAL_LOGS
