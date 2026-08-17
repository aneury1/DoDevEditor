#define DODEV_PLUGIN_BUILD 1
#include "plugin/DoDevPluginAPI.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cctype>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <fstream>
#include <iomanip>
#include <memory>
#include <mutex>
#include <regex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#ifdef _WIN32
#  define NOMINMAX
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  include <windows.h>
using SocketHandle = SOCKET;
using SocketLength = int;
static constexpr SocketHandle kInvalidSocket = INVALID_SOCKET;
#else
#  include <arpa/inet.h>
#  include <dlfcn.h>
#  include <errno.h>
#  include <netdb.h>
#  include <sys/select.h>
#  include <sys/socket.h>
#  include <sys/types.h>
#  include <unistd.h>
using SocketHandle = int;
using SocketLength = socklen_t;
static constexpr SocketHandle kInvalidSocket = -1;
#endif

namespace
{
constexpr uint16_t kDefaultPort = 9934;
constexpr size_t kDefaultMaxTextBytes = 4u * 1024u * 1024u;
constexpr size_t kMaximumRequestHeader = 64u * 1024u;
constexpr uint64_t kMaximumWebSocketPayload = 64u * 1024u;
constexpr size_t kEventHistoryLimit = 4096u;
constexpr const char* kWebSocketGuid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

struct ServerConfig
{
    std::string bindAddress = "0.0.0.0";
    uint16_t port = kDefaultPort;
    std::string authToken;
    size_t maxTextBytes = kDefaultMaxTextBytes;
};

struct FileSnapshot
{
    bool open = false;
    DoDevHandle editor = nullptr;
    std::string room;
    std::string title;
    std::string path;
    std::string text;
    size_t caret = 0;
    size_t selectionStart = 0;
    size_t selectionEnd = 0;
    bool truncated = false;
    uint64_t generation = 0;
};

struct FileEvent
{
    uint64_t sequence = 0;
    std::string type;
    std::string room;
    FileSnapshot snapshot;
};

struct State
{
    const DoDevHostApi* host = nullptr;
    std::mutex mutex;
    std::condition_variable changed;
    ServerConfig config;
    std::string configPath;
    std::unordered_map<std::string, FileSnapshot> rooms;
    std::unordered_map<uintptr_t, std::string> roomByHandle;
    std::string activeRoom;
    std::deque<FileEvent> events;
    uint64_t eventSequence = 0;

    std::atomic<bool> stopping{false};
    std::atomic<bool> running{false};
    SocketHandle listener = kInvalidSocket;
    std::thread acceptThread;
    std::mutex clientsMutex;
    std::vector<std::thread> clientThreads;
    std::unordered_set<SocketHandle> liveSockets;
    std::atomic<uint64_t> nextClientId{1};
    std::atomic<uint64_t> activeHttpClients{0};
    std::atomic<uint64_t> activeWebSocketClients{0};
    DoDevHandle statusPanel = nullptr;
#ifdef _WIN32
    bool winsockInitialized = false;
#endif
};

void ModuleAddressMarker() {}

void CloseSocket(SocketHandle socket)
{
    if (socket == kInvalidSocket)
        return;
#ifdef _WIN32
    closesocket(socket);
#else
    close(socket);
#endif
}

void ShutdownSocket(SocketHandle socket)
{
    if (socket == kInvalidSocket)
        return;
#ifdef _WIN32
    shutdown(socket, SD_BOTH);
#else
    shutdown(socket, SHUT_RDWR);
#endif
}

std::string DirectoryName(const std::string& path)
{
    const size_t slash = path.find_last_of("/\\");
    if (slash == std::string::npos)
        return ".";
    if (slash == 0)
        return path.substr(0, 1);
    return path.substr(0, slash);
}

std::string JoinPath(const std::string& directory, const std::string& name)
{
    if (directory.empty() || directory == ".")
        return directory.empty() ? name : directory + "/" + name;
    const char last = directory.back();
    if (last == '/' || last == '\\')
        return directory + name;
#ifdef _WIN32
    return directory + "\\" + name;
#else
    return directory + "/" + name;
#endif
}

std::string CurrentModulePath()
{
#ifdef _WIN32
    HMODULE module = nullptr;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCSTR>(&ModuleAddressMarker), &module))
        return {};
    std::vector<char> buffer(4096, 0);
    const DWORD length = GetModuleFileNameA(module, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= buffer.size())
        return {};
    return std::string(buffer.data(), length);
#else
    Dl_info info{};
    if (dladdr(reinterpret_cast<void*>(&ModuleAddressMarker), &info) == 0 || !info.dli_fname)
        return {};
    return info.dli_fname;
#endif
}

void Log(State* state, DoDevLogLevel level, const std::string& message)
{
    if (state && state->host && state->host->log)
        state->host->log(state->host->context, level, message.c_str());
}

std::string Trim(std::string value)
{
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())))
        value.erase(value.begin());
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
        value.pop_back();
    return value;
}

std::string Lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::string JsonEscape(const std::string& value)
{
    std::ostringstream out;
    for (unsigned char c : value)
    {
        switch (c)
        {
            case '\\': out << "\\\\"; break;
            case '"': out << "\\\""; break;
            case '\b': out << "\\b"; break;
            case '\f': out << "\\f"; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default:
                if (c < 0x20)
                {
                    out << "\\u" << std::hex << std::uppercase << std::setw(4)
                        << std::setfill('0') << static_cast<int>(c)
                        << std::dec << std::nouppercase << std::setfill(' ');
                }
                else
                    out << static_cast<char>(c);
                break;
        }
    }
    return out.str();
}

std::string ReadHostString(size_t (*reader)(void*, DoDevHandle, char*, size_t),
                           void* context,
                           DoDevHandle handle)
{
    if (!reader || !handle)
        return {};
    const size_t required = reader(context, handle, nullptr, 0);
    if (required == 0)
        return {};
    std::vector<char> buffer(required + 1, 0);
    reader(context, handle, buffer.data(), buffer.size());
    return std::string(buffer.data());
}

std::string ReadTabString(size_t (*reader)(void*, int, char*, size_t),
                          void* context,
                          int index)
{
    if (!reader || index < 0)
        return {};
    const size_t required = reader(context, index, nullptr, 0);
    if (required == 0)
        return {};
    std::vector<char> buffer(required + 1, 0);
    reader(context, index, buffer.data(), buffer.size());
    return std::string(buffer.data());
}

uint64_t Fnv1a64(const std::string& text)
{
    uint64_t hash = 1469598103934665603ull;
    for (unsigned char c : text)
    {
        hash ^= static_cast<uint64_t>(c);
        hash *= 1099511628211ull;
    }
    return hash;
}

std::string Hex64(uint64_t value)
{
    std::ostringstream out;
    out << std::hex << std::setfill('0') << std::setw(16) << value;
    return out.str();
}

std::string MakeRoomId(const std::string& path, DoDevHandle editor)
{
    if (!path.empty())
        return "file:" + Hex64(Fnv1a64(path));
    const uintptr_t address = reinterpret_cast<uintptr_t>(editor);
    return "untitled:" + Hex64(static_cast<uint64_t>(address));
}

std::string SnapshotJson(const FileSnapshot& snapshot)
{
    std::ostringstream out;
    out << "{\"room\":\"" << JsonEscape(snapshot.room) << "\""
        << ",\"open\":" << (snapshot.open ? "true" : "false")
        << ",\"generation\":" << snapshot.generation
        << ",\"title\":\"" << JsonEscape(snapshot.title) << "\""
        << ",\"path\":\"" << JsonEscape(snapshot.path) << "\""
        << ",\"caret\":" << snapshot.caret
        << ",\"selection\":{\"start\":" << snapshot.selectionStart
        << ",\"end\":" << snapshot.selectionEnd << "}"
        << ",\"truncated\":" << (snapshot.truncated ? "true" : "false")
        << ",\"text\":\"" << JsonEscape(snapshot.text) << "\"}";
    return out.str();
}

