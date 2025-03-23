# TreeSitter.cmake - Fetch and Compile Tree-sitter C++ as a Static Library
project(DoDevEditor)
include(FetchContent)
message("Fetching Tree Sitter")
# Fetch tree-sitter core
FetchContent_Declare(
    tree_sitter_core
    GIT_REPOSITORY https://github.com/tree-sitter/tree-sitter.git
    GIT_TAG        master
)
FetchContent_MakeAvailable(tree_sitter_core)
message("Fetching Tree Sitter cpp")

# Fetch tree-sitter-cpp (C++ grammar)
FetchContent_Declare(
    tree_sitter_cpp
    GIT_REPOSITORY https://github.com/tree-sitter/tree-sitter-cpp.git
    GIT_TAG        master
)
FetchContent_MakeAvailable(tree_sitter_cpp)

# Define static libraries
add_library(tree_sitter STATIC ${tree_sitter_core_SOURCE_DIR}/lib/src/lib.c)
target_include_directories(tree_sitter PUBLIC ${tree_sitter_core_SOURCE_DIR}/lib/include)

add_library(tree_sitter_cpp STATIC ${tree_sitter_cpp_SOURCE_DIR}/src/parser.c)
target_include_directories(tree_sitter_cpp PUBLIC ${tree_sitter_cpp_SOURCE_DIR}/src)

# Set global properties for external projects
set(TREE_SITTER_LIBRARIES tree_sitter tree_sitter_cpp)
set(TREE_SITTER_INCLUDE_DIRS ${tree_sitter_core_SOURCE_DIR}/lib/include ${tree_sitter_cpp_SOURCE_DIR}/src)

# Provide a target for linking
add_library(TreeSitter::Core ALIAS tree_sitter)

#add_library(TreeSitter::Cpp STATIC 
#    ${tree_sitter_cpp_SOURCE_DIR}/src/parser.c
#    ${tree_sitter_cpp_SOURCE_DIR}/src/scanner.c
#)


 add_library(TreeSitter::Cpp ALIAS tree_sitter_cpp)