# Each bundled plugin owns its own CMakeLists.txt and installation rules.
if(DODEV_BUILD_QRCODE_PLUGIN)
    add_subdirectory(thirdparty/QRCode)
    dodev_plugin_filename(dodev_qrcode_plugin)
    dodev_runtime_directory(dodev_qrcode_plugin "${PROJECT_BINARY_DIR}/plugins/qrcode")
endif()

add_subdirectory(thirdparty/SerialMonitor)
add_subdirectory(thirdparty/SourceSymbols)
add_subdirectory(thirdparty/RemoteJournal)
add_subdirectory(thirdparty/AIAssistant)
add_subdirectory(thirdparty/GdbDebugger)
add_subdirectory(thirdparty/GitHistory)
if(DODEV_BUILD_HTTP_CLIENT_PLUGIN)
    add_subdirectory(thirdparty/HttpClient)
endif()

# Keep manifest paths stable for custom multi-config configurations as well.
function(dodev_bundled_layout target directory)
    dodev_plugin_filename(${target})
    dodev_runtime_directory(${target} "${PROJECT_BINARY_DIR}/plugins/${directory}")
endfunction()
dodev_bundled_layout(dodev_serial_monitor_plugin serial-monitor)
dodev_bundled_layout(dodev_source_symbols_plugin source-symbols)
dodev_bundled_layout(dodev_remote_journal_plugin remote-journal)
dodev_bundled_layout(dodev_ai_assistant_plugin ai-assistant)
dodev_bundled_layout(dodev_gdb_plugin gdb-debugger)
if(TARGET dodev-serial-reader-helper)
    dodev_runtime_directory(dodev-serial-reader-helper "${PROJECT_BINARY_DIR}")
endif()

# Example SDK plugins. Docker is also installed as a bundled plugin.
if(DODEV_BUILD_PLUGIN_EXAMPLES)
    foreach(extension IN ITEMS HelloPlugin ClangFormat Docker)
        if(extension STREQUAL "HelloPlugin")
            set(name hello)
            set(source HelloPlugin.cpp)
        else()
            string(TOLOWER "${extension}" name)
            set(source "${extension}Plugin.cpp")
        endif()
        set(target "${name}_plugin")
        set(source_dir "${PROJECT_SOURCE_DIR}/plugins/examples/${extension}")
        set(output_dir "${PROJECT_BINARY_DIR}/plugin-examples/${name}")
        add_library(${target} MODULE "${source_dir}/${source}")
        target_include_directories(${target} PRIVATE "${PROJECT_SOURCE_DIR}/include")
        dodev_runtime_directory(${target} "${output_dir}")
        dodev_plugin_filename(${target})
        configure_file("${source_dir}/plugin.json" "${output_dir}/plugin.json" COPYONLY)
    endforeach()

    add_subdirectory(plugins/examples/SimpleModule)

    set(docker_output "${PROJECT_BINARY_DIR}/plugins/docker")
    set(docker_install "${CMAKE_INSTALL_BINDIR}/plugins/docker")
    set(docker_manifest "${PROJECT_SOURCE_DIR}/plugins/examples/Docker/plugin.json")
    # Configure the manifest independently of relinking the binary.
    configure_file("${docker_manifest}" "${docker_output}/plugin.json" COPYONLY)
    add_custom_command(TARGET docker_plugin POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${docker_output}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:docker_plugin>" "${docker_output}/$<TARGET_FILE_NAME:docker_plugin>"
        VERBATIM)
    install(TARGETS docker_plugin
        LIBRARY DESTINATION "${docker_install}"
        RUNTIME DESTINATION "${docker_install}")
    install(FILES "${docker_manifest}" DESTINATION "${docker_install}")
endif()
