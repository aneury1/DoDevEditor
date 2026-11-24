#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace dodevqr {
enum class ErrorCorrection { Low = 0, Medium = 1, Quartile = 2, High = 3 };
struct Matrix {
  int version = 0;
  int size = 0;
  int mask = 0;
  std::vector<uint8_t> pixels;  // row-major, black=1
  bool black(int x, int y) const {
    return pixels.at(static_cast<size_t>(y * size + x)) != 0;
  }
};
// Encodes the exact UTF-8 bytes, not a remote URL. QR Version 1..40, byte mode.
// Raises std::length_error if the input cannot be represented in a single QR
// symbol.
Matrix encode(std::string_view utf8Text,
              ErrorCorrection ecc = ErrorCorrection::Medium);
std::string toSvg(const Matrix& qr, int quietModules = 4);
std::vector<uint8_t> toPng(const Matrix& qr, int pixelsPerModule = 8,
                           int quietModules = 4);
}  // namespace dodevqr
