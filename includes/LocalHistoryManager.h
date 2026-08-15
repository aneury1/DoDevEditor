#pragma once

#include <wx/string.h>
#include <vector>

struct LocalHistoryEntry
{
    long long id = 0;
    wxString timestamp;
    wxString kind;
    wxString note;
    wxString snapshotFile;
    wxString patchFile;
    long long sizeBytes = 0;
    int lineCount = 0;
    int addedLines = 0;
    int removedLines = 0;
};

class LocalHistoryManager
{
public:
    // Capture the current on-disk file as the baseline before its first
    // DoDevEditor-managed overwrite. No-op when history already exists.
    static bool EnsureBaseline(const wxString& filePath,
                               const wxString& workspaceRoot,
                               wxString* error = nullptr);

    // Record a successfully saved version.
    static bool RecordSave(const wxString& filePath,
                           const wxString& workspaceRoot,
                           const wxString& content,
                           const wxString& note = "Save",
                           wxString* error = nullptr);

    static std::vector<LocalHistoryEntry> List(const wxString& filePath,
                                                const wxString& workspaceRoot,
                                                wxString* error = nullptr);

    static bool LoadSnapshot(const wxString& filePath,
                             const wxString& workspaceRoot,
                             const LocalHistoryEntry& entry,
                             wxString& content,
                             wxString* error = nullptr);

    static bool LoadPatch(const wxString& filePath,
                          const wxString& workspaceRoot,
                          const LocalHistoryEntry& entry,
                          wxString& patch,
                          wxString* error = nullptr);

    static wxString HistoryDirectory(const wxString& filePath,
                                     const wxString& workspaceRoot);

private:
    static bool Record(const wxString& filePath,
                       const wxString& workspaceRoot,
                       const wxString& content,
                       const wxString& kind,
                       const wxString& note,
                       wxString* error);
};
