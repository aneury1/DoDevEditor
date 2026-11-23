#include "SourceSymbols.h"
#include <stdexcept>
#include <string>
#define REQUIRE(x)                                                             \
  do {                                                                         \
    if (!(x))                                                                  \
      throw std::runtime_error(#x);                                            \
  } while (0)
int main() {
  using namespace dodev::symbols;
  auto cpp = extract("sample.cpp", R"(#include <vector>
#define MAGIC 42
namespace example {
class Engine { public:
  int start(int x);
};
int test_run(int input) {return input;}
// int commented(int x);
})");
  REQUIRE(cpp.rows.size() >= 5);
  REQUIRE(filter(cpp.rows, "Engine").size() == 1);
  REQUIRE(filter(cpp.rows, "commented").empty());
  REQUIRE(toCsv(cpp.rows).find("test_run") != std::string::npos);
  auto py = extract("a.py", "class Example:\n    def run(self):\n        "
                            "pass\nasync def main():\n    pass\n");
  REQUIRE(py.rows.size() == 3);
  auto js = extract(
      "index.ts",
      "export async function hello(x: string) {\n}\nconst go = (a) => 42;\n");
  REQUIRE(js.rows.size() == 2);
  return 0;
}
