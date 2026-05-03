cmake_minimum_required(VERSION 3.26 FATAL_ERROR)

project(CFBox
    VERSION 0.0.1
    DESCRIPTION "A Modern C++ Toy Busybox"
    HOMEPAGE_URL "https://github.com/Awesome-Embedded-Learning-Studio/CFBox"
    LANGUAGES CXX
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── CPM dependency manager ────────────────────────────────────
include(cmake/third_party/CPM.cmake)

# ── Compiler flags ────────────────────────────────────────────
include(cmake/compile/CompilerFlag.cmake)

# ── Dependencies ──────────────────────────────────────────────
CPMAddPackage(
    NAME zlib
    GITHUB_REPOSITORY madler/zlib
    GIT_TAG v1.3.1
    OPTIONS "CMAKE_POSITION_INDEPENDENT_CODE ON"
)

# ── Applet configuration ─────────────────────────────────────
include(cmake/Config.cmake)

# ── Applet sources (conditional on config) ───────────────────
set(CFBOX_APPLET_SOURCES)

# Multi-file applets
if(CFBOX_ENABLE_SH)
    file(GLOB_RECURSE SH_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/applets/sh/*.cpp)
    list(APPEND CFBOX_APPLET_SOURCES ${SH_SOURCES})
endif()
if(CFBOX_ENABLE_AWK)
    file(GLOB_RECURSE AWK_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/applets/awk/*.cpp)
    list(APPEND CFBOX_APPLET_SOURCES ${AWK_SOURCES})
endif()
if(CFBOX_ENABLE_INIT)
    file(GLOB_RECURSE INIT_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/applets/init/*.cpp)
    list(APPEND CFBOX_APPLET_SOURCES ${INIT_SOURCES})
endif()
if(CFBOX_ENABLE_TOP)
    file(GLOB_RECURSE TOP_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/applets/top/*.cpp)
    list(APPEND CFBOX_APPLET_SOURCES ${TOP_SOURCES})
endif()

# Single-file applets
foreach(applet IN LISTS CFBOX_APPLETS)
    string(TOUPPER "${applet}" APPLET_UPPER)
    if(NOT applet STREQUAL "sh" AND NOT applet STREQUAL "awk" AND NOT applet STREQUAL "init" AND NOT applet STREQUAL "top" AND CFBOX_ENABLE_${APPLET_UPPER})
        list(APPEND CFBOX_APPLET_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/applets/${applet}.cpp)
    endif()
endforeach()

# ── Main executable ───────────────────────────────────────────
add_executable(cfbox src/main.cpp ${CFBOX_APPLET_SOURCES})
target_include_directories(cfbox PUBLIC include)
target_include_directories(cfbox PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/include)
target_link_libraries(cfbox PRIVATE cfbox_compiler_flags)
if(zlib_ADDED)
    target_link_libraries(cfbox PRIVATE zlibstatic)
    target_include_directories(cfbox SYSTEM PRIVATE ${zlib_SOURCE_DIR} ${zlib_BINARY_DIR})
else()
    find_package(ZLIB REQUIRED)
    target_link_libraries(cfbox PRIVATE ZLIB::ZLIB)
endif()

# ── GTest via CPM (FetchContent) ──────────────────────────────
if(NOT CMAKE_CROSSCOMPILING)
CPMAddPackage(
    NAME GTest
    GITHUB_REPOSITORY google/googletest
    GIT_TAG v1.14.0
)

if(GTest_ADDED)
    enable_testing()

    file(GLOB_RECURSE CFBOX_TEST_SOURCES CONFIGURE_DEPENDS tests/unit/*.cpp)

    if(CFBOX_TEST_SOURCES)
        add_executable(cfbox_tests ${CFBOX_TEST_SOURCES} ${CFBOX_APPLET_SOURCES})
        target_include_directories(cfbox_tests PUBLIC include)
        target_include_directories(cfbox_tests PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/include)
        target_link_libraries(cfbox_tests PRIVATE
            cfbox_compiler_flags
            GTest::gtest_main
        )
        if(zlib_ADDED)
            target_link_libraries(cfbox_tests PRIVATE zlibstatic)
            target_include_directories(cfbox_tests SYSTEM PRIVATE ${zlib_SOURCE_DIR} ${zlib_BINARY_DIR})
        else()
            target_link_libraries(cfbox_tests PRIVATE ZLIB::ZLIB)
        endif()

        include(GoogleTest)
        gtest_discover_tests(cfbox_tests)
    endif()
endif()
endif()
