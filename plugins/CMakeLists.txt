cmake_minimum_required(VERSION 3.20)

project(pluginManager)

message("Creating a dll")

add_library(${PROJECT_NAME} STATIC main.cpp)

# Define target name for Windows DLL export
if (WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE PLUGIN_EXPORTS)
endif()

# Optional: Set output directory for the built plugin
set_target_properties(${PROJECT_NAME} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins
)

# Optional: Set C++ standard
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)