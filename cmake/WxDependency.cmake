# One dependency interface for system wxWidgets and the source fallback.
# Keep all wx options target-local, so standalone plugins remain independent.
set(DODEV_WX_COMPONENTS core base aui stc adv html)
set(DODEV_WX_PROVIDER "")
add_library(dodev_wx_dependencies INTERFACE)

# First try an installed modern CMake package on Windows (e.g. vcpkg), then
# CMake's traditional FindwxWidgets module (wx-config or Windows SDK paths).
if(NOT DODEV_WX_FORCE_FETCH AND NOT FETCHCONTENT_SOURCE_DIR_WXWIDGETS AND NOT DODEV_WX_SOURCE_ARCHIVE)
    message(STATUS "[wxWidgets] Searching for an installed wxWidgets 3.2+ (core/base/aui/stc/adv/html)")
    if(WIN32)
        find_package(wxWidgets 3.2 QUIET CONFIG COMPONENTS ${DODEV_WX_COMPONENTS})
        # Imported targets cannot be removed once a config package has loaded.
        # If a broken package registered only some targets, explain how to bypass
        # it instead of failing later on duplicate wx:: target names.
        set(_wx_targets)
        foreach(component IN LISTS DODEV_WX_COMPONENTS)
            if(TARGET wx::${component})
                list(APPEND _wx_targets wx::${component})
            endif()
        endforeach()
        list(LENGTH _wx_targets _wx_count)
        list(LENGTH DODEV_WX_COMPONENTS _wx_required_count)
        if(wxWidgets_FOUND AND _wx_count EQUAL _wx_required_count)
            target_link_libraries(dodev_wx_dependencies INTERFACE ${_wx_targets})
            set(DODEV_WX_PROVIDER "installed-config")
            set(wxWidgets_VERSION_STRING "${wxWidgets_VERSION}")
        elseif(_wx_count GREATER 0)
            message(FATAL_ERROR "[wxWidgets] Installed CMake package is incomplete. Select a complete package or reconfigure with -DDODEV_WX_FORCE_FETCH=ON in a fresh build directory.")
        endif()
    endif()
    if(NOT DODEV_WX_PROVIDER)
        find_package(wxWidgets 3.2 QUIET MODULE COMPONENTS ${DODEV_WX_COMPONENTS})
        if(wxWidgets_FOUND)
            set(DODEV_WX_PROVIDER "installed-module")
            target_include_directories(dodev_wx_dependencies SYSTEM INTERFACE ${wxWidgets_INCLUDE_DIRS})
            target_compile_definitions(dodev_wx_dependencies INTERFACE ${wxWidgets_DEFINITIONS})
            foreach(definition IN LISTS wxWidgets_DEFINITIONS_DEBUG)
                target_compile_definitions(dodev_wx_dependencies INTERFACE "$<$<CONFIG:Debug>:${definition}>")
            endforeach()
            target_compile_options(dodev_wx_dependencies INTERFACE ${wxWidgets_CXX_FLAGS})
            if(wxWidgets_LIBRARY_DIRS)
                target_link_directories(dodev_wx_dependencies INTERFACE ${wxWidgets_LIBRARY_DIRS})
            endif()
            target_link_libraries(dodev_wx_dependencies INTERFACE ${wxWidgets_LIBRARIES})
        endif()
    endif()
endif()

if(NOT DODEV_WX_PROVIDER)
    if(NOT DODEV_FETCH_WXWIDGETS AND NOT DODEV_WX_FORCE_FETCH AND NOT FETCHCONTENT_SOURCE_DIR_WXWIDGETS AND NOT DODEV_WX_SOURCE_ARCHIVE)
        message(FATAL_ERROR
            "wxWidgets 3.2+ with core, base, aui, stc, adv and html is required.\n"
            "Arch/CachyOS: install wxwidgets-gtk3; Debian/Ubuntu: libwxgtk3.2-dev.\n"
            "Custom Unix install: -DwxWidgets_CONFIG_EXECUTABLE=/path/to/wx-config\n"
            "Windows: -DwxWidgets_ROOT_DIR=C:/path/to/wxWidgets and, if needed, -DwxWidgets_LIB_DIR=...\n"
            "Enable source download: -DDODEV_FETCH_WXWIDGETS=ON\n"
            "For a backend-only build: -DDODEV_BUILD_GUI=OFF")
    endif()
    message(STATUS "[wxWidgets] No installed package selected; using source fallback")
    set(_wx_url "https://github.com/wxWidgets/wxWidgets/releases/download/v3.2.6/wxWidgets-3.2.6.tar.bz2")
    # SHA-256 of the full official release archive (also recorded by FreeBSD ports).
    set(_wx_sha "939e5b77ddc5b6092d1d7d29491fe67010a2433cf9b9c0d841ee4d04acb9dce7")
    if(DODEV_WX_SOURCE_ARCHIVE)
        get_filename_component(_wx_url "${DODEV_WX_SOURCE_ARCHIVE}" ABSOLUTE)
        if(NOT EXISTS "${_wx_url}")
            message(FATAL_ERROR "[wxWidgets] Local archive does not exist: ${_wx_url}")
        endif()
    endif()
    include("${CMAKE_CURRENT_LIST_DIR}/WxFetch.cmake")
    dodev_fetch_wxwidgets("${_wx_url}" "${_wx_sha}")
    foreach(component IN LISTS DODEV_WX_COMPONENTS)
        if(NOT TARGET wx::${component})
            message(FATAL_ERROR "[wxWidgets] Fetched source is missing required target wx::${component}")
        endif()
        target_link_libraries(dodev_wx_dependencies INTERFACE wx::${component})
    endforeach()
    set(DODEV_WX_PROVIDER "fetched-static")
    set(wxWidgets_VERSION_STRING "3.2.6 (pinned source; local source override may differ)")
endif()
message(STATUS "[wxWidgets] Provider: ${DODEV_WX_PROVIDER}")
