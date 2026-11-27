# wxWidgets frontend. All wx-specific flags stay on its dependency target.
add_executable(DoDevEditor src/gui/wx/Main.cpp)
# Source files contain UTF-8 menu labels and symbols.
if(MSVC)
    target_compile_options(DoDevEditor PRIVATE /utf-8)
endif()
target_link_libraries(DoDevEditor PRIVATE
    dodev_wx_dependencies
    dodev_core
    dodev_services
    dodev_terminal
    dodev_launch
    dodev_symbols_core
    dodev_gdb_core
    dodev_ai_core
    dodev_serial_core
    dodev_journal_core
    Threads::Threads
)
dodev_runtime_directory(DoDevEditor "${PROJECT_BINARY_DIR}")
dodev_wx_runtime_path(DoDevEditor)

# GTK CSS is optional and only used with a GTK3 wxWidgets build.
set(DODEV_GTK_THEME_ACTIVE OFF)
if(DODEV_WX_GTK_THEME AND UNIX AND NOT APPLE AND wxWidgets_CONFIG_EXECUTABLE)
    execute_process(
        COMMAND "${wxWidgets_CONFIG_EXECUTABLE}" --selected-config
        OUTPUT_VARIABLE DODEV_WX_CONFIG
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    if(DODEV_WX_CONFIG MATCHES "gtk3")
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(DODEV_GTK3 QUIET IMPORTED_TARGET gtk+-3.0)
            if(TARGET PkgConfig::DODEV_GTK3)
                target_link_libraries(DoDevEditor PRIVATE PkgConfig::DODEV_GTK3)
                target_compile_definitions(DoDevEditor PRIVATE DODEV_GTK3_THEME=1)
                set(DODEV_GTK_THEME_ACTIVE ON)
            endif()
        endif()
    endif()
endif()

# Compile a standalone header check with the same wx/GTK definitions as the GUI.
# No display is required for this compile-only regression check.
if(DODEV_BUILD_TESTS)
    target_sources(DoDevEditor PRIVATE tests/compile_wx_native_theme.cpp)
endif()
