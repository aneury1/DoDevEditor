cmake_minimum_required(VERSION 3.20)

option(BUILD_WINDOWS "Cross-compile for Windows using MinGW-w64" OFF)

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

if(BUILD_WINDOWS)
    enable_language(RC)
endif()

set(CMAKE_CXX_STANDARD          17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)

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
set(wxUSE_ZLIB              builtin CACHE STRING "" FORCE)
set(wxUSE_REGEX             builtin CACHE STRING "" FORCE)
set(wxUSE_NANOSVG           builtin CACHE STRING "" FORCE)

FetchContent_Declare(wxWidgets
    URL https://github.com/wxWidgets/wxWidgets/releases/download/v3.2.4/wxWidgets-3.2.4.tar.bz2
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(wxWidgets)

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

if(NOT SI_SOURCES)
    message(FATAL_ERROR "No source files found in ${CMAKE_SOURCE_DIR}/src/")
endif()

if(BUILD_WINDOWS)
    add_executable(${PROJECT_NAME} WIN32 ${SI_SOURCES})
else()
    add_executable(${PROJECT_NAME} ${SI_SOURCES})
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_SOURCE_DIR}/includes")

target_link_libraries(${PROJECT_NAME} PRIVATE
    wx::stc
    wx::aui
    wx::adv
    wx::core
    wx::base
    vterm
    jsoncpp_static)

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
    target_link_options(${PROJECT_NAME} PRIVATE
        -static-libgcc
        -static-libstdc++
        -Wl,-subsystem,windows
        -Wl,--gc-sections)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        comctl32 ole32 oleaut32 uuid winspool winmm
        shell32 shlwapi gdi32 comdlg32 wsock32)
else()
    target_link_options(${PROJECT_NAME} PRIVATE -Wl,--gc-sections)
endif()

if(CMAKE_BUILD_TYPE MATCHES "^(Release|MinSizeRel)$" AND CMAKE_STRIP)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND "${CMAKE_STRIP}" --strip-all "$<TARGET_FILE:${PROJECT_NAME}>"
        VERBATIM)
endif()