std::string RoomSummaryJson(const FileSnapshot& snapshot, bool active)
{
    std::ostringstream out;
    out << "{\"room\":\"" << JsonEscape(snapshot.room) << "\""
        << ",\"channel\":\"" << JsonEscape(snapshot.room) << "\""
        << ",\"title\":\"" << JsonEscape(snapshot.title) << "\""
        << ",\"path\":\"" << JsonEscape(snapshot.path) << "\""
        << ",\"active\":" << (active ? "true" : "false")
        << ",\"generation\":" << snapshot.generation
        << ",\"websocket_path\":\"/ws/v1/files/" << JsonEscape(snapshot.room) << "\"}";
    return out.str();
}

void PushEventLocked(State* state,
                     const std::string& type,
                     const std::string& room,
                     const FileSnapshot& snapshot)
{
    FileEvent event;
    event.sequence = ++state->eventSequence;
    event.type = type;
    event.room = room;
    event.snapshot = snapshot;
    state->events.push_back(std::move(event));
    while (state->events.size() > kEventHistoryLimit)
        state->events.pop_front();
}

FileSnapshot CaptureEditor(State* state, DoDevHandle editor, int tabIndex)
{
    FileSnapshot next;
    next.open = true;
    next.editor = editor;
    next.path = ReadHostString(state->host->editor_get_path, state->host->context, editor);
    next.title = ReadTabString(state->host->tab_title, state->host->context, tabIndex);
    next.room = MakeRoomId(next.path, editor);
    next.caret = state->host->editor_get_caret
                     ? state->host->editor_get_caret(state->host->context, editor)
                     : 0;
    if (state->host->editor_get_selection)
        state->host->editor_get_selection(state->host->context, editor,
                                          &next.selectionStart, &next.selectionEnd);

    size_t maxBytes = kDefaultMaxTextBytes;
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        maxBytes = std::max<size_t>(1, state->config.maxTextBytes);
    }
    if (state->host->editor_get_text)
    {
        const size_t required = state->host->editor_get_text(state->host->context, editor, nullptr, 0);
        if (required > 0)
        {
            const size_t capacity = std::min(required + 1, maxBytes + 1);
            std::vector<char> buffer(capacity, 0);
            state->host->editor_get_text(state->host->context, editor, buffer.data(), buffer.size());
            size_t length = 0;
            while (length < buffer.size() && buffer[length] != '\0')
                ++length;
            next.text.assign(buffer.data(), length);
            next.truncated = required > (maxBytes + 1);
        }
    }
    return next;
}

void UpsertEditor(State* state,
                  DoDevHandle editor,
                  int tabIndex,
                  const std::string& eventType,
                  bool emitEvent)
{
    if (!state || !state->host || !editor ||
        (state->host->is_editor && !state->host->is_editor(state->host->context, editor)))
        return;

    FileSnapshot next = CaptureEditor(state, editor, tabIndex);
    const uintptr_t handleKey = reinterpret_cast<uintptr_t>(editor);
    bool roomMigrated = false;
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        const auto oldRoomIt = state->roomByHandle.find(handleKey);
        if (oldRoomIt != state->roomByHandle.end() && oldRoomIt->second != next.room)
        {
            roomMigrated = true;
            const auto oldIt = state->rooms.find(oldRoomIt->second);
            if (oldIt != state->rooms.end())
            {
                FileSnapshot closed = oldIt->second;
                closed.open = false;
                PushEventLocked(state, "file.closed", closed.room, closed);
                state->rooms.erase(oldIt);
            }
        }

        const auto current = state->rooms.find(next.room);
        next.generation = current == state->rooms.end() ? 1 : current->second.generation + 1;
        state->rooms[next.room] = next;
        state->roomByHandle[handleKey] = next.room;

        if (state->host->active_editor &&
            state->host->active_editor(state->host->context) == editor)
            state->activeRoom = next.room;

        if (emitEvent)
            PushEventLocked(state, roomMigrated ? "file.opened" : eventType, next.room, next);
    }
    state->changed.notify_all();
}

void CloseEditorRoom(State* state, DoDevHandle editor)
{
    if (!state || !editor)
        return;
    const uintptr_t handleKey = reinterpret_cast<uintptr_t>(editor);
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        const auto handleIt = state->roomByHandle.find(handleKey);
        if (handleIt == state->roomByHandle.end())
            return;
        const std::string room = handleIt->second;
        const auto roomIt = state->rooms.find(room);
        if (roomIt != state->rooms.end())
        {
            FileSnapshot closed = roomIt->second;
            closed.open = false;
            PushEventLocked(state, "file.closed", room, closed);
            state->rooms.erase(roomIt);
        }
        state->roomByHandle.erase(handleIt);
        if (state->activeRoom == room)
            state->activeRoom.clear();
    }
    state->changed.notify_all();
}

void SetActiveEditor(State* state, DoDevHandle editor, int tabIndex)
{
    if (!state)
        return;
    if (!editor || (state->host->is_editor && !state->host->is_editor(state->host->context, editor)))
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        state->activeRoom.clear();
        FileSnapshot empty;
        PushEventLocked(state, "editor.active_changed", "", empty);
        state->changed.notify_all();
        return;
    }

    UpsertEditor(state, editor, tabIndex, "file.changed", false);
    std::lock_guard<std::mutex> lock(state->mutex);
    const auto it = state->roomByHandle.find(reinterpret_cast<uintptr_t>(editor));
    if (it != state->roomByHandle.end())
    {
        state->activeRoom = it->second;
        const auto roomIt = state->rooms.find(it->second);
        if (roomIt != state->rooms.end())
            PushEventLocked(state, "editor.active_changed", it->second, roomIt->second);
    }
    state->changed.notify_all();
}

void SyncOpenEditors(State* state)
{
    if (!state || !state->host || !state->host->tab_count || !state->host->tab_at)
        return;
    const int count = state->host->tab_count(state->host->context);
    for (int i = 0; i < count; ++i)
    {
        DoDevHandle handle = state->host->tab_at(state->host->context, i);
        if (handle && (!state->host->is_editor || state->host->is_editor(state->host->context, handle)))
            UpsertEditor(state, handle, i, "file.opened", false);
    }
    if (state->host->active_editor && state->host->active_tab_index)
        SetActiveEditor(state,
                        state->host->active_editor(state->host->context),
                        state->host->active_tab_index(state->host->context));
}

std::string ReadFile(const std::string& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
        return {};
    std::ostringstream out;
    out << input.rdbuf();
    return out.str();
}

void WriteDefaultConfig(const std::string& path)
{
    std::ifstream existing(path, std::ios::binary);
    if (existing.good())
        return;
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output)
        return;
    output << "{\n"
              "  \"bind_address\": \"0.0.0.0\",\n"
              "  \"port\": 9934,\n"
              "  \"auth_token\": \"\",\n"
              "  \"max_text_bytes\": 4194304\n"
              "}\n";
}

bool RegexString(const std::string& text, const char* key, std::string* value)
{
    const std::regex pattern(std::string("\\\"") + key + "\\\"\\s*:\\s*\\\"([^\\\"]*)\\\"");
    std::smatch match;
    if (!std::regex_search(text, match, pattern) || match.size() < 2)
        return false;
    *value = match[1].str();
    return true;
}

bool RegexUnsigned(const std::string& text, const char* key, uint64_t* value)
{
    const std::regex pattern(std::string("\\\"") + key + "\\\"\\s*:\\s*([0-9]+)");
    std::smatch match;
    if (!std::regex_search(text, match, pattern) || match.size() < 2)
        return false;
    try
    {
        *value = std::stoull(match[1].str());
        return true;
    }
    catch (...)
    {
        return false;
    }
}

