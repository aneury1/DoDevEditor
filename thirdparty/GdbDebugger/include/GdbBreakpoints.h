#pragma once
#include "GdbConfig.h"
namespace dodev::gdb {
struct Breakpoint {
  std::string file;
  int line = 0;
};
std::vector<Breakpoint> loadBreakpoints(const Context& ctx);
bool toggleBreakpoint(const Context& ctx, int line);
std::string breakpointLocation(const fs::path& file, int line);
}  // namespace dodev::gdb
