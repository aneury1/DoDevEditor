#include <cassert>
#include <filesystem>
#include <string>
#include <vector>

#include "dodev/launch/LaunchArgs.h"
namespace fs = std::filesystem;
using dodev::launch::parse;

int main() {
  {
    const char* a[] = {"dodeveditor", "--wait", ".", "hello world.cpp",
                       "another.h"};
    const auto r = parse(5, a, true);
    assert(r.error.empty() && r.wait && !r.foreground && r.paths.size() == 3);
    assert(r.paths[0].is_absolute());
    assert(r.paths[0] == fs::current_path());
    assert(r.paths[1].filename() == "hello world.cpp");
    assert(r.paths[2].filename() == "another.h");
  }
  {
    const char* a[] = {"dodeveditor", "--", "-test.cpp"};
    const auto r = parse(3, a, true);
    assert(r.error.empty() && r.paths.size() == 1 &&
           r.paths[0].filename() == "-test.cpp");
  }
  {
    const char* a[] = {"dodeveditor", "--bad-option"};
    const auto r = parse(2, a, true);
    assert(!r.error.empty());
  }
  {
    const char* a[] = {"DoDevEditor", "/tmp/one.h", "/tmp/two.cpp"};
    const auto r = parse(3, a, false);
    assert(r.error.empty() && r.paths.size() == 2);
  }
  {
    const char* a[] = {"dodeveditor", "--foreground", "main.cpp"};
    const auto r = parse(3, a, true);
    assert(r.foreground && r.paths.size() == 1);
  }
}
