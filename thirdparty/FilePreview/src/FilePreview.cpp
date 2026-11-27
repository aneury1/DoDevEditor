#include "FilePreview.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>
namespace preview {
namespace {
std::string lower(std::string s) {
  for (auto& c : s)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}
std::string trim(std::string s) {
  auto a = s.find_first_not_of(" \t\r\n");
  if (a == std::string::npos) return {};
  return s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1);
}
std::string page(const std::string& body) {
  return "<html><body>" + body + "</body></html>";
}
// Deliberately static HTML: preserve structural markup but strip ALL
// attributes, active content and resource elements. Never resolve external or
// local URLs.
std::string safeHtml(const std::string& s) {
  static const std::set<std::string> tags = {
      "p",          "div",   "span",  "br",     "hr", "h1",     "h2",
      "h3",         "h4",    "h5",    "h6",     "b",  "strong", "i",
      "em",         "u",     "s",     "strike", "tt", "code",   "pre",
      "blockquote", "ul",    "ol",    "li",     "dl", "dt",     "dd",
      "table",      "thead", "tbody", "tfoot",  "tr", "th",     "td",
      "caption",    "sub",   "sup",   "center", "a"};
  static const std::set<std::string> blocks = {
      "script", "style", "head", "iframe", "object", "svg", "math", "template"};
  auto lc = lower(s);
  std::string out;
  for (size_t i = 0; i < s.size();) {
    if (s[i] != '<') {
      out += s[i++];
      continue;
    }
    if (lc.compare(i, 9, "<!doctype") == 0) {
      auto end = s.find('>', i + 9);
      i = end == std::string::npos ? s.size() : end + 1;
      continue;
    }
    if (lc.compare(i, 4, "<!--") == 0) {
      auto end = s.find("-->", i + 4);
      i = end == std::string::npos ? s.size() : end + 3;
      continue;
    }
    size_t j = i + 1;
    bool closing = j < s.size() && s[j] == '/';
    if (closing) ++j;
    auto start = j;
    while (j < s.size() && std::isalnum(static_cast<unsigned char>(s[j]))) ++j;
    if (j == start) {
      out += "&lt;";
      ++i;
      continue;
    }
    auto tag = lc.substr(start, j - start);
    char quote = 0;
    size_t end = j;
    for (; end < s.size(); ++end) {
      char c = s[end];
      if (quote) {
        if (c == quote) quote = 0;
      } else if (c == '\'' || c == '"')
        quote = c;
      else if (c == '>')
        break;
    }
    if (end == s.size()) {
      out += escape(s.substr(i));
      break;
    }
    i = end + 1;
    if (!closing && blocks.count(tag)) {
      auto close = lc.find("</" + tag, i);
      if (close == std::string::npos) break;
      auto e = lc.find('>', close);
      i = e == std::string::npos ? s.size() : e + 1;
      continue;
    }
    if (tags.count(tag))
      out += "<" + std::string(closing ? "/" : "") +
             (tag == "a" ? "span" : tag) + ">";
  }
  return out;
}
// Small, safe Markdown subset. Inline markup never becomes arbitrary HTML.
std::string inlines(const std::string& s) {
  if (s.size() > 16384) return escape(s);
  std::string out;
  for (size_t i = 0; i < s.size();) {
    bool found = false;
    for (auto mark : {std::string("**"), std::string("`"), std::string("*")}) {
      if (s.compare(i, mark.size(), mark) != 0) continue;
      auto j = s.find(mark, i + mark.size());
      if (j == std::string::npos || j == i + mark.size()) continue;
      auto tag = mark == "**" ? "b" : mark == "`" ? "tt" : "i";
      out += "<" + std::string(tag) + ">" +
             escape(s.substr(i + mark.size(), j - i - mark.size())) + "</" +
             tag + ">";
      i = j + mark.size();
      found = true;
      break;
    }
    if (!found) {
      out += escape(s.substr(i, 1));
      ++i;
    }
  }
  return out;
}
// A simple pipe-table parser (no escaped pipes or nested inline constructs).
std::vector<std::string> pipeCells(std::string line) {
  line = trim(line);
  if (!line.empty() && line.front() == '|') line.erase(0, 1);
  if (!line.empty() && line.back() == '|') line.pop_back();
  std::vector<std::string> cells;
  size_t pos = 0;
  while (pos <= line.size()) {
    auto end = line.find('|', pos);
    cells.push_back(
        trim(line.substr(pos, end == std::string::npos ? end : end - pos)));
    if (end == std::string::npos) break;
    pos = end + 1;
  }
  return cells;
}
bool tableDivider(const std::vector<std::string>& cells) {
  if (cells.empty() || cells.size() > MaxColumns) return false;
  for (auto c : cells) {
    if (!c.empty() && c.front() == ':') c.erase(0, 1);
    if (!c.empty() && c.back() == ':') c.pop_back();
    if (c.size() < 3 || c.find_first_not_of('-') != std::string::npos)
      return false;
  }
  return true;
}
std::string markdown(const std::string& s) {
  std::istringstream input(s);
  std::vector<std::string> lines;
  std::string line, out, list;
  while (std::getline(input, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    lines.push_back(std::move(line));
  }
  bool code = false;
  auto endList = [&] {
    if (!list.empty()) {
      out += "</" + list + ">";
      list.clear();
    }
  };
  for (size_t i = 0; i < lines.size(); ++i) {
    const auto& raw = lines[i];
    if (raw.rfind("```", 0) == 0 || raw.rfind("~~~", 0) == 0) {
      endList();
      out += code ? "</pre>" : "<pre>";
      code = !code;
      continue;
    }
    if (code) {
      out += escape(raw) + "\n";
      continue;
    }
    auto t = trim(raw);
    if (t.empty()) {
      endList();
      continue;
    }
    // Header followed by a delimiter row identifies a Markdown table.
    if (t.find('|') != std::string::npos && i + 1 < lines.size()) {
      auto headers = pipeCells(t), sep = pipeCells(lines[i + 1]);
      if (headers.size() == sep.size() && tableDivider(sep)) {
        endList();
        out += "<table border=\"1\" cellpadding=\"4\"><tr>";
        for (const auto& cell : headers)
          out += "<th>" + inlines(cell) + "</th>";
        out += "</tr>";
        ++i;
        size_t nr = 0;
        while (i + 1 < lines.size() &&
               lines[i + 1].find('|') != std::string::npos && nr < MaxRows) {
          auto cells = pipeCells(lines[++i]);
          out += "<tr>";
          for (size_t k = 0; k < headers.size(); ++k)
            out +=
                "<td>" + (k < cells.size() ? inlines(cells[k]) : "") + "</td>";
          out += "</tr>";
          ++nr;
        }
        out += "</table>";
        continue;
      }
    }
    size_t n = 0;
    while (n < t.size() && t[n] == '#') ++n;
    if (n && n <= 6 && n < t.size() && t[n] == ' ') {
      endList();
      auto h = "h" + std::to_string(n);
      out += "<" + h + ">" + inlines(t.substr(n + 1)) + "</" + h + ">";
      continue;
    }
    std::string kind;
    size_t pos = 0;
    if (t.rfind("- ", 0) == 0 || t.rfind("* ", 0) == 0 ||
        t.rfind("+ ", 0) == 0) {
      kind = "ul";
      pos = 2;
    } else {
      while (pos < t.size() && std::isdigit(static_cast<unsigned char>(t[pos])))
        ++pos;
      if (pos && t.compare(pos, 2, ". ") == 0) {
        kind = "ol";
        pos += 2;
      }
    }
    if (!kind.empty()) {
      if (list != kind) {
        endList();
        list = kind;
        out += "<" + list + ">";
      }
      out += "<li>" + inlines(t.substr(pos)) + "</li>";
      continue;
    }
    endList();
    if (t == "---" || t == "***")
      out += "<hr>";
    else if (t.rfind("> ", 0) == 0)
      out += "<blockquote>" + inlines(t.substr(2)) + "</blockquote>";
    else
      out += "<p>" + inlines(t) + "</p>";
  }
  endList();
  if (code) out += "</pre>";
  return out;
}
Result table(const std::string& s, char sep, bool header) {
  std::vector<std::vector<std::string>> rows;
  std::vector<std::string> row;
  std::string field;
  bool quoted = false, closed = false, truncated = false;
  size_t columns = 0;
  auto addField = [&] {
    if (row.size() < MaxColumns)
      row.push_back(field);
    else
      truncated = true;
    field.clear();
    closed = false;
  };
  auto addRow = [&] {
    addField();
    columns = std::max(columns, row.size());
    if (rows.size() < MaxRows)
      rows.push_back(std::move(row));
    else
      truncated = true;
    row.clear();
  };
  for (size_t i = 0; i < s.size(); ++i) {
    char c = s[i];
    if (quoted) {
      if (c == '"') {
        if (i + 1 < s.size() && s[i + 1] == '"') {
          field += '"';
          ++i;
        } else {
          quoted = false;
          closed = true;
        }
      } else
        field += c;
      continue;
    }
    if (c == sep) {
      addField();
      continue;
    }
    if (c == '\r' || c == '\n') {
      if (c == '\r' && i + 1 < s.size() && s[i + 1] == '\n') ++i;
      addRow();
      if (rows.size() == MaxRows && i + 1 < s.size()) {
        truncated = true;
        break;
      }
      continue;
    }
    if (closed)
      throw std::runtime_error("Invalid CSV: characters after closing quote");
    if (c == '"') {
      if (!field.empty())
        throw std::runtime_error("Invalid CSV: quote in unquoted field");
      quoted = true;
    } else
      field += c;
  }
  if (quoted)
    throw std::runtime_error("Invalid CSV: unterminated quoted field");
  if (!row.empty() || !field.empty() || closed ||
      (!s.empty() && s.back() == sep))
    addRow();
  std::string out =
      "<p>" + std::to_string(rows.size()) + " displayed rows; " +
      std::to_string(columns) +
      " columns</p><table border=\"1\" cellpadding=\"4\" cellspacing=\"0\">";
  if (!header && columns) {
    out += "<tr>";
    for (size_t k = 0; k < columns; ++k)
      out += "<th>Column " + std::to_string(k + 1) + "</th>";
    out += "</tr>";
  }
  for (size_t i = 0; i < rows.size(); ++i) {
    out += "<tr>";
    std::string tag = header && i == 0 ? "th" : "td";
    for (size_t k = 0; k < columns; ++k) {
      auto val = k < rows[i].size() ? escape(rows[i][k]) : "";
      std::string cell;
      for (char c : val)
        if (c == '\n')
          cell += "<br>";
        else if (c != '\r')
          cell += c;
      out += "<" + tag + ">" + cell + "</" + tag + ">";
    }
    out += "</tr>";
  }
  out += "</table>";
  if (truncated)
    out += "<p>Preview truncated (maximum 2000 rows / 80 columns).</p>";
  return {page(out), truncated};
}
// Lexical formatting preserves JSON numbers and key order. It is not a
// validator.
std::string jsonFormat(const std::string& s) {
  std::string out;
  bool str = false, escaped = false;
  int depth = 0;
  auto newline = [&] {
    out += '\n';
    out.append(static_cast<size_t>(depth) * 2, ' ');
  };
  for (char c : s) {
    if (out.size() > 8 * 1024 * 1024)
      throw std::runtime_error("Formatted JSON exceeds preview limit");
    if (str) {
      out += c;
      if (escaped)
        escaped = false;
      else if (c == '\\')
        escaped = true;
      else if (c == '"')
        str = false;
      continue;
    }
    if (c == '"') {
      str = true;
      out += c;
    } else if (c == '{' || c == '[') {
      out += c;
      if (++depth > 128)
        throw std::runtime_error("JSON nesting exceeds preview limit");
      newline();
    } else if (c == '}' || c == ']') {
      depth = std::max(0, depth - 1);
      newline();
      out += c;
    } else if (c == ',') {
      out += c;
      newline();
    } else if (c == ':')
      out += ": ";
    else if (!std::isspace(static_cast<unsigned char>(c)))
      out += c;
  }
  return out;
}
}  // namespace
std::string escape(const std::string& s) {
  std::string out;
  for (char c : s) {
    switch (c) {
      case '&':
        out += "&amp;";
        break;
      case '<':
        out += "&lt;";
        break;
      case '>':
        out += "&gt;";
        break;
      case '"':
        out += "&quot;";
        break;
      default:
        out += c;
    }
  }
  return out;
}
std::string read(const std::filesystem::path& p) {
  if (!std::filesystem::is_regular_file(p))
    throw std::runtime_error("Select a regular file");
  std::ifstream in(p, std::ios::binary);
  if (!in) throw std::runtime_error("Cannot read file");
  std::string s(MaxInput + 1, '\0');
  in.read(s.data(), static_cast<std::streamsize>(s.size()));
  s.resize(static_cast<size_t>(in.gcount()));
  if (s.size() > MaxInput) throw std::runtime_error("Preview limit: 2 MiB");
  if (in.bad()) throw std::runtime_error("Read failed");
  return s;
}
Result render(std::string ext, const std::string& source, bool csvHeader) {
  if (source.size() > MaxInput)
    throw std::runtime_error("Preview limit: 2 MiB");
  if (source.find('\0') != std::string::npos)
    throw std::runtime_error(
        "Binary file is not supported by this text preview");
  auto s = source;
  if (s.compare(0, 3, "\xef\xbb\xbf") == 0) s.erase(0, 3);
  ext = lower(ext);
  if (ext == ".csv" || ext == ".tsv")
    return table(s, ext == ".csv" ? ',' : '\t', csvHeader);
  if (ext == ".html" || ext == ".htm") return {page(safeHtml(s)), false};
  if (ext == ".md" || ext == ".markdown") return {page(markdown(s)), false};
  if (ext == ".json")
    return {page("<pre>" + escape(jsonFormat(s)) + "</pre>"), false};
  if (ext == ".txt" || ext == ".log" || ext == ".xml" || ext == ".yaml" ||
      ext == ".yml" || ext == ".ini" || ext == ".toml")
    return {page("<pre>" + escape(s) + "</pre>"), false};
  throw std::runtime_error("Unsupported preview format: " + ext);
}
}  // namespace preview
