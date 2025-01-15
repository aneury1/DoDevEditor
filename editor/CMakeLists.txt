cmake_minimum_required(VERSION 3.25.0)

set(PROJECT_NAME DoDevEditor)
project(${PROJECT_NAME} LANGUAGES CXX)
set(CMAKE_BUILD_TYPE Debug)

option(ENABLE_CLANG_TIDY "Enable clang-tidy" FALSE)
option(ENABLE_COVERAGE "Enable code coverage" FALSE)
option(ENABLE_TESTING "Enable code Testing" FALSE)
option(ENABLE_THIRDPARTY "Allow Insta" FALSE)
option(FETCH_WXWIDGET "Allow Insta" TRUE)
option(FETCH_JSON "Allow JSON" FALSE)
option(USING_PLUGINS "Allow Insta" FALSE)

set (CMAKE_CXX_STANDARD 17)

if(ENABLE_CLANG_TIDY)
    set(CMAKE_CXX_CLANG_TIDY "clang-tidy;-checks=*")
endif()

if (ENABLE_COVERAGE)
    message(STATUS "Code Coverage enabled")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage -O0 -g")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(FETCH_WXWIDGET)
    message("Fetching wxWidget version v3.2.2")
    include(FetchContent)
    # Fetch wxWidgets
    FetchContent_Declare(
        wxWidgets
        GIT_REPOSITORY https://github.com/wxWidgets/wxWidgets.git
        GIT_TAG        v3.2.2  # Use the latest stable version
    )

    FetchContent_MakeAvailable(wxWidgets)
endif()

if(FETCH_JSON)
    message("Fetching nlohmann version v3.11.2")
    include(FetchContent)
   
    FetchContent_Declare(
        nlohmann
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        # GIT_TAG        v3.11.2  # Use the latest stable version
    )

    FetchContent_MakeAvailable(wxWidgets)
endif()


add_definitions(-D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS)

file(GLOB_RECURSE SRCS src/*.cpp)

include_directories(includes PUBLIC)

add_executable(${PROJECT_NAME} ${SRCS})

if(USING_PLUGINS)
    find_package(wxWidgets REQUIRED COMPONENTS core base stc aui DoDevEditorSDK) 
else()
    find_package(wxWidgets REQUIRED COMPONENTS core base stc aui  ) 
endif()

include(${wxWidgets_USE_FILE})

target_link_libraries(${PROJECT_NAME} ${wxWidgets_LIBRARIES})

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wwrite-strings")
   target_compile_options(${PROJECT_NAME} PRIVATE -Wwrite-strings)
endif()



list(
        APPEND IncludesDirs 
        PRIVATE includes
    )
    
if(USING_PLUGINS)
    list(APPEND IncludesDirs PRIVATE ${CMAKE_SOURCE_DIR}/plugins-sdk)
endif()

target_include_directories(${PROJECT_NAME} BEFORE ${IncludesDirs})

# set(PLUGIN_OUTPUT_DIR ${CMAKE_BINARY_DIR}/plugins)

# file(MAKE_DIRECTORY ${PLUGIN_OUTPUT_DIR})

if(ENABLE_TESTING)
    message("Testing is enabled")
    enable_testing()
    include(CTest)
    add_subdirectory(test)
endif()