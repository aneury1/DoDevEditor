add_executable(qrcode_encoder_tests encoder_tests.cpp)
target_link_libraries(qrcode_encoder_tests PRIVATE dodev_qr_core)
add_test(NAME bundled_qrcode_encoder_validation COMMAND qrcode_encoder_tests)

add_executable(qrcode_cli_test qr_cli.cpp)
target_link_libraries(qrcode_cli_test PRIVATE dodev_qr_core)
add_test(NAME bundled_qrcode_cli COMMAND qrcode_cli_test
         "${CMAKE_CURRENT_SOURCE_DIR}/demo.txt" "${CMAKE_CURRENT_BINARY_DIR}/sample.png" M)

if(UNIX AND NOT APPLE)
    add_executable(qrcode_plugin_smoke plugin_smoke.cpp)
    target_include_directories(qrcode_plugin_smoke PRIVATE "${PROJECT_SOURCE_DIR}/include")
    target_link_libraries(qrcode_plugin_smoke PRIVATE ${CMAKE_DL_LIBS})
    add_dependencies(qrcode_plugin_smoke dodev_qrcode_plugin)
    add_test(NAME bundled_qrcode_plugin_smoke COMMAND qrcode_plugin_smoke
             "$<TARGET_FILE:dodev_qrcode_plugin>")
endif()

add_executable(dodev_bundled_plugin_discovery_tests "${PROJECT_SOURCE_DIR}/tests/test_bundled_plugins.cpp")
target_link_libraries(dodev_bundled_plugin_discovery_tests PRIVATE dodev_services)
add_dependencies(dodev_bundled_plugin_discovery_tests dodev_qrcode_plugin)
target_compile_definitions(dodev_bundled_plugin_discovery_tests PRIVATE
    DODEV_QR_BUILD_ROOT="${PROJECT_BINARY_DIR}/plugins")
add_test(NAME bundled_qrcode_discovery COMMAND dodev_bundled_plugin_discovery_tests)
