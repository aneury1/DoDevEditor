#pragma once
#include <map>
#include <string>
#include <variant>
#include <vector>

namespace dodev {
class Json {
public:
  using Object = std::map<std::string, Json>;
  using Array = std::vector<Json>;
  using Value =
      std::variant<std::nullptr_t, bool, double, std::string, Array, Object>;
  Json() : value_(nullptr) {}
  Json(std::nullptr_t) : value_(nullptr) {}
  Json(bool v) : value_(v) {}
  Json(int v) : value_(static_cast<double>(v)) {}
  Json(double v) : value_(v) {}
  Json(const char *v) : value_(std::string(v ? v : "")) {}
  Json(std::string v) : value_(std::move(v)) {}
  Json(Array v) : value_(std::move(v)) {}
  Json(Object v) : value_(std::move(v)) {}
  static Json object() { return Object{}; }
  static Json array() { return Array{}; }
  static bool parse(const std::string &text, Json &out,
                    std::string *error = nullptr);
  std::string dump(int indent = 2) const;
  bool isObject() const { return std::holds_alternative<Object>(value_); }
  bool isArray() const { return std::holds_alternative<Array>(value_); }
  bool isString() const { return std::holds_alternative<std::string>(value_); }
  bool isNumber() const { return std::holds_alternative<double>(value_); }
  bool isBool() const { return std::holds_alternative<bool>(value_); }
  const Object &asObject() const;
  const Array &asArray() const;
  const Json *find(const std::string &key) const;
  Json &operator[](const std::string &key);
  const Json &operator[](const std::string &key) const;
  std::string string(const std::string &fallback = "") const;
  int integer(int fallback = 0) const;
  double number(double fallback = 0) const;
  bool boolean(bool fallback = false) const;
  void merge(const Json &overlay);

private:
  Value value_;
};
} // namespace dodev
