# CompilerFlag.cmake — CFBox compiler/flag configuration

include_guard()

# ── Build options ──────────────────────────────────────────────
option(CFBOX_OPTIMIZE_FOR_SIZE "Use -Os instead of -O2 for Release builds" OFF)
option(CFBOX_STATIC_LINK "Link cfbox statically (-static)" OFF)

# ── C++ standard ──────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Common warning flags (GCC / Clang) ────────────────────────
add_library(cfbox_compiler_flags INTERFACE)

target_compile_options(cfbox_compiler_flags INTERFACE
    -Wall -Wextra -Wpedantic -Werror
    -Wconversion
    -Wshadow
    -Wnon-virtual-dtor
    -Wold-style-cast
    -Wcast-align
    -Wunused
    -Woverloaded-virtual
    -Wmisleading-indentation
    -Wnull-dereference
    -Wdouble-promotion
    -Wformat=2
)

# ── Debug-specific flags ──────────────────────────────────────
target_compile_options(cfbox_compiler_flags INTERFACE
    $<$<CONFIG:Debug>:-g3 -O0 -fno-omit-frame-pointer>
    $<$<CONFIG:Debug>:-fsanitize=address,undefined>
    $<$<CONFIG:Debug>:-fno-sanitize-recover=all>
)

target_link_options(cfbox_compiler_flags INTERFACE
    $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

# ── Release-specific flags ────────────────────────────────────
if(CFBOX_OPTIMIZE_FOR_SIZE)
    target_compile_options(cfbox_compiler_flags INTERFACE
        $<$<CONFIG:Release>:-Os -DNDEBUG>
        $<$<CONFIG:Release>:-flto>
    )
else()
    target_compile_options(cfbox_compiler_flags INTERFACE
        $<$<CONFIG:Release>:-O2 -DNDEBUG>
        $<$<CONFIG:Release>:-flto>
    )
endif()

# ── Static linking ────────────────────────────────────────────
if(CFBOX_STATIC_LINK)
    target_link_options(cfbox_compiler_flags INTERFACE -static)
endif()
