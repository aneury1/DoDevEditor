#include <algorithm>
#include <stdexcept>
#include <string>

#include "SourceSymbols.h"
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
              "class Example:\n    def run(self):\n        pass\nasync def "
              "main():\n    pass\n\"\"\"def fake():\"\"\"\n");
  REQUIRE(has(py, "Example") && has(py, "run") && has(py, "main") &&
          !has(py, "fake"));
  const auto js =
      extract("index.ts",
              "export async function hello(x: string) {\n}\nconst go = (a) => "
              "42;\nclass People { greet(name) { return name; } }\n");
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
              "namespace demo { class Widget { public: int run(int v) { return "
              "v; } }; int execute(int n) { return n; } }\n");
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
  return 0;
}
