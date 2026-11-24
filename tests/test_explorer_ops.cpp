#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "dodev/explorer/ExplorerOps.h"
namespace fs = std::filesystem;
int main() {
  auto root =
      fs::temp_directory_path() /
      ("dodev-explorer-" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()));
  fs::create_directory(root);
  using namespace dodev::explorer;
  assert(!validEntryName("../oops"));
  assert(!validEntryName(".."));
  assert(!validEntryName(""));
  auto dir = createEntry(root, "src", true);
  assert(dir.ok && fs::is_directory(dir.path));
  auto file = createEntry(dir.path, "main.cpp", false);
  assert(file.ok && fs::file_size(file.path) == 0);
  assert(!createEntry(dir.path, "main.cpp", false).ok);
  assert(!createEntry(dir.path, "../breakout", false).ok);
  auto renamed = renameEntry(file.path, "app.cpp");
  assert(renamed.ok);
  assert(!fs::exists(file.path) && fs::exists(renamed.path));
  assert(!renameEntry(renamed.path, "../escape.cpp").ok);
  assert(!removeEntry(root, root).ok);
  assert(removeEntry(dir.path, root).ok && !fs::exists(dir.path));
  fs::remove_all(root);
  std::cout << "explorer operations validated\n";
}
