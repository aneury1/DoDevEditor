#include <cassert>
#include <chrono>
#include <fstream>
#include <iostream>

#include "QRMatrix.h"
#include "QRPayload.h"
namespace fs = std::filesystem;
int main() {
  auto root =
      fs::temp_directory_path() /
      ("dodevqr-" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()));
  fs::create_directories(root / "empty");
  struct Cleanup {
    fs::path p;
    ~Cleanup() {
      std::error_code e;
      fs::remove_all(p, e);
    }
  } c{root};
  std::string binary("A\0B\xff", 4);
  std::ofstream(root / "bin.dat", std::ios::binary)
      .write(binary.data(), binary.size());
  auto bundle = dodevqr::packPath(root);
  assert(bundle.rfind("DQA1", 0) == 0 &&
         bundle.find(binary) != std::string::npos &&
         bundle.find("empty") != std::string::npos);
  auto plain = dodevqr::wrapPayload(bundle, false);
  assert(dodevqr::unwrapPayload(plain) == bundle);
  assert(dodevqr::encode(plain).version > 0);
  if (dodevqr::encryptionAvailable()) {
    auto encrypted =
        dodevqr::wrapPayload(bundle, true, "example strong test password");
    assert(encrypted !=
           dodevqr::wrapPayload(bundle, true, "example strong test password"));
    assert(dodevqr::unwrapPayload(encrypted, "example strong test password") ==
           bundle);
    bool failed = false;
    try {
      dodevqr::unwrapPayload(encrypted, "wrong");
    } catch (...) {
      failed = true;
    }
    assert(failed);
    auto tampered = encrypted;
    tampered[50] = tampered[50] == 'A' ? 'B' : 'A';
    failed = false;
    try {
      dodevqr::unwrapPayload(tampered, "example strong test password");
    } catch (...) {
      failed = true;
    }
    assert(failed);
    assert(dodevqr::encode(encrypted).version > 0);
    std::cout << "AES-256-GCM roundtrip, randomized nonce, wrong password and "
                 "tamper rejection passed\n";
  } else
    std::cout << "OpenSSL unavailable: encryption explicitly disabled\n";
  std::cout << "Binary folder, empty directory, compressed/uncompressed "
               "envelope and QR encoding passed\n";
}
