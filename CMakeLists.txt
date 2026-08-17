cmake_minimum_required(VERSION 3.20)

option(BUILD_WINDOWS "Cross-compile for Windows using MinGW-w64" OFF)
set(THREADS_PREFER_PTHREAD_FLAG ON)
set(CMAKE_THREAD_LIBS_INIT "-lpthread")
set(CMAKE_COMPILE_WARNING_AS_ERROR OFF)

if(BUILD_WINDOWS)
    find_program(_MCC NAMES x86_64-w64-mingw32-gcc-posix x86_64-w64-mingw32-gcc REQUIRED)
    find_program(_MCX NAMES x86_64-w64-mingw32-g++-posix x86_64-w64-mingw32-g++ REQUIRED)
    find_program(_MRC NAMES x86_64-w64-mingw32-windres REQUIRED)
    find_program(_MST NAMES x86_64-w64-mingw32-strip)
    set(CMAKE_SYSTEM_NAME      Windows CACHE INTERNAL "")
    set(CMAKE_SYSTEM_PROCESSOR x86_64  CACHE INTERNAL "")
    set(CMAKE_C_COMPILER   "${_MCC}" CACHE FILEPATH "" FORCE)
    set(CMAKE_CXX_COMPILER "${_MCX}" CACHE FILEPATH "" FORCE)
    set(CMAKE_RC_COMPILER  "${_MRC}" CACHE FILEPATH "" FORCE)
    if(_MST)
        set(CMAKE_STRIP "${_MST}" CACHE FILEPATH "" FORCE)
    endif()
    set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)
    set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
    set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
    set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
    set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
else()
  # add_compile_defintion(LINUX_PLATFORM)
endif()

project(DoDevEditor VERSION 1.0.0 LANGUAGES C CXX)

find_package(Threads REQUIRED)

# Optional LLVM/Clang source analysis. The editor remains fully buildable when
# libclang is absent; only symbols/call-tree/go-to-definition are disabled.
option(DODEV_ENABLE_LLVM "Enable optional libclang C/C++ analysis" ON)
option(DODEV_REQUIRE_LLVM "Fail configuration when libclang is requested but missing" OFF)

# Optional SSH/systemd journal inspector. This module has no compile-time SSH
# dependency: at runtime it uses an installed OpenSSH client to reach a Linux
# host and runs journalctl there. Import/export/filtering remain cross-platform.
option(DODEV_ENABLE_JOURNAL_LOGS "Enable SSH journal log inspector and log importer" ON)

# Optional standalone file comparison editor. The implementation is entirely
# wxWidgets/C++ and supports text and binary files on Linux and Windows.
option(DODEV_ENABLE_FILE_COMPARE "Enable side-by-side text/binary file comparison editor" ON)

# Optional dependency-free source symbol/call parser for C, C++ and Kotlin.
# This deliberately uses no LLVM/tree-sitter/third-party parsing library.
option(DODEV_ENABLE_MANUAL_SYMBOLS "Enable built-in C/C++/Kotlin symbol and call parser" ON)

# Optional runtime plugin system. Plugins use a small C ABI and may be built
# independently as .so/.dll files.
option(DODEV_ENABLE_PLUGINS "Enable runtime dynamic plugin system" ON)
option(DODEV_BUILD_EXAMPLE_PLUGIN "Build and copy the SDK example plugin" OFF)

set(DODEV_HAS_LIBCLANG OFF)
set(DODEV_LIBCLANG_TARGET "")

