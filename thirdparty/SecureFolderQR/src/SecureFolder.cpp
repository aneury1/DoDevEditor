#include "SecureFolder.h"

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <zlib.h>

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <map>
#include <memory>
#include <stdexcept>

#include "QRMatrix.h"
namespace secureqr {
namespace fs = std::filesystem;
using S = std::string;
constexpr size_t Limit = 64 * 1024 * 1024, Chunk = 1800, MaxParts = 256;
void check(bool ok, const char* msg) {
  if (!ok) throw std::runtime_error(msg);
}
struct Secret {
  S s;
  ~Secret() {
    if (!s.empty()) OPENSSL_cleanse(s.data(), s.size());
  }
};
S read(const fs::path& p, size_t max) {
  check(fs::is_regular_file(p) && !fs::is_symlink(p), "Expected regular file");
  auto n = fs::file_size(p);
  check(n <= max, "File exceeds limit");
  S s(static_cast<size_t>(n), '\0');
  std::ifstream f(p, std::ios::binary);
  check(bool(f.read(s.data(), s.size())), "Read failed");
  return s;
}
void write(const fs::path& p, const S& s) {
  std::ofstream f(p, std::ios::binary);
  check(bool(f.write(s.data(), s.size())), "Write failed");
  f.close();
  check(bool(f), "Close failed");
}
void put(S& s, uint32_t n) {
  for (int i = 24; i >= 0; i -= 8) s += char(n >> i);
}
uint32_t get(const S& s, size_t& p) {
  check(p <= s.size() && s.size() - p >= 4, "Truncated data");
  uint32_t n = 0;
  for (int i = 0; i < 4; ++i) n = (n << 8) | static_cast<unsigned char>(s[p++]);
  return n;
}
S take(const S& s, size_t& p, size_t n) {
  check(p <= s.size() && n <= s.size() - p, "Truncated data");
  auto r = s.substr(p, n);
  p += n;
  return r;
}
S random(size_t n) {
  S s(n, '\0');
  check(RAND_bytes(reinterpret_cast<unsigned char*>(s.data()), int(n)) == 1,
        "Random generation failed");
  return s;
}
S b64(const S& s) {
  S r(4 * ((s.size() + 2) / 3) + 1, '\0');
  int n = EVP_EncodeBlock(reinterpret_cast<unsigned char*>(r.data()),
                          reinterpret_cast<const unsigned char*>(s.data()),
                          int(s.size()));
  check(n >= 0, "Base64 error");
  r.resize(n);
  return r;
}
S unb64(const S& s) {
  check(!s.empty() && s.size() % 4 == 0 && s.size() <= Chunk * MaxParts,
        "Invalid Base64 size");
  S r(s.size(), '\0');
  int n = EVP_DecodeBlock(reinterpret_cast<unsigned char*>(r.data()),
                          reinterpret_cast<const unsigned char*>(s.data()),
                          int(s.size()));
  check(n >= 0, "Invalid Base64");
  if (s.back() == '=') --n;
  if (s[s.size() - 2] == '=') --n;
  check(n >= 0, "Invalid padding");
  r.resize(n);
  check(b64(r) == s, "Noncanonical Base64");
  return r;
}
S crypt(const S& data, const S& password, const S& header, bool enc,
        const S& tag = {}) {
  Secret key{S(32, '\0')};
  check(PKCS5_PBKDF2_HMAC(
            password.data(), int(password.size()),
            reinterpret_cast<const unsigned char*>(header.data() + 4), 16,
            600000, EVP_sha256(), 32,
            reinterpret_cast<unsigned char*>(key.s.data())) == 1,
        "KDF failed");
  std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)> c(
      EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
  check(bool(c), "Cipher allocation failed");
  auto ptr = [](const S& s) {
    return reinterpret_cast<const unsigned char*>(s.data());
  };
  check(EVP_CipherInit_ex(c.get(), EVP_aes_256_gcm(), nullptr, ptr(key.s),
                          ptr(header) + 20, enc) == 1,
        "Cipher init failed");
  int n = 0, total = 0;
  check(EVP_CipherUpdate(c.get(), nullptr, &n, ptr(header),
                         int(header.size())) == 1,
        "AAD failed");
  S out(data.size() + 16, '\0');
  auto dst = reinterpret_cast<unsigned char*>(out.data());
  check(EVP_CipherUpdate(c.get(), dst, &n, ptr(data), int(data.size())) == 1,
        "Cipher update failed");
  total = n;
  if (!enc)
    check(tag.size() == 16 &&
              EVP_CIPHER_CTX_ctrl(c.get(), EVP_CTRL_GCM_SET_TAG, 16,
                                  const_cast<char*>(tag.data())) == 1,
          "Invalid tag");
  check(EVP_CipherFinal_ex(c.get(), dst + total, &n) == 1,
        "Authentication failed: incorrect secret or damaged data");
  out.resize(total + n);
  if (enc) {
    S t(16, '\0');
    check(EVP_CIPHER_CTX_ctrl(c.get(), EVP_CTRL_GCM_GET_TAG, 16, t.data()) == 1,
          "Tag failed");
    out += t;
  }
  return out;
}
void validateName(const S& name) {
  check(!name.empty() && name.find('\\') == S::npos &&
            name.find(':') == S::npos && name.find('\0') == S::npos,
        "Unsafe path");
  auto rel = fs::u8path(name);
  check(!rel.is_absolute() && !rel.has_root_path(), "Unsafe absolute path");
  for (auto& part : rel) {
    auto v = part.u8string();
    check(v != ".." && v != "." && !v.empty() && v.back() != '.' &&
              v.back() != ' ',
          "Unsafe path component");
    auto stem = v.substr(0, v.find('.'));
    std::transform(stem.begin(), stem.end(), stem.begin(), [](unsigned char c) {
      return c >= 'a' && c <= 'z' ? char(c - 32) : char(c);
    });
    check(stem != "CON" && stem != "PRN" && stem != "AUX" && stem != "NUL" &&
              !(stem.size() == 4 &&
                (stem.substr(0, 3) == "COM" || stem.substr(0, 3) == "LPT") &&
                stem[3] >= '0' && stem[3] <= '9'),
          "Reserved path component");
    for (unsigned char c : v)
      check(c >= 32 && c != '<' && c != '>' && c != '"' && c != '|' &&
                c != '?' && c != '*',
            "Unsupported path character");
  }
}
S pack(const fs::path& root) {
  check(fs::is_directory(root) && !fs::is_symlink(root),
        "Source must be a real folder");
  S s = "DIR1";
  size_t count = 0;
  for (auto& e : fs::recursive_directory_iterator(root)) {
    check(++count <= 100000, "Too many entries");
    check(!e.is_symlink() && (e.is_directory() || e.is_regular_file()),
          "Symlinks and special files are not supported");
    auto name = e.path().lexically_relative(root).generic_u8string();
    check(name.size() <= 4096, "Path too long");
    validateName(name);
    S data = e.is_directory() ? S() : read(e.path(), Limit);
    check(s.size() + name.size() + data.size() + 9 <= Limit,
          "Archive exceeds 64 MiB");
    s += e.is_directory() ? 'D' : 'F';
    put(s, uint32_t(name.size()));
    put(s, uint32_t(data.size()));
    s += name;
    s += data;
  }
  return s;
}
void unpack(const S& s, const fs::path& out) {
  check(s.substr(0, 4) == "DIR1", "Invalid archive");
  size_t p = 4, count = 0;
  while (p < s.size()) {
    check(++count <= 100000, "Too many entries");
    char type = s[p++];
    auto nl = get(s, p), dl = get(s, p);
    check(nl > 0 && nl <= 4096, "Invalid path size");
    auto name = take(s, p, nl);
    validateName(name);
    auto rel = fs::u8path(name);
    auto dest = out / rel;
    check(!fs::exists(dest), "Duplicate archive path");
    check(type == 'D' || type == 'F', "Invalid entry type");
    if (type == 'D') {
      check(dl == 0, "Invalid directory");
      fs::create_directories(dest);
    } else {
      fs::create_directories(dest.parent_path());
      write(dest, take(s, p, dl));
    }
  }
}
struct Output {
  fs::path p;
  bool done = false;
  explicit Output(fs::path x) : p(std::move(x)) {
    check(fs::create_directory(p),
          "Output must be a new folder with an existing parent");
  }
  ~Output() {
    if (!done) {
      std::error_code e;
      fs::remove_all(p, e);
    }
  }
};
void encrypt(const fs::path& source, const fs::path& output,
             const fs::path& secret) {
  check(!fs::is_symlink(source), "Source symlink not supported");
  auto root = fs::canonical(source);
  auto keypath = fs::canonical(secret);
  auto rel = keypath.lexically_relative(root);
  check(rel.empty() || *rel.begin() == "..",
        "Secret file must be outside source folder");
  Secret key{read(secret, 4096)};
  check(key.s.size() >= 16, "Secret must contain at least 16 bytes");
  auto raw = pack(root);
  uLongf n = compressBound(raw.size());
  S compressed(n, '\0');
  check(compress2(reinterpret_cast<Bytef*>(compressed.data()), &n,
                  reinterpret_cast<const Bytef*>(raw.data()), raw.size(),
                  9) == Z_OK,
        "Compression failed");
  compressed.resize(n);
  S header = "SFQ1" + random(16) + random(12);
  put(header, uint32_t(raw.size()));
  check(compressed.size() + 52 <= Chunk * MaxParts * 3 / 4,
        "Encrypted archive too large for 256 QR codes");
  auto encoded = b64(header + crypt(compressed, key.s, header, true));
  size_t count = (encoded.size() + Chunk - 1) / Chunk;
  check(count <= MaxParts, "Encrypted archive too large for 256 QR codes");
  auto id = b64(header.substr(4, 12));
  Output o(output);
  S all;
  for (size_t i = 0; i < count; ++i) {
    S line = "SFQ1:" + id + ":" + std::to_string(i + 1) + ":" +
             std::to_string(count) + ":" + encoded.substr(i * Chunk, Chunk);
    auto base = output / ("part-" + std::to_string(i + 1));
    write(fs::u8path(base.u8string() + ".txt"), line + "\n");
    auto png = dodevqr::toPng(
        dodevqr::encode(line, dodevqr::ErrorCorrection::Medium), 4);
    write(fs::u8path(base.u8string() + ".png"),
          S(reinterpret_cast<const char*>(png.data()), png.size()));
    all += line + "\n";
  }
  write(output / "all-parts.txt", all);
  o.done = true;
}
void decrypt(const fs::path& input, const fs::path& output,
             const fs::path& secret) {
  Secret key{read(secret, 4096)};
  auto lines = read(fs::is_directory(input) ? input / "all-parts.txt" : input,
                    Chunk * MaxParts + 32768);
  std::map<size_t, S> parts;
  S id;
  size_t count = 0;
  size_t pos = 0;
  while (pos < lines.size()) {
    auto end = lines.find('\n', pos);
    if (end == S::npos) end = lines.size();
    auto line = lines.substr(pos, end - pos);
    pos = end + 1;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty()) continue;
    check(line.rfind("SFQ1:", 0) == 0, "Invalid QR prefix");
    size_t a = 5, b = line.find(':', a);
    check(b != S::npos, "Invalid QR header");
    auto thisid = line.substr(a, b - a);
    a = b + 1;
    b = line.find(':', a);
    check(b != S::npos, "Invalid QR index");
    auto number = [](const S& v) {
      check(!v.empty() && v.size() <= 3 &&
                std::all_of(v.begin(), v.end(),
                            [](char c) { return c >= '0' && c <= '9'; }),
            "Invalid part number");
      return size_t(std::stoul(v));
    };
    auto index = number(line.substr(a, b - a));
    a = b + 1;
    b = line.find(':', a);
    check(b != S::npos, "Invalid QR count");
    auto total = number(line.substr(a, b - a));
    check(total > 0 && total <= MaxParts && index > 0 && index <= total,
          "Invalid part range");
    if (!count) {
      count = total;
      id = thisid;
    }
    check(count == total && id == thisid, "Mixed QR sets");
    auto payload = line.substr(b + 1);
    check(!payload.empty() && payload.size() <= Chunk, "Invalid payload size");
    auto inserted = parts.emplace(index, payload);
    check(inserted.second || inserted.first->second == payload,
          "Conflicting duplicate part");
  }
  check(count > 0 && parts.size() == count, "Missing QR parts");
  S joined;
  for (auto& part : parts) joined += part.second;
  auto data = unb64(joined);
  check(data.size() >= 52 && data.substr(0, 4) == "SFQ1" &&
            b64(data.substr(4, 12)) == id,
        "Invalid envelope");
  auto header = data.substr(0, 36);
  size_t off = 32;
  auto size = get(header, off);
  check(size >= 4 && size <= Limit, "Invalid archive size");
  auto compressed = crypt(data.substr(36, data.size() - 52), key.s, header,
                          false, data.substr(data.size() - 16));
  S raw(size, '\0');
  uLongf n = size;
  check(uncompress(reinterpret_cast<Bytef*>(raw.data()), &n,
                   reinterpret_cast<const Bytef*>(compressed.data()),
                   compressed.size()) == Z_OK &&
            n == size,
        "Decompression failed");
  Output o(output);
  unpack(raw, output);
  o.done = true;
}
}  // namespace secureqr
