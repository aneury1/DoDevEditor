cmake_minimum_required(VERSION 3.20)

project(JournalLogDiff
    VERSION 1.0.0
    DESCRIPTION "Side-by-side journal log comparison tool"
    LANGUAGES CXX
)

option(JLD_BUILD_TESTS "Build the DiffEngine unit tests" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(wxWidgets REQUIRED COMPONENTS core base stc)
include(${wxWidgets_USE_FILE})

add_library(jld_diff STATIC
    src/DiffEngine.cpp
    src/LogNormalizer.cpp
)

target_include_directories(jld_diff PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_compile_features(jld_diff PUBLIC cxx_std_17)

add_executable(journal-log-diff WIN32
    src/main.cpp
    src/MainFrame.cpp
)

target_include_directories(journal-log-diff PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(journal-log-diff PRIVATE jld_diff ${wxWidgets_LIBRARIES})

if(MSVC)
    target_compile_options(jld_diff PRIVATE /W4 /permissive-)
    target_compile_options(journal-log-diff PRIVATE /W4 /permissive-)
else()
    target_compile_options(jld_diff PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(journal-log-diff PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(GNUInstallDirs)
install(TARGETS journal-log-diff RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(JLD_BUILD_TESTS)
    enable_testing()
    add_executable(diff-engine-tests tests/DiffEngineTests.cpp)
    target_link_libraries(diff-engine-tests PRIVATE jld_diff)
    add_test(NAME diff-engine-tests COMMAND diff-engine-tests)
endif()
