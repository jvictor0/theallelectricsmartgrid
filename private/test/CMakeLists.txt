cmake_minimum_required(VERSION 3.16)

project(smartgrid_tests CXX)

# The DSP core is C++17 (matches the .jucer / clang-tidy config).
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default to an optimized build with asserts kept on: the system tests simulate
# seconds of 48kHz audio and run ~6x slower in -O0. Use -DCMAKE_BUILD_TYPE=Debug
# for debugging sessions.
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()
# RelWithDebInfo defines NDEBUG by default, which would strip the DSP asserts the
# tests rely on (e.g. VectorPhaseShaper's phase assert). Keep asserts enabled.
string(REPLACE "-DNDEBUG" "" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")

enable_testing()

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------
set(TEST_DIR        ${CMAKE_CURRENT_SOURCE_DIR})
set(REPO_ROOT       ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set(SRC_DIR         ${CMAKE_CURRENT_SOURCE_DIR}/../src)

# ---------------------------------------------------------------------------
# JSON is now provided by the self-contained, header-only arena library
# (private/src/Json.hpp). No external jansson submodule is required.
# ---------------------------------------------------------------------------
# Test executable
# ---------------------------------------------------------------------------
set(TEST_SOURCES
    ${TEST_DIR}/support/TestMain.cpp
    ${SRC_DIR}/SmartGrid.cpp
    ${TEST_DIR}/unit/infra_spike.cpp
)

# Pick up any additional unit/ and system/ tests as they get added.
file(GLOB EXTRA_UNIT_TESTS   CONFIGURE_DEPENDS ${TEST_DIR}/unit/*.cpp)
file(GLOB EXTRA_SYSTEM_TESTS CONFIGURE_DEPENDS ${TEST_DIR}/system/*.cpp)
list(REMOVE_ITEM EXTRA_UNIT_TESTS ${TEST_DIR}/unit/infra_spike.cpp)
list(APPEND TEST_SOURCES ${EXTRA_UNIT_TESTS} ${EXTRA_SYSTEM_TESTS})

add_executable(smartgrid_tests ${TEST_SOURCES})

target_include_directories(smartgrid_tests PRIVATE
    ${TEST_DIR}
    ${SRC_DIR}
)

# EMBEDDED_BUILD strips the JUCE dependency out of the core headers
# (AsyncLogger.hpp, ios_stubs.hpp). JSON is provided by the self-contained
# arena library (Json.hpp) in every build. -funroll-loops mirrors the VCV Rack
# plugin Makefile.
#
# DOCTEST_CONFIG_NO_SHORT_MACRO_NAMES stops doctest from defining bare INFO/WARN
# macros, which would otherwise collide with the codebase's own INFO(...) and
# WARN(...) logging macros. Use the DOCTEST_-prefixed forms in tests if needed.
target_compile_definitions(smartgrid_tests PRIVATE
    EMBEDDED_BUILD=1
    DOCTEST_CONFIG_NO_SHORT_MACRO_NAMES=1
    SMARTGRID_REPO_ROOT="${REPO_ROOT}"
)
target_compile_options(smartgrid_tests PRIVATE -funroll-loops)

# Keep warnings on but non-fatal; the DSP core has pre-existing warnings.
target_compile_options(smartgrid_tests PRIVATE -Wall -Wno-unused-parameter)

# ---------------------------------------------------------------------------
# CTest registration. doctest test filtering is supported by passing
# --test-case=... etc. to the binary directly.
# ---------------------------------------------------------------------------
add_test(NAME smartgrid_tests COMMAND smartgrid_tests)
