#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "FilePreview.h"
#ifdef PREVIEW_SQLITE
#include <sqlite3.h>
#endif
#include <stdexcept>
void check(bool v, const char* m) {
  if (!v) throw std::runtime_error(m);
}
int main() {
  try {
    auto csv = preview::render(
        ".CSV",
        "\xef\xbb\xbfName,Value\r\n\"A,B\",\"x\"\"y\nline\"\r\n<test>,42\r\n");
    check(csv.html.find("<th>Name</th>") != std::string::npos, "CSV header");
    check(csv.html.find("A,B") != std::string::npos &&
              csv.html.find("x&quot;y<br>line") != std::string::npos,
          "quoted CSV");
    check(csv.html.find("&lt;test&gt;") != std::string::npos, "escape cells");
    auto tsv = preview::render(".tsv", "a\tb\n1\t2", false);
    check(tsv.html.find("Column 2") != std::string::npos &&
              tsv.html.find("<td>2</td>") != std::string::npos,
          "TSV");
    auto html = preview::render(
        ".html",
        "<h1 onclick='evil()'>Title</h1><script>alert(1)</script><img "
        "src='http://localhost/secret'><a "
        "href='file:///secret'>link</a><table><tr><td>Hi</td></tr></"
        "table><style>body{}</style>");
    check(html.html.find("<h1>Title</h1>") != std::string::npos &&
              html.html.find("<td>Hi</td>") != std::string::npos,
          "HTML structure");
    for (auto token :
         {"onclick", "alert(1)", "http://", "file:///", "<img", "<style"})
      check(html.html.find(token) == std::string::npos,
            "unsafe markup stripped");
    auto md =
        preview::render(".md",
                        "# Hello\n- **bold**\n- `code`\n\n```cpp\n#include "
                        "<iostream>\n```\n<script>bad</script>");
    check(md.html.find("<h1>Hello</h1>") != std::string::npos &&
              md.html.find("<li><b>bold</b></li>") != std::string::npos &&
              md.html.find("&lt;iostream&gt;") != std::string::npos &&
              md.html.find("<script>") == std::string::npos,
          "Markdown subset");
    auto mdtable = preview::render(
        ".md", "| Name | Count |\n| --- | ---: |\n| Test | 42 |\n");
    check(mdtable.html.find("<th>Name</th>") != std::string::npos &&
              mdtable.html.find("<td>42</td>") != std::string::npos,
          "Markdown pipe table");
    auto json =
        preview::render(".json", "{\"x\":9007199254740993,\"s\":\"a,b\"}");
    check(json.html.find("9007199254740993") != std::string::npos &&
              json.html.find("a,b") != std::string::npos,
          "JSON lexical format");
    std::string large;
    for (int i = 0; i < 2100; ++i) large += "a,b\n";
    check(preview::render(".csv", large).truncated, "row limit");
    bool invalid = false;
    try {
      preview::render(".csv", "\"unterminated");
    } catch (...) {
      invalid = true;
    }
    check(invalid, "malformed CSV");
    invalid = false;
    try {
      preview::render(".txt", std::string(preview::MaxInput + 1, 'x'));
    } catch (...) {
      invalid = true;
    }
    check(invalid, "input cap");
    auto nodes = preview::jsonTree("{\"items\":[9007199254740993,true]}");
    check(nodes.size() == 4 && nodes[2].depth == 2 &&
              nodes[2].label.find("9007199254740993") != std::string::npos,
          "JSON tree exact integer");
    invalid = false;
    try {
      preview::jsonTree("{\"x\":01}");
    } catch (...) {
      invalid = true;
    }
    check(invalid, "invalid JSON tree");
    check(preview::hexView(std::string("A\0B", 3)).html.find("41 00 42") !=
              std::string::npos,
          "hex binary");
#ifdef PREVIEW_EXPAT
    auto xml = preview::xmlTree("<root x='1'><child>value</child></root>");
    check(xml.size() == 4 && xml[3].label == "value", "XML tree");
    invalid = false;
    try {
      preview::xmlTree("<!DOCTYPE root [<!ENTITY x 'test'>]><root>&x;</root>");
    } catch (...) {
      invalid = true;
    }
    check(invalid, "DTD blocked");
#endif
#ifdef PREVIEW_SQLITE
    auto path =
        std::filesystem::temp_directory_path() /
        ("dodev-preview-test-531-" +
         std::to_string(
             std::chrono::steady_clock::now().time_since_epoch().count()) +
         ".sqlite");
    std::filesystem::remove(path);
    sqlite3* db = nullptr;
    check(sqlite3_open(path.u8string().c_str(), &db) == SQLITE_OK, "test DB");
    check(sqlite3_exec(db,
                       "CREATE TABLE sample(id INTEGER, payload TEXT); INSERT "
                       "INTO sample VALUES(42,'<test>');",
                       nullptr, nullptr, nullptr) == SQLITE_OK,
          "create table");
    sqlite3_close(db);
    auto result = preview::sqliteView(path, "sample");
    check(result.html.find("&lt;test&gt;") != std::string::npos &&
              result.html.find("42") != std::string::npos,
          "SQLite table");
    auto query =
        preview::sqliteView(path, "", "SELECT count(*) AS total FROM sample");
    check(query.html.find("<td>1</td>") != std::string::npos,
          "SQL read-only query");
    invalid = false;
    try {
      preview::sqliteView(path, "", "DELETE FROM sample");
    } catch (...) {
      invalid = true;
    }
    check(invalid, "write rejected");
    invalid = false;
    try {
      preview::sqliteView(path, "", "ATTACH DATABASE 'bad.db' AS bad");
    } catch (...) {
      invalid = true;
    }
    check(invalid, "attach rejected");
    std::filesystem::remove(path);
#endif
#ifdef PREVIEW_TEST_PDF
    // Minimal PDF fixture with a valid cross-reference table, no external
    // assets.
    auto pdf =
        std::filesystem::temp_directory_path() /
        ("dodev-preview-" +
         std::to_string(
             std::chrono::steady_clock::now().time_since_epoch().count()) +
         ".pdf");
    std::string bytes = "%PDF-1.4\n";
    std::vector<size_t> offsets{0};
    std::vector<std::string> objects = {
        "<< /Type /Catalog /Pages 2 0 R >>",
        "<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
        "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 100 100] /Contents 4 0 R "
        ">>",
        "<< /Length 15 >>\nstream\n0 0 50 50 re f\nendstream"};
    for (size_t i = 0; i < objects.size(); ++i) {
      offsets.push_back(bytes.size());
      bytes += std::to_string(i + 1) + " 0 obj\n" + objects[i] + "\nendobj\n";
    }
    auto start = bytes.size();
    std::ostringstream xref;
    xref << "xref\n0 5\n0000000000 65535 f \n";
    for (size_t i = 1; i < offsets.size(); ++i)
      xref << std::setw(10) << std::setfill('0') << offsets[i] << " 00000 n \n";
    bytes += xref.str() + "trailer\n<< /Size 5 /Root 1 0 R >>\nstartxref\n" +
             std::to_string(start) + "\n%%EOF\n";
    {
      std::ofstream f(pdf, std::ios::binary);
      f << bytes;
    }
    auto png = preview::externalImage(pdf, "", ".pdf", "pdftoppm", 1);
    std::filesystem::remove(pdf);
    check(png.size() > 8 && png.compare(0, 8, "\x89PNG\r\n\x1a\n") == 0,
          "actual PDF rasterization");
#endif
    std::cout << "File preview formats passed\n";
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
