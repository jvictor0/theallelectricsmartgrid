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

find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_package(PkgConfig REQUIRED)
pkg_check_modules(FLAC REQUIRED IMPORTED_TARGET flac)
set(BUILD_INFO_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
add_custom_target(smartgrid_build_info
    COMMAND "${Python3_EXECUTABLE}" "${REPO_ROOT}/scripts/generate_build_info.py"
            "${REPO_ROOT}" "${BUILD_INFO_DIR}/SmartGridBuildInfo.hpp"
    BYPRODUCTS "${BUILD_INFO_DIR}/SmartGridBuildInfo.hpp"
    VERBATIM
)

# ---------------------------------------------------------------------------
# JSON is now provided by the self-contained, header-only arena library
# (private/src/Json.hpp). No external jansson submodule is required.
# ---------------------------------------------------------------------------
# Test executable
# ---------------------------------------------------------------------------
set(TEST_SOURCES
    ${TEST_DIR}/support/TestMain.cpp
    ${SRC_DIR}/RecordingFlacCodec.cpp
    ${TEST_DIR}/unit/infra_spike.cpp
)

# Pick up any additional unit/ and system/ tests as they get added.
file(GLOB EXTRA_UNIT_TESTS   CONFIGURE_DEPENDS ${TEST_DIR}/unit/*.cpp)
file(GLOB EXTRA_SYSTEM_TESTS CONFIGURE_DEPENDS ${TEST_DIR}/system/*.cpp)
list(REMOVE_ITEM EXTRA_UNIT_TESTS ${TEST_DIR}/unit/infra_spike.cpp)
list(APPEND TEST_SOURCES ${EXTRA_UNIT_TESTS} ${EXTRA_SYSTEM_TESTS})

add_executable(smartgrid_tests ${TEST_SOURCES})
add_dependencies(smartgrid_tests smartgrid_build_info)

target_include_directories(smartgrid_tests PRIVATE
    ${TEST_DIR}
    ${SRC_DIR}
    "${BUILD_INFO_DIR}"
)

# The standalone tests compile the core headers without JUCE. JSON is provided
# by the self-contained arena library (Json.hpp) in every build. -funroll-loops
# mirrors the VCV Rack plugin Makefile.
#
# DOCTEST_CONFIG_NO_SHORT_MACRO_NAMES stops doctest from defining bare INFO/WARN
# macros, which would otherwise collide with the codebase's own INFO(...) and
# WARN(...) logging macros. Use the DOCTEST_-prefixed forms in tests if needed.
target_compile_definitions(smartgrid_tests PRIVATE
    DOCTEST_CONFIG_NO_SHORT_MACRO_NAMES=1
    SMARTGRID_RECORDING_SYSTEM_FLAC=1
    SMARTGRID_REPO_ROOT="${REPO_ROOT}"
)
target_compile_options(smartgrid_tests PRIVATE -funroll-loops)
target_link_libraries(smartgrid_tests PRIVATE PkgConfig::FLAC)

# Keep warnings on but non-fatal; the DSP core has pre-existing warnings.
target_compile_options(smartgrid_tests PRIVATE -Wall -Wno-unused-parameter)

# ---------------------------------------------------------------------------
# CTest registration. doctest test filtering is supported by passing
# --test-case=... etc. to the binary directly.
# ---------------------------------------------------------------------------
add_test(NAME smartgrid_tests COMMAND smartgrid_tests)

# Exercise the rate-generic tests at ratio 1, where every rate boundary is a
# passthrough, without changing the production rate. Copy headers so quoted sibling includes use the same rate everywhere.
# configure_file tracks source edits and only updates changed copies.
set(PASSTHROUGH_RATE_SRC_DIR "${CMAKE_CURRENT_BINARY_DIR}/passthrough-rate-src")
file(GLOB_RECURSE PASSTHROUGH_RATE_HEADERS CONFIGURE_DEPENDS "${SRC_DIR}/*.hpp" "${SRC_DIR}/*.h")
foreach(header IN LISTS PASSTHROUGH_RATE_HEADERS)
    file(RELATIVE_PATH relative "${SRC_DIR}" "${header}")
    if(NOT relative STREQUAL "SampleTimer.hpp")
        configure_file("${header}" "${PASSTHROUGH_RATE_SRC_DIR}/${relative}" COPYONLY)
    endif()
endforeach()
configure_file("${SRC_DIR}/SampleTimer.hpp" "${PASSTHROUGH_RATE_SRC_DIR}/SampleTimer.hpp.in" COPYONLY)
file(READ "${PASSTHROUGH_RATE_SRC_DIR}/SampleTimer.hpp.in" PASSTHROUGH_RATE_TIMER)
if(NOT PASSTHROUGH_RATE_TIMER MATCHES "static constexpr size_t x_oversample = [0-9]+;")
    message(FATAL_ERROR "Update the passthrough-rate test configuration for SampleTimer's rate declaration")
endif()
string(REGEX REPLACE "static constexpr size_t x_oversample = [0-9]+;"
    "static constexpr size_t x_oversample = 1;" PASSTHROUGH_RATE_TIMER "${PASSTHROUGH_RATE_TIMER}")
file(GENERATE OUTPUT "${PASSTHROUGH_RATE_SRC_DIR}/SampleTimer.hpp" CONTENT "${PASSTHROUGH_RATE_TIMER}")
configure_file("${SRC_DIR}/RecordingFlacCodec.cpp" "${PASSTHROUGH_RATE_SRC_DIR}/RecordingFlacCodec.cpp" COPYONLY)

add_executable(smartgrid_passthrough_rate_tests
    ${TEST_DIR}/support/TestMain.cpp
    ${TEST_DIR}/unit/encoder_host_rate.cpp
    ${TEST_DIR}/unit/encoder_motion.cpp
    ${TEST_DIR}/unit/effect_rate_boundaries.cpp
    ${TEST_DIR}/unit/dsp_wavefolder.cpp
    "${PASSTHROUGH_RATE_SRC_DIR}/RecordingFlacCodec.cpp"
)
add_dependencies(smartgrid_passthrough_rate_tests smartgrid_build_info)
target_include_directories(smartgrid_passthrough_rate_tests PRIVATE
    ${TEST_DIR} "${PASSTHROUGH_RATE_SRC_DIR}" "${BUILD_INFO_DIR}")
target_compile_definitions(smartgrid_passthrough_rate_tests PRIVATE
    DOCTEST_CONFIG_NO_SHORT_MACRO_NAMES=1
    SMARTGRID_RECORDING_SYSTEM_FLAC=1
    SMARTGRID_REPO_ROOT="${REPO_ROOT}"
    SMARTGRID_TEST_OVERSAMPLE=1
)
target_compile_options(smartgrid_passthrough_rate_tests PRIVATE -funroll-loops -Wall -Wno-unused-parameter)
target_link_libraries(smartgrid_passthrough_rate_tests PRIVATE PkgConfig::FLAC)
add_test(NAME smartgrid_passthrough_rate_tests COMMAND smartgrid_passthrough_rate_tests)

option(SMARTGRID_PVDR_EXPERIMENT "Build the neutral feedback phase experiment" OFF)
if(SMARTGRID_PVDR_EXPERIMENT)
    include("${TEST_DIR}/experiments/pvdr_phase/targets.cmake")
endif()
