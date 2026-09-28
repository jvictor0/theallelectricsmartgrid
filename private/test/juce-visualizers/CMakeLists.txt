cmake_minimum_required(VERSION 3.22)
project(smartgrid_visualizer_tests LANGUAGES C CXX OBJC OBJCXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_OBJCXX_STANDARD 17)
set(CMAKE_BUILD_TYPE Release)
get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
set(MODULES "${REPO_ROOT}/third_party/JUCE/modules")
find_package(Python3 REQUIRED COMPONENTS Interpreter)
execute_process(COMMAND "${Python3_EXECUTABLE}" "${REPO_ROOT}/scripts/generate_build_info.py"
    "${REPO_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/SmartGridBuildInfo.hpp"
    COMMAND_ERROR_IS_FATAL ANY)
include("${REPO_ROOT}/third_party/JUCE/extras/Build/CMake/JUCEModuleSupport.cmake")
juce_add_modules("${MODULES}/juce_core" "${MODULES}/juce_events"
    "${MODULES}/juce_graphics" "${MODULES}/juce_data_structures"
    "${MODULES}/juce_gui_basics" "${MODULES}/juce_audio_basics")
add_executable(smartgrid_visualizer_tests melody.cpp)
target_include_directories(smartgrid_visualizer_tests PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}" "${MODULES}"
    "${REPO_ROOT}/JUCE/SmartGridOne/JuceLibraryCode"
    "${REPO_ROOT}/JUCE/SmartGridOne/Source"
    "${REPO_ROOT}/private/src" "${REPO_ROOT}/private/test")
target_compile_definitions(smartgrid_visualizer_tests PRIVATE
    JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1 JUCE_STANDALONE_APPLICATION=1
    JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0 DOCTEST_CONFIG_NO_SHORT_MACRO_NAMES=1)
target_compile_options(smartgrid_visualizer_tests PRIVATE -Wno-deprecated-declarations)
target_link_libraries(smartgrid_visualizer_tests PRIVATE juce_gui_basics juce_audio_basics)
enable_testing()
add_test(NAME smartgrid_visualizer_tests COMMAND smartgrid_visualizer_tests)
