#include "LocalHistoryManager.h"

#include <wx/datetime.h>
#include <wx/ffile.h>
#include <wx/filename.h>
#include <wx/filefn.h>

#include <json/json.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
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

wxString JoinPath(const wxString& left, const wxString& right)
{
    if (left.IsEmpty())
        return right;
    wxString result = left;
    if (result.Last() != wxFileName::GetPathSeparator())
        result += wxFileName::GetPathSeparator();
    result += right;
    return result;
}

wxString NormalizeFilePath(const wxString& path)
{
    wxFileName file(path);
    file.Normalize(wxPATH_NORM_DOTS | wxPATH_NORM_ABSOLUTE);
    return file.GetFullPath();
}

wxString ResolveRoot(const wxString& filePath, const wxString& workspaceRoot)
{
    if (!workspaceRoot.IsEmpty())
    {
        wxFileName root(workspaceRoot, wxEmptyString);
        root.Normalize(wxPATH_NORM_DOTS | wxPATH_NORM_ABSOLUTE);
        return root.GetPath();
    }
    return wxFileName(NormalizeFilePath(filePath)).GetPath();
}

uint64_t Fnv1a64(const std::string& text)
{
    uint64_t hash = 14695981039346656037ULL;
    for (unsigned char ch : text)
    {
        hash ^= static_cast<uint64_t>(ch);
        hash *= 1099511628211ULL;
    }
    return hash;
}

wxString HashPath(const wxString& path)
{
    std::ostringstream out;
    out << std::hex << std::setw(16) << std::setfill('0')
        << Fnv1a64(ToUtf8(NormalizeFilePath(path)));
    return FromUtf8(out.str());
}

bool ReadTextFile(const wxString& path, wxString& content)
{
    wxFFile file(path, "rb");
    if (!file.IsOpened())
        return false;
    return file.ReadAll(&content, wxConvUTF8);
}

bool WriteTextFile(const wxString& path, const wxString& content)
{
    wxFFile file(path, "wb");
    if (!file.IsOpened())
        return false;
    return file.Write(content, wxConvUTF8);
}

wxString IndexPath(const wxString& directory)
{
    return JoinPath(directory, "index.json");
}

Json::Value LoadIndex(const wxString& directory)
{
    Json::Value root(Json::objectValue);
    root["version"] = 1;
    root["entries"] = Json::Value(Json::arrayValue);

    wxString text;
    if (!ReadTextFile(IndexPath(directory), text))
        return root;

    Json::CharReaderBuilder builder;
    std::string errors;
    std::istringstream input(ToUtf8(text));
    Json::Value parsed;
    if (!Json::parseFromStream(builder, input, &parsed, &errors) || !parsed.isObject())
        return root;
    if (!parsed["entries"].isArray())
        parsed["entries"] = Json::Value(Json::arrayValue);
    return parsed;
}

bool SaveIndex(const wxString& directory, const Json::Value& root)
{
    Json::StreamWriterBuilder writer;
    writer["indentation"] = "  ";
    return WriteTextFile(IndexPath(directory), FromUtf8(Json::writeString(writer, root)));
}

std::vector<wxString> SplitLines(const wxString& text)
{
    std::vector<wxString> lines;
    wxString current;
    for (size_t i = 0; i < text.length(); ++i)
    {
        const wxUniChar ch = text[i];
        if (ch == '\r')
            continue;
        if (ch == '\n')
        {
            lines.push_back(current);
            current.clear();
        }
        else
        {
            current += ch;
        }
    }
    if (!current.IsEmpty() || text.IsEmpty() || text.Last() != '\n')
        lines.push_back(current);
    return lines;
}

wxString RelativeDisplayPath(const wxString& filePath, const wxString& root)
{
    wxFileName relative(NormalizeFilePath(filePath));
    if (!root.IsEmpty())
    {
        wxFileName rootDir(root, wxEmptyString);
        rootDir.Normalize(wxPATH_NORM_DOTS | wxPATH_NORM_ABSOLUTE);
        relative.MakeRelativeTo(rootDir.GetPath());
    }
    return relative.GetFullPath();
}

struct PatchResult
{
    wxString text;
    int added = 0;
    int removed = 0;
};

