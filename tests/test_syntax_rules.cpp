#include <chrono>
#include <iostream>
#include <stdexcept>

#include "dodev/editor/SyntaxRules.h"
#include "dodev/settings/SettingsManager.h"
using namespace dodev;
namespace syn = dodev::syntax;
static void check(bool condition, const char* label) {
  if (!condition) throw std::runtime_error(label);
}
int main() {
  check(syn::detect("main.cpp") == "cpp", "C++ detection");
  check(syn::detect("main.c") == "c", "C detection");
  check(syn::detect("main.KT") == "kotlin", "Kotlin detection");
  check(syn::detect("CMakeLists.txt") == "cmake", "CMake detection");
  check(syn::detect("Makefile") == "make", "Make detection");
  check(syn::detect(".bashrc") == "shell", "shell detection");
  check(syn::detect("tasks.jsonc") == "json", "JSONC detection");
  check(syn::detect("data.unknown") == "text", "plain text fallback");
  Json r = Json::object();
  syn::setRule(
      r, "", "*", "keyword", "",
      Json::Object{
          {"foreground", "#112233"}, {"fontWeight", 400}, {"strike", true}});
  syn::setRule(r, "", "cpp", "keyword", "",
               Json::Object{{"fontSize", 18}, {"foreground", "#223344"}});
  syn::setRule(r, "Dark", "*", "keyword", "",
               Json::Object{{"foreground", "#334455"}, {"italic", true}});
  syn::setRule(r, "Dark", "cpp", "keyword", "",
               Json::Object{{"fontWeight", 700}, {"strike", false}});
  syn::setRule(r, "", "cpp", "keyword", "return",
               Json::Object{{"fontSize", 22}});
  syn::setRule(r, "Dark", "cpp", "keyword", "return",
               Json::Object{{"background", "#abcdef"}, {"underline", true}});
  auto category = syn::resolve(r, "Dark", "cpp", "keyword");
  check(category["foreground"].string() == "#334455", "theme general override");
  check(category["fontSize"].integer() == 18, "language font inheritance");
  check(category["fontWeight"].integer() == 700 &&
            !category["strike"].boolean(true),
        "explicit false and weight");
  auto keyword = syn::resolve(r, "Dark", "cpp", "keyword", "return");
  check(keyword["fontSize"].integer() == 22 &&
            keyword["background"].string() == "#abcdef" &&
            keyword["underline"].boolean(),
        "exact keyword layers");
  check(syn::resolve(r, "Light", "cpp", "keyword", "return")["background"]
                .isString() == false,
        "theme isolation");
  check(syn::resolve(r, "Dark", "python", "keyword", "return")["fontSize"]
                .isNumber() == false,
        "language isolation");
  check(syn::keywordOverrides(r, "Dark", "cpp").size() == 1,
        "unique keyword rule");
  check(syn::keywordOverrides(r, "Dark", "python").empty(),
        "no cross-language keyword");
  syn::setRule(r, "Dark", "cpp", "keyword", "return", Json::object());
  check(syn::resolve(r, "Dark", "cpp", "keyword", "return")["fontSize"]
                .integer() == 22,
        "reset inherits exact global");
  check(!syn::resolve(r, "Dark", "cpp", "keyword", "return")["background"]
             .isString(),
        "reset removes override");
  auto invalid = syn::sanitize(Json::Object{{"foreground", "bad"},
                                            {"background", "#12345678"},
                                            {"fontSize", 4},
                                            {"fontWeight", 901},
                                            {"strike", "true"},
                                            {"italic", false},
                                            {"unknown", 1}});
  check(invalid.asObject().size() == 1 && invalid["italic"].isBool(),
        "invalid property filtering");
  auto root =
      std::filesystem::temp_directory_path() /
      ("dodev-syntax-" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()));
  SettingsManager settings(root);
  settings.set("syntaxHighlighting", r);
  std::string err;
  check(settings.save(&err), "save syntax settings");
  SettingsManager other(root);
  check(other.load(), "load syntax settings");
  check(syn::resolve(other.get("syntaxHighlighting"), "Dark", "cpp", "keyword",
                     "return")["fontSize"]
                .integer() == 22,
        "persistent rule");
  std::filesystem::remove_all(root);
  std::cout << "Syntax rule inheritance, detection, validation and persistence "
               "passed\n";
}
