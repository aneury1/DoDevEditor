cmake_minimum_required(VERSION 3.20)
project(DoDevEditorExamplePlugin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT DODEV_SDK_ROOT)
    get_filename_component(DODEV_SDK_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
endif()

add_library(dodev_example_plugin SHARED ExamplePlugin.cpp)
target_include_directories(dodev_example_plugin PRIVATE "${DODEV_SDK_ROOT}/includes")
target_compile_definitions(dodev_example_plugin PRIVATE DODEV_PLUGIN_BUILD=1)
set_target_properties(dodev_example_plugin PROPERTIES PREFIX "")

if(MSVC)
    target_compile_options(dodev_example_plugin PRIVATE /W4)
else()
    target_compile_options(dodev_example_plugin PRIVATE -Wall -Wextra -Wpedantic)
endif()
