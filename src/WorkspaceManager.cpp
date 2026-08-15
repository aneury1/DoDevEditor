#include "WorkspaceManager.h"

#include <wx/ffile.h>
#include <wx/filename.h>
#include <wx/filefn.h>

#include <json/json.h>

#include <algorithm>
#include <sstream>
#include <utility>

namespace
{
wxString FromUtf8(const std::string& value)
{
    return wxString::FromUTF8(value.c_str());
}

std::string ToUtf8(const wxString& value)
{
    const wxScopedCharBuffer utf8 = value.utf8_str();
    return utf8.data() ? std::string(utf8.data()) : std::string();
}

bool PathStartsWith(const wxString& path, const wxString& root)
{
#ifdef __WXMSW__
    return path.Lower().StartsWith(root.Lower());
#else
    return path.StartsWith(root);
#endif
}
}

wxString WorkspaceManager::NormalizeDirectory(const wxString& path)
{
    if (path.IsEmpty())
        return wxString();

    wxFileName directory(path, wxEmptyString);
    directory.Normalize(wxPATH_NORM_DOTS | wxPATH_NORM_ABSOLUTE);
    wxString normalized = directory.GetPath();

    while (normalized.length() > 1 &&
           (normalized.Last() == '/' || normalized.Last() == '\\'))
    {
#ifdef __WXMSW__
        if (normalized.length() == 3 && normalized[1] == ':')
            break;
#endif
        normalized.RemoveLast();
    }
    return normalized;
}

wxString WorkspaceManager::DefaultFolderName(const wxString& path)
{
    wxFileName directory(path, wxEmptyString);
    const wxArrayString dirs = directory.GetDirs();
    wxString name = path;
    if (!dirs.IsEmpty())
        name = dirs[dirs.size() - 1];
    if (name.IsEmpty())
        name = path;
    return name;
}

bool WorkspaceManager::ContainsFolder(const wxString& normalizedPath) const
{
    for (const auto& folder : m_folders)
    {
#ifdef __WXMSW__
        if (folder.path.CmpNoCase(normalizedPath) == 0)
#else
        if (folder.path == normalizedPath)
#endif
            return true;
    }
    return false;
}

void WorkspaceManager::Clear()
{
    m_folders.clear();
    m_workspaceFile.clear();
}

bool WorkspaceManager::OpenFolder(const wxString& path)
{
    const wxString normalized = NormalizeDirectory(path);
    if (normalized.IsEmpty() || !wxDirExists(normalized))
        return false;

    m_folders.clear();
    m_folders.push_back({normalized, DefaultFolderName(normalized)});
    m_workspaceFile.clear();
    return true;
}

bool WorkspaceManager::SetFolders(const std::vector<wxString>& paths)
{
    m_folders.clear();
    m_workspaceFile.clear();
    for (const wxString& path : paths)
    {
        const wxString normalized = NormalizeDirectory(path);
        if (normalized.IsEmpty() || !wxDirExists(normalized) || ContainsFolder(normalized))
            continue;
        m_folders.push_back({normalized, DefaultFolderName(normalized)});
    }
    return !m_folders.empty();
}

bool WorkspaceManager::AddFolder(const wxString& path)
{
    const wxString normalized = NormalizeDirectory(path);
    if (normalized.IsEmpty() || !wxDirExists(normalized))
        return false;
    if (ContainsFolder(normalized))
        return true;

    m_folders.push_back({normalized, DefaultFolderName(normalized)});
    return true;
}

bool WorkspaceManager::RemoveFolder(const wxString& path)
{
    const wxString normalized = NormalizeDirectory(path);
    const auto oldSize = m_folders.size();
    m_folders.erase(
        std::remove_if(m_folders.begin(), m_folders.end(), [&](const WorkspaceFolder& folder)
        {
#ifdef __WXMSW__
            return folder.path.CmpNoCase(normalized) == 0;
#else
            return folder.path == normalized;
#endif
        }),
        m_folders.end());
    return m_folders.size() != oldSize;
}

std::vector<wxString> WorkspaceManager::GetFolderPaths() const
{
    std::vector<wxString> result;
    result.reserve(m_folders.size());
    for (const auto& folder : m_folders)
        result.push_back(folder.path);
    return result;
}

wxString WorkspaceManager::GetFolderLabel(const wxString& rootPath) const
{
    const wxString normalized = NormalizeDirectory(rootPath);
    for (const auto& folder : m_folders)
    {
#ifdef __WXMSW__
        if (folder.path.CmpNoCase(normalized) == 0)
#else
        if (folder.path == normalized)
#endif
            return folder.name.IsEmpty() ? DefaultFolderName(folder.path) : folder.name;
    }
    return DefaultFolderName(normalized);
}

wxString WorkspaceManager::GetDisplayName() const
{
    if (!m_workspaceFile.IsEmpty())
        return wxFileName(m_workspaceFile).GetName();
    if (m_folders.size() == 1)
        return GetFolderLabel(m_folders.front().path);
    if (m_folders.size() > 1)
        return "Untitled Workspace";
    return "DoDevEditor";
}

