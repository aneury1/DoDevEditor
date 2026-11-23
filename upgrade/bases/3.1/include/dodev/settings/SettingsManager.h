#pragma once
#include "dodev/settings/Json.h"
#include <filesystem>
#include <string>
#include <vector>
namespace dodev {
namespace files = std::filesystem;
class SettingsManager {
public:
  explicit SettingsManager(files::path root = {});
  static files::path defaultDirectory();
  const files::path &directory() const { return root_; }
  files::path file() const { return root_ / "settings.json"; }
  bool load(); // Missing files are normal; malformed files preserve defaults
               // and remain untouched.
  bool save(std::string *error =
                nullptr); // Never automatically overwrites malformed files.
  bool maySave() const { return !malformed_; }
  const std::vector<std::string> &diagnostics() const { return diagnostics_; }
  const Json &data() const { return data_; }
  Json &data() { return data_; }
  const Json &get(const std::string &dotted) const;
  void set(const std::string &dotted, Json value);
  std::string getString(const std::string &key,
                        const std::string &fallback = "") const;
  int getInt(const std::string &key, int fallback = 0) const;
  bool getBool(const std::string &key, bool fallback = false) const;
  Json loadPluginSettings(const std::string &id);
  bool savePluginSettings(const std::string &id, const Json &object,
                          std::string *error = nullptr);
  static bool validPluginId(const std::string &id);
  static Json defaultSettings();
  static bool writeAtomic(const files::path &destination,
                          const std::string &contents,
                          std::string *error = nullptr);

private:
  files::path root_;
  Json data_;
  bool malformed_ = false;
  std::vector<std::string> diagnostics_;
  void tryLegacyIni();
  void validate();
};
} // namespace dodev
