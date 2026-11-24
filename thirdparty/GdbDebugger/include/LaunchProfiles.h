#pragma once
#include "GdbConfig.h"
#include "dodev/terminal/TerminalTasks.h"
namespace dodev::gdb {
// Build a native execution plan for a local cppdbg launch, without GDB.
std::vector<terminal::TaskLaunch> planWithoutDebug(
    const terminal::TaskContext& ctx, const std::string& name);
// Append one profile without changing existing JSON values. Caller controls
// saving.
Json appendLaunchProfile(Json document, const std::string& name,
                         const std::string& program);
}  // namespace dodev::gdb