wxString WorkspaceManager::FindContainingFolder(const wxString& fileOrDirectory) const
{
    if (fileOrDirectory.IsEmpty())
        return wxString();

    wxFileName candidateFile(fileOrDirectory);
    wxString candidate;
    if (wxDirExists(fileOrDirectory))
        candidate = NormalizeDirectory(fileOrDirectory);
    else
    {
        candidateFile.Normalize(wxPATH_NORM_DOTS | wxPATH_NORM_ABSOLUTE);
        candidate = candidateFile.GetFullPath();
    }

    wxString best;
    for (const auto& folder : m_folders)
    {
        wxString prefix = folder.path;
        if (!prefix.IsEmpty() && prefix.Last() != wxFileName::GetPathSeparator())
            prefix += wxFileName::GetPathSeparator();

#ifdef __WXMSW__
        const bool exact = candidate.CmpNoCase(folder.path) == 0;
#else
        const bool exact = candidate == folder.path;
#endif
        if ((exact || PathStartsWith(candidate, prefix)) && folder.path.length() > best.length())
            best = folder.path;
    }
    return best;
}

bool WorkspaceManager::LoadWorkspace(const wxString& filePath, wxString* error)
{
    wxFFile input(filePath, "rb");
    if (!input.IsOpened())
    {
        if (error)
            *error = "Could not open workspace file.";
        return false;
    }

    wxString text;
    if (!input.ReadAll(&text))
    {
        if (error)
            *error = "Could not read workspace file as UTF-8.";
        return false;
    }

    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string parseError;
    std::istringstream stream(ToUtf8(text));
    if (!Json::parseFromStream(builder, stream, &root, &parseError))
    {
        if (error)
            *error = wxString("Invalid workspace JSON: ") + FromUtf8(parseError);
        return false;
    }

    if (!root["folders"].isArray())
    {
        if (error)
            *error = "Workspace file does not contain a folders array.";
        return false;
    }

    wxFileName workspaceFile(filePath);
    workspaceFile.Normalize(wxPATH_NORM_DOTS | wxPATH_NORM_ABSOLUTE);
    const wxString baseDirectory = workspaceFile.GetPath();

    std::vector<WorkspaceFolder> loaded;
    for (const auto& item : root["folders"])
    {
        std::string rawPath;
        std::string rawName;
        if (item.isString())
            rawPath = item.asString();
        else if (item.isObject())
        {
            rawPath = item.get("path", "").asString();
            rawName = item.get("name", "").asString();
        }

        if (rawPath.empty())
            continue;

        wxString folderPath = FromUtf8(rawPath);
        wxFileName folder(folderPath, wxEmptyString);
        if (folder.IsRelative())
            folder.MakeAbsolute(baseDirectory);
        folder.Normalize(wxPATH_NORM_DOTS | wxPATH_NORM_ABSOLUTE);
        folderPath = folder.GetPath();
        if (!wxDirExists(folderPath))
            continue;

        bool duplicate = false;
        for (const auto& existing : loaded)
        {
#ifdef __WXMSW__
            duplicate = existing.path.CmpNoCase(folderPath) == 0;
#else
            duplicate = existing.path == folderPath;
#endif
            if (duplicate)
                break;
        }
        if (duplicate)
            continue;

        wxString name = FromUtf8(rawName);
        if (name.IsEmpty())
            name = DefaultFolderName(folderPath);
        loaded.push_back({folderPath, name});
    }

    if (loaded.empty())
    {
        if (error)
            *error = "No existing folders from the workspace could be loaded.";
        return false;
    }

    m_folders = std::move(loaded);
    m_workspaceFile = workspaceFile.GetFullPath();
    return true;
}

bool WorkspaceManager::SaveWorkspace(const wxString& filePath, wxString* error)
{
    if (m_folders.empty())
    {
        if (error)
            *error = "There are no folders in the workspace.";
        return false;
    }

    wxFileName workspaceFile(filePath);
    workspaceFile.Normalize(wxPATH_NORM_DOTS | wxPATH_NORM_ABSOLUTE);
    const wxString baseDirectory = workspaceFile.GetPath();

    Json::Value root(Json::objectValue);
    root["version"] = 1;
    root["folders"] = Json::Value(Json::arrayValue);

    for (const auto& folder : m_folders)
    {
        wxFileName relative(folder.path, wxEmptyString);
        wxString storedPath = folder.path;
        if (!baseDirectory.IsEmpty() && relative.MakeRelativeTo(baseDirectory))
            storedPath = relative.GetPath();

        Json::Value item(Json::objectValue);
        item["path"] = ToUtf8(storedPath);
        if (!folder.name.IsEmpty() && folder.name != DefaultFolderName(folder.path))
            item["name"] = ToUtf8(folder.name);
        root["folders"].append(item);
    }

    Json::StreamWriterBuilder writer;
    writer["indentation"] = "  ";
    const std::string json = Json::writeString(writer, root) + "\n";

    wxFFile output(workspaceFile.GetFullPath(), "wb");
    if (!output.IsOpened() || !output.Write(wxString::FromUTF8(json.c_str())))
    {
        if (error)
            *error = "Could not write workspace file.";
        return false;
    }

    m_workspaceFile = workspaceFile.GetFullPath();
    return true;
}