if(DODEV_ENABLE_LLVM AND NOT BUILD_WINDOWS)
    # Prefer Clang's CMake package when the distro exports it, then fall back
    # to the stable clang-c/Index.h + libclang shared library pair.
    find_package(Clang CONFIG QUIET)

    if(TARGET libclang)
        set(DODEV_HAS_LIBCLANG ON)
        set(DODEV_LIBCLANG_TARGET libclang)
    else()
        set(_DODEV_CLANG_INCLUDE_HINTS ${CLANG_INCLUDE_DIRS})
        set(_DODEV_CLANG_LIBRARY_HINTS "")
        if(DEFINED LLVM_LIBRARY_DIRS)
            list(APPEND _DODEV_CLANG_LIBRARY_HINTS ${LLVM_LIBRARY_DIRS})
        endif()
        if(DEFINED CLANG_LIBRARY_DIRS)
            list(APPEND _DODEV_CLANG_LIBRARY_HINTS ${CLANG_LIBRARY_DIRS})
        endif()

        # llvm-config makes versioned distro layouts such as
        # /usr/lib/llvm-20/include and /usr/lib/llvm-20/lib discoverable.
        find_program(DODEV_LLVM_CONFIG
            NAMES llvm-config llvm-config-22 llvm-config-21 llvm-config-20 llvm-config-19
                  llvm-config-18 llvm-config-17 llvm-config-16 llvm-config-15 llvm-config-14)
        if(DODEV_LLVM_CONFIG)
            execute_process(
                COMMAND "${DODEV_LLVM_CONFIG}" --includedir
                OUTPUT_VARIABLE _DODEV_LLVM_CONFIG_INCLUDE
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET)
            execute_process(
                COMMAND "${DODEV_LLVM_CONFIG}" --libdir
                OUTPUT_VARIABLE _DODEV_LLVM_CONFIG_LIB
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET)
            if(_DODEV_LLVM_CONFIG_INCLUDE)
                list(APPEND _DODEV_CLANG_INCLUDE_HINTS "${_DODEV_LLVM_CONFIG_INCLUDE}")
            endif()
            if(_DODEV_LLVM_CONFIG_LIB)
                list(APPEND _DODEV_CLANG_LIBRARY_HINTS "${_DODEV_LLVM_CONFIG_LIB}")
            endif()
        endif()

        find_path(DODEV_LIBCLANG_INCLUDE_DIR
            NAMES clang-c/Index.h
            HINTS ${_DODEV_CLANG_INCLUDE_HINTS}
            PATH_SUFFIXES include llvm/include
                lib/llvm14/include lib/llvm15/include lib/llvm16/include lib/llvm17/include
                lib/llvm18/include lib/llvm19/include lib/llvm20/include lib/llvm21/include lib/llvm22/include
                llvm14/include llvm15/include llvm16/include llvm17/include llvm18/include
                llvm19/include llvm20/include llvm21/include llvm22/include)

        find_library(DODEV_LIBCLANG_LIBRARY
            NAMES clang libclang clang-22 clang-21 clang-20 clang-19 clang-18 clang-17 clang-16 clang-15 clang-14
            HINTS ${_DODEV_CLANG_LIBRARY_HINTS}
            PATH_SUFFIXES lib lib64 llvm/lib llvm14/lib llvm15/lib llvm16/lib llvm17/lib
                llvm18/lib llvm19/lib llvm20/lib llvm21/lib llvm22/lib)

        if(DODEV_LIBCLANG_INCLUDE_DIR AND DODEV_LIBCLANG_LIBRARY)
            set(DODEV_HAS_LIBCLANG ON)
        endif()
    endif()

    if(DODEV_HAS_LIBCLANG)
        message(STATUS "DoDevEditor LLVM analysis: enabled")
    elseif(DODEV_REQUIRE_LLVM)
        message(FATAL_ERROR
            "DODEV_REQUIRE_LLVM=ON but libclang was not found. Install Clang/LLVM development files or set DODEV_ENABLE_LLVM=OFF.")
    else()
        message(STATUS "DoDevEditor LLVM analysis: disabled (libclang not found; editor will still build)")
    endif()
elseif(DODEV_ENABLE_LLVM AND BUILD_WINDOWS)
    message(STATUS "DoDevEditor LLVM analysis: disabled for the default MinGW cross-build; provide a target libclang integration to enable it")
endif()

if(BUILD_WINDOWS)
    enable_language(RC)
endif()

set(CMAKE_CXX_STANDARD          17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)

# Keep C dependencies on a known GNU C17 baseline. GCC 15 defaults to GNU C23,
# which can expose additional compatibility problems in older third-party C
# sources. This language-mode selection is intentionally separate from the
# wxWidgets zlib fix below: missing POSIX declarations must be solved by using
# the correct headers/library, not by changing the C standard.
set(CMAKE_C_STANDARD          17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS        ON)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options($<$<COMPILE_LANGUAGE:C>:-std=gnu17>)
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()
 
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

include(FetchContent)
set(FETCHCONTENT_QUIET               OFF)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

set(wxBUILD_SHARED          OFF  CACHE BOOL   "" FORCE)
set(wxBUILD_TESTS           OFF CACHE STRING "" FORCE)
set(wxBUILD_SAMPLES         OFF  CACHE BOOL   "" FORCE)
set(wxBUILD_DEMOS           OFF  CACHE BOOL   "" FORCE)
set(wxBUILD_INSTALL         OFF  CACHE BOOL   "" FORCE)
set(wxBUILD_MONOLITHIC      OFF  CACHE BOOL   "" FORCE)
set(wxUSE_STC               ON   CACHE BOOL   "" FORCE)
set(wxUSE_AUI               ON   CACHE BOOL   "" FORCE)
set(wxUSE_OPENGL            OFF  CACHE BOOL   "" FORCE)
set(wxUSE_WEBVIEW           OFF  CACHE BOOL   "" FORCE)
set(wxUSE_MEDIACTRL         OFF  CACHE BOOL   "" FORCE)
set(wxUSE_LIBLZMA           OFF  CACHE BOOL   "" FORCE)
set(wxUSE_EXPAT             builtin CACHE STRING "" FORCE)
set(wxUSE_LIBJPEG           builtin CACHE STRING "" FORCE)
set(wxUSE_LIBPNG            builtin CACHE STRING "" FORCE)
set(wxUSE_LIBTIFF           builtin CACHE STRING "" FORCE)
# On Linux/Unix use the system zlib.  wxWidgets' bundled zlib is old enough
# that modern GCC (14+) rejects its missing POSIX function declarations
# (read/close/lseek) as errors.  Windows cross-builds keep the bundled copy so
# the project remains self-contained for MinGW.
if(BUILD_WINDOWS)
    set(wxUSE_ZLIB          builtin CACHE STRING "" FORCE)
