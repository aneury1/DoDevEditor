#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "SourceSymbols.h"
#include "dodev/settings/Json.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <csignal>
#endif

namespace dodev::symbols {
Result extractRegex(std::string_view path, std::string_view contents);
namespace {
namespace fs = std::filesystem;
constexpr std::size_t kMaxInput = 4u * 1024u * 1024u;
constexpr std::uintmax_t kMaxAst = 16u * 1024u * 1024u;
const Json* member(const Json& node, const char* key) {
  return node.isObject() ? node.find(key) : nullptr;
}
std::string field(const Json& node, const char* key) {
  auto v = member(node, key);
  return v ? v->string() : "";
}
int number(const Json& node, const char* key, int fallback = -1) {
  auto v = member(node, key);
  return v ? v->integer(fallback) : fallback;
}
std::string lower(std::string v) {
  for (char& ch : v)
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  return v;
}
bool cFamily(std::string_view path) {
  const auto ext = lower(fs::u8path(std::string(path)).extension().string());
  return ext.empty() || ext == ".c" || ext == ".cc" || ext == ".cpp" ||
         ext == ".cxx" || ext == ".h" || ext == ".hh" || ext == ".hpp" ||
         ext == ".hxx" || ext == ".ipp" || ext == ".inl" || ext == ".m" ||
         ext == ".mm";
}
std::string kindFromAst(const std::string& ast) {
  if (ast == "NamespaceDecl") return "namespace";
  if (ast == "CXXRecordDecl" || ast == "RecordDecl" ||
      ast == "ClassTemplateDecl" || ast == "ClassTemplateSpecializationDecl")
    return "class";
  if (ast == "EnumDecl") return "enum";
  if (ast == "EnumConstantDecl") return "enumerator";
  if (ast == "FunctionDecl" || ast == "FunctionTemplateDecl") return "function";
  if (ast == "CXXMethodDecl" || ast == "CXXConstructorDecl" ||
      ast == "CXXDestructorDecl" || ast == "CXXConversionDecl")
    return "method";
  if (ast == "FieldDecl") return "field";
  if (ast == "VarDecl") return "variable";
  if (ast == "TypedefDecl" || ast == "TypeAliasDecl" ||
      ast == "TypeAliasTemplateDecl")
    return "type";
  if (ast == "ConceptDecl") return "concept";
  return {};
}
void scanTree(const Json& node, std::string_view source,
              std::vector<Symbol>& result, std::set<std::string>& seen,
              bool inheritedMain, int depth = 0) {
  if (depth > 120 || !node.isObject() || result.size() >= 15000) return;
  const auto* loc = member(node, "loc");
  bool inMain = inheritedMain;
  if (loc && loc->isObject()) {
    const auto filename = field(*loc, "file");
    if (!filename.empty()) inMain = (filename == "<stdin>" || filename == "-");
    // Included headers often record this instead of a file. Do not publish
    // declarations from other files even when their offsets coincide.
    if (member(*loc, "includedFrom")) inMain = false;
  }
  const auto astKind = field(node, "kind");
  const auto kind = kindFromAst(astKind);
  const auto name = field(node, "name");
  const int offset = (loc && loc->isObject()) ? number(*loc, "offset") : -1;
  const bool implicit =
      member(node, "isImplicit") && node["isImplicit"].boolean(false);
  if (inMain && !implicit && !kind.empty() && !name.empty() && offset >= 0 &&
      static_cast<std::size_t>(offset) < source.size()) {
    const std::size_t o = static_cast<std::size_t>(offset);
    const auto last = source.rfind('\n', o);
    const std::size_t line =
        1 + static_cast<std::size_t>(
                std::count(source.begin(), source.begin() + offset, '\n'));
    const std::size_t column =
        o - (last == std::string_view::npos ? 0 : last + 1) + 1;
    std::size_t start = o, end = std::min(source.size(), o + name.size());
    if (const auto* range = member(node, "range")) {
      if (const auto* begin = member(*range, "begin")) {
        const int b = number(*begin, "offset");
        if (b >= 0 && static_cast<std::size_t>(b) <= o)
          start = static_cast<std::size_t>(b);
      }
      if (const auto* finish = member(*range, "end")) {
        const int e = number(*finish, "offset");
        const int len = number(*finish, "tokLen", 1);
        if (e >= 0 && static_cast<std::size_t>(e) < source.size())
          end = std::min(source.size(),
                         static_cast<std::size_t>(e) +
                             static_cast<std::size_t>(std::max(1, len)));
      }
    }
    std::string signature(source.substr(
        start, std::min<std::size_t>(end > start ? end - start : 0, 280)));
    for (char& ch : signature)
      if (ch == '\n' || ch == '\t' || ch == '\r') ch = ' ';
    const auto key = std::to_string(o) + ":" + name;
    if (seen.insert(key).second)
      result.push_back({kind, name, signature, line, column});
  }
  const auto* children = member(node, "inner");
  if (children && children->isArray())
    for (const auto& child : children->asArray())
      scanTree(child, source, result, seen, inMain, depth + 1);
}

#ifndef _WIN32
struct TempFiles {
  std::string input, output, err;
  ~TempFiles() {
    for (const auto& p : {input, output, err})
      if (!p.empty()) ::unlink(p.c_str());
  }
};
bool tempFile(std::string& out) {
  std::string pattern =
      (fs::temp_directory_path() / "dodev-symbols-XXXXXX").string();
  std::vector<char> buf(pattern.begin(), pattern.end());
  buf.push_back('\0');
  const int fd = mkstemp(buf.data());
  if (fd < 0) return false;
  out = buf.data();
  close(fd);
  return true;
}
bool writeBuffer(const std::string& path, std::string_view data) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out.write(data.data(), static_cast<std::streamsize>(data.size()));
  return static_cast<bool>(out);
}
std::string readLimited(const std::string& path, std::size_t bytes) {
  std::ifstream input(path, std::ios::binary);
  std::string out;
  out.resize(bytes);
  input.read(out.data(), static_cast<std::streamsize>(bytes));
  out.resize(static_cast<std::size_t>(input.gcount()));
  return out;
}
std::string findExecutable(const std::string& explicitPath) {
  if (!explicitPath.empty()) return explicitPath;
  if (const char* overrideTool = std::getenv("DODEV_CLANG_EXECUTABLE")) {
    if (*overrideTool) return overrideTool;
  }
  const char* path = std::getenv("PATH");
  if (!path) return "clang++";
  std::string directories(path);
  for (const char* candidate : {"clang++", "clang"}) {
    std::size_t start = 0;
    while (start <= directories.size()) {
      const auto end = directories.find(':', start);
      const std::string dir = directories.substr(
          start, end == std::string::npos ? std::string::npos : end - start);
      const fs::path tool = fs::path(dir.empty() ? "." : dir) / candidate;
      if (access(tool.c_str(), X_OK) == 0) return tool.string();
      if (end == std::string::npos) break;
      start = end + 1;
    }
  }
  return "clang++";
}
bool runClang(const std::string& path, std::string_view source,
              const Options& options, std::string& ast,
              std::string& diagnostic) {
  TempFiles tmp;
  if (!tempFile(tmp.input) || !tempFile(tmp.output) || !tempFile(tmp.err)) {
    diagnostic = "cannot create temporary Clang streams";
    return false;
  }
  if (!writeBuffer(tmp.input, source)) {
    diagnostic = "cannot prepare unsaved document";
    return false;
  }
  const std::string tool = findExecutable(options.clangExecutable);
  const auto extension = lower(fs::u8path(path).extension().string());
  const char* language = (extension == ".c" || extension == ".h") ? "c" : "c++";
  const char* standard =
      (*language == 'c' && language[1] == '\0') ? "-std=c11" : "-std=c++17";
  const std::string parent = fs::path(path).has_parent_path()
                                 ? fs::path(path).parent_path().string()
                                 : ".";
  const int pid = fork();
  if (pid < 0) {
    diagnostic = "unable to launch Clang";
    return false;
  }
  if (pid == 0) {
    const int in = open(tmp.input.c_str(), O_RDONLY),
              out = open(tmp.output.c_str(), O_WRONLY | O_TRUNC),
              err = open(tmp.err.c_str(), O_WRONLY | O_TRUNC);
    if (in < 0 || out < 0 || err < 0) _exit(126);
    dup2(in, STDIN_FILENO);
    dup2(out, STDOUT_FILENO);
    dup2(err, STDERR_FILENO);
    close(in);
    close(out);
    close(err);
    const std::string include = "-I" + parent;
    const char* argv[] = {tool.c_str(),
                          "-x",
                          language,
                          standard,
                          "-fsyntax-only",
                          "-fno-color-diagnostics",
                          "-Wno-everything",
                          "-ferror-limit=3",
                          "-Xclang",
                          "-ast-dump=json",
                          include.c_str(),
                          "-",
                          nullptr};
    execvp(tool.c_str(), const_cast<char* const*>(argv));
    _exit(127);
  }
  int status = 0;
  bool completed = false, oversize = false;
  const auto start = std::chrono::steady_clock::now();
  while (std::chrono::steady_clock::now() - start <
         std::chrono::milliseconds(std::clamp(options.timeoutMs, 250, 20000))) {
    const auto waited = waitpid(pid, &status, WNOHANG);
    if (waited == pid) {
      completed = true;
      break;
    }
    if (waited < 0) break;
    std::error_code ec;
    if (fs::file_size(tmp.output, ec) > kMaxAst && !ec) {
      oversize = true;
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(15));
  }
  if (!completed) {
    kill(pid, SIGKILL);
    waitpid(pid, &status, 0);
  }
  const auto error = readLimited(tmp.err, 1024);
  if (!completed) {
    diagnostic = oversize ? "Clang AST exceeded the 16 MB limit"
                          : "Clang analysis timed out";
    return false;
  }
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    diagnostic = WIFEXITED(status) && WEXITSTATUS(status) == 127
                     ? "Clang executable not found"
                     : "Clang could not parse the document";
    if (!error.empty()) diagnostic += " (" + error.substr(0, 200) + ")";
    return false;
  }
  std::error_code ec;
  const auto bytes = fs::file_size(tmp.output, ec);
  if (ec || bytes > kMaxAst) {
    diagnostic = "Clang AST is too large";
    return false;
  }
  ast = readLimited(tmp.output, static_cast<std::size_t>(bytes));
  if (ast.empty()) {
    diagnostic = "Clang returned an empty AST";
    return false;
  }
  return true;
}
#else
bool runClang(const std::string&, std::string_view, const Options&,
              std::string&, std::string& diagnostic) {
  diagnostic = "Clang subprocess integration is not yet available on Windows";
  return false;
}
#endif
}  // namespace

