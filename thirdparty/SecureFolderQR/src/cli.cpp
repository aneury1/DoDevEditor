#include <iostream>

#include "SecureFolder.h"
int main(int argc, char** argv) {
  if (argc != 5) {
    std::cerr << "Usage: securefolderqr encrypt|decrypt INPUT NEW_OUTPUT_DIR "
                 "SECRET_FILE\n";
    return 2;
  }
  try {
    auto a = std::filesystem::u8path(argv[2]),
         b = std::filesystem::u8path(argv[3]),
         c = std::filesystem::u8path(argv[4]);
    std::string op = argv[1];
    if (op == "encrypt")
      secureqr::encrypt(a, b, c);
    else if (op == "decrypt")
      secureqr::decrypt(a, b, c);
    else
      return 2;
    std::cout << "Completed\n";
    return 0;
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
