#pragma once
#include <filesystem>
#include <string>
#include <vector>
namespace dodev::http {
struct Request {
  std::string method = "GET", url, headers, body;
  int timeout = 30;
};
struct Response {
  long status = 0;
  double elapsed = 0;
  std::string headers, body, error;
};
using Cancelled = int (*)(void*);
// Performs one HTTP(S) request. TLS verification is always on; redirects are
// not followed.
Response send(const Request&, Cancelled = nullptr, void* = nullptr);
std::string report(const Request&, const Response&);
std::string safeUrl(const std::string&);
struct Entry {
  long long id = 0;
  std::string date, method, url, status, detail;
};
// History is local to the workspace. Metadata-only mode does not store paths,
// query strings, headers or payloads; full history is explicit opt-in.
void remember(const std::filesystem::path&, const Request&, const Response&,
              bool full, int limit);
std::vector<Entry> history(const std::filesystem::path&);
Entry lookup(const std::filesystem::path&, long long id);
void clear(const std::filesystem::path&);
}  // namespace dodev::http
