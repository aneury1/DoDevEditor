cmake_minimum_required(VERSION 3.21)
project(DoDevEditor VERSION 3.5.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(DODEV_BUILD_GUI "Compilar la interfaz nativa Raven GUI" ON)
option(DODEV_BUILD_TESTS "Compilar pruebas del nucleo CSV/DLT/archivos" ON)
option(DODEV_BUILD_PLUGIN_EXAMPLES "Build bundled native plugin example" ON)
option(DODEV_BUILD_QRCODE_PLUGIN "Build and install the bundled QR Code plugin" ON)
include(GNUInstallDirs)
set(DODEV_RAVEN_REPOSITORY "https://github.com/aneury1/Raven.git" CACHE STRING "Repositorio Git de Raven GUI")
set(DODEV_RAVEN_GIT_TAG "main" CACHE STRING "Rama/tag/commit de Raven GUI")

add_library(dodev_core STATIC src/EditorData.cpp src/editor/EditorCommands.cpp)
add_library(dodev_services STATIC
    src/settings/Json.cpp
    src/settings/SettingsManager.cpp
    src/keybindings/KeybindingManager.cpp
    src/templates/TemplateRegistry.cpp
    src/themes/ThemeManager.cpp
    src/plugins/PluginManager.cpp
    src/plugins/PluginPaths.cpp
    src/terminal/TerminalTasks.cpp
)
target_include_directories(dodev_services PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(dodev_services PUBLIC cxx_std_17)
if(UNIX)
    target_link_libraries(dodev_services PUBLIC ${CMAKE_DL_LIBS})
endif()
if(MSVC)
    target_compile_options(dodev_services PRIVATE /W4 /utf-8)
else()
    target_compile_options(dodev_services PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(dodev_terminal STATIC src/TerminalSession.cpp src/TerminalScreen.cpp)
target_include_directories(dodev_terminal PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
if(WIN32)
    target_compile_definitions(dodev_terminal PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN)
elseif(UNIX)
    find_library(DODEV_UTIL_LIBRARY NAMES util)
    if(DODEV_UTIL_LIBRARY)
        target_link_libraries(dodev_terminal PUBLIC "${DODEV_UTIL_LIBRARY}")
    endif()
endif()
find_package(Threads REQUIRED)
target_link_libraries(dodev_terminal PUBLIC Threads::Threads)

target_include_directories(dodev_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
if(MSVC)
    target_compile_options(dodev_core PRIVATE /W4 /utf-8)
else()
    target_compile_options(dodev_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(DODEV_BUILD_GUI)
    include(FetchContent)
    # Mostrar realmente las lineas de progreso de 'git clone', no ocultarlas con FetchContent.
    set(FETCHCONTENT_QUIET OFF CACHE BOOL "Mostrar progreso de dependencias" FORCE)
    set(FETCHCONTENT_UPDATES_DISCONNECTED OFF)
    set(RAVEN_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(RAVEN_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(RAVEN_BUILD_TOOLS OFF CACHE BOOL "" FORCE)

    message(STATUS "[DoDevEditor] Descargando Raven GUI desde: ${DODEV_RAVEN_REPOSITORY}")
    message(STATUS "[DoDevEditor] Revision: ${DODEV_RAVEN_GIT_TAG}")
    message(STATUS "[DoDevEditor] Git clone / checkout: progreso habilitado (GIT_PROGRESS=TRUE)")
    FetchContent_Declare(raven
        GIT_REPOSITORY "${DODEV_RAVEN_REPOSITORY}"
        GIT_TAG "${DODEV_RAVEN_GIT_TAG}"
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
        GIT_SUBMODULES_RECURSE TRUE
        USES_TERMINAL_DOWNLOAD TRUE
    )
    FetchContent_MakeAvailable(raven)
    if(NOT TARGET raven::raven)
        if(TARGET raven)
            add_library(raven::raven ALIAS raven)
        else()
            message(FATAL_ERROR "No se encontro target raven::raven tras el FetchContent")
        endif()
    endif()

    add_executable(DoDevEditor
        src/main.cpp
        src/EditorApp.cpp
        src/EditorSettings.cpp
        src/EditorMenus.cpp
        src/EditorKeybindings.cpp
        src/EditorSerial.cpp
        src/EditorSymbols.cpp
        src/EditorJournal.cpp
        resources/EmbeddedJournalUi.h
        resources/EmbeddedSerialUi.h
        resources/EmbeddedPluginPromptUi.h
        resources/EmbeddedNavigationUi.h
        resources/EmbeddedSettingsUi.h
        include/EditorApp.h
        resources/EmbeddedUi.h
    )
    target_include_directories(DoDevEditor PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/resources")
    target_link_libraries(DoDevEditor PRIVATE dodev_core dodev_services dodev_terminal raven::raven)
    target_link_libraries(DoDevEditor PRIVATE dodev_serial_core dodev_symbols_core dodev_journal_core)
    if(MSVC)
        target_compile_options(DoDevEditor PRIVATE /W4 /utf-8)
    else()
        target_compile_options(DoDevEditor PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    install(TARGETS DoDevEditor RUNTIME DESTINATION bin)
    message(STATUS "[DoDevEditor] GUI Raven 3.3 + 13 themes + JSON Settings + native plugin tools")
endif()

if(DODEV_BUILD_QRCODE_PLUGIN)
    add_subdirectory(thirdparty/QRCode)
endif()

add_subdirectory(thirdparty/SerialMonitor)
add_subdirectory(thirdparty/SourceSymbols)
add_subdirectory(thirdparty/RemoteJournal)

if(DODEV_BUILD_PLUGIN_EXAMPLES)
    add_library(hello_plugin MODULE plugins/examples/HelloPlugin/HelloPlugin.cpp)
    target_include_directories(hello_plugin PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
    set_target_properties(hello_plugin PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/plugin-examples/hello"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/plugin-examples/hello"
    )
    foreach(extension IN ITEMS ClangFormat Docker)
        string(TOLOWER "${extension}" extension_lower)
        add_library(${extension_lower}_plugin MODULE
            "plugins/examples/${extension}/${extension}Plugin.cpp")
        target_include_directories(${extension_lower}_plugin PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
        set_target_properties(${extension_lower}_plugin PROPERTIES
            LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/plugin-examples/${extension_lower}"
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/plugin-examples/${extension_lower}")
        configure_file("${CMAKE_CURRENT_SOURCE_DIR}/plugins/examples/${extension}/plugin.json"
                       "${CMAKE_CURRENT_BINARY_DIR}/plugin-examples/${extension_lower}/plugin.json" COPYONLY)
    endforeach()
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/plugins/examples/HelloPlugin/plugin.json"
        "${CMAKE_CURRENT_BINARY_DIR}/plugin-examples/hello/plugin.json" COPYONLY)
endif()

if(DODEV_BUILD_TESTS)
    enable_testing()
    if(DODEV_BUILD_QRCODE_PLUGIN)
        add_subdirectory(thirdparty/QRCode/tests)
    endif()
    add_executable(dodev_symbols_tests thirdparty/SourceSymbols/tests/test_symbols.cpp)
    target_link_libraries(dodev_symbols_tests PRIVATE dodev_symbols_core)
    add_test(NAME source_symbols_outline_csv COMMAND dodev_symbols_tests)
    add_executable(dodev_journal_tests thirdparty/RemoteJournal/tests/test_journal.cpp)
    target_link_libraries(dodev_journal_tests PRIVATE dodev_journal_core)
    add_test(NAME remote_journal_ssh_filter COMMAND dodev_journal_tests)
    add_executable(dodev_new_plugins_tests tests/test_new_plugins.cpp)
    target_link_libraries(dodev_new_plugins_tests PRIVATE dodev_services)
    target_compile_definitions(dodev_new_plugins_tests PRIVATE DODEV_NEW_PLUGIN_ROOT="${PROJECT_BINARY_DIR}/plugins")
    add_dependencies(dodev_new_plugins_tests dodev_source_symbols_plugin dodev_remote_journal_plugin)
    add_test(NAME new_symbol_and_remote_journal_plugins COMMAND dodev_new_plugins_tests)
    add_executable(dodev_serial_tests thirdparty/SerialMonitor/tests/test_serial.cpp)
        target_link_libraries(dodev_serial_tests PRIVATE dodev_serial_core)
        if(UNIX AND NOT APPLE)
            find_library(DODEV_SERIAL_UTIL NAMES util)
            if(DODEV_SERIAL_UTIL)
                target_link_libraries(dodev_serial_tests PRIVATE ${DODEV_SERIAL_UTIL})
            endif()
        endif()
        add_test(NAME serial_monitor_filter_and_pty COMMAND dodev_serial_tests)
        add_executable(dodev_serial_plugin_tests thirdparty/SerialMonitor/tests/test_plugin.cpp)
        target_link_libraries(dodev_serial_plugin_tests PRIVATE dodev_services)
        target_compile_definitions(dodev_serial_plugin_tests PRIVATE
            DODEV_SERIAL_PLUGIN_DIR="${PROJECT_BINARY_DIR}/plugins/serial-monitor")
        add_dependencies(dodev_serial_plugin_tests dodev_serial_monitor_plugin)
        add_test(NAME serial_plugin_registration COMMAND dodev_serial_plugin_tests)
    add_executable(dodev_keybindings_tests tests/test_keybindings.cpp)
    target_link_libraries(dodev_keybindings_tests PRIVATE dodev_services)
    add_test(NAME vscode_keybinding_settings COMMAND dodev_keybindings_tests)
    add_executable(dodev_settings_tests tests/test_settings.cpp)
    target_link_libraries(dodev_settings_tests PRIVATE dodev_services)
    target_include_directories(dodev_settings_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/resources")
    add_test(NAME settings_themes_templates_json COMMAND dodev_settings_tests)
    if(DODEV_BUILD_PLUGIN_EXAMPLES)
        add_executable(dodev_plugin_tests tests/test_plugin.cpp)
        target_link_libraries(dodev_plugin_tests PRIVATE dodev_services)
        target_compile_definitions(dodev_plugin_tests PRIVATE DODEV_TEST_PLUGIN_DIR="${CMAKE_CURRENT_BINARY_DIR}/plugin-examples")
        add_dependencies(dodev_plugin_tests hello_plugin)
        add_test(NAME native_plugin_integration COMMAND dodev_plugin_tests)
    endif()
    if(DODEV_BUILD_PLUGIN_EXAMPLES)
        add_executable(dodev_tool_plugins_tests tests/test_tool_plugins.cpp)
        target_link_libraries(dodev_tool_plugins_tests PRIVATE dodev_services)
        target_compile_definitions(dodev_tool_plugins_tests PRIVATE DODEV_TEST_PLUGIN_DIR="${CMAKE_CURRENT_BINARY_DIR}/plugin-examples")
        add_dependencies(dodev_tool_plugins_tests clangformat_plugin docker_plugin)
        add_test(NAME clangformat_docker_plugin_integration COMMAND dodev_tool_plugins_tests)
    endif()
    add_executable(dodev_terminal_tasks_tests tests/test_terminal_tasks.cpp)
    target_link_libraries(dodev_terminal_tasks_tests PRIVATE dodev_services)
    add_test(NAME vscode_terminal_tasks COMMAND dodev_terminal_tasks_tests)
    add_executable(dodev_editor_commands_tests tests/test_editor_commands.cpp)
    target_link_libraries(dodev_editor_commands_tests PRIVATE dodev_core)
    add_test(NAME vscode_edit_selection_commands COMMAND dodev_editor_commands_tests)
    add_executable(dodev_core_tests tests/test_core.cpp)
    add_executable(dodev_terminal_tests tests/test_terminal.cpp)
    target_link_libraries(dodev_terminal_tests PRIVATE dodev_terminal)
    add_test(NAME terminal_pty_interactive COMMAND dodev_terminal_tests)
    target_link_libraries(dodev_core_tests PRIVATE dodev_core)
    add_test(NAME core_csv_dlt_files COMMAND dodev_core_tests)
    find_package(Python3 QUIET COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        add_test(NAME journal_symbols_terminal_ui_contract
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_new_ui.py")
        add_test(NAME embedded_serial_ui_contract
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_serial_ui.py")
        add_test(NAME embedded_plugin_prompt_xml
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_plugin_prompt_ui.py")
        add_test(NAME vscode_keybinding_ui_contract
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_keybinding_ui.py")
        add_test(NAME vscode_menus_xml_contract
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_vscode_menus.py")
        add_test(NAME embedded_music_theme_xml
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_embedded_ui.py")
    endif()
endif()
