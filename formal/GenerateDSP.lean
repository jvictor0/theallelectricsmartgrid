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
import DSP.Filters.Coupled
import DSP.Filters.Responses
import DSP.Filters.LinkwitzRileySplitter
import DSP.Filters.Capture

#use_cpp_record DSP.ResponseFrequency as ResponseFrequencyGeneric from "ResponseFrequency.hpp"

#generate_cpp_sampled_filter DSP.OnePoleZOHLowPass.stepFloat DSP.OnePoleZOHLowPass.prepareCoefficientsFloat
  [DSP.OnePoleZOHLowPass.Snapshot.magnitudeSquaredFloat as MagnitudeSquared]
  as OnePoleZOHLowPass to "build/lean/generated/OnePoleZOHLowPass.hpp"

#generate_cpp_sampled_filter DSP.OnePoleTPTLowPass.stepFloat DSP.OnePoleTPTLowPass.prepareCoefficientsFloat
  [DSP.OnePoleTPTLowPass.Snapshot.magnitudeSquaredFloat as MagnitudeSquared]
  as OnePoleTPTLowPass to "build/lean/generated/OnePoleTPTLowPass.hpp"

#generate_cpp_sampled_filter DSP.OnePoleTPTHighPass.stepFloat DSP.OnePoleTPTHighPass.prepareCoefficientsFloat
  [DSP.OnePoleTPTHighPass.Snapshot.magnitudeSquaredFloat as MagnitudeSquared]
  as OnePoleTPTHighPass to "build/lean/generated/OnePoleTPTHighPass.hpp"

#generate_cpp_sampled_filter DSP.TPTBaseWidth.stepFloat DSP.TPTBaseWidth.prepareCoefficientsFloat
  [DSP.TPTBaseWidth.Snapshot.magnitudeSquaredFloat as MagnitudeSquared]
  as TPTBaseWidth to "build/lean/generated/TPTBaseWidth.hpp"

#generate_cpp_sampled_filter DSP.TPTSVF.stepFloat DSP.TPTSVF.prepareCoefficientsFloat
  prepared DSP.TPTSVF.prepareFromSnapshotFloat
  [DSP.TPTSVF.Snapshot.lowPassMagnitudeSquaredFloat as LowPassMagnitudeSquared,
    DSP.TPTSVF.Snapshot.bandPassMagnitudeSquaredFloat as BandPassMagnitudeSquared,
    DSP.TPTSVF.Snapshot.highPassMagnitudeSquaredFloat as HighPassMagnitudeSquared,
    DSP.TPTSVF.Snapshot.notchMagnitudeSquaredFloat as NotchMagnitudeSquared,
    DSP.TPTSVF.Snapshot.allPassMagnitudeSquaredFloat as AllPassMagnitudeSquared]
  as TPTSVF to "build/lean/generated/TPTSVF.hpp"

#generate_cpp_sampled_filter DSP.MorphingTPTSVF.stepFloat DSP.MorphingTPTSVF.prepareCoefficientsFloat
  [DSP.MorphingTPTSVF.Snapshot.magnitudeSquaredFloat as MagnitudeSquared]
  as MorphingTPTSVF to "build/lean/generated/MorphingTPTSVF.hpp"

#generate_cpp_sampled_filter DSP.TPTSVF4Pole.stepHighPassFloat DSP.TPTSVF4Pole.prepareCoefficientsFloat
  [DSP.TPTSVF4Pole.Snapshot.highPassMagnitudeSquaredFloat as MagnitudeSquared]
  as TPTSVF4PoleHighPass to "build/lean/generated/TPTSVF4PoleHighPass.hpp"

#generate_cpp_sampled_filter DSP.TPTSVF4Pole.stepLowPassFloat DSP.TPTSVF4Pole.prepareCoefficientsFloat
  [DSP.TPTSVF4Pole.Snapshot.lowPassMagnitudeSquaredFloat as MagnitudeSquared]
  as TPTSVF4PoleLowPass to "build/lean/generated/TPTSVF4PoleLowPass.hpp"

#generate_cpp_sampled_filter DSP.TPTSVF4Pole.stepBandPassFloat DSP.TPTSVF4Pole.prepareCoefficientsFloat
  [DSP.TPTSVF4Pole.Snapshot.bandPassMagnitudeSquaredFloat as MagnitudeSquared]
  as TPTSVF4PoleBandPass to "build/lean/generated/TPTSVF4PoleBandPass.hpp"

#generate_cpp_sampled_filter DSP.TPTLadder.stepFloat DSP.TPTLadder.prepareCoefficientsFloat
  [DSP.TPTLadder.Snapshot.magnitudeSquaredFloat as MagnitudeSquared]
  as TPTLadder to "build/lean/generated/TPTLadder.hpp"

#generate_cpp DSP.Coupled.processorFloat as CoupledProcessor to "build/lean/generated/CoupledProcessor.hpp"