ServerConfig LoadConfig(State* state)
{
    ServerConfig config;
    WriteDefaultConfig(state->configPath);
    const std::string text = ReadFile(state->configPath);
    std::string value;
    uint64_t number = 0;
    if (RegexString(text, "bind_address", &value) && !value.empty())
        config.bindAddress = value;
    if (RegexUnsigned(text, "port", &number) && number > 0 && number <= 65535)
        config.port = static_cast<uint16_t>(number);
    if (RegexString(text, "auth_token", &value))
        config.authToken = value;
    if (RegexUnsigned(text, "max_text_bytes", &number) && number > 0)
        config.maxTextBytes = static_cast<size_t>(std::min<uint64_t>(number, 256ull * 1024ull * 1024ull));

    if (const char* env = std::getenv("DODEV_HTTP_BIND"))
        if (*env) config.bindAddress = env;
    if (const char* env = std::getenv("DODEV_HTTP_PORT"))
    {
        try
        {
            const unsigned long port = std::stoul(env);
            if (port > 0 && port <= 65535)
                config.port = static_cast<uint16_t>(port);
        }
        catch (...) {}
    }
    if (const char* env = std::getenv("DODEV_HTTP_TOKEN"))
        config.authToken = env;
    if (const char* env = std::getenv("DODEV_HTTP_MAX_TEXT_BYTES"))
    {
        try
        {
            const unsigned long long bytes = std::stoull(env);
            if (bytes > 0)
                config.maxTextBytes = static_cast<size_t>(std::min<unsigned long long>(bytes, 256ull * 1024ull * 1024ull));
        }
        catch (...) {}
    }
    return config;
}

bool SendAll(SocketHandle socket, const char* data, size_t size)
{
    size_t sent = 0;
    while (sent < size)
    {
#ifdef _WIN32
        const int chunk = send(socket, data + sent,
                               static_cast<int>(std::min<size_t>(size - sent, 1u << 20)), 0);
#else
        const ssize_t chunk = send(socket, data + sent, size - sent,
#  ifdef MSG_NOSIGNAL
                                   MSG_NOSIGNAL
#  else
                                   0
#  endif
        );
#endif
        if (chunk <= 0)
            return false;
        sent += static_cast<size_t>(chunk);
    }
    return true;
}

bool SendAll(SocketHandle socket, const std::string& data)
{
    return SendAll(socket, data.data(), data.size());
}

bool RecvExact(SocketHandle socket, void* out, size_t size)
{
    auto* bytes = static_cast<unsigned char*>(out);
    size_t received = 0;
    while (received < size)
    {
#ifdef _WIN32
        const int count = recv(socket, reinterpret_cast<char*>(bytes + received),
                               static_cast<int>(size - received), 0);
#else
        const ssize_t count = recv(socket, bytes + received, size - received, 0);
#endif
        if (count <= 0)
            return false;
        received += static_cast<size_t>(count);
    }
    return true;
}

struct HttpRequest
{
    std::string method;
    std::string target;
    std::string path;
    std::string query;
    std::unordered_map<std::string, std::string> headers;
};

bool ReceiveRequest(SocketHandle socket, HttpRequest* request)
{
    std::string data;
    data.reserve(4096);
    char buffer[4096];
    while (data.size() < kMaximumRequestHeader)
    {
#ifdef _WIN32
        const int count = recv(socket, buffer, sizeof(buffer), 0);
#else
        const ssize_t count = recv(socket, buffer, sizeof(buffer), 0);
#endif
        if (count <= 0)
            return false;
        data.append(buffer, static_cast<size_t>(count));
        if (data.find("\r\n\r\n") != std::string::npos)
            break;
    }
    const size_t end = data.find("\r\n\r\n");
    if (end == std::string::npos)
        return false;
    const size_t firstLineEnd = data.find("\r\n");
    if (firstLineEnd == std::string::npos)
        return false;

    std::istringstream firstLine(data.substr(0, firstLineEnd));
    std::string version;
    firstLine >> request->method >> request->target >> version;
    if (request->method.empty() || request->target.empty())
        return false;

    const size_t queryAt = request->target.find('?');
    request->path = request->target.substr(0, queryAt);
    request->query = queryAt == std::string::npos ? std::string() : request->target.substr(queryAt + 1);

    size_t lineStart = firstLineEnd + 2;
    while (lineStart < end)
    {
        const size_t lineEnd = data.find("\r\n", lineStart);
        const size_t actualEnd = lineEnd == std::string::npos || lineEnd > end ? end : lineEnd;
        const std::string line = data.substr(lineStart, actualEnd - lineStart);
        const size_t colon = line.find(':');
        if (colon != std::string::npos)
            request->headers[Lower(Trim(line.substr(0, colon)))] = Trim(line.substr(colon + 1));
        if (actualEnd == end)
            break;
        lineStart = actualEnd + 2;
    }
    return true;
}

std::string Header(const HttpRequest& request, const std::string& name)
{
    const auto it = request.headers.find(Lower(name));
    return it == request.headers.end() ? std::string() : it->second;
}

int HexDigit(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

std::string UrlDecode(const std::string& value)
{
    std::string out;
    out.reserve(value.size());
    for (size_t i = 0; i < value.size(); ++i)
    {
        if (value[i] == '%' && i + 2 < value.size())
        {
            const int hi = HexDigit(value[i + 1]);
            const int lo = HexDigit(value[i + 2]);
            if (hi >= 0 && lo >= 0)
            {
                out.push_back(static_cast<char>((hi << 4) | lo));
                i += 2;
                continue;
            }
        }
        out.push_back(value[i] == '+' ? ' ' : value[i]);
    }
    return out;
}

std::string QueryParam(const std::string& query, const std::string& key)
{
    size_t begin = 0;
    while (begin <= query.size())
    {
        const size_t amp = query.find('&', begin);
        const std::string item = query.substr(begin, amp == std::string::npos ? std::string::npos : amp - begin);
        const size_t equal = item.find('=');
        const std::string name = UrlDecode(item.substr(0, equal));
        if (name == key)
            return equal == std::string::npos ? std::string() : UrlDecode(item.substr(equal + 1));
        if (amp == std::string::npos)
            break;
        begin = amp + 1;
    }
    return {};
}

bool Authorized(const ServerConfig& config, const HttpRequest& request)
{
    if (config.authToken.empty())
        return true;
    const std::string custom = Header(request, "x-dodev-token");
    if (custom == config.authToken)
        return true;
    const std::string authorization = Header(request, "authorization");
    if (authorization.size() >= 7 && Lower(authorization.substr(0, 7)) == "bearer " &&
        authorization.substr(7) == config.authToken)
        return true;
    return QueryParam(request.query, "token") == config.authToken;
}

void SendSimple(SocketHandle socket,
                int status,
                const char* reason,
                const char* contentType,
                const std::string& body)
{
    std::ostringstream response;
    response << "HTTP/1.1 " << status << ' ' << reason << "\r\n"
             << "Content-Type: " << contentType << "\r\n"
             << "Content-Length: " << body.size() << "\r\n"
             << "Cache-Control: no-store\r\n"
             << "Connection: close\r\n"
             << "Access-Control-Allow-Origin: *\r\n\r\n"
             << body;
    SendAll(socket, response.str());
}

std::array<unsigned char, 20> Sha1(const std::string& input)
{
    uint64_t bitLength = static_cast<uint64_t>(input.size()) * 8ull;
    std::vector<unsigned char> message(input.begin(), input.end());
    message.push_back(0x80);
    while ((message.size() % 64) != 56)
        message.push_back(0);
    for (int shift = 56; shift >= 0; shift -= 8)
        message.push_back(static_cast<unsigned char>((bitLength >> shift) & 0xFF));

    uint32_t h0 = 0x67452301u;
    uint32_t h1 = 0xEFCDAB89u;
    uint32_t h2 = 0x98BADCFEu;
    uint32_t h3 = 0x10325476u;
    uint32_t h4 = 0xC3D2E1F0u;

    auto rol = [](uint32_t value, unsigned shift) {
        return static_cast<uint32_t>((value << shift) | (value >> (32u - shift)));
    };

    for (size_t offset = 0; offset < message.size(); offset += 64)
    {
        uint32_t words[80]{};
        for (size_t i = 0; i < 16; ++i)
        {
            const size_t p = offset + i * 4;
            words[i] = (static_cast<uint32_t>(message[p]) << 24) |
                       (static_cast<uint32_t>(message[p + 1]) << 16) |
                       (static_cast<uint32_t>(message[p + 2]) << 8) |
                       static_cast<uint32_t>(message[p + 3]);
        }
        for (size_t i = 16; i < 80; ++i)
            words[i] = rol(words[i - 3] ^ words[i - 8] ^ words[i - 14] ^ words[i - 16], 1);

        uint32_t a = h0, b = h1, c = h2, d = h3, e = h4;
        for (size_t i = 0; i < 80; ++i)
        {
            uint32_t f = 0, k = 0;
            if (i < 20) { f = (b & c) | ((~b) & d); k = 0x5A827999u; }
            else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1u; }
            else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDCu; }
            else { f = b ^ c ^ d; k = 0xCA62C1D6u; }
            const uint32_t temp = rol(a, 5) + f + e + k + words[i];
            e = d;
            d = c;
            c = rol(b, 30);
            b = a;
            a = temp;
        }
        h0 += a; h1 += b; h2 += c; h3 += d; h4 += e;
    }

    std::array<unsigned char, 20> digest{};
    const uint32_t values[5] = {h0, h1, h2, h3, h4};
    for (size_t i = 0; i < 5; ++i)
    {
        digest[i * 4] = static_cast<unsigned char>((values[i] >> 24) & 0xFF);
        digest[i * 4 + 1] = static_cast<unsigned char>((values[i] >> 16) & 0xFF);
        digest[i * 4 + 2] = static_cast<unsigned char>((values[i] >> 8) & 0xFF);
        digest[i * 4 + 3] = static_cast<unsigned char>(values[i] & 0xFF);
    }
    return digest;
}

