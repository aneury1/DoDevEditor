# Core and plugin C++ regression tests. Script/GUI tests are isolated below.
add_executable(dodev_syntax_tests tests/test_syntax_rules.cpp)
target_link_libraries(dodev_syntax_tests PRIVATE dodev_services)
add_test(NAME dodev_syntax_tests COMMAND dodev_syntax_tests)

add_executable(dodev_docker_tests tests/test_docker_tools.cpp)
target_link_libraries(dodev_docker_tests PRIVATE dodev_services dodev_terminal)
add_test(NAME docker_tools COMMAND dodev_docker_tests)

add_executable(dodev_gdb_tests thirdparty/GdbDebugger/tests/test_gdb.cpp)
target_link_libraries(dodev_gdb_tests PRIVATE dodev_gdb_core dodev_services dodev_terminal)
target_compile_definitions(dodev_gdb_tests PRIVATE DODEV_GDB_TEST_ROOT="${PROJECT_BINARY_DIR}/plugins")
add_dependencies(dodev_gdb_tests dodev_gdb_plugin)
add_test(NAME gdb_configuration_and_native_plugin COMMAND dodev_gdb_tests)

add_executable(dodev_launch_profiles_tests tests/test_launch_profiles.cpp)
target_link_libraries(dodev_launch_profiles_tests PRIVATE dodev_gdb_core)
add_test(NAME vscode_launch_profiles COMMAND dodev_launch_profiles_tests)

add_executable(dodev_wx_explorer_tests tests/test_wx_explorer.cpp)
target_include_directories(dodev_wx_explorer_tests PRIVATE src/gui/wx)
add_test(NAME wx_explorer_filesystem COMMAND dodev_wx_explorer_tests)

add_executable(dodev_tree_tests tests/test_directory_tree.cpp)
target_link_libraries(dodev_tree_tests PRIVATE dodev_core)
add_test(NAME directory_tree COMMAND dodev_tree_tests)

add_executable(dodev_explorer_tests tests/test_explorer_ops.cpp)
target_link_libraries(dodev_explorer_tests PRIVATE dodev_core)
add_test(NAME explorer_context_filesystem COMMAND dodev_explorer_tests)

add_executable(dodev_workspace_tests tests/test_workspace_search.cpp)
target_link_libraries(dodev_workspace_tests PRIVATE dodev_core)
add_test(NAME workspace_search_replace_core COMMAND dodev_workspace_tests)

add_executable(dodev_launch_tests tests/test_launch_args.cpp)
target_link_libraries(dodev_launch_tests PRIVATE dodev_launch)
add_test(NAME launcher_paths_and_flags COMMAND dodev_launch_tests)

if(DODEV_BUILD_QRCODE_PLUGIN)
    add_subdirectory(thirdparty/QRCode/tests)
endif()
add_executable(dodev_ai_plugin_tests tests/test_ai_plugin.cpp)
target_link_libraries(dodev_ai_plugin_tests PRIVATE dodev_services)
target_compile_definitions(dodev_ai_plugin_tests PRIVATE DODEV_AI_TEST_ROOT="${PROJECT_BINARY_DIR}/plugins")
add_dependencies(dodev_ai_plugin_tests dodev_ai_assistant_plugin)
add_test(NAME ai_assistant_native_plugin_registration COMMAND dodev_ai_plugin_tests)

add_executable(dodev_ai_tests thirdparty/AIAssistant/tests/test_ai.cpp)
target_link_libraries(dodev_ai_tests PRIVATE dodev_ai_core)
add_test(NAME ai_chat_provider_json COMMAND dodev_ai_tests)

add_executable(dodev_symbols_tests thirdparty/SourceSymbols/tests/test_symbols.cpp)
target_link_libraries(dodev_symbols_tests PRIVATE dodev_symbols_core)
add_test(NAME source_symbols_outline_csv COMMAND dodev_symbols_tests)

add_executable(dodev_journal_tests thirdparty/RemoteJournal/tests/test_journal.cpp)
target_link_libraries(dodev_journal_tests PRIVATE dodev_journal_core)
add_test(NAME remote_journal_ssh_filter COMMAND dodev_journal_tests)

add_executable(dodev_journal_table_tests tests/test_journal_table.cpp)
target_link_libraries(dodev_journal_table_tests PRIVATE dodev_journal_core)
add_test(NAME remote_journal_structured_table COMMAND dodev_journal_table_tests)

