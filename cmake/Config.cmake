# cmake/Config.cmake — Per-applet configuration
include_guard()

# ── Per-applet options ──────────────────────────────────────
# Default all to ON. Each generates CFBOX_ENABLE_<APPLET> in applet_config.hpp
set(CFBOX_APPLETS
    echo printf cat head tail wc sort uniq
    mkdir rm cp mv ls grep find sed init
)

foreach(applet IN LISTS CFBOX_APPLETS)
    string(TOUPPER "${applet}" APPLET_UPPER)
    option(CFBOX_ENABLE_${APPLET_UPPER} "Enable ${applet} applet" ON)
endforeach()

# ── Preset profiles ─────────────────────────────────────────
# Usage: cmake -DCFBOX_PROFILE=minimal ..
set(CFBOX_PROFILE "" CACHE STRING "Configuration profile: full, desktop, minimal, embedded")

if(CFBOX_PROFILE)
    if(CFBOX_PROFILE STREQUAL "minimal")
        # Minimal: only essential file operations
        set(CFBOX_ENABLE_ECHO    ON  CACHE BOOL "" FORCE)
        set(CFBOX_ENABLE_PRINTF  OFF CACHE BOOL "" FORCE)
        set(CFBOX_ENABLE_CAT     ON  CACHE BOOL "" FORCE)
        set(CFBOX_ENABLE_HEAD    OFF CACHE BOOL "" FORCE)
        set(CFBOX_ENABLE_TAIL    OFF CACHE BOOL "" FORCE)
        set(CFBOX_ENABLE_WC      OFF CACHE BOOL "" FORCE)
        set(CFBOX_ENABLE_SORT    OFF CACHE BOOL "" FORCE)
        set(CFBOX_ENABLE_UNIQ    OFF CACHE BOOL "" FORCE)
        set(CFBOX_ENABLE_MKDIR   ON  CACHE BOOL "" FORCE)
        set(CFBOX_ENABLE_RM      ON  CACHE BOOL "" FORCE)
        set(CFBOX_ENABLE_CP      ON  CACHE BOOL "" FORCE)
        set(CFBOX_ENABLE_MV      ON  CACHE BOOL "" FORCE)
        set(CFBOX_ENABLE_LS      ON  CACHE BOOL "" FORCE)
        set(CFBOX_ENABLE_GREP    ON  CACHE BOOL "" FORCE)
        set(CFBOX_ENABLE_FIND    OFF CACHE BOOL "" FORCE)
        set(CFBOX_ENABLE_SED     OFF CACHE BOOL "" FORCE)
        set(CFBOX_ENABLE_INIT    OFF CACHE BOOL "" FORCE)
    elseif(CFBOX_PROFILE STREQUAL "embedded")
        # Embedded: everything except optional text processing
        set(CFBOX_ENABLE_SORT    OFF CACHE BOOL "" FORCE)
        set(CFBOX_ENABLE_UNIQ    OFF CACHE BOOL "" FORCE)
        set(CFBOX_ENABLE_SED     OFF CACHE BOOL "" FORCE)
    elseif(CFBOX_PROFILE STREQUAL "desktop" OR CFBOX_PROFILE STREQUAL "full")
        # All enabled (same as default)
    else()
        message(WARNING "Unknown CFBOX_PROFILE: ${CFBOX_PROFILE}")
    endif()
endif()

# ── Generate applet_config.hpp ──────────────────────────────
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/include/cfbox/applet_config.hpp.in
    ${CMAKE_CURRENT_BINARY_DIR}/include/cfbox/applet_config.hpp
    @ONLY
)
