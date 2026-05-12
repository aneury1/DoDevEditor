# ──────────────────────────────────────────────────────────────
#  Dependencies.cmake
#  Locates (or fetches) every external library the project needs.
#  Requires: Options.cmake and Platform.cmake to be included first.
# ──────────────────────────────────────────────────────────────

include(FetchContent)

# ── wxWidgets ─────────────────────────────────────────────────
if(FETCH_WXWIDGETS)
    message(STATUS "[Deps] Fetching wxWidgets v3.2.4 via FetchContent …")
    FetchContent_Declare(
        wxWidgets
        GIT_REPOSITORY https://github.com/wxWidgets/wxWidgets.git
        GIT_TAG        v3.2.4
    )
    FetchContent_MakeAvailable(wxWidgets)
else()
    # Find a system-installed wxWidgets.
    # The components list is the union of everything the project uses;
    # remove any you do not need to keep link times short.
    find_package(wxWidgets REQUIRED
        COMPONENTS
            core
            base
            stc     # Styled Text Control – used by the editor widget
            aui     # Advanced UI docking framework
            grid
    )
    include(${wxWidgets_USE_FILE})
    message(STATUS "[Deps] wxWidgets found: ${wxWidgets_VERSION}")
endif()