std::string Base64(const unsigned char* data, size_t size)
{
    static constexpr char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((size + 2) / 3) * 4);
    for (size_t i = 0; i < size; i += 3)
    {
        const uint32_t a = data[i];
        const uint32_t b = i + 1 < size ? data[i + 1] : 0;
        const uint32_t c = i + 2 < size ? data[i + 2] : 0;
        const uint32_t triple = (a << 16) | (b << 8) | c;
        out.push_back(alphabet[(triple >> 18) & 0x3F]);
        out.push_back(alphabet[(triple >> 12) & 0x3F]);
        out.push_back(i + 1 < size ? alphabet[(triple >> 6) & 0x3F] : '=');
        out.push_back(i + 2 < size ? alphabet[triple & 0x3F] : '=');
    }
    return out;
}

bool IsWebSocketUpgrade(const HttpRequest& request)
{
    return request.method == "GET" &&
           Lower(Header(request, "upgrade")) == "websocket" &&
           Lower(Header(request, "connection")).find("upgrade") != std::string::npos &&
           Header(request, "sec-websocket-version") == "13" &&
           !Header(request, "sec-websocket-key").empty();
}

bool SendWebSocketHandshake(SocketHandle socket, const HttpRequest& request)
{
    const std::string source = Header(request, "sec-websocket-key") + kWebSocketGuid;
    const auto digest = Sha1(source);
    const std::string accept = Base64(digest.data(), digest.size());
    std::ostringstream response;
    response << "HTTP/1.1 101 Switching Protocols\r\n"
             << "Upgrade: websocket\r\n"
             << "Connection: Upgrade\r\n"
             << "Sec-WebSocket-Accept: " << accept << "\r\n\r\n";
    return SendAll(socket, response.str());
}

bool SendWsFrame(SocketHandle socket, uint8_t opcode, const std::string& payload)
{
    std::vector<unsigned char> frame;
    frame.reserve(payload.size() + 16);
    frame.push_back(static_cast<unsigned char>(0x80u | (opcode & 0x0Fu)));
    const uint64_t length = payload.size();
    if (length <= 125)
    {
        frame.push_back(static_cast<unsigned char>(length));
    }
    else if (length <= 65535)
    {
        frame.push_back(126);
        frame.push_back(static_cast<unsigned char>((length >> 8) & 0xFF));
        frame.push_back(static_cast<unsigned char>(length & 0xFF));
    }
    else
    {
        frame.push_back(127);
        for (int shift = 56; shift >= 0; shift -= 8)
            frame.push_back(static_cast<unsigned char>((length >> shift) & 0xFF));
    }
    frame.insert(frame.end(), payload.begin(), payload.end());
    return SendAll(socket, reinterpret_cast<const char*>(frame.data()), frame.size());
}

struct WsFrame
{
    uint8_t opcode = 0;
    std::string payload;
};

bool ReceiveWsFrame(SocketHandle socket, WsFrame* frame, std::string* error)
{
    unsigned char first[2]{};
    if (!RecvExact(socket, first, sizeof(first)))
        return false;
    const bool fin = (first[0] & 0x80u) != 0;
    const uint8_t opcode = first[0] & 0x0Fu;
    const bool masked = (first[1] & 0x80u) != 0;
    uint64_t length = first[1] & 0x7Fu;
    if (!fin)
    {
        *error = "fragmented frames are not supported";
        return false;
    }
    if (!masked)
    {
        *error = "client websocket frames must be masked";
        return false;
    }
    if (length == 126)
    {
        unsigned char ext[2]{};
        if (!RecvExact(socket, ext, 2)) return false;
        length = (static_cast<uint64_t>(ext[0]) << 8) | ext[1];
    }
    else if (length == 127)
    {
        unsigned char ext[8]{};
        if (!RecvExact(socket, ext, 8)) return false;
        length = 0;
        for (unsigned char byte : ext)
            length = (length << 8) | byte;
    }
    if (length > kMaximumWebSocketPayload)
    {
        *error = "websocket control message exceeds 64 KiB";
        return false;
    }
    if ((opcode & 0x08u) != 0 && length > 125)
    {
        *error = "invalid websocket control frame";
        return false;
    }

    unsigned char mask[4]{};
    if (!RecvExact(socket, mask, 4))
        return false;
    frame->payload.assign(static_cast<size_t>(length), '\0');
    if (length > 0 && !RecvExact(socket, &frame->payload[0], static_cast<size_t>(length)))
        return false;
    for (size_t i = 0; i < frame->payload.size(); ++i)
        frame->payload[i] = static_cast<char>(static_cast<unsigned char>(frame->payload[i]) ^ mask[i % 4]);
    frame->opcode = opcode;
    return true;
}

bool WaitReadable(SocketHandle socket, int milliseconds)
{
    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(socket, &readSet);
    timeval timeout{};
    timeout.tv_sec = milliseconds / 1000;
    timeout.tv_usec = (milliseconds % 1000) * 1000;
#ifdef _WIN32
    const int result = select(0, &readSet, nullptr, nullptr, &timeout);
#else
    const int result = select(socket + 1, &readSet, nullptr, nullptr, &timeout);
#endif
    return result > 0 && FD_ISSET(socket, &readSet);
}

std::string JsonStringField(const std::string& json, const char* field)
{
    const std::regex pattern(std::string("\\\"") + field + "\\\"\\s*:\\s*\\\"([^\\\"]*)\\\"");
    std::smatch match;
    if (std::regex_search(json, match, pattern) && match.size() >= 2)
        return match[1].str();
    return {};
}

std::string RoomsJson(State* state)
{
    std::lock_guard<std::mutex> lock(state->mutex);
    std::vector<std::string> keys;
    keys.reserve(state->rooms.size());
    for (const auto& item : state->rooms)
        keys.push_back(item.first);
    std::sort(keys.begin(), keys.end());
    std::ostringstream out;
    out << "{\"rooms\":[";
    bool first = true;
    for (const std::string& key : keys)
    {
        const auto it = state->rooms.find(key);
        if (it == state->rooms.end()) continue;
        if (!first) out << ',';
        first = false;
        out << RoomSummaryJson(it->second, key == state->activeRoom);
    }
    out << "],\"count\":" << keys.size() << "}";
    return out.str();
}

