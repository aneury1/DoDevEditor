#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <vector>

#include "SecureFolder.h"
namespace fs = std::filesystem;
void require(bool b) {
  if (!b) throw std::runtime_error("Test failed");
}
void put(const fs::path& p, const std::string& s) {
  std::ofstream f(p, std::ios::binary);
  f.write(s.data(), s.size());
}
std::string get(const fs::path& p) {
  std::ifstream f(p, std::ios::binary);
  return {std::istreambuf_iterator<char>(f), {}};
}
int main() {
  auto root =
      fs::temp_directory_path() /
      ("sfq-test-" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()));
  fs::create_directory(root);
  try {
    fs::create_directories(root / "src/sub/empty");
    put(root / "secret", "test secret only 123456789");
    put(root / "wrong", "wrong secret only 123456789");
    std::mt19937 rng(42);
    std::string binary(5000, '\0');
    for (auto& c : binary) c = char(rng());
    put(root / "src/sub/file.bin", binary);
    put(root / "src/a.txt", "Hello QR");
    secureqr::encrypt(root / "src", root / "qr", root / "secret");
    require(fs::exists(root / "qr/part-2.png"));
    secureqr::decrypt(root / "qr", root / "restored", root / "secret");
    require(get(root / "restored/sub/file.bin") == binary);
    require(fs::is_directory(root / "restored/sub/empty"));
    require(get(root / "restored/a.txt") == "Hello QR");
    auto rejects = [&](const fs::path& in, const fs::path& key) {
      bool rejected = false;
      try {
        secureqr::decrypt(in, root / "bad", key);
      } catch (...) {
        rejected = true;
      }
      require(rejected && !fs::exists(root / "bad"));
    };
    rejects(root / "qr", root / "wrong");
    auto all = get(root / "qr/all-parts.txt");
    std::vector<std::string> lines;
    std::ifstream stream(root / "qr/all-parts.txt");
    for (std::string line; std::getline(stream, line);) lines.push_back(line);
    std::reverse(lines.begin(), lines.end());
    std::string rev;
    for (auto& l : lines) rev += l + "\n";
    put(root / "reverse.txt", rev);
    secureqr::decrypt(root / "reverse.txt", root / "reverse", root / "secret");
    require(get(root / "reverse/sub/file.bin") == binary);
    put(root / "missing.txt", all.substr(all.find('\n') + 1));
    rejects(root / "missing.txt", root / "secret");
    auto pos = all.find('\n') - 5;
    all[pos] = all[pos] == 'A' ? 'B' : 'A';
    put(root / "tampered.txt", all);
    rejects(root / "tampered.txt", root / "secret");
    fs::remove_all(root);
    std::cout << "Round trip, reorder, wrong secret, missing and tampered "
                 "parts passed\n";
    return 0;
  } catch (const std::exception& e) {
    fs::remove_all(root);
    std::cerr << e.what() << '\n';
    return 1;
  }
}
