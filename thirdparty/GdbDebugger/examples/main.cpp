#include <iostream>
#include <vector>
int sum(const std::vector<int>& values) {
  int result = 0;
  for (int value : values) result += value;
  return result;
}
int main(int argc, char** argv) {
  std::vector<int> numbers{10, 20, 30, 40};
  int result = sum(numbers);
  std::cout << "Total: " << result << '\n';
  for (int i = 1; i < argc; ++i) std::cout << argv[i] << '\n';
  return 0;
}
