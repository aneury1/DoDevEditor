#include "dodev/settings/Json.h"
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
namespace dodev {
namespace {
void utf8(std::string &o, unsigned cp) {
  if (cp <= 0x7f)
    o += char(cp);
  else if (cp <= 0x7ff) {
    o += char(0xc0 | (cp >> 6));
    o += char(0x80 | (cp & 63));
  } else if (cp <= 0xffff) {
    o += char(0xe0 | (cp >> 12));
    o += char(0x80 | ((cp >> 6) & 63));
    o += char(0x80 | (cp & 63));
  } else {
    o += char(0xf0 | (cp >> 18));
    o += char(0x80 | ((cp >> 12) & 63));
    o += char(0x80 | ((cp >> 6) & 63));
    o += char(0x80 | (cp & 63));
  }
}
struct Parser {
  const std::string &s;
  size_t p = 0;
  std::string err;
  void ws() {
    while (p < s.size() && std::isspace(static_cast<unsigned char>(s[p])))
      ++p;
  }
  bool fail(const char *why) {
    err = std::string(why) + " at byte " + std::to_string(p);
    return false;
  }
  bool hex4(unsigned &out) {
    out = 0;
    for (int i = 0; i < 4; ++i) {
      if (p >= s.size())
        return fail("truncated unicode escape");
      char c = s[p++];
      unsigned d = c >= '0' && c <= '9'   ? c - '0'
                   : c >= 'a' && c <= 'f' ? c - 'a' + 10
                   : c >= 'A' && c <= 'F' ? c - 'A' + 10
                                          : 16;
      if (d > 15)
        return fail("invalid unicode escape");
      out = out * 16 + d;
    }
    return true;
  }
  bool str(std::string &out) {
    if (p >= s.size() || s[p++] != '"')
      return fail("expected string");
    out.clear();
    while (p < s.size()) {
      char c = s[p++];
      if (c == '"')
        return true;
      if (static_cast<unsigned char>(c) < 0x20)
        return fail("control character in string");
      if (c != '\\') {
        out += c;
        continue;
      }
      if (p >= s.size())
        return fail("truncated escape");
      char esc = s[p++];
      switch (esc) {
      case '"':
        out += '"';
        break;
      case '\\':
        out += '\\';
        break;
      case '/':
        out += '/';
        break;
      case 'b':
        out += '\b';
        break;
      case 'f':
        out += '\f';
        break;
      case 'n':
        out += '\n';
        break;
      case 'r':
        out += '\r';
        break;
      case 't':
        out += '\t';
        break;
      case 'u': {
        unsigned a = 0;
        if (!hex4(a))
          return false;
        if (a >= 0xd800 && a <= 0xdbff) {
          if (p + 2 > s.size() || s[p] != '\\' || s[p + 1] != 'u')
            return fail("missing low surrogate");
          p += 2;
          unsigned b = 0;
          if (!hex4(b))
            return false;
          if (b < 0xdc00 || b > 0xdfff)
            return fail("invalid low surrogate");
          a = 0x10000 + ((a - 0xd800) << 10) + (b - 0xdc00);
        } else if (a >= 0xdc00 && a <= 0xdfff)
          return fail("unpaired low surrogate");
        utf8(out, a);
        break;
      }
      default:
        return fail("invalid escape");
      }
    }
    return fail("unterminated string");
  }
  bool val(Json &out, int depth = 0) {
    if (depth > 80)
      return fail("max nesting exceeded");
    ws();
    if (p >= s.size())
      return fail("unexpected end");
    char c = s[p];
    if (c == '"') {
      std::string x;
      if (!str(x))
        return false;
      out = Json(std::move(x));
      return true;
    }
    if (c == '{') {
      ++p;
      Json::Object o;
      ws();
      if (p < s.size() && s[p] == '}') {
        ++p;
        out = Json(std::move(o));
        return true;
      }
      for (;;) {
        ws();
        std::string key;
        if (!str(key))
          return false;
        ws();
        if (p >= s.size() || s[p++] != ':')
          return fail("expected colon");
        Json v;
        if (!val(v, depth + 1))
          return false;
        if (!o.emplace(std::move(key), std::move(v)).second)
          return fail("duplicate key");
        ws();
        if (p >= s.size())
          return fail("unterminated object");
        char e = s[p++];
        if (e == '}')
          break;
        if (e != ',')
          return fail("expected comma");
      }
      out = Json(std::move(o));
      return true;
    }
    if (c == '[') {
      ++p;
      Json::Array a;
      ws();
      if (p < s.size() && s[p] == ']') {
        ++p;
        out = Json(std::move(a));
        return true;
      }
      for (;;) {
        Json v;
        if (!val(v, depth + 1))
          return false;
        a.push_back(std::move(v));
        ws();
        if (p >= s.size())
          return fail("unterminated array");
        char e = s[p++];
        if (e == ']')
          break;
        if (e != ',')
          return fail("expected comma");
      }
      out = Json(std::move(a));
      return true;
    }
    if (s.compare(p, 4, "true") == 0) {
      p += 4;
      out = Json(true);
      return true;
    }
    if (s.compare(p, 5, "false") == 0) {
      p += 5;
      out = Json(false);
      return true;
    }
    if (s.compare(p, 4, "null") == 0) {
      p += 4;
      out = Json(nullptr);
      return true;
    }
    if (c == '-' || (c >= '0' && c <= '9')) {
      size_t start = p;
      if (s[p] == '-')
        ++p;
      if (p >= s.size())
        return fail("missing digits");
      if (s[p] == '0')
        ++p;
      else if (s[p] >= '1' && s[p] <= '9') {
        while (p < s.size() && std::isdigit(static_cast<unsigned char>(s[p])))
          ++p;
      } else
        return fail("bad number");
      if (p < s.size() && s[p] == '.') {
        ++p;
        size_t begin = p;
        while (p < s.size() && std::isdigit(static_cast<unsigned char>(s[p])))
          ++p;
        if (begin == p)
          return fail("bad decimal");
      }
      if (p < s.size() && (s[p] == 'e' || s[p] == 'E')) {
        ++p;
        if (p < s.size() && (s[p] == '+' || s[p] == '-'))
          ++p;
        size_t begin = p;
        while (p < s.size() && std::isdigit(static_cast<unsigned char>(s[p])))
          ++p;
        if (begin == p)
          return fail("bad exponent");
      }
      char *end = nullptr;
      const double n = std::strtod(s.c_str() + start, &end);
      if (!std::isfinite(n) || static_cast<size_t>(end - s.c_str()) != p)
        return fail("number out of range");
      out = Json(n);
      return true;
    }
    return fail("unexpected token");
  }
};
std::string esc(const std::string &s) {
  std::string result = "\"";
  for (unsigned char c : s) {
    switch (c) {
    case '"':
      result += "\\\"";
      break;
    case '\\':
      result += "\\\\";
      break;
    case '\n':
      result += "\\n";
      break;
    case '\r':
      result += "\\r";
      break;
    case '\t':
      result += "\\t";
      break;
    default:
      if (c < 0x20) {
        char b[7];
        std::snprintf(b, sizeof(b), "\\u%04X", c);
        result += b;
      } else
        result += char(c);
    }
  }
  return result + '"';
}
void write(std::ostringstream &o, const Json &v, int indent, int depth) {
  const std::string padding =
      indent > 0 ? std::string(static_cast<size_t>(depth * indent), ' ') : "";
  if (v.isString()) {
    o << esc(v.string());
    return;
  }
  if (v.isBool()) {
    o << (v.boolean() ? "true" : "false");
    return;
  }
  if (v.isNumber()) {
    o << std::setprecision(15) << v.number();
    return;
  }
  if (v.isObject()) {
    o << '{';
    int i = 0;
    for (const auto &pair : v.asObject()) {
      if (i++)
        o << ',';
      if (indent > 0)
        o << '\n'
          << std::string(static_cast<size_t>((depth + 1) * indent), ' ');
      o << esc(pair.first) << ':';
      if (indent > 0)
        o << ' ';
      write(o, pair.second, indent, depth + 1);
    }
    if (i && indent > 0)
      o << '\n' << padding;
    o << '}';
    return;
  }
  if (v.isArray()) {
    o << '[';
    int i = 0;
    for (const auto &item : v.asArray()) {
      if (i++)
        o << ',';
      if (indent > 0)
        o << '\n'
          << std::string(static_cast<size_t>((depth + 1) * indent), ' ');
      write(o, item, indent, depth + 1);
    }
    if (i && indent > 0)
      o << '\n' << padding;
    o << ']';
    return;
  }
  o << "null";
}
} // namespace
bool Json::parse(const std::string &s, Json &out, std::string *error) {
  Parser parser{s, 0, {}};
  Json tmp;
  if (!parser.val(tmp)) {
    if (error)
      *error = parser.err;
    return false;
  }
  parser.ws();
  if (parser.p != s.size()) {
    if (error)
      *error = "unexpected trailing data at byte " + std::to_string(parser.p);
    return false;
  }
  out = std::move(tmp);
  if (error)
    error->clear();
  return true;
}
std::string Json::dump(int indent) const {
  std::ostringstream o;
  write(o, *this, indent, 0);
  return o.str();
}
const Json::Object &Json::asObject() const {
  static const Object empty;
  return isObject() ? std::get<Object>(value_) : empty;
}
const Json::Array &Json::asArray() const {
  static const Array empty;
  return isArray() ? std::get<Array>(value_) : empty;
}
const Json *Json::find(const std::string &key) const {
  if (!isObject())
    return nullptr;
  const auto &o = std::get<Object>(value_);
  const auto it = o.find(key);
  return it == o.end() ? nullptr : &it->second;
}
Json &Json::operator[](const std::string &key) {
  if (!isObject())
    value_ = Object{};
  return std::get<Object>(value_)[key];
}
const Json &Json::operator[](const std::string &key) const {
  static const Json null;
  const Json *found = find(key);
  return found ? *found : null;
}
std::string Json::string(const std::string &fallback) const {
  return isString() ? std::get<std::string>(value_) : fallback;
}
int Json::integer(int fallback) const {
  if (!isNumber())
    return fallback;
  double v = std::get<double>(value_);
  return v >= static_cast<double>(std::numeric_limits<int>::min()) &&
                 v <= static_cast<double>(std::numeric_limits<int>::max())
             ? static_cast<int>(v)
             : fallback;
}
double Json::number(double fallback) const {
  return isNumber() ? std::get<double>(value_) : fallback;
}
bool Json::boolean(bool fallback) const {
  return isBool() ? std::get<bool>(value_) : fallback;
}
void Json::merge(const Json &overlay) {
  if (isObject() && overlay.isObject()) {
    for (const auto &kv : overlay.asObject())
      (*this)[kv.first].merge(kv.second);
  } else
    *this = overlay;
}
} // namespace dodev
