import DSP.Codegen.Cpp
import DSP.Filters.OnePoleZOHLowPass
import DSP.Filters.OnePoleTPTLowPass
import DSP.Filters.OnePoleTPTHighPass
import DSP.Filters.TPTBaseWidth
import DSP.Filters.TPTSVF
import DSP.Filters.MorphingTPTSVF
import DSP.Filters.TPTSVF4Pole
import DSP.Filters.TPTLadder
import DSP.Filters.LinkwitzRileyCrossover
import DSP.Filters.Coupled

#generate_cpp DSP.OnePoleZOHLowPass.stepFloat as OnePoleZOHLowPass to "build/lean/generated/OnePoleZOHLowPass.hpp"

#generate_cpp DSP.OnePoleTPTLowPass.stepFloat as OnePoleTPTLowPass to "build/lean/generated/OnePoleTPTLowPass.hpp"

#generate_cpp DSP.OnePoleTPTHighPass.stepFloat as OnePoleTPTHighPass to "build/lean/generated/OnePoleTPTHighPass.hpp"

#generate_cpp DSP.TPTBaseWidth.stepFloat as TPTBaseWidth to "build/lean/generated/TPTBaseWidth.hpp"

#generate_cpp DSP.TPTSVF.stepFloat as TPTSVF to "build/lean/generated/TPTSVF.hpp"

#generate_cpp DSP.MorphingTPTSVF.stepFloat as MorphingTPTSVF to "build/lean/generated/MorphingTPTSVF.hpp"

#generate_cpp DSP.TPTSVF4Pole.stepHighPassFloat as TPTSVF4PoleHighPass to "build/lean/generated/TPTSVF4PoleHighPass.hpp"

#generate_cpp DSP.TPTSVF4Pole.stepLowPassFloat as TPTSVF4PoleLowPass to "build/lean/generated/TPTSVF4PoleLowPass.hpp"

#generate_cpp DSP.TPTSVF4Pole.stepBandPassFloat as TPTSVF4PoleBandPass to "build/lean/generated/TPTSVF4PoleBandPass.hpp"

#generate_cpp DSP.TPTLadder.stepFloat as TPTLadder to "build/lean/generated/TPTLadder.hpp"

#generate_cpp DSP.LinkwitzRileyCrossover.stepFloat as LinkwitzRileyCrossover to "build/lean/generated/LinkwitzRileyCrossover.hpp"

#generate_cpp DSP.Coupled.processorFloat as CoupledProcessor to "build/lean/generated/CoupledProcessor.hpp"
