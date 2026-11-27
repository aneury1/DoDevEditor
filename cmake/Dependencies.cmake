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
    include("${CMAKE_CURRENT_LIST_DIR}/WxDependency.cmake")
    include("${CMAKE_CURRENT_LIST_DIR}/WxRuntimePath.cmake")
endif()