bool RoomSnapshot(State* state, const std::string& room, FileSnapshot* snapshot)
{
    std::lock_guard<std::mutex> lock(state->mutex);
    const auto it = state->rooms.find(room);
    if (it == state->rooms.end())
        return false;
    *snapshot = it->second;
    return true;
}

std::vector<FileSnapshot> AllRoomSnapshots(State* state)
{
    std::lock_guard<std::mutex> lock(state->mutex);
    std::vector<FileSnapshot> result;
    result.reserve(state->rooms.size());
    for (const auto& item : state->rooms)
        result.push_back(item.second);
    std::sort(result.begin(), result.end(), [](const FileSnapshot& a, const FileSnapshot& b)
    {
        return a.room < b.room;
    });
    return result;
}

FileSnapshot ActiveSnapshot(State* state)
{
    std::lock_guard<std::mutex> lock(state->mutex);
    const auto it = state->rooms.find(state->activeRoom);
    return it == state->rooms.end() ? FileSnapshot{} : it->second;
}

std::string WsEnvelope(const std::string& type,
                       const std::string& room,
                       const std::string& dataJson)
{
    std::ostringstream out;
    out << "{\"type\":\"" << JsonEscape(type) << "\"";
    if (!room.empty())
        out << ",\"room\":\"" << JsonEscape(room) << "\""
            << ",\"channel\":\"" << JsonEscape(room) << "\"";
    if (!dataJson.empty())
        out << ",\"data\":" << dataJson;
    out << '}';
    return out.str();
}

void LogWs(State* state,
           uint64_t clientId,
           const char* direction,
           const std::string& type,
           const std::string& room,
           size_t bytes)
{
    std::ostringstream out;
    out << "WebSocket client=" << clientId << ' ' << direction
        << " type=" << type;
    if (!room.empty()) out << " room=" << room;
    out << " bytes=" << bytes;
    Log(state, DODEV_LOG_INFO, out.str());
}

bool SendWsMessage(State* state,
                   SocketHandle socket,
                   uint64_t clientId,
                   const std::string& type,
                   const std::string& room,
                   const std::string& json)
{
    LogWs(state, clientId, "send", type, room, json.size());
    return SendWsFrame(socket, 0x1, json);
}

void SendSubscribedSnapshot(State* state,
                            SocketHandle socket,
                            uint64_t clientId,
                            const std::string& room)
{
    FileSnapshot snapshot;
    if (!RoomSnapshot(state, room, &snapshot))
    {
        const std::string error = WsEnvelope("error", room, "{\"code\":\"room_not_found\"}");
        SendWsMessage(state, socket, clientId, "error", room, error);
        return;
    }
    const std::string message = WsEnvelope("file.snapshot", room, SnapshotJson(snapshot));
    SendWsMessage(state, socket, clientId, "file.snapshot", room, message);
}

void WebSocketSession(State* state,
                      SocketHandle socket,
                      const HttpRequest& request,
                      uint64_t clientId,
                      const std::string& initialRoom)
{
    if (!SendWebSocketHandshake(socket, request))
        return;

    state->activeWebSocketClients.fetch_add(1);
    Log(state, DODEV_LOG_INFO, "WebSocket client=" + std::to_string(clientId) + " connected path=" + request.path);

    std::unordered_set<std::string> subscriptions;
    if (!initialRoom.empty())
    {
        FileSnapshot snapshot;
        if (RoomSnapshot(state, initialRoom, &snapshot))
            subscriptions.insert(initialRoom);
    }

    const std::string readyData = std::string("{\"protocol\":\"dodev.files.v1\",\"client_id\":") +
                                  std::to_string(clientId) + ",\"rooms\":" + RoomsJson(state) + "}";
    const std::string ready = WsEnvelope("session.ready", "", readyData);
    if (!SendWsMessage(state, socket, clientId, "session.ready", "", ready))
    {
        state->activeWebSocketClients.fetch_sub(1);
        return;
    }
    if (!initialRoom.empty() && subscriptions.empty())
    {
        const std::string error = WsEnvelope("error", initialRoom, "{\"code\":\"room_not_found\"}");
        SendWsMessage(state, socket, clientId, "error", initialRoom, error);
        state->activeWebSocketClients.fetch_sub(1);
        Log(state, DODEV_LOG_WARNING, "WebSocket client=" + std::to_string(clientId) +
                                     " direct room not found: " + initialRoom);
        return;
    }
    for (const std::string& room : subscriptions)
        SendSubscribedSnapshot(state, socket, clientId, room);

    uint64_t lastSequence = 0;
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        lastSequence = state->eventSequence;
    }

    bool keepRunning = true;
    while (keepRunning && !state->stopping.load())
    {
        if (WaitReadable(socket, 150))
        {
            WsFrame frame;
            std::string error;
            if (!ReceiveWsFrame(socket, &frame, &error))
            {
                if (!error.empty())
                    Log(state, DODEV_LOG_WARNING,
                        "WebSocket client=" + std::to_string(clientId) + " protocol error: " + error);
                break;
            }
            if (frame.opcode == 0x8)
            {
                LogWs(state, clientId, "recv", "close", "", frame.payload.size());
                SendWsFrame(socket, 0x8, frame.payload);
                break;
            }
            if (frame.opcode == 0x9)
            {
                LogWs(state, clientId, "recv", "ping", "", frame.payload.size());
                SendWsFrame(socket, 0xA, frame.payload);
                LogWs(state, clientId, "send", "pong", "", frame.payload.size());
                continue;
            }
            if (frame.opcode == 0xA)
            {
                LogWs(state, clientId, "recv", "pong", "", frame.payload.size());
                continue;
            }
            if (frame.opcode != 0x1)
            {
                LogWs(state, clientId, "recv", "unsupported", "", frame.payload.size());
                const std::string msg = WsEnvelope("error", "", "{\"code\":\"text_frames_only\"}");
                if (!SendWsMessage(state, socket, clientId, "error", "", msg)) break;
                continue;
            }

            const std::string type = JsonStringField(frame.payload, "type");
            const std::string room = JsonStringField(frame.payload, "room");
            LogWs(state, clientId, "recv", type.empty() ? "unknown" : type, room, frame.payload.size());

            if (type == "room.subscribe")
            {
                if (room == "*")
                {
                    subscriptions.insert("*");
                    const std::string ack = WsEnvelope("room.subscribed", "*", "{}");
                    if (!SendWsMessage(state, socket, clientId, "room.subscribed", "*", ack)) break;
                    for (const FileSnapshot& snapshot : AllRoomSnapshots(state))
                    {
                        const std::string snapshotMessage = WsEnvelope("file.snapshot", snapshot.room, SnapshotJson(snapshot));
                        if (!SendWsMessage(state, socket, clientId, "file.snapshot", snapshot.room, snapshotMessage))
                        {
                            keepRunning = false;
                            break;
                        }
                    }
                    if (!keepRunning) break;
                }
                else
                {
                    FileSnapshot snapshot;
                    if (!RoomSnapshot(state, room, &snapshot))
                    {
                        const std::string msg = WsEnvelope("error", room, "{\"code\":\"room_not_found\"}");
                        if (!SendWsMessage(state, socket, clientId, "error", room, msg)) break;
                    }
                    else
                    {
                        subscriptions.insert(room);
                        const std::string ack = WsEnvelope("room.subscribed", room, "{}");
                        if (!SendWsMessage(state, socket, clientId, "room.subscribed", room, ack)) break;
                        SendSubscribedSnapshot(state, socket, clientId, room);
                    }
                }
            }
            else if (type == "room.unsubscribe")
            {
                subscriptions.erase(room);
                const std::string ack = WsEnvelope("room.unsubscribed", room, "{}");
                if (!SendWsMessage(state, socket, clientId, "room.unsubscribed", room, ack)) break;
            }
            else if (type == "room.list")
            {
                const std::string msg = WsEnvelope("room.list", "", RoomsJson(state));
                if (!SendWsMessage(state, socket, clientId, "room.list", "", msg)) break;
            }
            else if (type == "ping")
            {
                const std::string msg = WsEnvelope("pong", "", "{}");
                if (!SendWsMessage(state, socket, clientId, "pong", "", msg)) break;
            }
            else
            {
                const std::string msg = WsEnvelope("error", room,
                                                   "{\"code\":\"unknown_message_type\"}");
                if (!SendWsMessage(state, socket, clientId, "error", room, msg)) break;
            }
        }

        std::vector<FileEvent> pending;
        bool historyMissed = false;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            if (!state->events.empty() && lastSequence + 1 < state->events.front().sequence)
                historyMissed = true;
            for (const FileEvent& event : state->events)
                if (event.sequence > lastSequence)
                    pending.push_back(event);
            if (!pending.empty())
                lastSequence = pending.back().sequence;
            else if (historyMissed)
                lastSequence = state->eventSequence;
        }

        if (historyMissed)
        {
            const std::string msg = WsEnvelope("stream.resync_required", "", RoomsJson(state));
            if (!SendWsMessage(state, socket, clientId, "stream.resync_required", "", msg)) break;
        }

        for (const FileEvent& event : pending)
        {
            const bool subscribed = subscriptions.find("*") != subscriptions.end() ||
                                    (!event.room.empty() && subscriptions.find(event.room) != subscriptions.end()) ||
                                    (event.type == "editor.active_changed" && subscriptions.find("*") != subscriptions.end());
            if (!subscribed)
                continue;
            const std::string data = SnapshotJson(event.snapshot);
            const std::string msg = WsEnvelope(event.type, event.room, data);
            if (!SendWsMessage(state, socket, clientId, event.type, event.room, msg))
            {
                keepRunning = false;
                break;
            }
        }
    }

    state->activeWebSocketClients.fetch_sub(1);
    Log(state, DODEV_LOG_INFO, "WebSocket client=" + std::to_string(clientId) + " disconnected");
}

