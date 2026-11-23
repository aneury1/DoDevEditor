add_library(dodev_serial_core STATIC src/SerialMonitor.cpp)
target_include_directories(dodev_serial_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(dodev_serial_core PUBLIC cxx_std_17)
target_link_libraries(dodev_serial_core PUBLIC Threads::Threads)
set_target_properties(dodev_serial_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(WIN32)
    target_compile_definitions(dodev_serial_core PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

add_library(dodev_serial_monitor_plugin MODULE src/SerialMonitorPlugin.cpp)
target_include_directories(dodev_serial_monitor_plugin PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_compile_features(dodev_serial_monitor_plugin PRIVATE cxx_std_17)
set(DODEV_SERIAL_BUILD_DIR "${PROJECT_BINARY_DIR}/plugins/serial-monitor")
set(DODEV_SERIAL_INSTALL_DIR "${CMAKE_INSTALL_BINDIR}/plugins/serial-monitor")
set_target_properties(dodev_serial_monitor_plugin PROPERTIES
    OUTPUT_NAME "dodev_serial_monitor_plugin"
    LIBRARY_OUTPUT_DIRECTORY "${DODEV_SERIAL_BUILD_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${DODEV_SERIAL_BUILD_DIR}")
foreach(config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(dodev_serial_monitor_plugin PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY_${config} "${DODEV_SERIAL_BUILD_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_${config} "${DODEV_SERIAL_BUILD_DIR}")
endforeach()
file(MAKE_DIRECTORY "${DODEV_SERIAL_BUILD_DIR}")
configure_file(plugin.json "${DODEV_SERIAL_BUILD_DIR}/plugin.json" COPYONLY)
install(TARGETS dodev_serial_monitor_plugin
    LIBRARY DESTINATION "${DODEV_SERIAL_INSTALL_DIR}"
    RUNTIME DESTINATION "${DODEV_SERIAL_INSTALL_DIR}")
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/plugin.json" DESTINATION "${DODEV_SERIAL_INSTALL_DIR}")
if(UNIX AND NOT APPLE)
    add_executable(dodev-serial-reader-helper src/SerialReaderHelper.cpp)
    target_link_libraries(dodev-serial-reader-helper PRIVATE dodev_serial_core)
    set_target_properties(dodev-serial-reader-helper PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
    install(TARGETS dodev-serial-reader-helper RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
endif()
add_custom_command(TARGET dodev_serial_monitor_plugin POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E echo "[DoDevEditor][Serial][build] Module: $<TARGET_FILE:dodev_serial_monitor_plugin>"
    COMMAND "${CMAKE_COMMAND}" -E echo "[DoDevEditor][Serial][build] Manifest: ${DODEV_SERIAL_BUILD_DIR}/plugin.json"
    VERBATIM)
install(CODE "message(STATUS \"[DoDevEditor][Serial][install] Plugin directory: \${CMAKE_INSTALL_PREFIX}/${DODEV_SERIAL_INSTALL_DIR}\")")
message(STATUS "[DoDevEditor][Serial] Bundled native plugin and core (offline)")