else()
    set(wxUSE_ZLIB          sys     CACHE STRING "" FORCE)
endif()
set(wxUSE_REGEX             builtin CACHE STRING "" FORCE)
set(wxUSE_NANOSVG           builtin CACHE STRING "" FORCE)

FetchContent_Declare(wxWidgets
    URL https://github.com/wxWidgets/wxWidgets/releases/download/v3.2.11/wxWidgets-3.2.11.tar.bz2
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(wxWidgets)

# Defensive fallback for custom configurations that explicitly switch zlib back
# to "builtin" on POSIX.  zlib's gz*.c files require <unistd.h> declarations.
if(TARGET wxzlib AND UNIX AND NOT WIN32)
    target_compile_definitions(wxzlib PRIVATE HAVE_UNISTD_H=1)
endif()

# libvterm is built directly from source because the editor only needs the
# library target and public headers. SOURCE_SUBDIR intentionally points to a
# non-existent directory so FetchContent populates the repository without
# trying to add libvterm's own build files.
FetchContent_Declare(libvterm
    GIT_REPOSITORY https://github.com/aneury1/libvterm.git
    GIT_TAG        nvim
    GIT_SHALLOW    TRUE
    SOURCE_SUBDIR  __dodeveditor_no_cmake_subdir__
)
FetchContent_MakeAvailable(libvterm)

file(GLOB VTERM_SOURCES CONFIGURE_DEPENDS "${libvterm_SOURCE_DIR}/src/*.c")
if(NOT VTERM_SOURCES)
    message(FATAL_ERROR "libvterm: no C sources found in ${libvterm_SOURCE_DIR}/src")
endif()

add_library(vterm STATIC ${VTERM_SOURCES})
target_include_directories(vterm PUBLIC "${libvterm_SOURCE_DIR}/include")

set(JSONCPP_WITH_TESTS               OFF CACHE BOOL "" FORCE)
set(JSONCPP_WITH_POST_BUILD_UNITTEST OFF CACHE BOOL "" FORCE)
set(JSONCPP_WITH_PKGCONFIG_SUPPORT   OFF CACHE BOOL "" FORCE)
set(JSONCPP_WITH_CMAKE_PACKAGE       OFF CACHE BOOL "" FORCE)
set(JSONCPP_WITH_EXAMPLE             OFF CACHE BOOL "" FORCE)

FetchContent_Declare(jsoncpp
    GIT_REPOSITORY https://github.com/open-source-parsers/jsoncpp.git
    GIT_TAG        1.9.5
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(jsoncpp)

file(GLOB_RECURSE SI_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/*.c")

# Remove optional implementation units entirely when their feature is disabled.
if(NOT DODEV_ENABLE_FILE_COMPARE)
    list(FILTER SI_SOURCES EXCLUDE REGEX ".*/FileComparePage\\.cpp$")
endif()
if(NOT DODEV_ENABLE_MANUAL_SYMBOLS)
    list(FILTER SI_SOURCES EXCLUDE REGEX ".*/symbols/ManualSymbolParser\\.cpp$")
endif()

if(NOT SI_SOURCES)
    message(FATAL_ERROR "No source files found in ${CMAKE_SOURCE_DIR}/src/")
endif()

if(BUILD_WINDOWS)
    add_executable(${PROJECT_NAME} WIN32 ${SI_SOURCES})
else()
    add_executable(${PROJECT_NAME} ${SI_SOURCES})
endif()

if(BUILD_WINDOWS)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_SOURCE_DIR}/includes")

if(DODEV_ENABLE_JOURNAL_LOGS)
    target_compile_definitions(${PROJECT_NAME} PRIVATE DODEV_ENABLE_JOURNAL_LOGS=1)
    message(STATUS "DoDevEditor Journal Logs: enabled")
else()
    target_compile_definitions(${PROJECT_NAME} PRIVATE DODEV_ENABLE_JOURNAL_LOGS=0)
    message(STATUS "DoDevEditor Journal Logs: disabled")
endif()

if(DODEV_ENABLE_FILE_COMPARE)
    target_compile_definitions(${PROJECT_NAME} PRIVATE DODEV_ENABLE_FILE_COMPARE=1)
    message(STATUS "DoDevEditor File Compare: enabled")
else()
    target_compile_definitions(${PROJECT_NAME} PRIVATE DODEV_ENABLE_FILE_COMPARE=0)
    message(STATUS "DoDevEditor File Compare: disabled")
endif()

if(DODEV_ENABLE_MANUAL_SYMBOLS)
    target_compile_definitions(${PROJECT_NAME} PRIVATE DODEV_ENABLE_MANUAL_SYMBOLS=1)
    message(STATUS "DoDevEditor Manual Symbols: enabled (C/C++/Kotlin, no external parser library)")
else()
    target_compile_definitions(${PROJECT_NAME} PRIVATE DODEV_ENABLE_MANUAL_SYMBOLS=0)
    message(STATUS "DoDevEditor Manual Symbols: disabled")
endif()

if(DODEV_ENABLE_PLUGINS)
    target_compile_definitions(${PROJECT_NAME} PRIVATE DODEV_ENABLE_PLUGINS=1)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${CMAKE_DL_LIBS})
    message(STATUS "DoDevEditor Plugins: enabled")
else()
    target_compile_definitions(${PROJECT_NAME} PRIVATE DODEV_ENABLE_PLUGINS=0)
    message(STATUS "DoDevEditor Plugins: disabled")
endif()

if(DODEV_HAS_LIBCLANG)
    target_compile_definitions(${PROJECT_NAME} PRIVATE DODEV_HAS_LIBCLANG=1)
    if(DODEV_LIBCLANG_TARGET)
        target_link_libraries(${PROJECT_NAME} PRIVATE ${DODEV_LIBCLANG_TARGET})
    else()
        target_include_directories(${PROJECT_NAME} PRIVATE "${DODEV_LIBCLANG_INCLUDE_DIR}")
        target_link_libraries(${PROJECT_NAME} PRIVATE "${DODEV_LIBCLANG_LIBRARY}")
    endif()
else()
    target_compile_definitions(${PROJECT_NAME} PRIVATE DODEV_HAS_LIBCLANG=0)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE
    wx::stc
    wx::aui
    wx::adv
    wx::core
    wx::base
    vterm
    jsoncpp_static
    Threads::Threads)

