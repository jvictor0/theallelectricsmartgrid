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
import DSP.Filters.Responses

#use_cpp_record DSP.ResponseFrequency as ResponseFrequencyGeneric from "ResponseFrequency.hpp"

#generate_cpp_filter DSP.OnePoleZOHLowPass.stepFloat
  response DSP.OnePoleZOHLowPass.Snapshot.magnitudeSquaredFloat
  as OnePoleZOHLowPass to "build/lean/generated/OnePoleZOHLowPass.hpp"

#generate_cpp_filter DSP.OnePoleTPTLowPass.stepFloat
  response DSP.OnePoleTPTLowPass.Snapshot.magnitudeSquaredFloat
  as OnePoleTPTLowPass to "build/lean/generated/OnePoleTPTLowPass.hpp"

#generate_cpp_filter DSP.OnePoleTPTHighPass.stepFloat
  response DSP.OnePoleTPTHighPass.Snapshot.magnitudeSquaredFloat
  as OnePoleTPTHighPass to "build/lean/generated/OnePoleTPTHighPass.hpp"

#generate_cpp_filter DSP.TPTBaseWidth.stepFloat
  response DSP.TPTBaseWidth.Snapshot.magnitudeSquaredFloat
  as TPTBaseWidth to "build/lean/generated/TPTBaseWidth.hpp"

#generate_cpp_filter_responses DSP.TPTSVF.stepFloat
  [DSP.TPTSVF.Snapshot.lowPassMagnitudeSquaredFloat as LowPassMagnitudeSquared,
    DSP.TPTSVF.Snapshot.bandPassMagnitudeSquaredFloat as BandPassMagnitudeSquared,
    DSP.TPTSVF.Snapshot.highPassMagnitudeSquaredFloat as HighPassMagnitudeSquared,
    DSP.TPTSVF.Snapshot.notchMagnitudeSquaredFloat as NotchMagnitudeSquared]
  as TPTSVF to "build/lean/generated/TPTSVF.hpp"

#generate_cpp_filter DSP.MorphingTPTSVF.stepFloat
  response DSP.MorphingTPTSVF.Snapshot.magnitudeSquaredFloat
  as MorphingTPTSVF to "build/lean/generated/MorphingTPTSVF.hpp"

#generate_cpp_filter DSP.TPTSVF4Pole.stepHighPassFloat
  response DSP.TPTSVF4Pole.Snapshot.highPassMagnitudeSquaredFloat
  as TPTSVF4PoleHighPass to "build/lean/generated/TPTSVF4PoleHighPass.hpp"

#generate_cpp_filter DSP.TPTSVF4Pole.stepLowPassFloat
  response DSP.TPTSVF4Pole.Snapshot.lowPassMagnitudeSquaredFloat
  as TPTSVF4PoleLowPass to "build/lean/generated/TPTSVF4PoleLowPass.hpp"

#generate_cpp_filter DSP.TPTSVF4Pole.stepBandPassFloat
  response DSP.TPTSVF4Pole.Snapshot.bandPassMagnitudeSquaredFloat
  as TPTSVF4PoleBandPass to "build/lean/generated/TPTSVF4PoleBandPass.hpp"

#generate_cpp_filter DSP.TPTLadder.stepFloat
  response DSP.TPTLadder.Snapshot.magnitudeSquaredFloat
  as TPTLadder to "build/lean/generated/TPTLadder.hpp"

#generate_cpp_filter_responses DSP.LinkwitzRileyCrossover.stepFloat
  [DSP.LinkwitzRileyCrossover.Snapshot.lowPassMagnitudeSquaredFloat as LowPassMagnitudeSquared,
    DSP.LinkwitzRileyCrossover.Snapshot.highPassMagnitudeSquaredFloat as HighPassMagnitudeSquared]
  as LinkwitzRileyCrossover to "build/lean/generated/LinkwitzRileyCrossover.hpp"