#generate_cpp_sampled_filter_generic DSP.OnePoleZOHLowPass.step DSP.OnePoleZOHLowPass.prepareCoefficients
  [DSP.OnePoleZOHLowPass.Snapshot.magnitudeSquared as MagnitudeSquared]
  as OnePoleZOHLowPassGeneric to "build/lean/generated/OnePoleZOHLowPassGeneric.hpp"

#generate_cpp_sampled_filter_generic DSP.OnePoleTPTLowPass.step DSP.OnePoleTPTLowPass.prepareCoefficients
  [DSP.OnePoleTPTLowPass.Snapshot.magnitudeSquared as MagnitudeSquared]
  as OnePoleTPTLowPassGeneric to "build/lean/generated/OnePoleTPTLowPassGeneric.hpp"

#generate_cpp_sampled_filter_generic DSP.OnePoleTPTHighPass.step DSP.OnePoleTPTHighPass.prepareCoefficients
  [DSP.OnePoleTPTHighPass.Snapshot.magnitudeSquared as MagnitudeSquared]
  as OnePoleTPTHighPassGeneric to "build/lean/generated/OnePoleTPTHighPassGeneric.hpp"

#generate_cpp_sampled_filter_generic DSP.TPTBaseWidth.step DSP.TPTBaseWidth.prepareCoefficients
  [DSP.TPTBaseWidth.Snapshot.magnitudeSquared as MagnitudeSquared]
  as TPTBaseWidthGeneric to "build/lean/generated/TPTBaseWidthGeneric.hpp"

#generate_cpp_sampled_filter_generic DSP.TPTSVF.step DSP.TPTSVF.prepareCoefficients
  prepared DSP.TPTSVF.prepareFromSnapshot
  [DSP.TPTSVF.Snapshot.lowPassMagnitudeSquared as LowPassMagnitudeSquared,
    DSP.TPTSVF.Snapshot.bandPassMagnitudeSquared as BandPassMagnitudeSquared,
    DSP.TPTSVF.Snapshot.highPassMagnitudeSquared as HighPassMagnitudeSquared,
    DSP.TPTSVF.Snapshot.notchMagnitudeSquared as NotchMagnitudeSquared,
    DSP.TPTSVF.Snapshot.allPassMagnitudeSquared as AllPassMagnitudeSquared]
  as TPTSVFGeneric to "build/lean/generated/TPTSVFGeneric.hpp"

#generate_cpp_sampled_filter_generic DSP.MorphingTPTSVF.step DSP.MorphingTPTSVF.prepareCoefficients
  [DSP.MorphingTPTSVF.Snapshot.magnitudeSquared as MagnitudeSquared]
  as MorphingTPTSVFGeneric to "build/lean/generated/MorphingTPTSVFGeneric.hpp"

#generate_cpp_sampled_filter_generic DSP.TPTSVF4Pole.stepHighPass DSP.TPTSVF4Pole.prepareCoefficients
  [DSP.TPTSVF4Pole.Snapshot.highPassMagnitudeSquared as MagnitudeSquared]
  as TPTSVF4PoleHighPassGeneric to "build/lean/generated/TPTSVF4PoleHighPassGeneric.hpp"

#generate_cpp_sampled_filter_generic DSP.TPTSVF4Pole.stepLowPass DSP.TPTSVF4Pole.prepareCoefficients
  [DSP.TPTSVF4Pole.Snapshot.lowPassMagnitudeSquared as MagnitudeSquared]
  as TPTSVF4PoleLowPassGeneric to "build/lean/generated/TPTSVF4PoleLowPassGeneric.hpp"

#generate_cpp_sampled_filter_generic DSP.TPTSVF4Pole.stepBandPass DSP.TPTSVF4Pole.prepareCoefficients
  [DSP.TPTSVF4Pole.Snapshot.bandPassMagnitudeSquared as MagnitudeSquared]
  as TPTSVF4PoleBandPassGeneric to "build/lean/generated/TPTSVF4PoleBandPassGeneric.hpp"

#generate_cpp_sampled_filter_generic DSP.TPTLadder.step DSP.TPTLadder.prepareCoefficients
  [DSP.TPTLadder.Snapshot.magnitudeSquared as MagnitudeSquared]
  as TPTLadderGeneric to "build/lean/generated/TPTLadderGeneric.hpp"

#generate_cpp_generic DSP.Coupled.processor as CoupledProcessorGeneric to "build/lean/generated/CoupledProcessorGeneric.hpp"

#generate_cpp_tuned_snapshot_generic DSP.LinkwitzRileySplitter.step4Band
  DSP.LinkwitzRileySplitter.computeCoefficients4Band DSP.LinkwitzRileySplitter.makeSnapshot4Band
  as LinkwitzRileySplitter4BandGeneric to "build/lean/generated/LinkwitzRileySplitter4BandGeneric.hpp"
