#include <algorithm>
#include <chrono>
#include <fstream>
#include <stdexcept>
#include <string>

#include "CallHierarchy.h"
#include "SourceSymbols.h"
#include "SymbolCache.h"
#define REQUIRE(x)                                                        \
  do {                                                                    \
    if (!(x))                                                             \
      throw std::runtime_error(std::string("Failed: ") + #x + " (line " + \
                               std::to_string(__LINE__) + ")");           \
  } while (0)
static bool has(const dodev::symbols::Result& r, const std::string& name) {
  return std::any_of(r.rows.begin(), r.rows.end(),
                     [&](const auto& x) { return x.name == name; });
}
int main() {
  using namespace dodev::symbols;
  const auto cpp = extract("sample.cpp", R"(#include <vector>
#define MAGIC 42
namespace example {
class Engine { public:
  int start(int x);
};
int test_run(int input) {return input;}
// int commented(int x);
/* class Fake { int bogus(); }; */
std::vector<int> scan(
  const std::string& value,
  int limit
) const {
 return {};
}
void Engine::reset() noexcept {}
}
)");
  REQUIRE(has(cpp, "Engine"));
  REQUIRE(has(cpp, "MAGIC"));
  REQUIRE(has(cpp, "start"));
  REQUIRE(has(cpp, "test_run"));
  REQUIRE(has(cpp, "scan"));
  REQUIRE(has(cpp, "reset") || has(cpp, "Engine::reset"));
  REQUIRE(!has(cpp, "commented"));
  REQUIRE(!has(cpp, "Fake"));
  REQUIRE(!has(cpp, "bogus"));
  REQUIRE(filter(cpp.rows, "engine").size() >= 1);
  REQUIRE(toCsv(cpp.rows).find("test_run") != std::string::npos);
  const auto py =
      extract("a.py",
              "class Example:\n    def run(self):\n        pass\nasync "
              "def main():\n    pass\n\"\"\"def fake():\"\"\"\n");
  REQUIRE(has(py, "Example") && has(py, "run") && has(py, "main") &&
          !has(py, "fake"));
  const auto js =
      extract("index.ts",
              "export async function hello(x: string) {\n}\nconst go = (a) "
              "=> 42;\nclass People { greet(name) { return name; } }\n");
  REQUIRE(has(js, "hello") && has(js, "go") && has(js, "People") &&
          has(js, "greet"));
  const auto kt = extract(
      "App.kt", "class MainActivity {\n fun start(\n  x: Int\n ) { }\n}\n");
  REQUIRE(has(kt, "MainActivity") && has(kt, "start"));
  const auto untitled =
      extract("Sin titulo 1", "void handler(int code) {}\nclass A {};\n");
  REQUIRE(has(untitled, "handler") && has(untitled, "A"));
  // Representative function definitions from DoDevEditor and modern C++.
  const auto real = extract("EditorSymbols.cpp",
                            "bool EditorApp::openSymbolsPanel(){return "
                            "true;}\nvoid EditorApp::filterSymbolsPanel(){}\n");
  REQUIRE(has(real, "EditorApp::openSymbolsPanel") &&
          has(real, "EditorApp::filterSymbolsPanel"));
  // Compiler-backed symbols are discovered from the unsaved document and
  // preserve editor navigation coordinates.
  const auto valid =
      extract("ok.cpp",
              "namespace demo { class Widget { public: int run(int v) { "
              "return v; } }; int execute(int n) { return n; } }\n");
  REQUIRE(has(valid, "Widget") && has(valid, "execute") && has(valid, "run"));
  if (valid.parser == "Clang") REQUIRE(valid.rows.size() >= 3);
  const auto forcedRegex = extract(
      "ok.cpp", "int calculate(int n){return n;}\n", {ParserMode::RegexOnly});
  REQUIRE(forcedRegex.parser == "Regex" && has(forcedRegex, "calculate"));
  const auto missing =
      extract("ok.cpp", "int calculate(int n){return n;}\n",
              {ParserMode::Auto, "/this/executable/does/not/exist", 500});
  REQUIRE(missing.parser == "Regex (fallback)" && has(missing, "calculate") &&
          !missing.diagnostics.empty());
  const auto strictMissing =
      extract("ok.cpp", "int calculate(int n){return n;}\n",
              {ParserMode::ClangOnly, "/this/executable/does/not/exist", 500});
  REQUIRE(strictMissing.rows.empty() &&
          strictMissing.parser == "Clang (unavailable)");
  namespace fs = std::filesystem;
  auto root =
      fs::temp_directory_path() /
      ("dodev-symbol-test-" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()));
  fs::create_directories(root / "src");
  fs::create_directories(root / "build");
  struct Cleanup {
    fs::path p;
    ~Cleanup() {
      std::error_code e;
      fs::remove_all(p, e);
    }
  } cleanup{root};
  auto put = [](const fs::path& p, const std::string& text) {
    std::ofstream(p) << text;
  };
  for (int i = 0; i < 155; ++i)
    put(root / "src" / (std::to_string(i) + ".cpp"),
        "int function" + std::to_string(i) + "() {return 0;}\n");
  put(root / "build" / "ignored.cpp", "int ignored() {}\n");
  Options opts;
  opts.mode = ParserMode::RegexOnly;
  auto first = indexWorkspace(root, opts);
  REQUIRE(first.parsed == 155 && first.hits == 0 && first.rows.size() >= 155);
  REQUIRE(fs::exists(root / ".dodev" / "symbols-cache.json"));
  auto second = indexWorkspace(root, opts);
  REQUIRE(second.parsed == 0 && second.hits == 155 &&
          second.rows.size() == first.rows.size());
  put(root / "src" / "0.cpp", "int changedFunction() {return 123;}\n");
  fs::remove(root / "src" / "1.cpp");
  put(root / "src" / "new.cpp", "int newFunction() {}\n");
  auto third = indexWorkspace(root, opts);
  REQUIRE(third.parsed == 2 && third.hits == 153);
  REQUIRE(std::none_of(third.rows.begin(), third.rows.end(), [](auto& row) {
    return row.name == "function0" || row.name == "function1";
  }));
  REQUIRE(std::any_of(third.rows.begin(), third.rows.end(),
                      [](auto& row) { return row.name == "changedFunction"; }));
  put(root / ".dodev" / "symbols-cache.json", "invalid json");
  REQUIRE(indexWorkspace(root, opts).parsed == 155);
  opts.timeoutMs = 999;
  REQUIRE(indexWorkspace(root, opts).parsed == 155);
  auto astCalls = extract(
      "calls.cpp", "int helper(){return 1;}\nint main(){return helper();}\n");
  REQUIRE(std::any_of(astCalls.calls.begin(), astCalls.calls.end(),
                      [](const auto& c) {
                        return c.caller == "main" && c.callee == "helper";
                      }));
  auto graphRoot = root / "graph";
  fs::create_directories(graphRoot);
  put(graphRoot / "a.cpp", "int helper();\nint main() { return helper(); }\n");
  put(graphRoot / "b.cpp", "int helper() { return helper(); }\n");
  auto graph = indexWorkspace(graphRoot, opts);
  REQUIRE(graph.calls.size() == 2);
  auto tree = hierarchy(graph, "main", graphRoot / "a.cpp", 2, false);
  REQUIRE(tree.size() == 3 && tree[1].name == "helper" &&
          tree[2].status == "recursive");
  auto incoming = hierarchy(graph, "helper", graphRoot / "b.cpp", 1, true);
  REQUIRE(incoming.size() >= 3);
  auto cached = indexWorkspace(graphRoot, opts);
  REQUIRE(cached.hits == 2 && cached.parsed == 0 && cached.calls.size() == 2);
  put(graphRoot / "b.cpp", "int helper() { return 42; }\n");
  auto changed = indexWorkspace(graphRoot, opts);
  REQUIRE(changed.parsed == 1 && changed.calls.size() == 1);
  fs::remove(graphRoot / "a.cpp");
  REQUIRE(indexWorkspace(graphRoot, opts).calls.empty());
  return 0;
}
