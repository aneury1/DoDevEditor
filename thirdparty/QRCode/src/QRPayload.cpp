#include "QRPayload.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <vector>
#ifdef DODEV_QR_ZLIB
#include <zlib.h>
#endif
#ifdef DODEV_QR_CRYPTO
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#endif
namespace dodevqr {
namespace {
namespace fs = std::filesystem;
constexpr size_t limit = 4 * 1024 * 1024;
constexpr char alphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
void u32(std::string& s, uint32_t n) {
  for (int i = 24; i >= 0; i -= 8) s += static_cast<char>(n >> i);
}
uint32_t get32(const std::string& s, size_t p) {
  if (p + 4 > s.size()) throw std::runtime_error("Invalid length");
  uint32_t n = 0;
  for (int i = 0; i < 4; i++)
    n = (n << 8) | static_cast<unsigned char>(s[p + i]);
  return n;
}
std::string b64(const std::string& data) {
  std::string out;
  unsigned v = 0;
  int bits = 0;
  for (unsigned char c : data) {
    v = (v << 8) | c;
    bits += 8;
    while (bits >= 6) {
      bits -= 6;
      out += alphabet[(v >> bits) & 63];
    }
  }
  if (bits) out += alphabet[(v << (6 - bits)) & 63];
  while (out.size() % 4) out += '=';
  return out;
}
std::string deb64(const std::string& data) {
  if (data.empty() || data.size() % 4 || data.size() > 2 * limit)
    throw std::runtime_error("Invalid base64 length");
  std::string out;
  unsigned v = 0;
  int bits = 0;
  bool end = false;
  for (char c : data) {
    if (c == '=') {
      end = true;
      continue;
    }
    if (end) throw std::runtime_error("Invalid padding");
    auto p = std::find(std::begin(alphabet), std::end(alphabet) - 1, c);
    if (p == std::end(alphabet) - 1) throw std::runtime_error("Invalid base64");
    v = (v << 6) | static_cast<unsigned>(p - alphabet);
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      out += static_cast<char>((v >> bits) & 255);
    }
  }
  if (b64(out) != data) throw std::runtime_error("Invalid base64 encoding");
  return out;
}
#ifdef DODEV_QR_CRYPTO
struct Key {
  std::array<unsigned char, 32> bytes{};
  ~Key() { OPENSSL_cleanse(bytes.data(), bytes.size()); }
};
std::string randomBytes(size_t n) {
  std::string r(n, '\0');
  if (RAND_bytes(reinterpret_cast<unsigned char*>(r.data()),
                 static_cast<int>(n)) != 1)
    throw std::runtime_error("Secure random generation failed");
  return r;
}
std::string crypt(bool encrypt, const std::string& input,
                  const std::string& header, const std::string& salt,
                  const std::string& nonce, const std::string& password,
                  const std::string& tag = {}) {
  Key key;
  if (password.empty() || password.size() > 4096)
    throw std::runtime_error("Password required (maximum 4096 bytes)");
  if (PKCS5_PBKDF2_HMAC(password.data(), static_cast<int>(password.size()),
                        reinterpret_cast<const unsigned char*>(salt.data()), 16,
                        600000, EVP_sha256(), 32, key.bytes.data()) != 1)
    throw std::runtime_error("Key derivation failed");
  std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)> ctx(
      EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
  if (!ctx) throw std::runtime_error("Cannot create cipher context");
  auto* c = ctx.get();
  int n = 0, total = 0;
  std::string out(input.size() + 16, '\0');
  if (EVP_CipherInit_ex(c, EVP_aes_256_gcm(), nullptr, key.bytes.data(),
                        reinterpret_cast<const unsigned char*>(nonce.data()),
                        encrypt ? 1 : 0) != 1 ||
      EVP_CipherUpdate(c, nullptr, &n,
                       reinterpret_cast<const unsigned char*>(header.data()),
                       static_cast<int>(header.size())) != 1 ||
      EVP_CipherUpdate(c, reinterpret_cast<unsigned char*>(out.data()), &n,
                       reinterpret_cast<const unsigned char*>(input.data()),
                       static_cast<int>(input.size())) != 1)
    throw std::runtime_error("Cipher operation failed");
  total = n;
  if (!encrypt && (tag.size() != 16 ||
                   EVP_CIPHER_CTX_ctrl(c, EVP_CTRL_GCM_SET_TAG, 16,
                                       const_cast<char*>(tag.data())) != 1))
    throw std::runtime_error("Invalid tag");
  if (EVP_CipherFinal_ex(
          c, reinterpret_cast<unsigned char*>(out.data()) + total, &n) != 1)
    throw std::runtime_error("Incorrect password or damaged payload");
  out.resize(total + n);
  if (encrypt) {
    std::array<unsigned char, 16> t{};
    if (EVP_CIPHER_CTX_ctrl(c, EVP_CTRL_GCM_GET_TAG, 16, t.data()) != 1)
      throw std::runtime_error("Cannot obtain tag");
    out.append(reinterpret_cast<const char*>(t.data()), t.size());
  }
  return out;
}
#endif
}  // namespace
bool encryptionAvailable() {
#ifdef DODEV_QR_CRYPTO
  return true;
#else
  return false;
#endif
}
std::string packPath(const fs::path& input) {
  auto root = fs::absolute(input).lexically_normal();
  if (root.filename().empty() && root != root.root_path())
    root = root.parent_path();
  if (root == root.root_path())
    throw std::runtime_error("Do not bundle a filesystem root");
  auto st = fs::symlink_status(root);
  if (fs::is_symlink(st) || (!fs::is_regular_file(st) && !fs::is_directory(st)))
    throw std::runtime_error(
        "Select a regular file or directory (no symlinks)");
  std::vector<fs::path> paths{root};
  if (fs::is_directory(st)) {
    for (auto& it : fs::recursive_directory_iterator(root)) {
      if (paths.size() >= 512)
        throw std::runtime_error("Folder exceeds 512 entries");
      paths.push_back(it.path());
    }
  }
  std::sort(paths.begin(), paths.end());
  std::string out = "DQA1";
  u32(out, static_cast<uint32_t>(paths.size()));
  auto base = root.parent_path();
  for (auto& path : paths) {
    auto status = fs::symlink_status(path);
    bool dir = fs::is_directory(status);
    if (fs::is_symlink(status) || (!dir && !fs::is_regular_file(status)))
      throw std::runtime_error("Bundle contains a symlink or special file");
    auto name = path.lexically_relative(base).generic_u8string();
    if (name.empty() || name == "." || name.size() > 4096)
      throw std::runtime_error("Invalid bundle entry path");
    auto bytes = dir ? 0 : fs::file_size(path);
    if (bytes > limit || out.size() + name.size() + bytes + 9 > limit)
      throw std::runtime_error("Bundle exceeds 4 MiB before compression");
    out += dir ? 'D' : 'F';
    u32(out, static_cast<uint32_t>(name.size()));
    u32(out, static_cast<uint32_t>(bytes));
    out += name;
    if (!dir) {
      std::ifstream f(path, std::ios::binary);
      if (!f) throw std::runtime_error("Cannot read selected file");
      std::string data(static_cast<size_t>(bytes), '\0');
      if (bytes && !f.read(data.data(), static_cast<std::streamsize>(bytes)))
        throw std::runtime_error("File changed while reading");
      if (f.peek() != std::char_traits<char>::eof())
        throw std::runtime_error("File changed while reading");
      out += data;
    }
  }
  return out;
}
std::string wrapPayload(const std::string& data, bool encrypt,
                        const std::string& password) {
  if (data.empty() || data.size() > limit)
    throw std::runtime_error("Payload must be 1 byte to 4 MiB");
  std::string body = data;
  unsigned char flags = 0;
#ifdef DODEV_QR_ZLIB
  uLongf n = compressBound(static_cast<uLong>(data.size()));
  std::string compressed(n, '\0');
  if (compress2(reinterpret_cast<Bytef*>(compressed.data()), &n,
                reinterpret_cast<const Bytef*>(data.data()),
                static_cast<uLong>(data.size()), Z_BEST_COMPRESSION) != Z_OK)
    throw std::runtime_error("Compression failed");
  compressed.resize(n);
  if (compressed.size() < body.size()) {
    body = std::move(compressed);
    flags |= 1;
  }
#endif
  if (encrypt) flags |= 2;
  std::string header = "DQP1";
  header += static_cast<char>(flags);
  u32(header, static_cast<uint32_t>(data.size()));
  if (encrypt) {
#ifdef DODEV_QR_CRYPTO
    auto salt = randomBytes(16), nonce = randomBytes(12);
    header += salt + nonce;
    body = crypt(true, body, header, salt, nonce, password);
#else
    throw std::runtime_error(
        "Encryption unavailable: rebuild with OpenSSL development libraries");
#endif
  }
  return "DODEVQR1:" + b64(header + body);
}
std::string unwrapPayload(const std::string& text,
                          const std::string& password) {
  if (text.rfind("DODEVQR1:", 0) != 0)
    throw std::runtime_error("Not a DoDev QR payload");
  auto raw = deb64(text.substr(9));
  if (raw.size() < 9 || raw.substr(0, 4) != "DQP1")
    throw std::runtime_error("Invalid payload header");
  unsigned char flags = raw[4];
  auto size = get32(raw, 5);
  if ((flags & ~3) || size == 0 || size > limit)
    throw std::runtime_error("Invalid payload flags or size");
  std::string body = raw.substr(9);
  if (flags & 2) {
#ifdef DODEV_QR_CRYPTO
    if (raw.size() < 53)
      throw std::runtime_error("Encrypted payload truncated");
    auto salt = raw.substr(9, 16), nonce = raw.substr(25, 12);
    body = crypt(false, raw.substr(37, raw.size() - 53), raw.substr(0, 37),
                 salt, nonce, password, raw.substr(raw.size() - 16));
#else
    throw std::runtime_error("Encryption unavailable");
#endif
  }
  if (flags & 1) {
#ifdef DODEV_QR_ZLIB
    uLongf n = size;
    std::string result(n, '\0');
    if (uncompress(reinterpret_cast<Bytef*>(result.data()), &n,
                   reinterpret_cast<const Bytef*>(body.data()),
                   static_cast<uLong>(body.size())) != Z_OK ||
        n != size)
      throw std::runtime_error("Invalid compressed payload");
    return result;
#else
    throw std::runtime_error("Compression unavailable");
#endif
  }
  if (body.size() != size) throw std::runtime_error("Payload length mismatch");
  return body;
}
}  // namespace dodevqr