void StreamActiveEditor(State* state, SocketHandle socket, uint64_t clientId)
{
    const std::string header =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/event-stream; charset=utf-8\r\n"
        "Cache-Control: no-cache\r\n"
        "Connection: keep-alive\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "X-Accel-Buffering: no\r\n\r\n";
    if (!SendAll(socket, header))
        return;

    uint64_t lastSequence = 0;
    while (!state->stopping.load())
    {
        FileSnapshot snapshot;
        uint64_t sequence = 0;
        {
            std::unique_lock<std::mutex> lock(state->mutex);
            sequence = state->eventSequence;
            if (lastSequence != 0 && sequence == lastSequence)
            {
                state->changed.wait_for(lock, std::chrono::seconds(15));
                sequence = state->eventSequence;
                if (state->stopping.load()) break;
                if (sequence == lastSequence)
                {
                    lock.unlock();
                    Log(state, DODEV_LOG_DEBUG, "SSE client=" + std::to_string(clientId) + " send keep-alive");
                    if (!SendAll(socket, ": keep-alive\n\n")) break;
                    continue;
                }
            }
            const auto it = state->rooms.find(state->activeRoom);
            if (it != state->rooms.end()) snapshot = it->second;
        }
        lastSequence = sequence;
        const std::string data = SnapshotJson(snapshot);
        const std::string event = "event: editor\nid: " + std::to_string(sequence) + "\ndata: " + data + "\n\n";
        Log(state, DODEV_LOG_INFO, "SSE client=" + std::to_string(clientId) + " send editor bytes=" + std::to_string(data.size()));
        if (!SendAll(socket, event)) break;
    }
}

void SetSocketTimeouts(SocketHandle socket)
{
#ifdef _WIN32
    DWORD millis = 5000;
    setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&millis), sizeof(millis));
    setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&millis), sizeof(millis));
#else
    timeval timeout{};
    timeout.tv_sec = 5;
    setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
#endif
}

std::string PeerAddress(const sockaddr_storage& peer, SocketLength peerLength)
{
    char host[NI_MAXHOST]{};
    char service[NI_MAXSERV]{};
    if (getnameinfo(reinterpret_cast<const sockaddr*>(&peer), peerLength,
                    host, sizeof(host), service, sizeof(service),
                    NI_NUMERICHOST | NI_NUMERICSERV) == 0)
        return std::string(host) + ":" + service;
    return "unknown";
}

void RegisterSocket(State* state, SocketHandle socket)
{
    std::lock_guard<std::mutex> lock(state->clientsMutex);
    state->liveSockets.insert(socket);
}

void UnregisterSocket(State* state, SocketHandle socket)
{
    std::lock_guard<std::mutex> lock(state->clientsMutex);
    state->liveSockets.erase(socket);
}

std::string RoomFromPath(const std::string& path)
{
    static const std::string prefix = "/api/v1/files/rooms/";
    if (path.size() > prefix.size() && path.compare(0, prefix.size(), prefix) == 0)
        return UrlDecode(path.substr(prefix.size()));
    return {};
}

std::string WsRoomFromPath(const std::string& path)
{
    static const std::string prefix = "/ws/v1/files/";
    if (path.size() > prefix.size() && path.compare(0, prefix.size(), prefix) == 0)
        return UrlDecode(path.substr(prefix.size()));
    return {};
}

void HandleClient(State* state, SocketHandle client, std::string peer)
{
    RegisterSocket(state, client);
    state->activeHttpClients.fetch_add(1);
    const uint64_t clientId = state->nextClientId.fetch_add(1);
    SetSocketTimeouts(client);

    HttpRequest request;
    if (!ReceiveRequest(client, &request))
    {
        Log(state, DODEV_LOG_WARNING, "HTTP client=" + std::to_string(clientId) + " peer=" + peer + " invalid request");
        state->activeHttpClients.fetch_sub(1);
        UnregisterSocket(state, client);
        CloseSocket(client);
        return;
    }

    ServerConfig config;
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        config = state->config;
    }

    if (!Authorized(config, request))
    {
        Log(state, DODEV_LOG_WARNING, "HTTP client=" + std::to_string(clientId) + " peer=" + peer +
                                     " " + request.method + " " + request.path + " -> 401");
        SendSimple(client, 401, "Unauthorized", "application/json; charset=utf-8",
                   "{\"error\":\"unauthorized\"}\n");
    }
    else if (request.method == "OPTIONS")
    {
        Log(state, DODEV_LOG_INFO, "HTTP client=" + std::to_string(clientId) + " " + request.method + " " + request.path + " -> 204");
        const std::string response =
            "HTTP/1.1 204 No Content\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
            "Access-Control-Allow-Headers: Authorization, Content-Type, X-DoDev-Token\r\n"
            "Content-Length: 0\r\nConnection: close\r\n\r\n";
        SendAll(client, response);
    }
    else if (IsWebSocketUpgrade(request) &&
             (request.path == "/ws/v1/files" || !WsRoomFromPath(request.path).empty()))
    {
        const std::string initialRoom = WsRoomFromPath(request.path);
        Log(state, DODEV_LOG_INFO, "HTTP client=" + std::to_string(clientId) + " peer=" + peer +
                                  " GET " + request.path + " -> 101 WebSocket");
        WebSocketSession(state, client, request, clientId, initialRoom);
    }
    else if (request.method == "GET" && (request.path == "/api/v1/health" || request.path == "/health"))
    {
        std::ostringstream body;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            body << "{\"ok\":true,\"service\":\"dodev-http-editor-server\",\"api_version\":\"v1\""
                 << ",\"rooms\":" << state->rooms.size()
                 << ",\"websocket_clients\":" << state->activeWebSocketClients.load()
                 << ",\"event_sequence\":" << state->eventSequence << "}\n";
        }
        Log(state, DODEV_LOG_INFO, "HTTP client=" + std::to_string(clientId) + " GET " + request.path + " -> 200");
        SendSimple(client, 200, "OK", "application/json; charset=utf-8", body.str());
    }
    else if (request.method == "GET" && request.path == "/api/v1/files/rooms")
    {
        Log(state, DODEV_LOG_INFO, "HTTP client=" + std::to_string(clientId) + " GET " + request.path + " -> 200");
        SendSimple(client, 200, "OK", "application/json; charset=utf-8", RoomsJson(state) + "\n");
    }
    else if (request.method == "GET" && !RoomFromPath(request.path).empty())
    {
        const std::string room = RoomFromPath(request.path);
        FileSnapshot snapshot;
        if (RoomSnapshot(state, room, &snapshot))
        {
            Log(state, DODEV_LOG_INFO, "HTTP client=" + std::to_string(clientId) + " GET " + request.path + " -> 200");
            SendSimple(client, 200, "OK", "application/json; charset=utf-8", SnapshotJson(snapshot) + "\n");
        }
        else
        {
            Log(state, DODEV_LOG_WARNING, "HTTP client=" + std::to_string(clientId) + " GET " + request.path + " -> 404 room_not_found");
            SendSimple(client, 404, "Not Found", "application/json; charset=utf-8",
                       "{\"error\":\"room_not_found\"}\n");
        }
    }
    else if ((request.method == "GET" && request.path == "/api/v1/editors/active") ||
             (request.method == "POST" && request.path == "/api/editor/snapshot"))
    {
        Log(state, DODEV_LOG_INFO, "HTTP client=" + std::to_string(clientId) + " " + request.method + " " + request.path + " -> 200");
        SendSimple(client, 200, "OK", "application/json; charset=utf-8", SnapshotJson(ActiveSnapshot(state)) + "\n");
    }
    else if ((request.method == "GET" && request.path == "/api/v1/editors/active/events") ||
             ((request.method == "POST" || request.method == "GET") && request.path == "/api/editor/stream"))
    {
        Log(state, DODEV_LOG_INFO, "HTTP client=" + std::to_string(clientId) + " " + request.method + " " + request.path + " -> 200 SSE");
        StreamActiveEditor(state, client, clientId);
    }
    else
    {
        Log(state, DODEV_LOG_WARNING, "HTTP client=" + std::to_string(clientId) + " " + request.method + " " + request.path + " -> 404");
        SendSimple(client, 404, "Not Found", "application/json; charset=utf-8",
                   "{\"error\":\"not_found\"}\n");
    }

    ShutdownSocket(client);
    UnregisterSocket(state, client);
    CloseSocket(client);
    state->activeHttpClients.fetch_sub(1);
}

