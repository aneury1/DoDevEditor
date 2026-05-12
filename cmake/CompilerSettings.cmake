# ──────────────────────────────────────────────────────────────
#  CompilerSettings.cmake
#  C++ standard, preprocessor definitions, warnings, coverage,
#  and clang-tidy wiring.
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
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-Wwrite-strings)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-Wwrite-strings)
endif()

# ── clang-tidy ────────────────────────────────────────────────
if(ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY_EXE NAMES clang-tidy REQUIRED)
    message(STATUS "[clang-tidy] found: ${CLANG_TIDY_EXE}")
    set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE};-checks=*")
endif()

# ── Code coverage ─────────────────────────────────────────────
if(ENABLE_COVERAGE)
    message(STATUS "[Coverage] instrumentation enabled (-O0 --coverage)")
    add_compile_options(--coverage -O0 -g)
    add_link_options(--coverage)
endif()