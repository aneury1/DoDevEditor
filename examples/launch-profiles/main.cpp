#include <cstdlib>
#include <iostream>
int main(int argc, char** argv) {
  std::cout << "Launch example\n";
  for (int i = 1; i < argc; ++i)
    std::cout << "Argument " << i << ": " << argv[i] << '\n';
  if (auto mode = std::getenv("APP_MODE"))
    std::cout << "APP_MODE=" << mode << '\n';
  return 0;
}
