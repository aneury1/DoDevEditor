# ──────────────────────────────────────────────────────────────
#  Install.cmake
#  Defines install destinations and CPack packaging metadata.
# ──────────────────────────────────────────────────────────────

# ── Install rules ─────────────────────────────────────────────
install(
    TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin          # cross-platform: bin/ instead of /usr/bin
)

# ── CPack metadata ────────────────────────────────────────────
set(CPACK_PACKAGE_NAME              "${PROJECT_NAME}")
set(CPACK_PACKAGE_VENDOR            "DoDevEditor contributors")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A developer-focused code editor")
set(CPACK_PACKAGE_VERSION_MAJOR     "0")
set(CPACK_PACKAGE_VERSION_MINOR     "1")
set(CPACK_PACKAGE_VERSION_PATCH     "0")
set(CPACK_PACKAGE_VERSION
    "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")

if(WIN32)
    set(CPACK_GENERATOR "NSIS;ZIP")
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
else()
    set(CPACK_GENERATOR "DEB;RPM;TGZ")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "you@example.com")
endif()

include(CPack)