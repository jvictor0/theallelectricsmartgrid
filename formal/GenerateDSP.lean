import DSP.Codegen.Cpp
import DSP.Filters.OnePoleZOHLowPass
import DSP.Filters.OnePoleZOHLowPass.Response
import DSP.Filters.OnePoleTPTLowPass
import DSP.Filters.OnePoleTPTHighPass
import DSP.Filters.TPTBaseWidth
import DSP.Filters.TPTSVF
import DSP.Filters.MorphingTPTSVF
import DSP.Filters.TPTSVF4Pole
import DSP.Filters.TPTLadder
import DSP.Filters.LinkwitzRileyCrossover
import DSP.Filters.Coupled

#use_cpp_record DSP.ResponseFrequency as ResponseFrequencyGeneric from "ResponseFrequency.hpp"

#generate_cpp_filter DSP.OnePoleZOHLowPass.stepFloat
  response DSP.OnePoleZOHLowPass.Snapshot.magnitudeSquaredFloat
  as OnePoleZOHLowPass to "build/lean/generated/OnePoleZOHLowPass.hpp"

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

#generate_cpp_filter_generic DSP.OnePoleZOHLowPass.step
  response DSP.OnePoleZOHLowPass.Snapshot.magnitudeSquared
  as OnePoleZOHLowPassGeneric to "build/lean/generated/OnePoleZOHLowPassGeneric.hpp"

#generate_cpp_generic DSP.OnePoleTPTLowPass.step as OnePoleTPTLowPassGeneric to "build/lean/generated/OnePoleTPTLowPassGeneric.hpp"

#generate_cpp_generic DSP.OnePoleTPTHighPass.step as OnePoleTPTHighPassGeneric to "build/lean/generated/OnePoleTPTHighPassGeneric.hpp"

#generate_cpp_generic DSP.TPTBaseWidth.step as TPTBaseWidthGeneric to "build/lean/generated/TPTBaseWidthGeneric.hpp"

#generate_cpp_generic DSP.TPTSVF.step as TPTSVFGeneric to "build/lean/generated/TPTSVFGeneric.hpp"

#generate_cpp_generic DSP.MorphingTPTSVF.step as MorphingTPTSVFGeneric to "build/lean/generated/MorphingTPTSVFGeneric.hpp"

#generate_cpp_generic DSP.TPTSVF4Pole.stepHighPass as TPTSVF4PoleHighPassGeneric to "build/lean/generated/TPTSVF4PoleHighPassGeneric.hpp"

#generate_cpp_generic DSP.TPTSVF4Pole.stepLowPass as TPTSVF4PoleLowPassGeneric to "build/lean/generated/TPTSVF4PoleLowPassGeneric.hpp"

#generate_cpp_generic DSP.TPTSVF4Pole.stepBandPass as TPTSVF4PoleBandPassGeneric to "build/lean/generated/TPTSVF4PoleBandPassGeneric.hpp"

#generate_cpp_generic DSP.TPTLadder.step as TPTLadderGeneric to "build/lean/generated/TPTLadderGeneric.hpp"

#generate_cpp_generic DSP.LinkwitzRileyCrossover.step as LinkwitzRileyCrossoverGeneric to "build/lean/generated/LinkwitzRileyCrossoverGeneric.hpp"

#generate_cpp_generic DSP.Coupled.processor as CoupledProcessorGeneric to "build/lean/generated/CoupledProcessorGeneric.hpp"
