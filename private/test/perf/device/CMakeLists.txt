cmake_minimum_required(VERSION 3.20)
project(SmartGridPerf LANGUAGES CXX OBJCXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OBJCXX_STANDARD 17)
get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../.." ABSOLUTE)
if(NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    message(FATAL_ERROR "Use the root perf-ipad-build target for this iOS app")
endif()
execute_process(COMMAND git -C "${REPO_ROOT}" rev-parse HEAD
    OUTPUT_VARIABLE PERF_COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND git -C "${REPO_ROOT}" status --porcelain --untracked-files=normal
    OUTPUT_VARIABLE PERF_STATUS OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
set(PERF_DIRTY false)
if(PERF_STATUS)
    set(PERF_DIRTY true)
endif()
configure_file(BuildInfo.hpp.in BuildInfo.hpp @ONLY)
add_executable(SmartGridPerf MACOSX_BUNDLE Main.mm Cases.cpp ../density.cpp)
target_include_directories(SmartGridPerf PRIVATE "${REPO_ROOT}/private/src" "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_options(SmartGridPerf PRIVATE -O3 -funroll-loops "$<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>")
target_compile_definitions(SmartGridPerf PRIVATE NDEBUG=1)
target_link_libraries(SmartGridPerf PRIVATE "-framework UIKit" "-framework Foundation")
set_target_properties(SmartGridPerf PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.theallelectricsmartgrid.perf"
    XCODE_ATTRIBUTE_CODE_SIGN_STYLE Automatic
    XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "2"
    XCODE_ATTRIBUTE_LLVM_LTO YES
    XCODE_ATTRIBUTE_GCC_OPTIMIZATION_LEVEL 3
    XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT "dwarf-with-dsym")
