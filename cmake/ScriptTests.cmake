find_package(Python3 QUIET COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    add_test(NAME wx_dependency_fetch COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_wx_fetch.py" "${CMAKE_COMMAND}")
    set_tests_properties(wx_dependency_fetch PROPERTIES TIMEOUT 240)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_test(NAME wx_install_runtime_path COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_wx_runtime_path.py")
    endif()
    add_test(NAME wx_workspace_plugin_views COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_wx_workspace_plugin_views.py")
    add_test(NAME wx_raven_menu_parity COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_wx_menu_parity.py")
    add_test(NAME vscode_context_menus_and_output_buttons
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_context_menus.py")
    add_test(NAME workspace_sidebar_xml_contract COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_workspace_ui.py")
    if(UNIX AND NOT APPLE)
        add_executable(dodev_direct_launch_tests tests/test_direct_launch.cpp)
        target_link_libraries(dodev_direct_launch_tests PRIVATE dodev_launch)
        add_test(NAME detached_direct_executable COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_direct_launch.py" "$<TARGET_FILE:dodev_direct_launch_tests>")
        add_test(NAME detached_cli_and_open_with
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_launcher.py"
                "$<TARGET_FILE:dodeveditor>" "${CMAKE_CURRENT_SOURCE_DIR}/packaging/dodeveditor.desktop")
    endif()
    get_target_property(ai_http_enabled dodev_ai_core DODEV_AI_HTTP_AVAILABLE)
    if(ai_http_enabled)
        add_executable(dodev_ai_http_tests thirdparty/AIAssistant/tests/test_ai_http.cpp)
        target_link_libraries(dodev_ai_http_tests PRIVATE dodev_ai_core)
        add_test(NAME ai_local_http_transport COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/AIAssistant/tests/test_ai_http.py"
            $<TARGET_FILE:dodev_ai_http_tests>)
    endif()
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

if(NOT Python3_Interpreter_FOUND)
    message(STATUS "DoDevEditor: Python3 missing; script regression tests skipped")
endif()
