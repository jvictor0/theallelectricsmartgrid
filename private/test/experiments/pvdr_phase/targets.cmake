set(PVDR_EXPERIMENT_DIR "${TEST_DIR}/experiments/pvdr_phase")
foreach(variant current floor60 schmitt)
    set(variant_dir "${CMAKE_CURRENT_BINARY_DIR}/pvdr-${variant}")
    file(MAKE_DIRECTORY "${variant_dir}")
    add_custom_command(
        OUTPUT "${variant_dir}/Resynthesis.hpp"
        COMMAND "${Python3_EXECUTABLE}" "${PVDR_EXPERIMENT_DIR}/variant.py"
            "${SRC_DIR}/Resynthesis.hpp" "${variant_dir}/Resynthesis.hpp" "${variant}"
        DEPENDS "${SRC_DIR}/Resynthesis.hpp" "${PVDR_EXPERIMENT_DIR}/variant.py"
        VERBATIM)
    add_executable(pvdr_${variant}
        "${PVDR_EXPERIMENT_DIR}/render.cpp" "${variant_dir}/Resynthesis.hpp")
    target_include_directories(pvdr_${variant} PRIVATE "${variant_dir}" "${PASSTHROUGH_RATE_SRC_DIR}")
    target_compile_options(pvdr_${variant} PRIVATE -Wall -Wextra -Wno-unused-parameter)
    if(variant STREQUAL "schmitt")
        target_compile_definitions(pvdr_${variant} PRIVATE PVDR_EXPERIMENT_SCHMITT=1)
    endif()
    add_test(NAME pvdr_${variant}_contracts COMMAND pvdr_${variant} --verify)
endforeach()
