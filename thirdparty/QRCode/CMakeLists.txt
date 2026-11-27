# QR Code Generator bundled with DoDevEditor. No network dependency.
# Builds on every platform where the parent project builds.
add_library(dodev_qr_core STATIC src/QRMatrix.cpp src/QRPayload.cpp)
target_include_directories(dodev_qr_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(dodev_qr_core PUBLIC cxx_std_17)
set_target_properties(dodev_qr_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
find_package(ZLIB QUIET)
if(ZLIB_FOUND)
    target_link_libraries(dodev_qr_core PUBLIC ZLIB::ZLIB)
    target_compile_definitions(dodev_qr_core PRIVATE DODEV_QR_ZLIB=1)
else()
    message(STATUS "DoDev QR: zlib absent; binary bundles remain uncompressed")
endif()
find_package(OpenSSL QUIET COMPONENTS Crypto)
if(TARGET OpenSSL::Crypto)
    target_link_libraries(dodev_qr_core PUBLIC OpenSSL::Crypto)
    target_compile_definitions(dodev_qr_core PRIVATE DODEV_QR_CRYPTO=1)
else()
    message(STATUS "DoDev QR: OpenSSL absent; encrypted exports disabled")
endif()

add_library(dodev_qrcode_plugin MODULE src/QRCodePlugin.cpp)
target_compile_features(dodev_qrcode_plugin PRIVATE cxx_std_17)
target_link_libraries(dodev_qrcode_plugin PRIVATE dodev_qr_core)
target_include_directories(dodev_qrcode_plugin PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${PROJECT_SOURCE_DIR}/include")
if(WIN32)
    target_link_libraries(dodev_qrcode_plugin PRIVATE shell32)
endif()

set(DODEV_QR_BUILD_DIR "${PROJECT_BINARY_DIR}/plugins/qrcode")
set(DODEV_QR_INSTALL_DIR "${CMAKE_INSTALL_BINDIR}/plugins/qrcode")
set_target_properties(dodev_qrcode_plugin PROPERTIES
    OUTPUT_NAME "dodev_qrcode_plugin"
    LIBRARY_OUTPUT_DIRECTORY "${DODEV_QR_BUILD_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${DODEV_QR_BUILD_DIR}")
# Multi-config generators (Visual Studio / Ninja Multi-Config) must not
# put the binary in a Debug/Release subfolder: the manifest expects it here.
foreach(config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(dodev_qrcode_plugin PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY_${config} "${DODEV_QR_BUILD_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_${config} "${DODEV_QR_BUILD_DIR}")
endforeach()
file(MAKE_DIRECTORY "${DODEV_QR_BUILD_DIR}")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/plugin.json"
               "${DODEV_QR_BUILD_DIR}/plugin.json" COPYONLY)

if(MSVC)
    target_compile_options(dodev_qrcode_plugin PRIVATE /W4 /utf-8)
    target_compile_options(dodev_qr_core PRIVATE /W4)
else()
    target_compile_options(dodev_qrcode_plugin PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(dodev_qr_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_custom_command(TARGET dodev_qrcode_plugin POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E echo "[DoDevEditor][QR][build] Binary: $<TARGET_FILE:dodev_qrcode_plugin>"
    COMMAND "${CMAKE_COMMAND}" -E echo "[DoDevEditor][QR][build] Manifest: ${DODEV_QR_BUILD_DIR}/plugin.json"
    VERBATIM)

install(TARGETS dodev_qrcode_plugin
    LIBRARY DESTINATION "${DODEV_QR_INSTALL_DIR}"
    RUNTIME DESTINATION "${DODEV_QR_INSTALL_DIR}")
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/plugin.json"
    DESTINATION "${DODEV_QR_INSTALL_DIR}")
# This message appears at `cmake --install` / `make install` time, reflecting
# --prefix overrides instead of a hardcoded build-time prefix.
install(CODE "message(STATUS \"[DoDevEditor][QR][install] Plugin directory: \${CMAKE_INSTALL_PREFIX}/${DODEV_QR_INSTALL_DIR}\")")

message(STATUS "[DoDevEditor][QR] Bundled source: ${CMAKE_CURRENT_SOURCE_DIR}")
message(STATUS "[DoDevEditor][QR] Build output: ${DODEV_QR_BUILD_DIR}")
message(STATUS "[DoDevEditor][QR] Install relative to prefix: ${DODEV_QR_INSTALL_DIR}")
