# Target-local helpers; no global warning flags or global dependency paths.
function(dodev_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /utf-8)
        if(DODEV_ENABLE_WARNINGS)
            target_compile_options(${target} PRIVATE /W4)
        endif()
    elseif(DODEV_ENABLE_WARNINGS AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

# Keep runtime artifacts together with manifests. For multi-config generators
# use one build tree per configuration to avoid overwriting plugin binaries.
function(dodev_runtime_directory target directory)
    set_target_properties(${target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${directory}"
        LIBRARY_OUTPUT_DIRECTORY "${directory}")
    foreach(config IN LISTS CMAKE_CONFIGURATION_TYPES)
        string(TOUPPER "${config}" config_upper)
        set_target_properties(${target} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY_${config_upper} "${directory}"
            LIBRARY_OUTPUT_DIRECTORY_${config_upper} "${directory}")
    endforeach()
endfunction()

# Match plugin.json on each platform (including MinGW and Apple MODULE libs).
function(dodev_plugin_filename target)
    if(WIN32)
        set_target_properties(${target} PROPERTIES PREFIX "")
    elseif(APPLE)
        set_target_properties(${target} PROPERTIES PREFIX "lib" SUFFIX ".dylib")
    endif()
endfunction()

# Several legacy tests use assert(), including expressions with side effects.
# Keep those checks enabled in Release and RelWithDebInfo test executables.
function(dodev_test_assertions target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /UNDEBUG)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -UNDEBUG)
    endif()
endfunction()