# Journal export tests use an actual SQLite database, when available.
find_package(SQLite3 QUIET)
if(SQLite3_FOUND)
    add_executable(dodev_journal_export_tests tests/test_journal_export.cpp)
    target_include_directories(dodev_journal_export_tests PRIVATE "${PROJECT_SOURCE_DIR}/thirdparty/RemoteJournal/src/wx")
    target_link_libraries(dodev_journal_export_tests PRIVATE dodev_journal_core SQLite::SQLite3)
    add_test(NAME remote_journal_sqlite_export COMMAND dodev_journal_export_tests)
endif()

add_executable(dodev_new_plugins_tests tests/test_new_plugins.cpp)
target_link_libraries(dodev_new_plugins_tests PRIVATE dodev_services)
target_compile_definitions(dodev_new_plugins_tests PRIVATE DODEV_NEW_PLUGIN_ROOT="${PROJECT_BINARY_DIR}/plugins")
add_dependencies(dodev_new_plugins_tests dodev_source_symbols_plugin dodev_remote_journal_plugin)
add_test(NAME new_symbol_and_remote_journal_plugins COMMAND dodev_new_plugins_tests)

add_executable(dodev_serial_tests thirdparty/SerialMonitor/tests/test_serial.cpp)
target_link_libraries(dodev_serial_tests PRIVATE dodev_serial_core)
if(UNIX AND NOT APPLE)
    if(DODEV_UTIL_LIBRARY)
        target_link_libraries(dodev_serial_tests PRIVATE ${DODEV_UTIL_LIBRARY})
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

add_executable(dodev_task_variables_tests tests/test_task_variables.cpp)
target_link_libraries(dodev_task_variables_tests PRIVATE dodev_services)
add_test(NAME vscode_task_variables COMMAND dodev_task_variables_tests)

add_executable(dodev_task_catalog_tests tests/test_task_catalog.cpp)
target_link_libraries(dodev_task_catalog_tests PRIVATE dodev_services)
add_test(NAME workspace_task_catalog COMMAND dodev_task_catalog_tests)

add_executable(dodev_editor_commands_tests tests/test_editor_commands.cpp)
target_link_libraries(dodev_editor_commands_tests PRIVATE dodev_core)
add_test(NAME vscode_edit_selection_commands COMMAND dodev_editor_commands_tests)

add_executable(dodev_core_tests tests/test_core.cpp)
add_executable(dodev_terminal_tests tests/test_terminal.cpp)
target_link_libraries(dodev_terminal_tests PRIVATE dodev_terminal)
add_test(NAME terminal_pty_interactive COMMAND dodev_terminal_tests)
target_link_libraries(dodev_core_tests PRIVATE dodev_core)
add_test(NAME core_csv_dlt_files COMMAND dodev_core_tests)

include("${CMAKE_CURRENT_LIST_DIR}/ScriptTests.cmake")
if(DODEV_BUILD_GUI AND DODEV_WX_SYNTAX_GUI_TEST)
    include("${CMAKE_CURRENT_LIST_DIR}/WxGuiTests.cmake")
endif()

# Do not silently disable assert-based tests in optimized builds.
get_property(test_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
foreach(test_target IN LISTS test_targets)
    if(test_target MATCHES "^dodev_.*_tests$")
        dodev_test_assertions(${test_target})
    endif()
endforeach()

if(TARGET simplemodule_plugin)
    add_executable(dodev_simple_module_tests tests/test_simple_module.cpp)
    target_link_libraries(dodev_simple_module_tests PRIVATE dodev_services)
    target_compile_definitions(dodev_simple_module_tests PRIVATE SIMPLE_MODULE_DIR="${PROJECT_BINARY_DIR}/plugins/simple-module")
    add_dependencies(dodev_simple_module_tests simplemodule_plugin)
    add_test(NAME simple_module_sdk COMMAND dodev_simple_module_tests)
endif()

# Generic workspace profile and dynamic Git output integration (5.2.4).
add_executable(dodev_workspace_plugin_extensions_tests tests/test_workspace_plugin_extensions.cpp)
target_link_libraries(dodev_workspace_plugin_extensions_tests PRIVATE dodev_services dodev_journal_core dodev_symbols_core dodev_git_history_core)
target_compile_definitions(dodev_workspace_plugin_extensions_tests PRIVATE DODEV_PLUGIN_ROOT="${PROJECT_BINARY_DIR}/plugins")
add_dependencies(dodev_workspace_plugin_extensions_tests dodev_serial_monitor_plugin dodev_remote_journal_plugin dodev_source_symbols_plugin dodev_git_history_plugin)
dodev_test_assertions(dodev_workspace_plugin_extensions_tests)
add_test(NAME workspace_plugin_extensions COMMAND dodev_workspace_plugin_extensions_tests)
