 
cmake_minimum_required(VERSION 3.25.0)

set(PROJECT_NAME DoDevEditor)
project(${PROJECT_NAME} LANGUAGES CXX)
set(CMAKE_BUILD_TYPE Debug)

set(FETCH_LIBGIT_DEFINITIONS FALSE)
set(COMPILE_CLANG_DEFINITIONS FALSE)

option(ENABLE_CLANG_TIDY "Enable clang-tidy" FALSE)
option(ENABLE_COVERAGE "Enable code coverage" FALSE)
option(ENABLE_TESTING "Enable code Testing" FALSE)
option(ENABLE_THIRDPARTY "Allow Insta" FALSE)
option(FETCH_WXWIDGET "Allow Insta" FALSE)
option(FETCH_JSON "Allow JSON" FALSE)
option(USING_PLUGINS "Allow Insta" FALSE)
option(COMPILE_CLANG_DEFINITIONS, "Allow clang-dev for cpp" ${COMPILE_CLANG_DEFINITIONS})

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
        GIT_TAG        v3.0.0  # Use the latest stable version
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



### Try static Compilation only
#set(wxWidgets_USE_STATIC ON)


#find_package(wxWidgets REQUIRED COMPONENTS core base)

#if(wxWidgets_FOUND)
#    message(STATUS "wxWidgets encontrado: ${wxWidgets_INCLUDE_DIRS}")
#else()
#    message(FATAL_ERROR "wxWidgets no encontrado")
#endif()
### Try static Compilation only, Above



# Find the libgit2 package


if(COMPILE_CLANG_DEFINITIONS)
    set(CLANG_EXTERNAL_LIBS /usr/lib/llvm-18)
    message("Using clang 18")
    link_directories(${CLANG_EXTERNAL_LIBS}/lib)
    include_directories(${CLANG_EXTERNAL_LIBS}/include)
    #add_definitions(-DLLVM_ENABLE_ABI_BREAKING_CHECKS)
    #add_definitions(${LLVM_DEFINITIONS})
    # Find Clang installation
    # find_package(LLVM REQUIRED CONFIG)
    #if (LLVM_FOUND)
    #else()
    #   message("Clang not foundxx")
    #endif()
    set(CLANG_LIBRARIES
        clangTooling
        clangBasic
        clangAST
        clangASTMatchers
        clangDriver
        clangFrontend
        clangFrontendTool
        clangToolingCore
        clangRewrite
        clangRewriteFrontend
        clangLex
        clangParse
        clangAnalysis
    )


else()
    message("Clang not foundvy")
    set(CLANG_LIBRARIES  )

endif()




add_executable(${PROJECT_NAME} ${SRCS})

find_package(wxWidgets REQUIRED COMPONENTS core base stc aui) 

include(${wxWidgets_USE_FILE})

target_link_libraries(${PROJECT_NAME} ${wxWidgets_LIBRARIES} ${CLANG_LIBRARIES} tinyxml2 git2 )

# If needed, pass compiler/linker flags
target_compile_options(${PROJECT_NAME} PRIVATE ${LIBGIT2_CFLAGS_OTHER})
target_link_options(${PROJECT_NAME} PRIVATE ${LIBGIT2_LDFLAGS_OTHER})



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