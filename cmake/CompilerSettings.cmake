# ──────────────────────────────────────────────────────────────
#  CompilerSettings.cmake
#  C++ standard, preprocessor definitions, warnings, coverage,
#  and optional instrumentation.
#  Requires: Options.cmake to be included first.
# ──────────────────────────────────────────────────────────────

# ── C++ standard ──────────────────────────────────────────────
set(CMAKE_CXX_STANDARD          17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)   # prefer -std=c++17 over -std=gnu++17

# ── Preprocessor macros required by some C99/POSIX headers ────
add_compile_definitions(
    __STDC_CONSTANT_MACROS
    __STDC_FORMAT_MACROS
    __STDC_LIMIT_MACROS
)

# ── Per-compiler warning flags ────────────────────────────────
if(NOT MSVC)
    add_compile_options(-Wwrite-strings)
endif()

# ── Code coverage ─────────────────────────────────────────────
if(ENABLE_COVERAGE)
    message(STATUS "[Coverage] instrumentation enabled (-O0 --coverage)")
    add_compile_options(--coverage -O0 -g)
    add_link_options(--coverage)
endif()