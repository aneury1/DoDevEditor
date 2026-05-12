# ──────────────────────────────────────────────────────────────
#  Platform.cmake
#  Handles platform-specific configuration.
#  Currently supports: MinGW cross-compilation on Windows.
#  Requires: Options.cmake to be included first.
# ──────────────────────────────────────────────────────────────

if(CMAKE_WIN32_MINGW)
    message(STATUS "[Platform] Configuring MinGW toolchain")

    # Load the hand-written MinGW helper and apply it
    include(mingw32)
    setup_mingw_toolchain()

    # libgit2 is fetched only for MinGW builds; on Linux/macOS it is
    # expected to be provided by the system package manager (see Dependencies.cmake).
    include(FetchContent)
    FetchContent_Declare(
        libgit2
        GIT_REPOSITORY https://github.com/libgit2/libgit2.git
        GIT_TAG        v1.7.2   # keep in sync with system version on Linux
    )
    FetchContent_MakeAvailable(libgit2)
    message(STATUS "[Platform] libgit2 fetched via FetchContent")

    # Expose a compile definition so source files can #ifdef on it if needed
    add_compile_definitions(__LIB_GIT2_DEFINED)
endif()