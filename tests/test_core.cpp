#include "EditorData.h"
#include <cassert>
#include <filesystem>
#include <iostream>
#include <vector>

using namespace dodev;

static void csvTests() {
  const auto csv = parseCsv("name,description,value\r\n\"Doe, "
                            "John\",\"line1\nline2\",\"a\"\"b\"\r\nlast,,42\n");
  assert(csv.valid);
  assert(csv.rows.size() == 3);
  assert(csv.rows[1].size() == 3);
  assert(csv.rows[1][0] == "Doe, John");
  assert(csv.rows[1][1] == "line1\nline2");
  assert(csv.rows[1][2] == "a\"b");
  assert(csv.rows[2][1].empty());
  assert(!parseCsv("one,\"unterminated").valid);
  assert(!parseCsv("\"ok\"bad,x").valid);
  const std::vector<std::vector<std::string>> rows = {
      {"header", "number"}, {"\"quoted\"", "1,2"}, {"new\nline", ""}};
  const auto roundtrip = parseCsv(writeCsv(rows));
  assert(roundtrip.valid && roundtrip.rows == rows);
  assert(parseCsv("").rows.empty());
}

static void dltTests() {
  std::vector<std::uint8_t> d(16, 0);
  d[0] = 'D';
  d[1] = 'L';
  d[2] = 'T';
  d[3] = 1;
  d[4] = 10;
  d[8] = 0x40;
  d[9] = 0x42;
  d[10] = 0x0f;
  d[12] = 'E';
  d[13] = 'C';
  d[14] = 'U';
  d[15] = '1';
  const std::vector<std::uint8_t> standard = {0x01, 0x05, 0x00, 0x13}; // 4+10+5
  d.insert(d.end(), standard.begin(), standard.end());
  const std::vector<std::uint8_t> extended = {0,   1,   'T', 'E', 'S',
                                              'T', 'C', 'T', 'X', '1'};
  d.insert(d.end(), extended.begin(), extended.end());
  const std::vector<std::uint8_t> payload = {'h', 'e', 'l', 'l', 'o'};
  d.insert(d.end(), payload.begin(), payload.end());
  const auto parsed = parseDlt(d);
  assert(parsed.entries.size() == 1);
  assert(parsed.entries.front().ecu == "ECU1");
  assert(parsed.entries.front().apid == "TEST");
  assert(parsed.entries.front().ctid == "CTX1");
  assert(parsed.entries.front().payload == "hello");
  assert(parsed.entries.front().counter == 5);
  assert(parseDlt({1, 2, 3, 4}).entries.empty());
}

static void filesTests() {
  const auto file =
      std::filesystem::temp_directory_path() / "dodev_raven_test_file.txt";
  writeText(file, "Hola Raven\n");
  assert(readText(file) == "Hola Raven\n");
  const auto bytes = readBytes(file);
  assert(bytes.size() == 11);
  assert(!looksBinary(bytes));
  assert(looksBinary({0, 1, 2, 3}));
  assert(hexDump({0x01, 0x41, 0x42}).find("41 42") != std::string::npos);
  assert(fileExtension("foo.CPP") == ".cpp");
  std::filesystem::remove(file);
}
int main() {
  csvTests();
  dltTests();
  filesTests();
  std::cout << "[OK] CSV (quoted/multiline), DLT, I/O, HEX, extensiones\n";
  return 0;
}