Result extract(std::string_view path, std::string_view contents,
               const Options& options) {
  if (options.mode == ParserMode::RegexOnly || !cFamily(path)) {
    auto result = extractRegex(path, contents);
    result.parser = "Regex";
    if (options.mode == ParserMode::ClangOnly && !cFamily(path))
      result.diagnostics = "Clang AST supports C/C++ sources only";
    return result;
  }
  if (contents.size() > kMaxInput) {
    auto result = extractRegex(path, contents);
    result.diagnostics = "Source buffer exceeds Clang input limit";
    return result;
  }
  std::string ast, diagnostic;
  if (runClang(std::string(path), contents, options, ast, diagnostic)) {
    Json tree;
    if (Json::parse(ast, tree, &diagnostic)) {
      Result result;
      result.language = "C/C++";
      result.parser = "Clang";
      std::set<std::string> seen;
      scanTree(tree, contents, result.rows, seen, true);
      if (!result.rows.empty()) {
        // Compiler AST omits preprocessor directives: retain lexical
        // macros/includes.
        const auto extra = extractRegex(path, contents);
        for (const auto& s : extra.rows)
          if (s.kind == "macro" || s.kind == "include")
            result.rows.push_back(s);
        std::stable_sort(result.rows.begin(), result.rows.end(),
                         [](const Symbol& a, const Symbol& b) {
                           return a.line == b.line ? a.column < b.column
                                                   : a.line < b.line;
                         });
        return result;
      }
      diagnostic = "Clang returned no document symbols";
    } else
      diagnostic =
          "Clang AST JSON could not be parsed: " + diagnostic.substr(0, 180);
  }
  Result result = extractRegex(path, contents);
  result.parser = options.mode == ParserMode::ClangOnly ? "Clang (unavailable)"
                                                        : "Regex (fallback)";
  result.diagnostics = diagnostic;
  if (options.mode == ParserMode::ClangOnly) result.rows.clear();
  return result;
}
}  // namespace dodev::symbols
