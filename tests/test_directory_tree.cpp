#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>

#include "dodev/explorer/DirectoryTree.h"
int main() {
  namespace fs = std::filesystem;
  using dodev::explorer::DirectoryTree;
  auto root =
      fs::temp_directory_path() /
      ("dodev-tree-" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()));
  fs::create_directories(root / "src" / "nested");
  fs::create_directories(root / ".vscode");
  std::ofstream(root / "src" / "nested" / "main.cpp") << "test";
  auto contents = dodev::explorer::directoryContents(root, true);
  assert(contents.size() == 2);
  assert(contents[0].directory);
  assert(dodev::explorer::directoryContents(root, false).size() == 1);
  assert(dodev::explorer::directoryContents(root / "src" / "nested", true)[0]
             .path.filename() == "main.cpp");
  DirectoryTree t;
  t.root(root);
  assert(t.rows().size() == 3);
  t.reveal(root / "src" / "nested" / "main.cpp");
  assert(t.rows().size() == 5);
  t.hidden(false);
  assert(t.rows().size() == 4);
  t.toggle(root / "src");
  assert(t.rows().size() == 2);
  t.collapse();
  assert(t.rows().size() == 2);
  fs::remove_all(root);
}
