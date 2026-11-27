#include "HttpClient.h"

#include <curl/curl.h>
#include <sqlite3.h>

#include <algorithm>
#include <cctype>
#include <memory>
#include <sstream>
#include <stdexcept>
namespace dodev::http {
namespace {
constexpr size_t MaxBody = 2 * 1024 * 1024, MaxHeaders = 128 * 1024;
struct CurlGlobal {
  CurlGlobal() {
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK)
      throw std::runtime_error("curl_global_init failed");
  }
  ~CurlGlobal() { curl_global_cleanup(); }
};
void init() { static CurlGlobal global; }
struct Buffer {
  std::string* target;
  size_t max;
  bool exceeded = false;
};
size_t append(char* p, size_t s, size_t n, void* d) {
  auto& b = *static_cast<Buffer*>(d);
  if (s && n > b.max / s) {
    b.exceeded = true;
    return 0;
  }
  auto bytes = s * n;
  if (bytes > b.max - b.target->size()) {
    b.exceeded = true;
    return 0;
  }
  b.target->append(p, bytes);
  return bytes;
}
struct Progress {
  Cancelled fn;
  void* data;
};
int progress(void* d, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
  auto& p = *static_cast<Progress*>(d);
  return p.fn && p.fn(p.data) ? 1 : 0;
}
bool token(const std::string& s) {
  return !s.empty() && std::all_of(s.begin(), s.end(), [](unsigned char c) {
    return std::isalnum(c) ||
           std::string("!#$%&'*+-.^_`|~").find(c) != std::string::npos;
  });
}
// Preserve valid UTF-8 for text rendering; escape invalid bytes/control
// characters.
std::string display(const std::string& s) {
  std::string out;
  const char* hex = "0123456789ABCDEF";
  auto escape = [&](unsigned char c) {
    out += "\\x";
    out += hex[c >> 4];
    out += hex[c & 15];
  };
  for (size_t i = 0; i < s.size();) {
    unsigned char c = s[i];
    if (c < 128) {
      if (c == 0 || c < 9 || (c > 13 && c < 32) || c == 127)
        escape(c);
      else
        out += char(c);
      ++i;
      continue;
    }
    size_t n = c >= 0xC2 && c <= 0xDF   ? 2
               : c >= 0xE0 && c <= 0xEF ? 3
               : c >= 0xF0 && c <= 0xF4 ? 4
                                        : 0;
    bool ok = n && i + n <= s.size();
    for (size_t k = 1; ok && k < n; ++k) {
      auto b = static_cast<unsigned char>(s[i + k]);
      ok = b >= 0x80 && b <= 0xBF;
      if (k == 1 && ((c == 0xE0 && b < 0xA0) || (c == 0xED && b >= 0xA0) ||
                     (c == 0xF0 && b < 0x90) || (c == 0xF4 && b >= 0x90)))
        ok = false;
    }
    if (ok) {
      out.append(s, i, n);
      i += n;
    } else {
      escape(c);
      ++i;
    }
  }
  return out;
}
struct Db {
  sqlite3* db = nullptr;
  explicit Db(const std::filesystem::path& p) {
    std::filesystem::create_directories(p.parent_path());
    if (sqlite3_open_v2(p.u8string().c_str(), &db,
                        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                        nullptr) != SQLITE_OK) {
      std::string e = db ? sqlite3_errmsg(db) : "open failed";
      sqlite3_close(db);
      throw std::runtime_error(e);
    }
    sqlite3_busy_timeout(db, 3000);
    try {
      exec(
          "CREATE TABLE IF NOT EXISTS requests(id INTEGER PRIMARY KEY "
          "AUTOINCREMENT, date TEXT NOT NULL DEFAULT "
          "(strftime('%Y-%m-%dT%H:%M:%fZ','now')), method TEXT, url TEXT, "
          "status TEXT, detail TEXT)");
    } catch (...) {
      sqlite3_close(db);
      throw;
    }
  }
  ~Db() { sqlite3_close(db); }
  void exec(const char* sql) {
    if (sqlite3_exec(db, sql, nullptr, nullptr, nullptr) != SQLITE_OK)
      throw std::runtime_error(sqlite3_errmsg(db));
  }
};
struct Stmt {
  sqlite3_stmt* s = nullptr;
  Db& d;
  Stmt(Db& db, const char* sql) : d(db) {
    if (sqlite3_prepare_v2(d.db, sql, -1, &s, nullptr) != SQLITE_OK)
      throw std::runtime_error(sqlite3_errmsg(d.db));
  }
  ~Stmt() { sqlite3_finalize(s); }
  void check(int rc) {
    if (rc != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(d.db));
  }
  void text(int i, const std::string& v) {
    check(sqlite3_bind_text(s, i, v.data(), static_cast<int>(v.size()),
                            SQLITE_TRANSIENT));
  }
  void number(int i, long long v) { check(sqlite3_bind_int64(s, i, v)); }
  void done() {
    if (sqlite3_step(s) != SQLITE_DONE)
      throw std::runtime_error(sqlite3_errmsg(d.db));
  }
  std::string str(int i) {
    auto p = sqlite3_column_text(s, i);
    return p ? std::string(reinterpret_cast<const char*>(p),
                           sqlite3_column_bytes(s, i))
             : "";
  }
  Entry entry() {
    return {sqlite3_column_int64(s, 0), str(1), str(2), str(3), str(4), str(5)};
  }
};
}  // namespace
std::string safeUrl(const std::string& url) {
  auto i = url.find("://");
  if (i == std::string::npos ||
      !(url.rfind("http://", 0) == 0 || url.rfind("https://", 0) == 0) ||
      url.find_first_of("\r\n\t \\") != std::string::npos ||
      url.find('\0') != std::string::npos)
    return "[invalid URL]";
  auto end = url.find_first_of("/?#", i + 3);
  auto origin = url.substr(0, end);
  if (origin.find('@') != std::string::npos) return "[URL with credentials]";
  return origin;
}
Response send(const Request& r, Cancelled cancel, void* data) {
  Response res;
  try {
    init();
    if (!token(r.method) ||
        r.url.find_first_of("\r\n\t ") != std::string::npos ||
        r.url.find('\0') != std::string::npos ||
        !(r.url.rfind("http://", 0) == 0 || r.url.rfind("https://", 0) == 0) ||
        safeUrl(r.url).front() == '[')
      throw std::runtime_error(
          "Use an HTTP(S) URL without embedded credentials and a valid method");
    if (r.body.size() > MaxBody || r.headers.size() > MaxHeaders)
      throw std::runtime_error("Request exceeds size limit");
    if (r.method == "HEAD" && !r.body.empty())
      throw std::runtime_error("HEAD requests cannot contain a body");
    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> c(curl_easy_init(),
                                                          curl_easy_cleanup);
    if (!c) throw std::runtime_error("curl_easy_init failed");
    curl_slist* raw = nullptr;
    std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)> list(
        nullptr, curl_slist_free_all);
    std::istringstream input(r.headers);
    std::string line;
    while (std::getline(input, line)) {
      if (!line.empty() && line.back() == '\r') line.pop_back();
      if (line.empty()) continue;
      auto colon = line.find(':');
      if (colon == std::string::npos || !token(line.substr(0, colon)) ||
          line.find('\r') != std::string::npos ||
          line.find('\0') != std::string::npos)
        throw std::runtime_error("Headers must use Name: Value, one per line");
      auto* next = curl_slist_append(raw, line.c_str());
      if (!next) throw std::bad_alloc();
      list.release();
      raw = next;
      list.reset(raw);
    }
    Buffer body{&res.body, MaxBody}, headers{&res.headers, MaxHeaders};
    Progress p{cancel, data};
    char err[CURL_ERROR_SIZE] = {};
    auto set = [&](CURLoption key, auto value) {
      if (curl_easy_setopt(c.get(), key, value) != CURLE_OK)
        throw std::runtime_error("Unsupported curl option");
    };
    set(CURLOPT_URL, r.url.c_str());
    set(CURLOPT_CUSTOMREQUEST, r.method.c_str());
#if LIBCURL_VERSION_NUM >= 0x075500
    set(CURLOPT_PROTOCOLS_STR, "http,https");
#else
    set(CURLOPT_PROTOCOLS, long(CURLPROTO_HTTP | CURLPROTO_HTTPS));
#endif
    set(CURLOPT_FOLLOWLOCATION, 0L);
    set(CURLOPT_SSL_VERIFYPEER, 1L);
    set(CURLOPT_SSL_VERIFYHOST, 2L);
    set(CURLOPT_NOSIGNAL, 1L);
    set(CURLOPT_TIMEOUT, long(std::clamp(r.timeout, 1, 300)));
    set(CURLOPT_CONNECTTIMEOUT,
        long(std::min(10, std::clamp(r.timeout, 1, 300))));
    set(CURLOPT_ERRORBUFFER, err);
    set(CURLOPT_HTTPHEADER, raw);
    set(CURLOPT_WRITEFUNCTION, &append);
    set(CURLOPT_WRITEDATA, &body);
    set(CURLOPT_HEADERFUNCTION, &append);
    set(CURLOPT_HEADERDATA, &headers);
    set(CURLOPT_NOPROGRESS, 0L);
    set(CURLOPT_XFERINFOFUNCTION, &progress);
    set(CURLOPT_XFERINFODATA, &p);
    set(CURLOPT_ACCEPT_ENCODING, "");
    if (r.method == "HEAD")
      set(CURLOPT_NOBODY, 1L);
    else if (!r.body.empty() || r.method == "POST" || r.method == "PUT" ||
             r.method == "PATCH") {
      set(CURLOPT_POSTFIELDS, r.body.data());
      set(CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(r.body.size()));
    }
    auto code = curl_easy_perform(c.get());
    curl_easy_getinfo(c.get(), CURLINFO_RESPONSE_CODE, &res.status);
    curl_easy_getinfo(c.get(), CURLINFO_TOTAL_TIME, &res.elapsed);
    if (code != CURLE_OK)
      res.error = body.exceeded || headers.exceeded
                      ? "Response size limit exceeded (partial response)"
                      : (code == CURLE_ABORTED_BY_CALLBACK
                             ? "Cancelled"
                             : (err[0] ? err : curl_easy_strerror(code)));
  } catch (const std::exception& e) {
    res.error = e.what();
  }
  return res;
}
std::string report(const Request& r, const Response& s) {
  std::ostringstream o;
  o << "REQUEST\n"
    << display(r.method) << " " << display(r.url)
    << "\n\nHeaders (configured)\n"
    << display(r.headers) << "\n\nBody\n"
    << display(r.body) << "\n\nRESPONSE\nStatus: " << s.status
    << "\nElapsed: " << s.elapsed * 1000 << " ms\nBody bytes: " << s.body.size()
    << "\n";
  if (!s.error.empty()) o << "Error: " << display(s.error) << "\n";
  o << "\nResponse headers\n"
    << display(s.headers) << "\n\nResponse body\n"
    << display(s.body);
  return o.str();
}
void remember(const std::filesystem::path& p, const Request& r,
              const Response& s, bool full, int limit) {
  Db db(p);
  db.exec("BEGIN IMMEDIATE");
  Stmt insert(db,
              "INSERT INTO requests(method,url,status,detail) VALUES(?,?,?,?)");
  insert.text(1, r.method);
  insert.text(2, full ? r.url : safeUrl(r.url));
  insert.text(3, s.error.empty() ? std::to_string(s.status) : "Error");
  insert.text(4, full ? report(r, s) : "");
  insert.done();
  Stmt prune(db,
             "DELETE FROM requests WHERE id NOT IN (SELECT id FROM requests "
             "ORDER BY id DESC LIMIT ?)");
  prune.number(1, std::clamp(limit, 1, 1000));
  prune.done();
  db.exec("COMMIT");
}
std::vector<Entry> history(const std::filesystem::path& p) {
  if (!std::filesystem::exists(p)) return {};
  Db db(p);
  Stmt q(db,
         "SELECT id,date,method,url,status,'' FROM requests ORDER BY id DESC");
  std::vector<Entry> out;
  int rc;
  while ((rc = sqlite3_step(q.s)) == SQLITE_ROW) out.push_back(q.entry());
  if (rc != SQLITE_DONE) throw std::runtime_error(sqlite3_errmsg(db.db));
  return out;
}
Entry lookup(const std::filesystem::path& p, long long id) {
  Db db(p);
  Stmt q(db,
         "SELECT id,date,method,url,status,detail FROM requests WHERE id=?");
  q.number(1, id);
  if (sqlite3_step(q.s) != SQLITE_ROW)
    throw std::runtime_error("History entry no longer exists");
  return q.entry();
}
void clear(const std::filesystem::path& p) {
  if (!std::filesystem::exists(p)) return;
  Db db(p);
  db.exec("PRAGMA secure_delete=ON; DELETE FROM requests; VACUUM;");
}
}  // namespace dodev::http