# JSON-driven syntax highlighting and editor themes are runtime resources.
# Keep a copy beside the executable so users can edit them without rebuilding.
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/config"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/config"
    COMMENT "Copying syntax/theme JSON configuration"
    VERBATIM)

if(DODEV_ENABLE_PLUGINS)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins"
        COMMENT "Creating runtime plugins directory"
        VERBATIM)
endif()

target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:
        -Wall -Wextra
        -Wno-unused-parameter
        -Wno-deprecated-declarations>
    $<$<CONFIG:Release>:        -O2>
    $<$<CONFIG:MinSizeRel>:     -Os>
    $<$<CONFIG:Debug>:          -g -O0>
    $<$<CONFIG:RelWithDebInfo>: -O2 -g>)

if(BUILD_WINDOWS)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        WINVER=0x0601
        _WIN32_WINNT=0x0601
        UNICODE
        _UNICODE)
   
   # shared Settings
   #     target_link_options(${PROJECT_NAME} PRIVATE
   #     -static-libgcc
   #     -static-libstdc++
   #     -Wl,-subsystem,windows
   #     -Wl,--gc-sections)
   
   target_link_options(${PROJECT_NAME} PRIVATE
    -static
    -static-libgcc
    -static-libstdc++
    -Wl,-subsystem,windows
    -Wl,--gc-sections
    -Wl,--as-needed
)
   
   
   
        target_link_libraries(${PROJECT_NAME} PRIVATE
        comctl32 ole32 oleaut32 uuid winspool winmm
        shell32 shlwapi gdi32 comdlg32 wsock32)
else()
    target_link_options(${PROJECT_NAME} PRIVATE -Wl,--gc-sections)
endif()

if(DODEV_ENABLE_PLUGINS AND DODEV_BUILD_EXAMPLE_PLUGIN)
    add_subdirectory(plugins/example_plugin)
    add_dependencies(${PROJECT_NAME} dodev_example_plugin)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:dodev_example_plugin>"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/$<TARGET_FILE_NAME:dodev_example_plugin>"
        COMMENT "Copying example DoDevEditor plugin"
        VERBATIM)
endif()

if(CMAKE_BUILD_TYPE MATCHES "^(Release|MinSizeRel)$" AND CMAKE_STRIP)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND "${CMAKE_STRIP}" --strip-all "$<TARGET_FILE:${PROJECT_NAME}>"
        VERBATIM)
endif()