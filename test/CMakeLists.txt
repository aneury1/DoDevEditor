
cmake_minimum_required(VERSION 3.28.3)
project(dodeveditor_test)

include_directories(${CMAKE_SOURCE_DIR}/includes)
include_directories(${CMAKE_SOURCE_DIR}/tests/)

message("include dirs: ${CMAKE_SOURCE_DIR}")

include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)

FetchContent_MakeAvailable(googletest)

add_library(GTest::GTest INTERFACE IMPORTED)

target_link_libraries(GTest::GTest INTERFACE gtest_main gmock_main)

add_executable(${PROJECT_NAME}
    test_main.cpp
    #variant_test.cpp
    #string_utils_test.cpp
    #io_utils_test.cpp
    #../src/test_window.cpp
)

add_compile_definitions(SIMPLE_SERVER_TESTING)

target_link_libraries(${PROJECT_NAME} PRIVATE
    GTest::GTest
)

add_test(NAME testapp_gtest
    COMMAND ${PROJECT_NAME})

include(GoogleTest)

gtest_discover_tests(${PROJECT_NAME})

if(SIMPLE_SERVER_COVERAGE)
    message("Compiling with coverage flags")
    # set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-arcs -ftest-coverage")
    # set(CMAKE_CXX_FLAGS " ${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
    # set(CMAKE_EXE_LINKER_FLAGS "-fprofile-arcs -ftest-coverage")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0")
    set(CMAKE_CXX_FLAGS " ${CMAKE_CXX_FLAGS} -O0")
    # append_coverage_compiler_flags()
    # setup_target_for_coverage_gcovr_html(NAME ${PROJECT_NAME}_cov
    #    BASE_DIRECTORY ../
    #    EXCLUDE "test/" "build/"
    #)
endif()
