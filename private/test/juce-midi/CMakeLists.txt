cmake_minimum_required(VERSION 3.22)
project(smartgrid_midi_tests LANGUAGES C CXX OBJC OBJCXX)
if(NOT APPLE)
    message(FATAL_ERROR "These integration tests require macOS CoreMIDI")
endif()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_OBJCXX_STANDARD 17)
set(CMAKE_BUILD_TYPE Release)
get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
set(MODULES "${REPO_ROOT}/third_party/JUCE/modules")
set(OVERLAY "${CMAKE_CURRENT_BINARY_DIR}/modules")
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${REPO_ROOT}/JUCE/SmartGridOne/scripts/prepare_apple_audio_module.py"
    "${REPO_ROOT}/private/src/MidiOutputSchedule.hpp")
execute_process(COMMAND "${Python3_EXECUTABLE}"
    "${REPO_ROOT}/JUCE/SmartGridOne/scripts/prepare_apple_audio_module.py"
    "${MODULES}/juce_audio_devices" "${OVERLAY}/juce_audio_devices"
    COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND "${Python3_EXECUTABLE}" "${REPO_ROOT}/scripts/generate_build_info.py"
    "${REPO_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/SmartGridBuildInfo.hpp"
    COMMAND_ERROR_IS_FATAL ANY)
include("${REPO_ROOT}/third_party/JUCE/extras/Build/CMake/JUCEModuleSupport.cmake")
juce_add_modules("${MODULES}/juce_core" "${MODULES}/juce_events"
    "${MODULES}/juce_audio_basics" "${MODULES}/juce_graphics"
    "${OVERLAY}/juce_audio_devices")
find_package(PkgConfig REQUIRED)
pkg_check_modules(FLAC REQUIRED IMPORTED_TARGET flac)
add_executable(smartgrid_midi_tests reconnect.cpp
    ../unit/midi_output_schedule.cpp
    ../unit/midi_sysex_queue.cpp
    ../unit/midi_realtime.cpp
    ../system/midi_clock_sync.cpp
    "${REPO_ROOT}/private/src/SmartGrid.cpp"
    "${REPO_ROOT}/private/src/RecordingFlacCodec.cpp")
target_include_directories(smartgrid_midi_tests PRIVATE
    "${OVERLAY}" "${MODULES}" "${CMAKE_CURRENT_BINARY_DIR}"
    "${REPO_ROOT}/JUCE/SmartGridOne/JuceLibraryCode"
    "${REPO_ROOT}/JUCE/SmartGridOne/Source"
    "${REPO_ROOT}/private/src" "${REPO_ROOT}/private/test")
target_compile_definitions(smartgrid_midi_tests PRIVATE
    JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1 JUCE_STANDALONE_APPLICATION=1
    JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0
    DOCTEST_CONFIG_NO_SHORT_MACRO_NAMES=1 SMARTGRID_RECORDING_SYSTEM_FLAC=1)
target_compile_options(smartgrid_midi_tests PRIVATE -Wno-deprecated-declarations -Wno-shorten-64-to-32)
target_link_libraries(smartgrid_midi_tests PRIVATE juce_audio_devices juce_graphics PkgConfig::FLAC)
enable_testing()
add_test(NAME smartgrid_midi_tests COMMAND smartgrid_midi_tests)
