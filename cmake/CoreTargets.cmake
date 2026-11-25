# CLI launcher, editor core, shared services and terminal backend.
add_library(dodev_launch STATIC
    src/launch/LaunchArgs.cpp
    src/launch/ProcessLaunch.cpp
)
target_include_directories(dodev_launch PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_compile_features(dodev_launch PUBLIC cxx_std_17)
target_compile_definitions(dodev_launch PRIVATE DODEV_VERSION="${PROJECT_VERSION}")

add_executable(dodeveditor src/launch/Launcher.cpp)
target_link_libraries(dodeveditor PRIVATE dodev_launch)
dodev_warnings(dodeveditor)
dodev_runtime_directory(dodeveditor "${PROJECT_BINARY_DIR}")
# Windows and default macOS filesystems cannot distinguish filenames by case.
if(WIN32 OR APPLE)
    set_target_properties(dodeveditor PROPERTIES OUTPUT_NAME dodeveditor-cli)
endif()

add_library(dodev_core STATIC
    src/EditorData.cpp
    src/editor/EditorCommands.cpp
    src/search/WorkspaceSearch.cpp
    src/explorer/ExplorerOps.cpp
    src/explorer/DirectoryTree.cpp
)
target_include_directories(dodev_core PUBLIC "${PROJECT_SOURCE_DIR}/include")
dodev_warnings(dodev_core)

add_library(dodev_services STATIC
    src/settings/Json.cpp
    src/settings/SettingsManager.cpp
    src/keybindings/KeybindingManager.cpp
    src/templates/TemplateRegistry.cpp
    src/themes/ThemeManager.cpp
    src/plugins/PluginManager.cpp
    src/plugins/PluginPaths.cpp
    src/terminal/TerminalTasks.cpp
    src/docker/DockerTools.cpp
)
target_include_directories(dodev_services PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(dodev_services PUBLIC cxx_std_17)
if(UNIX)
    target_link_libraries(dodev_services PUBLIC ${CMAKE_DL_LIBS})
endif()
dodev_warnings(dodev_services)

add_library(dodev_terminal STATIC
    src/TerminalSession.cpp
    src/TerminalScreen.cpp
)
target_include_directories(dodev_terminal PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(dodev_terminal PUBLIC Threads::Threads)
if(WIN32)
    target_compile_definitions(dodev_terminal PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN)
elseif(DODEV_UTIL_LIBRARY)
    target_link_libraries(dodev_terminal PUBLIC "${DODEV_UTIL_LIBRARY}")
endif()
