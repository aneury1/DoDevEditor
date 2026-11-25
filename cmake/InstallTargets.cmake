# Preserve the existing bin/plugins layout used by the native plugin host.
if(DODEV_BUILD_GUI)
    install(TARGETS DoDevEditor dodeveditor
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
    if(UNIX AND NOT APPLE)
        configure_file("${PROJECT_SOURCE_DIR}/packaging/install_desktop.cmake.in"
            "${PROJECT_BINARY_DIR}/dodev_install_desktop.cmake" @ONLY)
        install(SCRIPT "${PROJECT_BINARY_DIR}/dodev_install_desktop.cmake")
    endif()
endif()
