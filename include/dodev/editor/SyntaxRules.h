#pragma once
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>
#include <vector>

#include "dodev/settings/Json.h"
namespace dodev::syntax {
struct Language {
  const char* id;
  const char* label;
  const char* extensions;
  const char* keywords;
  const char* sample;
};
inline const std::vector<Language>& languages() {
  static const std::vector<Language> all = {
      {"text", "Plain text", ".txt", "", "Plain text\n"},
      {"c", "C", ".c",
       "auto break case char const continue default do double else enum extern "
       "float for goto if inline int long register restrict return short "
       "signed sizeof static struct switch typedef union unsigned void "
       "volatile while _Bool _Atomic _Thread_local",
       "// C\nint main(void) { return 42; }\n"},
      {"cpp", "C++", ".cpp .cc .cxx .h .hpp .hh .hxx .ipp .inl .tpp .c++ .h++",
       "alignas alignof asm auto bool break case catch char char8_t char16_t "
       "char32_t class concept const consteval constexpr constinit const_cast "
       "continue co_await co_return co_yield decltype default delete do double "
       "dynamic_cast else enum explicit export extern false float for friend "
       "goto if inline int long mutable namespace new noexcept nullptr "
       "operator private protected public register reinterpret_cast requires "
       "return short signed sizeof static static_assert static_cast struct "
       "switch template this thread_local throw true try typedef typeid "
       "typename union unsigned using virtual void volatile wchar_t while",
       "// C++\nclass Store { public: int count = 42; };\nint main() { if "
       "(true) return 0; }\nconst char* s = \"return\";\n"},
      {"java", "Java", ".java",
       "abstract assert boolean break byte case catch char class const "
       "continue default do double else enum exports extends final finally "
       "float for goto if implements import instanceof int interface long "
       "module native new non-sealed null open package permits private "
       "protected public record requires return sealed short static strictfp "
       "super switch synchronized this throw throws transient true false try "
       "var void volatile while yield",
       "// Java\npublic class App { public static void main(String[] args) { "
       "return; } }\n"},
      {"kotlin", "Kotlin", ".kt .kts",
       "as break class continue do else false for fun if in interface is null "
       "object package return super this throw true try typealias typeof val "
       "var when while by catch constructor delegate dynamic field file "
       "finally get import init param property receiver set setparam where "
       "actual abstract annotation companion const crossinline data enum "
       "expect external final infix inline inner internal lateinit noinline "
       "open operator out override private protected public reified sealed "
       "suspend tailrec vararg",
       "// Kotlin\nfun main() { val n = 42; if (n > 0) println(\"hello\") }\n"},
      {"csharp", "C#", ".cs",
       "abstract as async await base bool break byte case catch char checked "
       "class const continue decimal default delegate do double else enum "
       "event explicit extern false finally fixed float for foreach goto if "
       "implicit in int interface internal is lock long namespace new null "
       "object operator out override params private protected public readonly "
       "record ref return sbyte sealed short sizeof stackalloc static string "
       "struct switch this throw true try typeof uint ulong unchecked unsafe "
       "ushort using var virtual void volatile while yield",
       "// C#\npublic class App { public int Count = 42; }\n"},
      {"javascript", "JavaScript", ".js .jsx .mjs .cjs",
       "async await break case catch class const continue debugger default "
       "delete do else export extends false finally for function if import in "
       "instanceof let new null return super switch this throw true try typeof "
       "undefined var void while with yield",
       "// JS\nconst value = 42;\nfunction run() { return \"hello\"; }\n"},
      {"typescript", "TypeScript", ".ts .tsx .mts .cts",
       "abstract any as async await boolean break case catch class const "
       "constructor continue declare default delete do else enum export "
       "extends false finally for function if implements import in infer "
       "instanceof interface is keyof let namespace never new null number "
       "object private protected public readonly return string super switch "
       "symbol this throw true try type typeof undefined unknown var void "
       "while yield",
       "// TS\ninterface Item { name: string; }\nfunction run(): number { "
       "return 42; }\n"},
      {"go", "Go", ".go",
       "break case chan const continue default defer else fallthrough for func "
       "go goto if import interface map package range return select struct "
       "switch type var bool byte int string true false nil",
       "// Go\npackage main\nfunc main() { var n = 42; _ = n }\n"},
      {"python", "Python", ".py .pyw .pyi",
       "and as assert async await break class continue def del elif else "
       "except False finally for from global if import in is lambda None "
       "nonlocal not or pass raise return True try while with yield match case",
       "# Python\ndef run():\n    if True:\n        return \"hello\"\n"},
      {"json", "JSON / JSONC", ".json .jsonc .code-workspace",
       "true false null",
       "{\n  \"enabled\": true,\n  \"count\": 42,\n  \"name\": \"hello\"\n}\n"},
      {"shell", "Shell / Bash", ".sh .bash .zsh",
       "if then else elif fi for while until do done case esac in function "
       "select time coproc return exit break continue export local readonly "
       "declare set unset source echo printf test true false",
       "#!/bin/bash\n# Shell\nif true; then echo \"hello\"; fi\n"},
      {"sql", "SQL", ".sql",
       "select from where join inner left right outer on as and or not null "
       "true false insert into values update set delete create table alter "
       "drop index primary key foreign references distinct group by order "
       "having limit offset union all case when then else end begin commit "
       "rollback grant revoke exists between like in is asc desc with "
       "returning",
       "-- SQL\nSELECT name FROM users WHERE id = 42;\n"},
      {"xml", "XML", ".xml .xsd .svg .xsl .ui", "",
       "<!-- XML -->\n<item id=\"42\">Hello</item>\n"},
      {"html", "HTML", ".html .htm .xhtml", "",
       "<!-- HTML -->\n<html><body><p "
       "class=\"message\">Hello</p></body></html>\n"},
      {"css", "CSS", ".css .scss .less", "",
       "/* CSS */\n.message { color: #ff8800; font-size: 14px; }\n"},
      {"yaml", "YAML", ".yaml .yml", "true false null yes no on off",
       "# YAML\nservice:\n  enabled: true\n  port: 8080\n"},
      {"cmake", "CMake", ".cmake",
       "cmake_minimum_required project set option add_executable add_library "
       "target_link_libraries target_include_directories find_package include "
       "if else elseif endif foreach endforeach function endfunction install "
       "message return add_subdirectory enable_testing add_test",
       "# CMake\nproject(Example)\nadd_executable(app main.cpp)\n"},
      {"make", "Makefile", ".mk .mak", "",
       "# Makefile\nall: app\n\t$(CXX) main.cpp -o app\n"},
      {"lua", "Lua", ".lua",
       "and break do else elseif end false for function goto if in local nil "
       "not or repeat return then true until while",
       "-- Lua\nlocal function run()\n  return \"hello\"\nend\n"},
      {"rust", "Rust", ".rs",
       "as async await break const continue crate dyn else enum extern false "
       "fn for if impl in let loop match mod move mut pub ref return self Self "
       "static struct super trait true type unsafe use where while",
       "// Rust\nfn main() { let n = 42; println!(\"{}\", n); }\n"},
      {"markdown", "Markdown", ".md .markdown", "",
       "# Heading\n**Bold** and `code`\n[Link](https://example.com)\n"}};
  return all;
}
inline const Language& language(const std::string& id) {
  for (const auto& l : languages())
    if (id == l.id) return l;
  return languages().front();
}
inline std::string lower(std::string s) {
  for (auto& c : s)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}
inline std::string detect(const std::filesystem::path& path) {
  auto name = lower(path.filename().string());
  if (name == "cmakelists.txt") return "cmake";
  if (name == "makefile" || name == "gnumakefile") return "make";
  if (name == ".bashrc" || name == ".zshrc") return "shell";
  if (path.extension() == ".C") return "cpp";
  auto ext = lower(path.extension().string());
  if (ext.empty()) return "text";
  for (auto& l : languages())
    if ((" " + std::string(l.extensions) + " ").find(" " + ext + " ") !=
        std::string::npos)
      return l.id;
  return "text";
}
inline const std::vector<std::string>& categories() {
  static const std::vector<std::string> c = {
      "default",  "keyword",    "comment", "string",   "number",
      "operator", "identifier", "type",    "function", "preprocessor",
      "property", "error",      "tag"};
  return c;
}
inline bool colorValid(const std::string& s) {
  return s.size() == 7 && s[0] == '#' &&
         std::all_of(s.begin() + 1, s.end(),
                     [](unsigned char c) { return std::isxdigit(c) != 0; });
}
inline Json sanitize(const Json& rule) {
  Json out = Json::object();
  for (auto k : {"foreground", "background"})
    if (colorValid(rule[k].string())) out[k] = rule[k];
  for (auto k : {"fontSize", "fontWeight"})
    if (rule[k].isNumber()) {
      double n = rule[k].number();
      if (n >= (std::string(k) == "fontSize" ? 8 : 100) &&
          n <= (std::string(k) == "fontSize" ? 64 : 900) &&
          n == static_cast<int>(n))
        out[k] = rule[k];
    }
  for (auto k : {"italic", "underline", "strike"})
    if (rule[k].isBool()) out[k] = rule[k];
  return out;
}
inline const Json& scope(const Json& rules, const std::string& theme) {
  return theme.empty() ? rules["global"] : rules["themes"][theme];
}
inline Json& scope(Json& rules, const std::string& theme) {
  return theme.empty() ? rules["global"] : rules["themes"][theme];
}
inline Json resolve(const Json& rules, const std::string& theme,
                    const std::string& lang, const std::string& category,
                    const std::string& word = {}) {
  Json out = Json::object();
  for (auto t : {std::string{}, theme}) {
    const auto& sc = scope(rules, t);
    for (auto l : {std::string("*"), lang})
      out.merge(sanitize(sc[l]["tokens"][category]));
  }
  if (!word.empty())
    for (auto t : {std::string{}, theme})
      out.merge(sanitize(scope(rules, t)[lang]["keywords"][word]));
  return out;
}
inline Json rule(const Json& rules, const std::string& theme,
                 const std::string& lang, const std::string& category,
                 const std::string& word = {}) {
  const auto& l = scope(rules, theme)[lang];
  return sanitize(word.empty() ? l["tokens"][category] : l["keywords"][word]);
}
inline void setRule(Json& rules, const std::string& theme,
                    const std::string& lang, const std::string& category,
                    const std::string& word, const Json& value) {
  auto& l = scope(rules, theme)[lang];
  if (word.empty())
    l["tokens"][category] = sanitize(value);
  else
    l["keywords"][word] = sanitize(value);
}
inline std::vector<std::string> keywordOverrides(const Json& rules,
                                                 const std::string& theme,
                                                 const std::string& lang) {
  std::vector<std::string> out;
  for (auto t : {std::string{}, theme})
    for (auto& kv : scope(rules, t)[lang]["keywords"].asObject())
      if (!sanitize(kv.second).asObject().empty()) out.push_back(kv.first);
  std::sort(out.begin(), out.end());
  out.erase(std::unique(out.begin(), out.end()), out.end());
  return out;
}
}  // namespace dodev::syntax