SocketHandle CreateListener(const ServerConfig& config, std::string* error)
{
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    addrinfo* results = nullptr;
    const std::string port = std::to_string(config.port);
    const int rc = getaddrinfo(config.bindAddress.empty() ? nullptr : config.bindAddress.c_str(),
                               port.c_str(), &hints, &results);
    if (rc != 0)
    {
#ifdef _WIN32
        *error = "getaddrinfo failed: " + std::to_string(rc);
#else
        *error = std::string("getaddrinfo failed: ") + gai_strerror(rc);
#endif
        return kInvalidSocket;
    }

    SocketHandle listener = kInvalidSocket;
    for (addrinfo* ai = results; ai; ai = ai->ai_next)
    {
        SocketHandle candidate = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (candidate == kInvalidSocket)
            continue;
        int reuse = 1;
#ifdef _WIN32
        setsockopt(candidate, SOL_SOCKET, SO_REUSEADDR,
                   reinterpret_cast<const char*>(&reuse), sizeof(reuse));
        const int addressLength = static_cast<int>(ai->ai_addrlen);
#else
        setsockopt(candidate, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
        const socklen_t addressLength = static_cast<socklen_t>(ai->ai_addrlen);
#endif
        if (bind(candidate, ai->ai_addr, addressLength) == 0 && listen(candidate, 64) == 0)
        {
            listener = candidate;
            break;
        }
        CloseSocket(candidate);
    }
    freeaddrinfo(results);
    if (listener == kInvalidSocket)
        *error = "could not bind/listen on " + config.bindAddress + ":" + std::to_string(config.port);
    return listener;
}

void AcceptLoop(State* state)
{
    while (!state->stopping.load())
    {
        sockaddr_storage peer{};
#ifdef _WIN32
        int peerLength = sizeof(peer);
#else
        socklen_t peerLength = sizeof(peer);
#endif
        SocketHandle client = accept(state->listener, reinterpret_cast<sockaddr*>(&peer), &peerLength);
        if (client == kInvalidSocket)
        {
            if (state->stopping.load())
                break;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }
#ifdef _WIN32
        const SocketLength peerForName = peerLength;
#else
        const SocketLength peerForName = peerLength;
#endif
        const std::string peerText = PeerAddress(peer, peerForName);
        std::lock_guard<std::mutex> lock(state->clientsMutex);
        state->clientThreads.emplace_back([state, client, peerText] { HandleClient(state, client, peerText); });
    }
}

void LogEndpoints(State* state, const ServerConfig& config)
{
    const std::string http = "http://" + config.bindAddress + ":" + std::to_string(config.port);
    const std::string ws = "ws://" + config.bindAddress + ":" + std::to_string(config.port);
    Log(state, DODEV_LOG_INFO, "Endpoint api.health: GET " + http + "/api/v1/health");
    Log(state, DODEV_LOG_INFO, "Endpoint api.active-editor: GET " + http + "/api/v1/editors/active");
    Log(state, DODEV_LOG_INFO, "Endpoint api.file-rooms: GET " + http + "/api/v1/files/rooms");
    Log(state, DODEV_LOG_INFO, "Endpoint api.file-room: GET " + http + "/api/v1/files/rooms/{room}");
    Log(state, DODEV_LOG_INFO, "Endpoint api.active-editor-events: GET " + http + "/api/v1/editors/active/events (SSE)");
    Log(state, DODEV_LOG_INFO, "Endpoint ws.files: GET " + ws + "/ws/v1/files");
    Log(state, DODEV_LOG_INFO, "Endpoint ws.file-room: GET " + ws + "/ws/v1/files/{room}");
    Log(state, DODEV_LOG_INFO, std::string("HTTP/WebSocket authentication: ") +
        (config.authToken.empty() ? "disabled" : "Bearer/X-DoDev-Token/query token required"));
}

bool StartServer(State* state)
{
    if (!state || state->running.load())
        return true;
#ifdef _WIN32
    if (!state->winsockInitialized)
    {
        WSADATA data{};
        if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
        {
            Log(state, DODEV_LOG_ERROR, "HTTP/WebSocket Editor Server: WSAStartup failed");
            return false;
        }
        state->winsockInitialized = true;
    }
#endif

    const ServerConfig loaded = LoadConfig(state);
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        state->config = loaded;
    }
    SyncOpenEditors(state);

    std::string error;
    state->listener = CreateListener(loaded, &error);
    if (state->listener == kInvalidSocket)
    {
        Log(state, DODEV_LOG_ERROR, "HTTP/WebSocket Editor Server: " + error);
        if (state->host && state->host->set_status)
            state->host->set_status(state->host->context, ("HTTP/WebSocket Editor Server: " + error).c_str());
        return false;
    }

    state->stopping.store(false);
    state->running.store(true);
    state->acceptThread = std::thread([state] { AcceptLoop(state); });
    const std::string address = loaded.bindAddress + ":" + std::to_string(loaded.port);
    Log(state, DODEV_LOG_INFO, "HTTP/WebSocket Editor Server listening on " + address);
    LogEndpoints(state, loaded);
    if (loaded.bindAddress == "0.0.0.0" && loaded.authToken.empty())
        Log(state, DODEV_LOG_WARNING,
            "HTTP/WebSocket Editor Server is exposed on all interfaces without authentication; editor contents are readable by reachable clients.");
    if (state->host && state->host->set_status)
        state->host->set_status(state->host->context, ("HTTP/WebSocket Editor Server listening on " + address).c_str());
    return true;
}

