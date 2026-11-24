#include "dodev/themes/ThemeManager.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <regex>

#include "dodev/settings/SettingsManager.h"
namespace dodev {
namespace {
std::string attr(const std::string& tag, const std::string& name,
                 const std::string& fallback = "") {
  const std::regex key("(?:^|\\s)" + name + "\\s*=\\s*\"([^\"]*)\"");
  std::smatch match;
  return std::regex_search(tag, match, key) ? match[1].str() : fallback;
}
std::string tag(const std::string& xml, const std::string& name) {
  const auto pos = xml.find('<' + name);
  if (pos == std::string::npos) return {};
  const auto end = xml.find('>', pos);
  return end == std::string::npos ? "" : xml.substr(pos, end - pos + 1);
}
std::string escape(std::string s) {
  std::string o;
  for (char c : s) {
    switch (c) {
      case '&':
        o += "&amp;";
        break;
      case '<':
        o += "&lt;";
        break;
      case '>':
        o += "&gt;";
        break;
      case '"':
        o += "&quot;";
        break;
      case '\'':
        o += "&apos;";
        break;
      default:
        o += c;
    }
  }
  return o;
}
bool hexColor(const std::string& s) {
  if ((s.size() != 7 && s.size() != 9) || s[0] != '#') return false;
  return std::all_of(s.begin() + 1, s.end(),
                     [](unsigned char c) { return std::isxdigit(c) != 0; });
}
std::string read(const std::filesystem::path& f) {
  std::ifstream in(f, std::ios::binary);
  return in ? std::string(std::istreambuf_iterator<char>(in),
                          std::istreambuf_iterator<char>())
            : "";
}
}  // namespace
bool ThemeDefinition::valid(std::string* error) const {
  if (name.empty() || name.size() > 80 ||
      name.find_first_of("<>\r\n") != std::string::npos) {
    if (error) *error = "Invalid theme name";
    return false;
  }
  if (base != "Dark" && base != "Light" && base != "GlassDark" &&
      base != "GlassLight") {
    if (error) *error = "Invalid Raven base theme";
    return false;
  }
  for (const auto& c : {accent, window, panel, control, input, text, separator,
                        keyword, stringColor, comment})
    if (!hexColor(c)) {
      if (error) *error = "Invalid #RRGGBB or #RRGGBBAA color: " + c;
      return false;
    }
  if (cornerRadius < 0 || cornerRadius > 64) {
    if (error) *error = "Invalid corner radius";
    return false;
  }
  return true;
}
Json ThemeDefinition::toJson() const {
  Json j = Json::object();
  j["formatVersion"] = 1;
  j["name"] = name;
  j["base"] = base;
  j["accent"] = accent;
  j["cornerRadius"] = cornerRadius;
  Json colors = Json::object();
  colors["window"] = window;
  colors["panel"] = panel;
  colors["control"] = control;
  colors["input"] = input;
  colors["text"] = text;
  colors["separator"] = separator;
  j["colors"] = colors;
  Json syntax = Json::object();
  syntax["keyword"] = keyword;
  syntax["string"] = stringColor;
  syntax["comment"] = comment;
  j["syntax"] = syntax;
  return j;
}
bool ThemeDefinition::fromJson(const Json& j, ThemeDefinition& out,
                               std::string* error) {
  if (!j.isObject()) {
    if (error) *error = "Expected JSON object";
    return false;
  }
  ThemeDefinition d;
  d.name = j["name"].string();
  d.base = j["base"].string("Dark");
  d.accent = j["accent"].string(d.accent);
  d.cornerRadius = j["cornerRadius"].integer(12);
  const auto& colors = j["colors"];
  d.window = colors["window"].string(d.window);
  d.panel = colors["panel"].string(d.panel);
  d.control = colors["control"].string(d.control);
  d.input = colors["input"].string(d.input);
  d.text = colors["text"].string(d.text);
  d.separator = colors["separator"].string(d.separator);
  const auto& syntax = j["syntax"];
  d.keyword = syntax["keyword"].string(d.keyword);
  d.stringColor = syntax["string"].string(d.stringColor);
  d.comment = syntax["comment"].string(d.comment);
  if (!d.valid(error)) return false;
  out = std::move(d);
  return true;
}
bool ThemeDefinition::fromXml(const std::string& xml, ThemeDefinition& out,
                              std::string* error) {
  if (xml.size() > 2 * 1024 * 1024) {
    if (error) *error = "Theme too large";
    return false;
  }
  const auto root = tag(xml, "Theme"), colors = tag(xml, "Colors"),
             syntax = tag(xml, "Syntax");
  if (root.empty() || colors.empty() ||
      xml.find("</Theme>") == std::string::npos) {
    if (error) *error = "Expected <Theme> with <Colors>";
    return false;
  }
  ThemeDefinition theme;
  theme.name = attr(root, "Name");
  theme.base = attr(root, "Base", "Dark");
  theme.accent = attr(root, "Accent", theme.accent);
  try {
    theme.cornerRadius = std::stoi(attr(root, "CornerRadius", "12"));
  } catch (...) {
    theme.cornerRadius = 12;
  }
  theme.window = attr(colors, "Window", theme.window);
  theme.panel = attr(colors, "Panel", theme.panel);
  theme.control = attr(colors, "Control", theme.control);
  theme.input = attr(colors, "Input", theme.input);
  theme.text = attr(colors, "Text", theme.text);
  theme.separator = attr(colors, "Separator", theme.separator);
  theme.keyword = attr(syntax, "Keyword", theme.keyword);
  theme.stringColor = attr(syntax, "String", theme.stringColor);
  theme.comment = attr(syntax, "Comment", theme.comment);
  if (!theme.valid(error)) return false;
  theme.originalXml = xml;
  out = std::move(theme);
  return true;
}
std::string ThemeDefinition::toXml() const {
  if (!originalXml.empty()) return originalXml;
  const auto e = [](const std::string& s) { return escape(s); };
  std::string xml = "<Theme Name=\"" + e(name) + "\" Base=\"" + e(base) +
                    "\" Accent=\"" + accent + "\" CornerRadius=\"" +
                    std::to_string(cornerRadius) + "\">\n";
  xml += "  <Colors Window=\"" + window + "\" Panel=\"" + panel +
         "\" Control=\"" + control + "\" Input=\"" + input + "\" Text=\"" +
         text + "\" Separator=\"" + separator + "\"/>\n";
  xml += "  <Metrics FontSize=\"14\" ControlHeight=\"34\" Spacing=\"8\"/>\n";
  xml += "  <Syntax Keyword=\"" + keyword + "\" String=\"" + stringColor +
         "\" Comment=\"" + comment + "\"/>\n";
  xml += "  <Style Class=\"music-editor\" Panel=\"" + input + "\" Input=\"" +
         input + "\"/>\n";
  xml += "  <Style Class=\"music-tabs\" Panel=\"" + panel + "\" Control=\"" +
         control + "\"/>\n";
  xml += "  <Style Class=\"music-toolbar\" Background=\"" + panel +
         "\" CornerRadius=\"" + std::to_string(cornerRadius) + "\"/>\n";
  xml += "  <Style Widget=\"Button\" Class=\"chip\" CornerRadius=\"100\"/>\n";
  return xml + "</Theme>\n";
}
void ThemeManager::add(ThemeDefinition theme) {
  std::string err;
  if (theme.valid(&err)) themes_[theme.name] = std::move(theme);
}
const ThemeDefinition* ThemeManager::find(const std::string& name) const {
  auto i = themes_.find(name);
  return i == themes_.end() ? nullptr : &i->second;
}
std::vector<std::string> ThemeManager::names() const {
  std::vector<std::string> out;
  for (const auto& pair : themes_) out.push_back(pair.first);
  return out;
}
bool ThemeManager::importFile(const std::filesystem::path& path,
                              std::string* error) {
  if (!std::filesystem::is_regular_file(path)) {
    if (error) *error = "Not a file";
    return false;
  }
  if (std::filesystem::file_size(path) > 2 * 1024 * 1024) {
    if (error) *error = "Theme file too large";
    return false;
  }
  const auto src = read(path);
  ThemeDefinition theme;
  if (path.extension() == ".xml") {
    if (!ThemeDefinition::fromXml(src, theme, error)) return false;
  } else if (path.extension() == ".json") {
    Json json;
    std::string err;
    if (!Json::parse(src, json, &err)) {
      if (error) *error = err;
      return false;
    }
    if (!ThemeDefinition::fromJson(json, theme, error)) return false;
  } else {
    if (error) *error = "Only .json or .xml";
    return false;
  }
  theme.source = path.string();
  add(std::move(theme));
  return true;
}
size_t ThemeManager::importDirectory(const std::filesystem::path& dir,
                                     std::vector<std::string>* warnings) {
  std::error_code ec;
  if (dir.empty() || !std::filesystem::is_directory(dir, ec)) return 0;
  size_t n = 0;
  for (const auto& f : std::filesystem::directory_iterator(dir, ec)) {
    if (!f.is_regular_file() ||
        (f.path().extension() != ".json" && f.path().extension() != ".xml"))
      continue;
    std::string error;
    if (importFile(f.path(), &error))
      ++n;
    else if (warnings)
      warnings->push_back(f.path().filename().string() + ": " + error);
  }
  return n;
}
bool ThemeManager::exportFile(const ThemeDefinition& theme,
                              const std::filesystem::path& file,
                              std::string* error) const {
  if (!theme.valid(error)) return false;
  if (file.extension() == ".json")
    return SettingsManager::writeAtomic(file, theme.toJson().dump(2) + "\n",
                                        error);
  if (file.extension() == ".xml")
    return SettingsManager::writeAtomic(file, theme.toXml(), error);
  if (error) *error = "Choose a .json or .xml file";
  return false;
}
bool ThemeManager::exportFile(const std::string& name,
                              const std::filesystem::path& path,
                              std::string* error) const {
  const auto* theme = find(name);
  if (!theme) {
    if (error) *error = "Theme not found: " + name;
    return false;
  }
  return exportFile(*theme, path, error);
}
}  // namespace dodev