#generate_cpp DSP.Coupled.processorFloat as CoupledProcessor to "build/lean/generated/CoupledProcessor.hpp"

#generate_cpp_filter_generic DSP.OnePoleZOHLowPass.step
  response DSP.OnePoleZOHLowPass.Snapshot.magnitudeSquared
  as OnePoleZOHLowPassGeneric to "build/lean/generated/OnePoleZOHLowPassGeneric.hpp"

#generate_cpp_filter_generic DSP.OnePoleTPTLowPass.step
  response DSP.OnePoleTPTLowPass.Snapshot.magnitudeSquared
  as OnePoleTPTLowPassGeneric to "build/lean/generated/OnePoleTPTLowPassGeneric.hpp"

#generate_cpp_filter_generic DSP.OnePoleTPTHighPass.step
  response DSP.OnePoleTPTHighPass.Snapshot.magnitudeSquared
  as OnePoleTPTHighPassGeneric to "build/lean/generated/OnePoleTPTHighPassGeneric.hpp"

#generate_cpp_filter_generic DSP.TPTBaseWidth.step
  response DSP.TPTBaseWidth.Snapshot.magnitudeSquared
  as TPTBaseWidthGeneric to "build/lean/generated/TPTBaseWidthGeneric.hpp"

#generate_cpp_filter_responses_generic DSP.TPTSVF.step
  [DSP.TPTSVF.Snapshot.lowPassMagnitudeSquared as LowPassMagnitudeSquared,
    DSP.TPTSVF.Snapshot.bandPassMagnitudeSquared as BandPassMagnitudeSquared,
    DSP.TPTSVF.Snapshot.highPassMagnitudeSquared as HighPassMagnitudeSquared,
    DSP.TPTSVF.Snapshot.notchMagnitudeSquared as NotchMagnitudeSquared]
  as TPTSVFGeneric to "build/lean/generated/TPTSVFGeneric.hpp"

#generate_cpp_filter_generic DSP.MorphingTPTSVF.step
  response DSP.MorphingTPTSVF.Snapshot.magnitudeSquared
  as MorphingTPTSVFGeneric to "build/lean/generated/MorphingTPTSVFGeneric.hpp"

#generate_cpp_filter_generic DSP.TPTSVF4Pole.stepHighPass
  response DSP.TPTSVF4Pole.Snapshot.highPassMagnitudeSquared
  as TPTSVF4PoleHighPassGeneric to "build/lean/generated/TPTSVF4PoleHighPassGeneric.hpp"

#generate_cpp_filter_generic DSP.TPTSVF4Pole.stepLowPass
  response DSP.TPTSVF4Pole.Snapshot.lowPassMagnitudeSquared
  as TPTSVF4PoleLowPassGeneric to "build/lean/generated/TPTSVF4PoleLowPassGeneric.hpp"

#generate_cpp_filter_generic DSP.TPTSVF4Pole.stepBandPass
  response DSP.TPTSVF4Pole.Snapshot.bandPassMagnitudeSquared
  as TPTSVF4PoleBandPassGeneric to "build/lean/generated/TPTSVF4PoleBandPassGeneric.hpp"

#generate_cpp_filter_generic DSP.TPTLadder.step
  response DSP.TPTLadder.Snapshot.magnitudeSquared
  as TPTLadderGeneric to "build/lean/generated/TPTLadderGeneric.hpp"

#generate_cpp_filter_responses_generic DSP.LinkwitzRileyCrossover.step
  [DSP.LinkwitzRileyCrossover.Snapshot.lowPassMagnitudeSquared as LowPassMagnitudeSquared,
    DSP.LinkwitzRileyCrossover.Snapshot.highPassMagnitudeSquared as HighPassMagnitudeSquared]
  as LinkwitzRileyCrossoverGeneric to "build/lean/generated/LinkwitzRileyCrossoverGeneric.hpp"

#generate_cpp_generic DSP.Coupled.processor as CoupledProcessorGeneric to "build/lean/generated/CoupledProcessorGeneric.hpp"
