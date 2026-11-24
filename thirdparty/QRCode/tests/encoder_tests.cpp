#include <cstdint>
#include <cstring>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

#include "QRMatrix.h"

void require(bool condition, const char* why) {
  if (!condition) throw std::runtime_error(why);
}
int main() {
  using namespace dodevqr;
  try {
    struct Case {
      std::string text;
      ErrorCorrection ecc;
      int minVersion;
    };
    const Case cases[]{
        {"A", ErrorCorrection::Low, 1},
        {"Hello from DoDevEditor", ErrorCorrection::Medium, 1},
        {"Espa\xC3\xB1ol QR \xF0\x9F\x8C\x8D", ErrorCorrection::Quartile, 1},
        {"High ECC", ErrorCorrection::High, 1},
        {std::string(1000, 'X'), ErrorCorrection::Low, 1},
        {std::string(2300, 'A'), ErrorCorrection::Medium, 1},
        {std::string(2900, 'B'), ErrorCorrection::Low, 1}};
    for (const auto& c : cases) {
      auto matrix = encode(c.text, c.ecc);
      require(matrix.version >= c.minVersion && matrix.version <= 40,
              "version outside 1..40");
      require(matrix.size == 17 + 4 * matrix.version, "QR size mismatch");
      require(matrix.pixels.size() ==
                  static_cast<size_t>(matrix.size * matrix.size),
              "matrix buffer mismatch");
      auto svg = toSvg(matrix);
      require(svg.find("<svg") != std::string::npos &&
                  svg.find("<path") != std::string::npos,
              "SVG invalid");
      auto png = toPng(matrix, 4);
      require(png.size() > 100, "PNG unexpectedly empty");
      const uint8_t header[] = {137, 80, 78, 71, 13, 10, 26, 10};
      require(std::memcmp(png.data(), header, 8) == 0,
              "PNG signature incorrect");
    }
    bool rejected = false;
    try {
      (void)encode(std::string(4000, 'X'));
    } catch (const std::length_error&) {
      rejected = true;
    }
    require(rejected, "oversize input was silently truncated");
    rejected = false;
    try {
      (void)encode("");
    } catch (const std::invalid_argument&) {
      rejected = true;
    }
    require(rejected, "empty input should be rejected");
    std::cout << "PASS: all QR versions/exports and error paths are "
                 "structurally valid\n";
  } catch (const std::exception& e) {
    std::cerr << "FAIL: " << e.what() << '\n';
    return 1;
  }
}
