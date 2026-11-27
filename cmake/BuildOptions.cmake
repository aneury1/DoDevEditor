# Public build switches. Keep existing DODEV_* cache names compatible.
option(DODEV_BUILD_GUI "Build wxWidgets desktop frontend" ON)
option(DODEV_BUILD_TESTS "Build and register regression tests" ON)
option(DODEV_BUILD_PLUGIN_EXAMPLES "Build native plugin examples (including Docker Manager)" ON)
option(DODEV_BUILD_QRCODE_PLUGIN "Build and install QR Code plugin" ON)
option(DODEV_BUILD_HTTP_CLIENT_PLUGIN "Build HTTP Client plugin" ON)
# Windows defaults to fetching only when an installed package is unavailable.
option(DODEV_FETCH_WXWIDGETS "Fetch wxWidgets from source if not installed" ${WIN32})
option(DODEV_WX_FORCE_FETCH "Skip installed wxWidgets and use source fallback" OFF)
option(DODEV_WX_FETCH_PROGRESS "Print download, extraction and dependency build logs" ON)
set(DODEV_WX_SOURCE_ARCHIVE "" CACHE FILEPATH "Optional local official wxWidgets 3.2.6 source archive")
option(DODEV_WX_GTK_THEME "Enable application-local GTK3 theme CSS" ON)
option(DODEV_WX_SYNTAX_GUI_TEST "Enable native syntax rendering test (requires display)" OFF)
option(DODEV_ENABLE_AI_HTTP "Enable AI HTTP transport when libcurl is available" ON)
option(DODEV_ENABLE_WARNINGS "Enable additional compiler warnings on core targets" ON)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Static libraries may be linked into native MODULE plugins.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Generate compilation database (Make/Ninja)")

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build configuration" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()
