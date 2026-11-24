import PhaseTime.InverseCodegen

#generate_cpp_effect PhaseTime.InverseRuntime.readPayloadFloat with_contract as TimeWarpInverseRead to "build/lean/generated/TimeWarpInverseRead.hpp"
#generate_cpp_effect PhaseTime.InverseRuntime.writeFloat with_contract scalar_inputs as TimeWarpInverseWrite to "build/lean/generated/TimeWarpInverseWrite.hpp"
