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

# ── Applet sources (shared between main exe and tests) ────────
file(GLOB_RECURSE CFBOX_APPLET_SOURCES CONFIGURE_DEPENDS src/applets/*.cpp)

# ── Main executable ───────────────────────────────────────────
add_executable(cfbox src/main.cpp ${CFBOX_APPLET_SOURCES})
target_include_directories(cfbox PUBLIC include)
target_link_libraries(cfbox PRIVATE cfbox_compiler_flags)

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
        target_link_libraries(cfbox_tests PRIVATE
            cfbox_compiler_flags
            GTest::gtest_main
        )

        include(GoogleTest)
        gtest_discover_tests(cfbox_tests)
    endif()
endif()
endif()
