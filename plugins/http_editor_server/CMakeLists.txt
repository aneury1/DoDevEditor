cmake_minimum_required(VERSION 3.20)
project(DoDevEditorHttpEditorServerPlugin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT DODEV_SDK_ROOT)
    get_filename_component(DODEV_SDK_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
endif()

find_package(Threads REQUIRED)

add_library(dodev_http_editor_server SHARED HttpEditorServerPlugin.cpp)
target_include_directories(dodev_http_editor_server PRIVATE "${DODEV_SDK_ROOT}/includes")
target_compile_definitions(dodev_http_editor_server PRIVATE DODEV_PLUGIN_BUILD=1)
target_link_libraries(dodev_http_editor_server PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
set_target_properties(dodev_http_editor_server PROPERTIES PREFIX "")

if(WIN32)
    target_link_libraries(dodev_http_editor_server PRIVATE ws2_32)
endif()

if(MSVC)
    target_compile_options(dodev_http_editor_server PRIVATE /W4)
else()
    target_compile_options(dodev_http_editor_server PRIVATE -Wall -Wextra -Wpedantic)
endif()
