set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# MinGW cross-compilation
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)

# Specify Windows paths
set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Function to apply this toolchain (for modularity)
function(setup_mingw_toolchain)
    set(CMAKE_SYSTEM_NAME Windows PARENT_SCOPE)
    set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc PARENT_SCOPE)
    set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++ PARENT_SCOPE)
    set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres PARENT_SCOPE)
endfunction()