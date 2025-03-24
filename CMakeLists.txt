 
cmake_minimum_required(VERSION 3.20.0)

set(PROJECT_NAME DoDevEditor)
project(${PROJECT_NAME} LANGUAGES CXX)
# set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")
set(CMAKE_WIN32_MINGW FALSE)
set (CMAKE_CXX_STANDARD 17)
set(FETCH_LIBGIT_DEFINITIONS TRUE)
set(COMPILE_CLANG_DEFINITIONS FALSE)
set(FETCH_TREE_SITTER FALSE)
set(INCLUDE_TREE_SITTER FALSE)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


include(FetchContent)

option(ENABLE_CLANG_TIDY "Enable clang-tidy" FALSE)
option(ENABLE_COVERAGE "Enable code coverage" FALSE)
option(ENABLE_TESTING "Enable code Testing" FALSE)
option(ENABLE_THIRDPARTY "Allow Insta" FALSE)
option(FETCH_WXWIDGET "Allow Insta" FALSE)
option(FETCH_JSON "Allow JSON" FALSE)
option(USING_PLUGINS "Allow Insta" FALSE)
option(COMPILE_CLANG_DEFINITIONS, "Allow clang-dev for cpp" ${COMPILE_CLANG_DEFINITIONS})



if(CMAKE_WIN32_MINGW)
    # Include the toolchain setup file
    include(cmake/mingw32.cmake)
    # Apply the toolchain settings
    setup_mingw_toolchain()
endif()


if(ENABLE_CLANG_TIDY)
    set(CMAKE_CXX_CLANG_TIDY "clang-tidy;-checks=*")
endif()

if (ENABLE_COVERAGE)
    message(STATUS "Code Coverage enabled")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage -O0 -g")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
endif()


if(FETCH_TREE_SITTER)
    include(FetchContent)

    # Fetch Tree-sitter Core
    FetchContent_Declare(
        tree_sitter
        GIT_REPOSITORY https://github.com/tree-sitter/tree-sitter.git
        GIT_TAG master
    )
    FetchContent_MakeAvailable(tree_sitter)

    # Fetch Tree-sitter C++ Grammar
    FetchContent_Declare(
        tree_sitter_cpp
        GIT_REPOSITORY https://github.com/tree-sitter/tree-sitter-cpp.git
        GIT_TAG master
    )
    FetchContent_MakeAvailable(tree_sitter_cpp)
    add_compile_definitions(USE_TREE_SITTER=1) 
   
    set(TREE_SITTER_LIBRARY tree_sitter tree_sitter_cpp)
elseif(INCLUDE_TREE_SITTER)
   include(TreeSitter)
   add_compile_definitions(USE_TREE_SITTER=1) 
   set(TREE_SITTER_LIBRARY tree_sitter tree_sitter_cpp)

else()
    set(TREE_SITTER_LIBRARY tree_sitter tree_sitter_cpp)
        
    message("No three sitter setup")
endif()

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


if(CMAKE_WIN32_MINGW)
    include(FetchContent)
    FetchContent_Declare(
        libgit2
        GIT_REPOSITORY https://github.com/libgit2/libgit2.git
        GIT_TAG v1.7.1  # Change this to the latest version
    )
    # Fetch and add libgit2
    FetchContent_MakeAvailable(libgit2)
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

if(CMAKE_WIN32_MINGW)
    add_compile_definitions(__lib_git_2_defined)
    # Include the toolchain setup file
    include(cmake/mingw32.cmake)
    # Apply the toolchain settings
    setup_mingw_toolchain()
endif()

find_package(Python3 COMPONENTS Development REQUIRED)

add_executable(${PROJECT_NAME} ${SRCS})
#cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/mingw-toolchain.cmake  ~/Documents/projects/myprojects/DoDevEditor/

target_include_directories(${PROJECT_NAME}  PRIVATE ${Python3_INCLUDE_DIRS})
 
message(STATUS "Python include dirs: ${Python3_INCLUDE_DIRS}")

message(STATUS "Python library: ${Python3_LIBRARIES}")

find_package(wxWidgets REQUIRED COMPONENTS core base stc aui) 

include(${wxWidgets_USE_FILE})

find_package(wxWidgets REQUIRED COMPONENTS core base  stc aui)

include(${wxWidgets_USE_FILE})
 


target_link_libraries(${PROJECT_NAME}
    ${wxWidgets_LIBRARIES} 
    ${TREE_SITTER_LIBRARY} 
    git2 
    ${Python3_LIBRARIES}
    dlt 
    )

# tinyxml2 git2 )

# If needed, pass compiler/linker flags

target_compile_options(${PROJECT_NAME} PRIVATE ${LIBGIT2_CFLAGS_OTHER})

target_link_options(${PROJECT_NAME} PRIVATE ${LIBGIT2_LDFLAGS_OTHER})

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wwrite-strings")
   target_compile_options(${PROJECT_NAME} PRIVATE -Wwrite-strings)
endif()

list(APPEND IncludesDirs PRIVATE includes)
    

target_include_directories(${PROJECT_NAME} BEFORE ${IncludesDirs} )#${TREE_SITTER_INCLUDE_DIRS})

if(ENABLE_TESTING)
    message("Testing is enabled")
    enable_testing()
    include(CTest)
    add_subdirectory(test)
endif()

include(CPack)

install(TARGETS ${PROJECT_NAME} DESTINATION /usr/bin)