void StopServer(State* state)
{
    if (!state || !state->running.exchange(false))
        return;
    state->stopping.store(true);
    state->changed.notify_all();
    ShutdownSocket(state->listener);
    CloseSocket(state->listener);
    state->listener = kInvalidSocket;

    std::vector<SocketHandle> sockets;
    {
        std::lock_guard<std::mutex> lock(state->clientsMutex);
        sockets.assign(state->liveSockets.begin(), state->liveSockets.end());
    }
    for (SocketHandle socket : sockets)
        ShutdownSocket(socket);

    if (state->acceptThread.joinable())
        state->acceptThread.join();

    std::vector<std::thread> clients;
    {
        std::lock_guard<std::mutex> lock(state->clientsMutex);
        clients.swap(state->clientThreads);
    }
    for (std::thread& thread : clients)
        if (thread.joinable()) thread.join();

#ifdef _WIN32
    if (state->winsockInitialized)
    {
        WSACleanup();
        state->winsockInitialized = false;
    }
#endif
    Log(state, DODEV_LOG_INFO, "HTTP/WebSocket Editor Server stopped");
}

std::string StatusText(State* state)
{
    ServerConfig config;
    size_t roomCount = 0;
    std::string activeRoom;
    uint64_t sequence = 0;
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        config = state->config;
        roomCount = state->rooms.size();
        activeRoom = state->activeRoom;
        sequence = state->eventSequence;
    }
    std::ostringstream out;
    out << "HTTP / WEBSOCKET EDITOR SERVER\n\n"
        << "State: " << (state->running.load() ? "RUNNING" : "STOPPED") << '\n'
        << "Bind: " << config.bindAddress << '\n'
        << "Port: " << config.port << '\n'
        << "Authentication: " << (config.authToken.empty() ? "none" : "token required") << '\n'
        << "Max editor text: " << config.maxTextBytes << " bytes\n"
        << "Config: " << state->configPath << "\n"
        << "Open file rooms: " << roomCount << '\n'
        << "Active room: " << activeRoom << '\n'
        << "Event sequence: " << sequence << '\n'
        << "HTTP clients: " << state->activeHttpClients.load() << '\n'
        << "WebSocket clients: " << state->activeWebSocketClients.load() << "\n\n"
        << "GET /api/v1/health\n"
        << "GET /api/v1/editors/active\n"
        << "GET /api/v1/files/rooms\n"
        << "GET /api/v1/files/rooms/{room}\n"
        << "GET /api/v1/editors/active/events  (SSE)\n"
        << "GET /ws/v1/files                  (WebSocket multiplexer)\n"
        << "GET /ws/v1/files/{room}           (WebSocket direct room)\n";
    return out.str();
}

void ShowStatus(void* userData)
{
    auto* state = static_cast<State*>(userData);
    if (!state || !state->host)
        return;
    const std::string text = StatusText(state);
    if (!state->statusPanel && state->host->create_text_panel)
        state->statusPanel = state->host->create_text_panel(state->host->context,
                                                             DODEV_PANEL_LOCATION_SIDE,
                                                             "HTTP + WS SERVER",
                                                             text.c_str(), 1);
    else if (state->statusPanel && state->host->text_panel_set_text)
        state->host->text_panel_set_text(state->host->context, state->statusPanel, text.c_str());
    Log(state, DODEV_LOG_INFO, "HTTP/WebSocket Editor Server status requested");
}

void StartMenu(void* userData)
{
    auto* state = static_cast<State*>(userData);
    Log(state, DODEV_LOG_INFO, "HTTP/WebSocket Editor Server start requested");
    StartServer(state);
    ShowStatus(state);
}

void StopMenu(void* userData)
{
    auto* state = static_cast<State*>(userData);
    Log(state, DODEV_LOG_INFO, "HTTP/WebSocket Editor Server stop requested");
    StopServer(state);
    ShowStatus(state);
}

void ReloadConfigMenu(void* userData)
{
    auto* state = static_cast<State*>(userData);
    if (!state)
        return;
    Log(state, DODEV_LOG_INFO, "HTTP/WebSocket Editor Server reload configuration requested");
    const bool wasRunning = state->running.load();
    if (wasRunning) StopServer(state);
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        state->config = LoadConfig(state);
    }
    if (wasRunning) StartServer(state);
    else SyncOpenEditors(state);
    ShowStatus(state);
}
}

extern "C" DODEV_PLUGIN_EXPORT int dodev_plugin_load(const DoDevHostApi* host,
                                                       DoDevPluginInfo* info,
                                                       void** pluginState)
{
    if (!host || host->abi_version != DODEV_PLUGIN_ABI_VERSION || !info || !pluginState)
        return 0;

    auto state = std::make_unique<State>();
    state->host = host;
    const std::string module = CurrentModulePath();
    state->configPath = JoinPath(DirectoryName(module.empty() ? "." : module),
                                 "dodev_http_editor_server.json");
    state->config = LoadConfig(state.get());

    info->abi_version = DODEV_PLUGIN_ABI_VERSION;
    info->size = sizeof(*info);
    info->id = "org.dodev.http-editor-server";
    info->name = "HTTP + WebSocket Editor Server";
    info->version = "2.0.0";
    info->description = "Versioned REST/SSE/WebSocket API with one room per open editor file.";

    Log(state.get(), DODEV_LOG_INFO, "HTTP/WebSocket Editor Server plugin loading");
    if (host->add_menu_item)
    {
        host->add_menu_item(host->context, "Plugins", "HTTP/WS Editor Server: Status", "", &ShowStatus, state.get());
        host->add_menu_item(host->context, "Plugins", "HTTP/WS Editor Server: Start", "", &StartMenu, state.get());
        host->add_menu_item(host->context, "Plugins", "HTTP/WS Editor Server: Stop", "", &StopMenu, state.get());
        host->add_menu_item(host->context, "Plugins", "HTTP/WS Editor Server: Reload Config", "", &ReloadConfigMenu, state.get());
    }

    SyncOpenEditors(state.get());
    StartServer(state.get());
    *pluginState = state.release();
    return 1;
}

extern "C" DODEV_PLUGIN_EXPORT void dodev_plugin_unload(void* pluginState)
{
    std::unique_ptr<State> state(static_cast<State*>(pluginState));
    if (!state)
        return;
    Log(state.get(), DODEV_LOG_INFO, "HTTP/WebSocket Editor Server plugin unloading");
    StopServer(state.get());
}

extern "C" DODEV_PLUGIN_EXPORT void dodev_plugin_on_event(void* pluginState,
                                                            const DoDevEvent* event)
{
    auto* state = static_cast<State*>(pluginState);
    if (!state || !event)
        return;

    switch (event->type)
    {
        case DODEV_EVENT_EDITOR_OPENED:
            Log(state, DODEV_LOG_DEBUG, "Editor event: opened path=" + std::string(event->path_utf8 ? event->path_utf8 : ""));
            UpsertEditor(state, event->object, event->index, "file.opened", true);
            break;
        case DODEV_EVENT_EDITOR_CHANGED:
            Log(state, DODEV_LOG_DEBUG, "Editor event: changed path=" + std::string(event->path_utf8 ? event->path_utf8 : ""));
            UpsertEditor(state, event->object, event->index, "file.changed", true);
            break;
        case DODEV_EVENT_EDITOR_CLOSED:
            Log(state, DODEV_LOG_DEBUG, "Editor event: closed path=" + std::string(event->path_utf8 ? event->path_utf8 : ""));
            CloseEditorRoom(state, event->object);
            break;
        case DODEV_EVENT_ACTIVE_TAB_CHANGED:
            Log(state, DODEV_LOG_DEBUG, "Editor event: active tab changed");
            SetActiveEditor(state, event->object, event->index);
            break;
        case DODEV_EVENT_WORKSPACE_CHANGED:
            Log(state, DODEV_LOG_DEBUG, "Editor event: workspace changed");
            break;
        default:
            break;
    }
}