PatchResult BuildPatch(const wxString& oldText,
                       const wxString& newText,
                       const wxString& displayPath)
{
    const std::vector<wxString> oldLines = SplitLines(oldText);
    const std::vector<wxString> newLines = SplitLines(newText);

    size_t prefix = 0;
    while (prefix < oldLines.size() && prefix < newLines.size() &&
           oldLines[prefix] == newLines[prefix])
    {
        ++prefix;
    }

    size_t oldSuffix = oldLines.size();
    size_t newSuffix = newLines.size();
    while (oldSuffix > prefix && newSuffix > prefix &&
           oldLines[oldSuffix - 1] == newLines[newSuffix - 1])
    {
        --oldSuffix;
        --newSuffix;
    }

    const size_t context = 3;
    const size_t hunkStart = prefix > context ? prefix - context : 0;
    const size_t oldHunkEnd = std::min(oldLines.size(), oldSuffix + context);
    const size_t newHunkEnd = std::min(newLines.size(), newSuffix + context);

    PatchResult result;
    result.removed = static_cast<int>(oldSuffix - prefix);
    result.added = static_cast<int>(newSuffix - prefix);

    result.text += "--- a/" + displayPath + "\n";
    result.text += "+++ b/" + displayPath + "\n";

    if (oldText == newText)
    {
        result.text += "# No textual changes from previous local-history revision.\n";
        return result;
    }

    const size_t oldCount = oldHunkEnd - hunkStart;
    const size_t newCount = newHunkEnd - hunkStart;
    result.text += wxString::Format("@@ -%llu,%llu +%llu,%llu @@\n",
                                   static_cast<unsigned long long>(hunkStart + 1),
                                   static_cast<unsigned long long>(oldCount),
                                   static_cast<unsigned long long>(hunkStart + 1),
                                   static_cast<unsigned long long>(newCount));

    for (size_t i = hunkStart; i < prefix; ++i)
        result.text += " " + oldLines[i] + "\n";
    for (size_t i = prefix; i < oldSuffix; ++i)
        result.text += "-" + oldLines[i] + "\n";
    for (size_t i = prefix; i < newSuffix; ++i)
        result.text += "+" + newLines[i] + "\n";

    const size_t commonSuffixCount = std::min(oldHunkEnd - oldSuffix,
                                               newHunkEnd - newSuffix);
    for (size_t i = 0; i < commonSuffixCount; ++i)
        result.text += " " + oldLines[oldSuffix + i] + "\n";

    return result;
}

long long NowMillis()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

wxString TimestampForDisplay(long long millis)
{
    wxDateTime value(static_cast<time_t>(millis / 1000));
    return value.FormatISODate() + " " + value.FormatISOTime();
}

wxString FileStem(long long id)
{
    return wxString::Format("%lld", id);
}

int CountLines(const wxString& text)
{
    if (text.IsEmpty())
        return 0;
    int count = 1;
    for (size_t i = 0; i < text.length(); ++i)
    {
        if (text[i] == '\n')
            ++count;
    }
    return count;
}

LocalHistoryEntry EntryFromJson(const Json::Value& value)
{
    LocalHistoryEntry entry;
    entry.id = value.get("id", Json::Int64(0)).asInt64();
    entry.timestamp = FromUtf8(value.get("timestamp", "").asString());
    entry.kind = FromUtf8(value.get("kind", "save").asString());
    entry.note = FromUtf8(value.get("note", "").asString());
    entry.snapshotFile = FromUtf8(value.get("snapshot", "").asString());
    entry.patchFile = FromUtf8(value.get("patch", "").asString());
    entry.sizeBytes = value.get("size_bytes", Json::Int64(0)).asInt64();
    entry.lineCount = value.get("line_count", 0).asInt();
    entry.addedLines = value.get("added", 0).asInt();
    entry.removedLines = value.get("removed", 0).asInt();
    return entry;
}
}

wxString LocalHistoryManager::HistoryDirectory(const wxString& filePath,
                                               const wxString& workspaceRoot)
{
    const wxString root = ResolveRoot(filePath, workspaceRoot);
    wxString directory = JoinPath(root, ".dodev");
    directory = JoinPath(directory, "history");
    directory = JoinPath(directory, HashPath(filePath));
    return directory;
}

bool LocalHistoryManager::EnsureBaseline(const wxString& filePath,
                                         const wxString& workspaceRoot,
                                         wxString* error)
{
    if (!wxFileExists(filePath))
        return true;

    const wxString directory = HistoryDirectory(filePath, workspaceRoot);
    const Json::Value index = LoadIndex(directory);
    if (index["entries"].isArray() && index["entries"].size() > 0)
        return true;

    wxString content;
    if (!ReadTextFile(filePath, content))
    {
        if (error)
            *error = "Could not read the current file to create the local-history baseline.";
        return false;
    }
    return Record(filePath, workspaceRoot, content, "baseline", "Initial disk state", error);
}

bool LocalHistoryManager::RecordSave(const wxString& filePath,
                                     const wxString& workspaceRoot,
                                     const wxString& content,
                                     const wxString& note,
                                     wxString* error)
{
    return Record(filePath, workspaceRoot, content, "save", note, error);
}

