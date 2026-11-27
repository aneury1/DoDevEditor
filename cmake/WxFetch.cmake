# Fetch the complete upstream release (not GitHub's generated source archive,
# which omits required submodules). All targets use the parent's compiler.
# The pinned version matches the existing 3.2.6 desktop development baseline.
function(dodev_fetch_wxwidgets archive_url archive_hash)
    # CMake >= 3.30 may otherwise hide download progress in direct mode.
    # The sub-build mode prints actual transfer/extract progress on 3.21+.
    if(POLICY CMP0168)
        cmake_policy(SET CMP0168 OLD)
    endif()
    if(POLICY CMP0135)
        cmake_policy(SET CMP0135 NEW)
    endif()
    include(FetchContent)
    if(DODEV_WX_FETCH_PROGRESS)
        set(FETCHCONTENT_QUIET OFF)
    else()
        set(FETCHCONTENT_QUIET ON)
    endif()

    # Older upstream CMake files still declare a 3.0 minimum. Scope the
    # compatibility floor to wxWidgets (needed with CMake 4.x).
    if(CMAKE_VERSION VERSION_GREATER_EQUAL "4.0")
        set(CMAKE_POLICY_VERSION_MINIMUM 3.10)
    endif()

    # Keep these cache settings local to the wx build; no global BUILD_SHARED_LIBS
    # change. Static wx avoids shipping extra wx DLLs with the fetched build.
    set(wxBUILD_SHARED OFF CACHE BOOL "Build wxWidgets statically" FORCE)
    set(wxBUILD_MONOLITHIC OFF CACHE BOOL "Build individual wx components" FORCE)
    set(wxBUILD_INSTALL OFF CACHE BOOL "Do not install wx development files" FORCE)
    set(wxBUILD_SAMPLES OFF CACHE STRING "Skip wx samples" FORCE)
    set(wxBUILD_TESTS OFF CACHE STRING "Skip wx tests" FORCE)
    set(wxBUILD_DEMOS OFF CACHE BOOL "Skip wx demos" FORCE)
    set(wxBUILD_BENCHMARKS OFF CACHE BOOL "Skip wx benchmarks" FORCE)
    set(wxUSE_GUI ON CACHE BOOL "Enable wx GUI" FORCE)
    set(wxUSE_AUI ON CACHE BOOL "Enable docking" FORCE)
    set(wxUSE_HTML ON CACHE BOOL "Enable static HTML view" FORCE)
    set(wxUSE_STC ON CACHE BOOL "Enable Scintilla" FORCE)
    set(wxUSE_UNICODE ON CACHE BOOL "Enable Unicode" FORCE)
    # No editor code uses these optional modules; avoid SDK/browser dependencies.
    set(wxUSE_WEBVIEW OFF CACHE BOOL "Skip wxWebView" FORCE)
    set(wxUSE_MEDIACTRL ${WIN32} CACHE BOOL "Enable native media on Windows" FORCE)
    # wx 3.2 uses a separate setting for the MSVC CRT. Match the main project's
    # default /MD[d] or an explicitly selected /MT[d] runtime.
    if(MSVC)
        if(CMAKE_MSVC_RUNTIME_LIBRARY MATCHES "DLL" OR NOT CMAKE_MSVC_RUNTIME_LIBRARY)
            set(wxBUILD_USE_STATIC_RUNTIME OFF CACHE BOOL "Match shared MSVC CRT" FORCE)
        else()
            set(wxBUILD_USE_STATIC_RUNTIME ON CACHE BOOL "Match static MSVC CRT" FORCE)
        endif()
    endif()

    message(STATUS "[wxWidgets] Source: ${archive_url}")
    message(STATUS "[wxWidgets] Source/build cache: ${FETCHCONTENT_BASE_DIR}")
    message(STATUS "[wxWidgets] Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
    if(FETCHCONTENT_SOURCE_DIR_WXWIDGETS)
        message(STATUS "[wxWidgets] Using local extracted source: ${FETCHCONTENT_SOURCE_DIR_WXWIDGETS}")
    else()
        message(STATUS "[wxWidgets] Downloading or reusing cached archive; verifying SHA-256, then extracting")
    endif()
    FetchContent_Declare(wxwidgets
        URL "${archive_url}"
        URL_HASH "SHA256=${archive_hash}"
        TLS_VERIFY TRUE
        DOWNLOAD_NO_PROGRESS FALSE
        TIMEOUT 900
        INACTIVITY_TIMEOUT 120
    )
    FetchContent_MakeAvailable(wxwidgets)
    message(STATUS "[wxWidgets] Sources ready: ${wxwidgets_SOURCE_DIR}")
    message(STATUS "[wxWidgets] Build directory: ${wxwidgets_BINARY_DIR}")
    message(STATUS "[wxWidgets] Static libraries will compile during cmake --build (normal compiler progress)")
endfunction()
