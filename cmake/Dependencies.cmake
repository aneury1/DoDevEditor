# Mandatory dependencies for the core, then optional desktop dependencies.
find_package(Threads REQUIRED)
if(UNIX)
    find_library(DODEV_UTIL_LIBRARY NAMES util)
    include(CheckCXXSourceCompiles)
    include(CMakePushCheckState)
    cmake_push_check_state(RESET)
    if(DODEV_UTIL_LIBRARY)
        set(CMAKE_REQUIRED_LIBRARIES "${DODEV_UTIL_LIBRARY}")
    endif()
    # The current POSIX terminal source includes <pty.h> and uses forkpty.
    check_cxx_source_compiles("#include <pty.h>
int main(){int fd; return forkpty(&fd, nullptr, nullptr, nullptr);}" DODEV_HAVE_FORKPTY)
    cmake_pop_check_state()
    if(NOT DODEV_HAVE_FORKPTY)
        message(FATAL_ERROR "POSIX terminal requires <pty.h> and forkpty(). Install the platform C library development files (libutil where required). Current terminal backend is not portable to POSIX systems without this header/API.")
    endif()
endif()

if(DODEV_BUILD_GUI)
    # Use CMake's FindwxWidgets module for wx-config and Windows installations.
    # Override wxWidgets_CONFIG_EXECUTABLE or wxWidgets_ROOT_DIR if needed.
    find_package(wxWidgets 3.2 QUIET MODULE COMPONENTS core base aui stc adv)
    if(NOT wxWidgets_FOUND)
        message(FATAL_ERROR
            "wxWidgets 3.2+ with core, base, aui, stc and adv is required.\n"
            "Arch/CachyOS: install wxwidgets-gtk3; Debian/Ubuntu: libwxgtk3.2-dev.\n"
            "Custom Unix install: -DwxWidgets_CONFIG_EXECUTABLE=/path/to/wx-config\n"
            "Windows: -DwxWidgets_ROOT_DIR=C:/path/to/wxWidgets and, if needed, -DwxWidgets_LIB_DIR=...\n"
            "Use a fresh build directory when switching compilers or wxWidgets installations.\n"
            "For a backend-only build: -DDODEV_BUILD_GUI=OFF")
    endif()
    # Use a target instead of UsewxWidgets.cmake (which changes directory-wide
    # compiler flags). Works with CMake 3.21 as well as newer releases.
    add_library(dodev_wx_dependencies INTERFACE)
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
    include("${CMAKE_CURRENT_LIST_DIR}/WxRuntimePath.cmake")
endif()
