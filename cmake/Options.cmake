# ──────────────────────────────────────────────────────────────
#  Options.cmake
#  Single source of truth for every build knob.
#  Include this before any other cmake/ module.
# ──────────────────────────────────────────────────────────────
 
# ── Toolchain ─────────────────────────────────────────────────
option(CMAKE_WIN32_MINGW        "Build with MinGW on Windows"         OFF)
 
# ── Static analysis / instrumentation ─────────────────────────
option(ENABLE_CLANG_TIDY        "Run clang-tidy during compilation"   OFF)
option(ENABLE_COVERAGE          "Instrument binaries for gcov/lcov"   OFF)
 
# ── Testing ───────────────────────────────────────────────────
option(ENABLE_TESTING           "Build the test suite (CTest)"        OFF)
 
# ── Optional third-party fetches ──────────────────────────────
option(FETCH_WXWIDGETS          "Fetch wxWidgets via FetchContent"     ON)
 
# ── Feature flags ─────────────────────────────────────────────
option(USING_PLUGINS            "Enable plugin subsystem"             OFF)
 
# ── Informational print (handy during first-time configuration)
message(STATUS "──────────────── Build options ────────────────────")
message(STATUS "  CMAKE_WIN32_MINGW  : ${CMAKE_WIN32_MINGW}")
message(STATUS "  ENABLE_CLANG_TIDY  : ${ENABLE_CLANG_TIDY}")
message(STATUS "  ENABLE_COVERAGE    : ${ENABLE_COVERAGE}")
message(STATUS "  ENABLE_TESTING     : ${ENABLE_TESTING}")
message(STATUS "  FETCH_WXWIDGETS    : ${FETCH_WXWIDGETS}")
message(STATUS "  USING_PLUGINS      : ${USING_PLUGINS}")
message(STATUS "───────────────────────────────────────────────────")
 