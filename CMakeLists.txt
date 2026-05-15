cmake_minimum_required(VERSION 3.20.0)

# ==============================================================================
# Project
# ==============================================================================
set(PROJECT_NAME DoDevEditor)
project(${PROJECT_NAME} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")

# ==============================================================================
# Options
# ==============================================================================
option(CMAKE_WIN32_MINGW64  "Cross-compile for Windows x86-64 (MinGW-w64)" FALSE)
option(CMAKE_WIN32_MINGW    "Legacy MinGW32 build (32-bit)"                FALSE)
option(ENABLE_CLANG_TIDY    "Enable clang-tidy"                            FALSE)
option(ENABLE_COVERAGE      "Enable code coverage"                         FALSE)
option(ENABLE_TESTING       "Enable unit tests"                            FALSE)
option(FETCH_WXWIDGET       "Fetch wxWidgets via FetchContent"             FALSE)
option(FETCH_JSON           "Fetch jsoncpp via FetchContent"               FALSE)
option(ENABLE_THIRDPARTY    "Allow extra third-party libs"                 FALSE)
option(USING_PLUGINS        "Enable plugin support"                        FALSE)

# Path to the Windows wxWidgets bin/ directory (wx*.dll files)
# Produced by:  ./scripts/build-wx-windows.sh --prefix /opt/wx-win
# Pass on cmake command line: -DWX_WIN_BIN_DIR=/opt/wx-win/bin
set(WX_WIN_BIN_DIR "" CACHE PATH
    "Directory with wx*.dll files for Windows deployment (only needed with CMAKE_WIN32_MINGW64=ON)")

# ==============================================================================
# MinGW64 cross-compilation setup
# Must come BEFORE the compiler is first used.
# Two ways to activate:
#   A) cmake -DCMAKE_WIN32_MINGW64=ON ...  (setup_mingw64_toolchain() runs here)
#   B) cmake -DCMAKE_TOOLCHAIN_FILE=cmake/mingw64-toolchain.cmake ...
#            (toolchain file is loaded by cmake before this file, no need to call setup)
# ==============================================================================
if(CMAKE_WIN32_MINGW64)
    include(cmake/mingw64.cmake)
    if(NOT CMAKE_CROSSCOMPILING)
        # Not using the toolchain file – set compilers ourselves
        setup_mingw64_toolchain()
    else()
        message(STATUS "[mingw64] Toolchain file in use – skipping setup_mingw64_toolchain()")
    endif()
endif()

# ==============================================================================
# Legacy MinGW32 (unchanged)
# ==============================================================================
if(CMAKE_WIN32_MINGW)
    include(cmake/mingw32.cmake)
    setup_mingw_toolchain()
endif()

# ==============================================================================
# Optional tooling
# ==============================================================================
if(ENABLE_CLANG_TIDY)
    set(CMAKE_CXX_CLANG_TIDY "clang-tidy;-checks=*")
endif()

if(ENABLE_COVERAGE)
    message(STATUS "Code coverage enabled")
    set(CMAKE_CXX_FLAGS    "${CMAKE_CXX_FLAGS} --coverage -O0 -g")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
endif()

# ==============================================================================
# Third-party libraries
# ==============================================================================
include(FetchContent)

# wxWidgets (optional fetch)
if(FETCH_WXWIDGET)
    message(STATUS "Fetching wxWidgets v3.2.4")
    FetchContent_Declare(wxWidgets
        GIT_REPOSITORY https://github.com/wxWidgets/wxWidgets.git
        GIT_TAG v3.2.4)
    FetchContent_MakeAvailable(wxWidgets)
endif()

# jsoncpp
FetchContent_Declare(jsoncpp
    GIT_REPOSITORY https://github.com/open-source-parsers/jsoncpp.git
    GIT_TAG master)
FetchContent_MakeAvailable(jsoncpp)

# libgit2 (Windows builds only)
if(CMAKE_WIN32_MINGW OR CMAKE_WIN32_MINGW64)
    FetchContent_Declare(libgit2
        GIT_REPOSITORY https://github.com/libgit2/libgit2.git
        GIT_TAG v1.7.1)
    FetchContent_MakeAvailable(libgit2)
endif()

# ==============================================================================
# Common definitions (kept from original)
# ==============================================================================
add_definitions(
    -D__STDC_CONSTANT_MACROS
    -D__STDC_FORMAT_MACROS
    -D__STDC_LIMIT_MACROS
)

# ==============================================================================
# Sources
# ==============================================================================
file(GLOB_RECURSE SRCS src/*.cpp)

# ==============================================================================
# Executable
# ==============================================================================
if(CMAKE_WIN32_MINGW64)
    add_executable(${PROJECT_NAME} WIN32 ${SRCS})   # WIN32 = WinMain, no console
else()
    add_executable(${PROJECT_NAME} ${SRCS})
endif()

include_directories(includes PUBLIC)
target_include_directories(${PROJECT_NAME} PRIVATE
    includes
    ${jsoncpp_SOURCE_DIR}/include
)

# ==============================================================================
# Windows/MinGW64 target configuration
# (compile definitions, flags, linker options, Win32 system libs)
# ==============================================================================
if(CMAKE_WIN32_MINGW64)
    configure_target_mingw64(${PROJECT_NAME})

    # Uncomment to embed version info + icon into the .exe:
    # add_windows_resource(${PROJECT_NAME}
    #     PRODUCT_NAME  "DoDevEditor"
    #     VERSION       "1.0.0.0"
    #     COMPANY       "YourName"
    #     ICON          "${CMAKE_SOURCE_DIR}/resources/app.ico"
    # )
endif()

# ==============================================================================
# wxWidgets (system-installed)
# ==============================================================================
find_package(wxWidgets REQUIRED COMPONENTS core base stc aui)
include(${wxWidgets_USE_FILE})

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${wxWidgets_LIBRARIES}
    jsoncpp_lib
)

# Linux-only libraries (not available when cross-compiling)
if(NOT CMAKE_WIN32_MINGW64 AND NOT CMAKE_WIN32_MINGW)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        vterm
        wx_gtk3u_webview-3.2
    )
endif()

# ==============================================================================
# Compiler warnings
# ==============================================================================
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(${PROJECT_NAME} PRIVATE -Wwrite-strings)
endif()

# ==============================================================================
# Deploy (Windows MinGW64 only)
#
# Available targets after cmake configure:
#   cmake --build <build_dir> --target deploy_DoDevEditor   <- stage all files
#   cmake --build <build_dir> --target deploy               <- same (alias)
#
# Portable ZIP is created automatically as a POST_BUILD step.
#
# NSIS installer:
#   cmake --build <build_dir> --target deploy_DoDevEditor
#   cd <build_dir> && cpack -G NSIS
# ==============================================================================
if(CMAKE_WIN32_MINGW64)
    include(cmake/deploy.cmake)

    set(_DEPLOY_DIR "${CMAKE_BINARY_DIR}/deploy")

    register_deploy_target(${PROJECT_NAME} "${_DEPLOY_DIR}"
        WX_BIN_DIR  "${WX_WIN_BIN_DIR}"
        EXTRA_FILES
            "${CMAKE_SOURCE_DIR}/README.md"
            # "${CMAKE_SOURCE_DIR}/LICENSE"
        # EXTRA_DIRS
        #    "${CMAKE_SOURCE_DIR}/resources"
    )

    create_portable_zip(${PROJECT_NAME} "${_DEPLOY_DIR}"
        APP_NAME "DoDevEditor"
        VERSION  "1.0.0"
    )

    create_nsis_installer(${PROJECT_NAME} "${_DEPLOY_DIR}"
        APP_NAME "DoDevEditor"
        VERSION  "1.0.0"
        VENDOR   "YourName"
        WEBSITE  "https://github.com/yourname/DoDevEditor"
        # ICON   "${CMAKE_SOURCE_DIR}/resources/app.ico"
        # LICENSE "${CMAKE_SOURCE_DIR}/LICENSE"
    )
endif()

# ==============================================================================
# Testing
# ==============================================================================
if(ENABLE_TESTING)
    message(STATUS "Testing enabled")
    enable_testing()
    include(CTest)
    add_subdirectory(test)
endif()

# ==============================================================================
# Install / CPack
# ==============================================================================
include(CPack)
install(TARGETS ${PROJECT_NAME} DESTINATION /usr/bin)