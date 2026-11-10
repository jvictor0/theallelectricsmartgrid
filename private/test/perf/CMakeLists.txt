cmake_minimum_required(VERSION 3.16)
project(smartgrid_perf LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
if(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    message(FATAL_ERROR "Performance measurements require the separate Release build")
endif()
include(CheckIPOSupported)
check_ipo_supported(RESULT PERF_IPO OUTPUT PERF_IPO_ERROR)
if(NOT PERF_IPO)
    message(FATAL_ERROR "Benchmark LTO unavailable: ${PERF_IPO_ERROR}")
endif()
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
pkg_check_modules(FLAC REQUIRED IMPORTED_TARGET flac)
set(BUILD_INFO_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
add_custom_target(perf_build_info
    COMMAND "${Python3_EXECUTABLE}" "${REPO_ROOT}/scripts/generate_build_info.py"
        "${REPO_ROOT}" "${BUILD_INFO_DIR}/SmartGridBuildInfo.hpp"
    BYPRODUCTS "${BUILD_INFO_DIR}/SmartGridBuildInfo.hpp"
    VERBATIM)
add_library(perf_codec STATIC "${REPO_ROOT}/private/src/RecordingFlacCodec.cpp")
target_compile_definitions(perf_codec PRIVATE SMARTGRID_RECORDING_SYSTEM_FLAC=1)
target_link_libraries(perf_codec PUBLIC PkgConfig::FLAC)
foreach(kind voices effects extra auxiliary encoders sgrec density)
    add_executable(perf_${kind} ${kind}.cpp)
    add_dependencies(perf_${kind} perf_build_info)
    target_include_directories(perf_${kind} PRIVATE
        "${REPO_ROOT}/private/test" "${REPO_ROOT}/private/src" "${BUILD_INFO_DIR}")
    target_compile_definitions(perf_${kind} PRIVATE
        SMARTGRID_RECORDING_SYSTEM_FLAC=1 SMARTGRID_REPO_ROOT="${REPO_ROOT}")
    target_compile_options(perf_${kind} PRIVATE -funroll-loops)
    target_link_libraries(perf_${kind} PRIVATE perf_codec Threads::Threads)
endforeach()
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/build.json" CONTENT
"{\"compiler\":\"${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}\",\"configuration\":\"${CMAKE_BUILD_TYPE}\",\"lto\":true,\"flags\":\"${CMAKE_CXX_FLAGS_RELEASE} -funroll-loops\"}\n")
enable_testing()
add_test(NAME perf_smoke COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/run.py"
    --bin-dir "${CMAKE_CURRENT_BINARY_DIR}" --smoke
    --output "${REPO_ROOT}/build/results/perf-smoke")
add_test(NAME perf_protocol COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_protocol.py"
    "${CMAKE_CURRENT_BINARY_DIR}")
add_test(NAME perf_sgrec_protocol COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_sgrec_replay.py"
    "$<TARGET_FILE:perf_sgrec>")
add_test(NAME perf_density_reference COMMAND "$<TARGET_FILE:perf_density>" self-test)
set_tests_properties(perf_smoke perf_protocol perf_sgrec_protocol PROPERTIES RUN_SERIAL TRUE TIMEOUT 600)
set_tests_properties(perf_density_reference PROPERTIES RUN_SERIAL TRUE TIMEOUT 60)
