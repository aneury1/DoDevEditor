
    include(FetchContent)
    FetchContent_Declare(
        libgit2
        GIT_REPOSITORY https://github.com/libgit2/libgit2.git
        GIT_TAG v1.7.1  # Change this to the latest version
    )
    # Fetch and add libgit2
FetchContent_MakeAvailable(libgit2)


    add_compile_definitions(__lib_git_2_defined)
    # Include the toolchain setup file
    include(cmake/mingw32.cmake)
    # Apply the toolchain settings
    setup_mingw_toolchain()