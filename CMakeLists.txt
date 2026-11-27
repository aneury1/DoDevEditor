cmake_minimum_required(VERSION 3.21)
project(DoDevEditor VERSION 5.2.9 LANGUAGES C CXX)

# All paths in these modules are relative to the project root. Modules
# deliberately use include() so existing target names and paths are preserved.
include(GNUInstallDirs)
include(cmake/BuildOptions.cmake)
include(cmake/ProjectHelpers.cmake)
include(cmake/Dependencies.cmake)

# Framework-independent libraries and bundled native extensions.
include(cmake/CoreTargets.cmake)
include(cmake/PluginTargets.cmake)

# Desktop application and installation rules.
if(DODEV_BUILD_GUI)
    include(cmake/WxFrontend.cmake)
endif()
include(cmake/InstallTargets.cmake)

# Tests are optional and can run without wxWidgets.
if(DODEV_BUILD_TESTS)
    enable_testing()
    include(cmake/TestTargets.cmake)
endif()
include(cmake/BuildSummary.cmake)