bool LocalHistoryManager::Record(const wxString& filePath,
                                 const wxString& workspaceRoot,
                                 const wxString& content,
                                 const wxString& kind,
                                 const wxString& note,
                                 wxString* error)
{
    const wxString directory = HistoryDirectory(filePath, workspaceRoot);
    if (!wxFileName::Mkdir(directory, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL) && !wxDirExists(directory))
    {
        if (error)
            *error = "Could not create local-history directory: " + directory;
        return false;
    }

    Json::Value index = LoadIndex(directory);
    index["version"] = 1;
    index["file"] = ToUtf8(NormalizeFilePath(filePath));
    index["workspace_root"] = ToUtf8(ResolveRoot(filePath, workspaceRoot));

    wxString previousContent;
    if (index["entries"].isArray() && index["entries"].size() > 0)
    {
        const Json::Value& previous = index["entries"][index["entries"].size() - 1];
        const wxString previousSnapshot = JoinPath(directory,
            FromUtf8(previous.get("snapshot", "").asString()));
        ReadTextFile(previousSnapshot, previousContent);
    }

    long long id = NowMillis();
    if (index["entries"].isArray() && index["entries"].size() > 0)
    {
        const long long previousId = index["entries"][index["entries"].size() - 1]
                                         .get("id", Json::Int64(0)).asInt64();
        if (id <= previousId)
            id = previousId + 1;
    }

    const wxString stem = FileStem(id);
    const wxString snapshotName = stem + ".snapshot";
    const wxString patchName = stem + ".patch";
    const wxString displayPath = RelativeDisplayPath(filePath, ResolveRoot(filePath, workspaceRoot));
    const PatchResult patch = BuildPatch(previousContent, content, displayPath);

    if (!WriteTextFile(JoinPath(directory, snapshotName), content))
    {
        if (error)
            *error = "Could not write local-history snapshot.";
        return false;
    }
    if (!WriteTextFile(JoinPath(directory, patchName), patch.text))
    {
        wxRemoveFile(JoinPath(directory, snapshotName));
        if (error)
            *error = "Could not write local-history patch.";
        return false;
    }

    Json::Value entry(Json::objectValue);
    entry["id"] = Json::Int64(id);
    entry["timestamp"] = ToUtf8(TimestampForDisplay(id));
    entry["kind"] = ToUtf8(kind);
    entry["note"] = ToUtf8(note);
    entry["snapshot"] = ToUtf8(snapshotName);
    entry["patch"] = ToUtf8(patchName);
    entry["size_bytes"] = Json::Int64(static_cast<long long>(ToUtf8(content).size()));
    entry["line_count"] = CountLines(content);
    entry["added"] = patch.added;
    entry["removed"] = patch.removed;
    index["entries"].append(entry);

    if (!SaveIndex(directory, index))
    {
        wxRemoveFile(JoinPath(directory, snapshotName));
        wxRemoveFile(JoinPath(directory, patchName));
        if (error)
            *error = "Could not update local-history index.";
        return false;
    }
    return true;
}

std::vector<LocalHistoryEntry> LocalHistoryManager::List(const wxString& filePath,
                                                          const wxString& workspaceRoot,
                                                          wxString* error)
{
    std::vector<LocalHistoryEntry> result;
    const wxString directory = HistoryDirectory(filePath, workspaceRoot);
    if (!wxDirExists(directory))
        return result;

    const Json::Value index = LoadIndex(directory);
    if (!index["entries"].isArray())
    {
        if (error)
            *error = "Local-history index is invalid.";
        return result;
    }

    for (const auto& value : index["entries"])
        result.push_back(EntryFromJson(value));

    std::sort(result.begin(), result.end(), [](const LocalHistoryEntry& a, const LocalHistoryEntry& b)
    {
        return a.id > b.id;
    });
    return result;
}

bool LocalHistoryManager::LoadSnapshot(const wxString& filePath,
                                       const wxString& workspaceRoot,
                                       const LocalHistoryEntry& entry,
                                       wxString& content,
                                       wxString* error)
{
    const wxString path = JoinPath(HistoryDirectory(filePath, workspaceRoot), entry.snapshotFile);
    if (!ReadTextFile(path, content))
    {
        if (error)
            *error = "Could not read local-history snapshot: " + path;
        return false;
    }
    return true;
}

bool LocalHistoryManager::LoadPatch(const wxString& filePath,
                                    const wxString& workspaceRoot,
                                    const LocalHistoryEntry& entry,
                                    wxString& patch,
                                    wxString* error)
{
    const wxString path = JoinPath(HistoryDirectory(filePath, workspaceRoot), entry.patchFile);
    if (!ReadTextFile(path, patch))
    {
        if (error)
            *error = "Could not read local-history patch: " + path;
        return false;
    }
    return true;
}
