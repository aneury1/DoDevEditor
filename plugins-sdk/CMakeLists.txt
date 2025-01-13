set(LIBRARY_NAME DoDevEditorSDK)

add_library(${LIBRARY_NAME} STATIC sdk.cpp sdk.h)

# Export the include directory for other targets
target_include_directories(${LIBRARY_NAME}  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})