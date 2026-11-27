#include <filesystem>
#include <iostream>
#include <stdexcept>

#include "HttpClient.h"
using namespace dodev::http;
void check(bool v, const char* message) {
  if (!v) throw std::runtime_error(message);
}
int main(int argc, char** argv) {
  try {
    if (argc != 3) return 2;
    std::string base = argv[1];
    auto path = std::filesystem::u8path(argv[2]) / "history.sqlite";
    Request r{"POST", base + "/echo?secret=hidden",
              "Content-Type: application/json\nX-Test: value", "{\"hello\":42}",
              5};
    auto s = send(r);
    check(s.error.empty() && s.status == 201 && s.body == r.body, "POST echo");
    check(s.headers.find("X-Test-Reply: value") != std::string::npos,
          "headers");
    check(report(r, s).find("RESPONSE") != std::string::npos, "report");
    auto g = send({"GET", base + "/error", "", "", 5});
    check(g.status == 404 && g.error.empty(), "HTTP error status");
    auto h = send({"HEAD", base + "/echo", "", "", 5});
    check(h.status == 200 && h.body.empty(), "HEAD");
    auto redirect = send({"GET", base + "/redirect", "", "", 5});
    check(redirect.status == 302, "redirect not followed");
    auto bad = send({"GET", "file:///etc/passwd", "", "", 5});
    check(!bad.error.empty(), "scheme restriction");
    check(!send({"GET", base, "Invalid header", "", 5}).error.empty(),
          "header validation");
    auto huge = send({"GET", base + "/huge", "", "", 5});
    check(!huge.error.empty() && huge.body.size() <= 2 * 1024 * 1024,
          "response cap");
    remember(path, r, s, false, 10);
    auto rows = history(path);
    check(rows.size() == 1 && rows[0].url == base, "metadata redaction");
    check(lookup(path, rows[0].id).detail.empty(), "metadata no body");
    remember(path, r, s, true, 1);
    rows = history(path);
    check(rows.size() == 1 &&
              lookup(path, rows[0].id).detail.find(r.body) != std::string::npos,
          "full history and retention");
    clear(path);
    check(history(path).empty(), "clear history");
    std::cout << "HTTP and history tests passed\n";
    return 0;
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
