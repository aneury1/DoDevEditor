project(DoDevEditor)
include(FetchContent)
message("Fetching wxWidget version v3.2.2")
include(FetchContent)

 FetchContent_Declare(
        wxWidgets
        GIT_REPOSITORY https://github.com/wxWidgets/wxWidgets.git
        GIT_TAG        v3.0.0  # Use the latest stable version
)
FetchContent_MakeAvailable(wxWidgets)