import DSP.Codegen.Cpp
import DSP.Filters.SquiggleBoyQuadReverb.Runtime
import DSP.Resynthesis.Runtime
import DSP.Resynthesis.PVDRStorage
import DSP.Resynthesis.SpectrumStorage
import DSP.Resynthesis.FrameStorage
import DSP.Filters.DelayKernels.Runtime
import DSP.Filters.SchroederAllPass.Storage
import DSP.Filters.OnePoleZOHLowPass
import DSP.Filters.OnePoleZOHLowPass.Response
import DSP.Filters.OnePoleTPTLowPass
import DSP.Filters.OnePoleTPTHighPass
import DSP.Filters.ButterworthDownsampler
import DSP.Filters.TPTBaseWidth
import DSP.Filters.TPTSVF
import DSP.Filters.MorphingTPTSVF
import DSP.Filters.TPTSVF4Pole
import DSP.Filters.TPTLadder
import DSP.Filters.Coupled
import DSP.Filters.Responses
import DSP.Filters.LinkwitzRileySplitter
import DSP.Filters.Capture
import DSP.Filters.NaiveReuppingLadder.Runtime
import DSP.Filters.MultibandSaturator.Runtime
import DSP.Filters.Runtime
import DSP.SmoothRandomLFO
import DSP.WanderingRandomLFO

#generate_cpp_preparation_generic DSP.SmoothRandomLFO.prepare as SmoothRandomLFOPrepareGeneric to "build/lean/generated/SmoothRandomLFOPrepareGeneric.hpp"
#generate_cpp_function_generic DSP.SmoothRandomLFO.evaluate as SmoothRandomLFOCubicGeneric method Evaluate to "build/lean/generated/SmoothRandomLFOCubicGeneric.hpp"

#generate_cpp_record DSP.WanderingRandomLFO.Stream as WanderingRandomLFOState to "build/lean/generated/WanderingRandomLFOState.hpp"
#generate_cpp_record DSP.WanderingRandomLFO.FramedStream as WanderingRandomLFOFramedState to "build/lean/generated/WanderingRandomLFOFramedState.hpp"
#generate_cpp_state DSP.WanderingRandomLFO.initialize32 as WanderingRandomLFOInit32 to "build/lean/generated/WanderingRandomLFOInit32.hpp"
#generate_cpp_state DSP.WanderingRandomLFO.initialize64 as WanderingRandomLFOInit64 to "build/lean/generated/WanderingRandomLFOInit64.hpp"
#generate_cpp_state DSP.WanderingRandomLFO.refresh32 outlined as WanderingRandomLFORefresh32 to "build/lean/generated/WanderingRandomLFORefresh32.hpp"
#generate_cpp_state DSP.WanderingRandomLFO.refresh64 outlined as WanderingRandomLFORefresh64 to "build/lean/generated/WanderingRandomLFORefresh64.hpp"
#generate_cpp_state DSP.WanderingRandomLFO.wrap32 outlined as WanderingRandomLFOWrap32 to "build/lean/generated/WanderingRandomLFOWrap32.hpp"
#generate_cpp_state DSP.WanderingRandomLFO.wrap64 outlined as WanderingRandomLFOWrap64 to "build/lean/generated/WanderingRandomLFOWrap64.hpp"
#generate_cpp_state DSP.WanderingRandomLFO.process32 as WanderingRandomLFO32 to "build/lean/generated/WanderingRandomLFO32.hpp"
#generate_cpp_state DSP.WanderingRandomLFO.process64 as WanderingRandomLFO64 to "build/lean/generated/WanderingRandomLFO64.hpp"
#generate_cpp_state DSP.WanderingRandomLFO.wrapFramed32 outlined as WanderingRandomLFOWrapFramed32 to "build/lean/generated/WanderingRandomLFOWrapFramed32.hpp"
#generate_cpp_state DSP.WanderingRandomLFO.wrapFramed64 outlined as WanderingRandomLFOWrapFramed64 to "build/lean/generated/WanderingRandomLFOWrapFramed64.hpp"
#generate_cpp_state DSP.WanderingRandomLFO.renewFrame32 outlined as WanderingRandomLFORenewFrame32 to "build/lean/generated/WanderingRandomLFORenewFrame32.hpp"
#generate_cpp_state DSP.WanderingRandomLFO.renewFrame64 outlined as WanderingRandomLFORenewFrame64 to "build/lean/generated/WanderingRandomLFORenewFrame64.hpp"
#generate_cpp_state DSP.WanderingRandomLFO.initializeFramed32 as WanderingRandomLFOFramedInit32 to "build/lean/generated/WanderingRandomLFOFramedInit32.hpp"
#generate_cpp_state DSP.WanderingRandomLFO.initializeFramed64 as WanderingRandomLFOFramedInit64 to "build/lean/generated/WanderingRandomLFOFramedInit64.hpp"
#generate_cpp_state DSP.WanderingRandomLFO.processFramed32 as WanderingRandomLFOFramed32 to "build/lean/generated/WanderingRandomLFOFramed32.hpp"
#generate_cpp_state DSP.WanderingRandomLFO.processFramed64 as WanderingRandomLFOFramed64 to "build/lean/generated/WanderingRandomLFOFramed64.hpp"

#generate_cpp_state_generic DSP.SquiggleBoyQuadReverb.Runtime.processFramed owning as SquiggleBoyQuadReverbFramed to "build/lean/generated/SquiggleBoyQuadReverbFramed.hpp"
#generate_cpp_state_generic DSP.SquiggleBoyQuadReverb.Runtime.processSample owning as SquiggleBoyQuadReverbEverySample to "build/lean/generated/SquiggleBoyQuadReverbEverySample.hpp"
#generate_cpp_state_generic DSP.SquiggleBoyQuadReverb.Runtime.initializeFramed as SquiggleBoyQuadReverbInitFramed to "build/lean/generated/SquiggleBoyQuadReverbInitFramed.hpp"
#generate_cpp_state_generic DSP.SquiggleBoyQuadReverb.Runtime.initializeSample as SquiggleBoyQuadReverbInitEverySample to "build/lean/generated/SquiggleBoyQuadReverbInitEverySample.hpp"
#generate_cpp_state_generic DSP.SquiggleBoyQuadReverb.Runtime.tuneFramed as SquiggleBoyQuadReverbTuning to "build/lean/generated/SquiggleBoyQuadReverbTuning.hpp"
#generate_cpp_effect DSP.Resynthesis.PVDR.Storage.analyze32 as AnalyzePVDR32 to "build/lean/generated/AnalyzePVDR32.hpp"
#generate_cpp_effect DSP.Resynthesis.PVDR.Storage.analyze64 as AnalyzePVDR64 to "build/lean/generated/AnalyzePVDR64.hpp"
#generate_cpp_effect DSP.Resynthesis.SpectrumStorage.synthesize32 as SynthesizeSpectrum32 to "build/lean/generated/SynthesizeSpectrum32.hpp"
#generate_cpp_effect DSP.Resynthesis.SpectrumStorage.synthesize64 as SynthesizeSpectrum64 to "build/lean/generated/SynthesizeSpectrum64.hpp"
#generate_cpp_effect DSP.Resynthesis.FrameStorage.run32 as ResynthesisFrame32 to "build/lean/generated/ResynthesisFrame32.hpp"
#generate_cpp_effect DSP.Resynthesis.FrameStorage.run64 as ResynthesisFrame64 to "build/lean/generated/ResynthesisFrame64.hpp"
#generate_cpp_view DSP.Resynthesis.Runtime.oscillatorControl32 as GrainControl32 to "build/lean/generated/GrainControl32.hpp"
#generate_cpp_view DSP.Resynthesis.Runtime.unisonBase32 as UnisonBase32 to "build/lean/generated/UnisonBase32.hpp"
#generate_cpp_view DSP.Resynthesis.Runtime.unisonBase64 as UnisonBase64 to "build/lean/generated/UnisonBase64.hpp"
#generate_cpp_generic DSP.DelayKernels.Runtime.AllPass.process as SchroederAllPassCoreGeneric to "build/lean/generated/SchroederAllPassCoreGeneric.hpp"
#generate_cpp_generic DSP.DelayKernels.Runtime.Comb.process as TunedDecayCombCoreGeneric to "build/lean/generated/TunedDecayCombCoreGeneric.hpp"
#generate_cpp_preparation_generic DSP.DelayKernels.Runtime.prepareCoefficients as TunedDecayCombTuningGeneric to "build/lean/generated/TunedDecayCombTuningGeneric.hpp"
#generate_cpp_generic DSP.DelayKernels.response as TunedDecayCombResponseGeneric to "build/lean/generated/TunedDecayCombResponseGeneric.hpp"

#generate_cpp_generic DSP.ButterworthDownsampler.process2 resetWith DSP.ButterworthDownsampler.reset2
  as ButterworthDownsampler2Generic to "build/lean/generated/ButterworthDownsampler2Generic.hpp"
#generate_cpp_generic DSP.ButterworthDownsampler.process4 resetWith DSP.ButterworthDownsampler.reset4
  as ButterworthDownsampler4Generic to "build/lean/generated/ButterworthDownsampler4Generic.hpp"
#generate_cpp_generic DSP.ButterworthDownsampler.process8 resetWith DSP.ButterworthDownsampler.reset8
  as ButterworthDownsampler8Generic to "build/lean/generated/ButterworthDownsampler8Generic.hpp"
#generate_cpp_preparation DSP.ButterworthDownsampler.prepare2Float
  as ButterworthDownsampler2Prepare to "build/lean/generated/ButterworthDownsampler2Prepare.hpp"
#generate_cpp_preparation DSP.ButterworthDownsampler.prepare4Float
  as ButterworthDownsampler4Prepare to "build/lean/generated/ButterworthDownsampler4Prepare.hpp"
#generate_cpp_preparation DSP.ButterworthDownsampler.prepare8Float
  as ButterworthDownsampler8Prepare to "build/lean/generated/ButterworthDownsampler8Prepare.hpp"

#generate_cpp_sampled_processor_generic DSP.NaiveReuppingLadder.Runtime.process DSP.NaiveReuppingLadder.Runtime.prepareCoefficients
  resetWith DSP.NaiveReuppingLadder.Runtime.reset
  as NaiveReuppingLadderCoreGeneric to "build/lean/generated/NaiveReuppingLadderCoreGeneric.hpp"

#generate_cpp_sampled_filter DSP.OnePoleZOHLowPass.stepFloat DSP.OnePoleZOHLowPass.prepareCoefficientsFloat
  [DSP.OnePoleZOHLowPass.Snapshot.magnitudeSquaredFloat as MagnitudeSquared]
  as OnePoleZOHLowPass to "build/lean/generated/OnePoleZOHLowPass.hpp"

#generate_cpp_sampled_filter DSP.OnePoleTPTLowPass.Runtime.processFloat DSP.OnePoleTPTLowPass.Runtime.prepareCoefficientsFloat
  [DSP.OnePoleTPTLowPass.Snapshot.magnitudeSquaredFloat as MagnitudeSquared]
  as OnePoleTPTLowPass to "build/lean/generated/OnePoleTPTLowPass.hpp"

#generate_cpp_sampled_filter DSP.OnePoleTPTHighPass.Runtime.processFloat DSP.OnePoleTPTHighPass.Runtime.prepareCoefficientsFloat
  [DSP.OnePoleTPTHighPass.Snapshot.magnitudeSquaredFloat as MagnitudeSquared]
  as OnePoleTPTHighPass to "build/lean/generated/OnePoleTPTHighPass.hpp"

#generate_cpp_sampled_filter DSP.TPTBaseWidth.Runtime.processFloat DSP.TPTBaseWidth.Runtime.prepareCoefficientsFloat
  [DSP.TPTBaseWidth.Snapshot.magnitudeSquaredFloat as MagnitudeSquared]
  as TPTBaseWidth to "build/lean/generated/TPTBaseWidth.hpp"

#generate_cpp_sampled_filter DSP.TPTSVF.Runtime.processFloat DSP.TPTSVF.Runtime.prepareCoefficientsFloat
  prepared DSP.TPTSVF.Runtime.prepareFromSnapshotFloat
  [DSP.TPTSVF.Snapshot.lowPassMagnitudeSquaredFloat as LowPassMagnitudeSquared,
    DSP.TPTSVF.Snapshot.bandPassMagnitudeSquaredFloat as BandPassMagnitudeSquared,
    DSP.TPTSVF.Snapshot.highPassMagnitudeSquaredFloat as HighPassMagnitudeSquared,
    DSP.TPTSVF.Snapshot.notchMagnitudeSquaredFloat as NotchMagnitudeSquared,
    DSP.TPTSVF.Snapshot.allPassMagnitudeSquaredFloat as AllPassMagnitudeSquared]
  as TPTSVF to "build/lean/generated/TPTSVF.hpp"

#generate_cpp_sampled_filter DSP.MorphingTPTSVF.Runtime.processFloat DSP.MorphingTPTSVF.Runtime.prepareCoefficientsFloat
  [DSP.MorphingTPTSVF.Snapshot.magnitudeSquaredFloat as MagnitudeSquared]
  as MorphingTPTSVF to "build/lean/generated/MorphingTPTSVF.hpp"

#generate_cpp_sampled_filter DSP.TPTSVF4Pole.Runtime.processHighPassFloat DSP.TPTSVF4Pole.Runtime.prepareCoefficientsFloat
  [DSP.TPTSVF4Pole.Snapshot.highPassMagnitudeSquaredFloat as MagnitudeSquared]
  as TPTSVF4PoleHighPass to "build/lean/generated/TPTSVF4PoleHighPass.hpp"

#generate_cpp_sampled_filter DSP.TPTSVF4Pole.Runtime.processLowPassFloat DSP.TPTSVF4Pole.Runtime.prepareCoefficientsFloat
  [DSP.TPTSVF4Pole.Snapshot.lowPassMagnitudeSquaredFloat as MagnitudeSquared]
  as TPTSVF4PoleLowPass to "build/lean/generated/TPTSVF4PoleLowPass.hpp"

#generate_cpp_sampled_filter DSP.TPTSVF4Pole.Runtime.processBandPassFloat DSP.TPTSVF4Pole.Runtime.prepareCoefficientsFloat
  [DSP.TPTSVF4Pole.Snapshot.bandPassMagnitudeSquaredFloat as MagnitudeSquared]
  as TPTSVF4PoleBandPass to "build/lean/generated/TPTSVF4PoleBandPass.hpp"

#generate_cpp_sampled_filter DSP.TPTLadder.Runtime.processFloat DSP.TPTLadder.Runtime.prepareCoefficientsFloat
  [DSP.TPTLadder.Snapshot.magnitudeSquaredFloat as MagnitudeSquared]
  as TPTLadder to "build/lean/generated/TPTLadder.hpp"

#generate_cpp DSP.Coupled.processorFloat as CoupledProcessor to "build/lean/generated/CoupledProcessor.hpp"

#generate_cpp_sampled_filter_generic DSP.OnePoleZOHLowPass.step DSP.OnePoleZOHLowPass.prepareCoefficients
  [DSP.OnePoleZOHLowPass.Snapshot.magnitudeSquared as MagnitudeSquared]
  as OnePoleZOHLowPassGeneric to "build/lean/generated/OnePoleZOHLowPassGeneric.hpp"

#generate_cpp_sampled_filter_generic DSP.OnePoleTPTLowPass.Runtime.process DSP.OnePoleTPTLowPass.Runtime.prepareCoefficients
  [DSP.OnePoleTPTLowPass.Snapshot.magnitudeSquared as MagnitudeSquared]
  as OnePoleTPTLowPassGeneric to "build/lean/generated/OnePoleTPTLowPassGeneric.hpp"

#generate_cpp_sampled_filter_generic DSP.OnePoleTPTHighPass.Runtime.process DSP.OnePoleTPTHighPass.Runtime.prepareCoefficients
  [DSP.OnePoleTPTHighPass.Snapshot.magnitudeSquared as MagnitudeSquared]
  as OnePoleTPTHighPassGeneric to "build/lean/generated/OnePoleTPTHighPassGeneric.hpp"

#generate_cpp_sampled_filter_generic DSP.TPTBaseWidth.Runtime.process DSP.TPTBaseWidth.Runtime.prepareCoefficients
  [DSP.TPTBaseWidth.Snapshot.magnitudeSquared as MagnitudeSquared]
  as TPTBaseWidthGeneric to "build/lean/generated/TPTBaseWidthGeneric.hpp"

#generate_cpp_sampled_filter_generic DSP.TPTSVF.Runtime.process DSP.TPTSVF.Runtime.prepareCoefficients
  prepared DSP.TPTSVF.Runtime.prepareFromSnapshot
  [DSP.TPTSVF.Snapshot.lowPassMagnitudeSquared as LowPassMagnitudeSquared,
    DSP.TPTSVF.Snapshot.bandPassMagnitudeSquared as BandPassMagnitudeSquared,
    DSP.TPTSVF.Snapshot.highPassMagnitudeSquared as HighPassMagnitudeSquared,
    DSP.TPTSVF.Snapshot.notchMagnitudeSquared as NotchMagnitudeSquared,
    DSP.TPTSVF.Snapshot.allPassMagnitudeSquared as AllPassMagnitudeSquared]
  as TPTSVFGeneric to "build/lean/generated/TPTSVFGeneric.hpp"

#generate_cpp_sampled_filter_generic DSP.MorphingTPTSVF.Runtime.process DSP.MorphingTPTSVF.Runtime.prepareCoefficients
  [DSP.MorphingTPTSVF.Snapshot.magnitudeSquared as MagnitudeSquared]
  as MorphingTPTSVFGeneric to "build/lean/generated/MorphingTPTSVFGeneric.hpp"

#generate_cpp_sampled_filter_generic DSP.TPTSVF4Pole.Runtime.processHighPass DSP.TPTSVF4Pole.Runtime.prepareCoefficients
  [DSP.TPTSVF4Pole.Snapshot.highPassMagnitudeSquared as MagnitudeSquared]
  as TPTSVF4PoleHighPassGeneric to "build/lean/generated/TPTSVF4PoleHighPassGeneric.hpp"

#generate_cpp_sampled_filter_generic DSP.TPTSVF4Pole.Runtime.processLowPass DSP.TPTSVF4Pole.Runtime.prepareCoefficients
  [DSP.TPTSVF4Pole.Snapshot.lowPassMagnitudeSquared as MagnitudeSquared]
  as TPTSVF4PoleLowPassGeneric to "build/lean/generated/TPTSVF4PoleLowPassGeneric.hpp"

#generate_cpp_sampled_filter_generic DSP.TPTSVF4Pole.Runtime.processBandPass DSP.TPTSVF4Pole.Runtime.prepareCoefficients
  [DSP.TPTSVF4Pole.Snapshot.bandPassMagnitudeSquared as MagnitudeSquared]
  as TPTSVF4PoleBandPassGeneric to "build/lean/generated/TPTSVF4PoleBandPassGeneric.hpp"

#generate_cpp_sampled_filter_generic DSP.TPTLadder.Runtime.process DSP.TPTLadder.Runtime.prepareCoefficients
  [DSP.TPTLadder.Snapshot.magnitudeSquared as MagnitudeSquared]
  as TPTLadderGeneric to "build/lean/generated/TPTLadderGeneric.hpp"

#generate_cpp_generic DSP.Coupled.processor as CoupledProcessorGeneric to "build/lean/generated/CoupledProcessorGeneric.hpp"

#generate_cpp_tuned_snapshot_generic DSP.LinkwitzRileySplitter.Runtime.process4Band
  DSP.LinkwitzRileySplitter.Runtime.computeCoefficients4Band DSP.LinkwitzRileySplitter.makeSnapshot4Band
  as LinkwitzRileySplitter4BandGeneric to "build/lean/generated/LinkwitzRileySplitter4BandGeneric.hpp"

#generate_cpp_sampled_filter_generic DSP.MultibandSaturator.Runtime.process4Band DSP.MultibandSaturator.Runtime.prepare4Band
  resetWith DSP.MultibandSaturator.Runtime.reset4Band
  preparedBank DSP.MultibandSaturator.Runtime.install4Band prepareWith DSP.LinkwitzRileySplitter.prepareSection
  [DSP.MultibandSaturator.magnitudeSquared4Band as MagnitudeSquared]
  as MultibandSaturator4BandCoreGeneric to "build/lean/generated/MultibandSaturator4BandCoreGeneric.hpp"

#generate_cpp_sampled_filter DSP.TPTSVF.Runtime.processLowPassFloat DSP.TPTSVF.Runtime.prepareCoefficientsFloat
  prepared DSP.TPTSVF.Runtime.prepareFromSnapshotFloat
  [DSP.TPTSVF.Snapshot.lowPassMagnitudeSquaredFloat as MagnitudeSquared]
  as TPTSVFLowPass to "build/lean/generated/TPTSVFLowPass.hpp"

#generate_cpp_sampled_filter_generic DSP.TPTSVF.Runtime.processLowPass DSP.TPTSVF.Runtime.prepareCoefficients
  prepared DSP.TPTSVF.Runtime.prepareFromSnapshot
  [DSP.TPTSVF.Snapshot.lowPassMagnitudeSquared as MagnitudeSquared]
  as TPTSVFLowPassGeneric to "build/lean/generated/TPTSVFLowPassGeneric.hpp"

#generate_cpp_sampled_filter DSP.TPTSVF.Runtime.processHighPassFloat DSP.TPTSVF.Runtime.prepareCoefficientsFloat
  prepared DSP.TPTSVF.Runtime.prepareFromSnapshotFloat
  [DSP.TPTSVF.Snapshot.highPassMagnitudeSquaredFloat as MagnitudeSquared]
  as TPTSVFHighPass to "build/lean/generated/TPTSVFHighPass.hpp"

#generate_cpp_sampled_filter_generic DSP.TPTSVF.Runtime.processHighPass DSP.TPTSVF.Runtime.prepareCoefficients
  prepared DSP.TPTSVF.Runtime.prepareFromSnapshot
  [DSP.TPTSVF.Snapshot.highPassMagnitudeSquared as MagnitudeSquared]
  as TPTSVFHighPassGeneric to "build/lean/generated/TPTSVFHighPassGeneric.hpp"

#generate_cpp_sampled_filter DSP.TPTSVF.Runtime.processBandPassFloat DSP.TPTSVF.Runtime.prepareCoefficientsFloat
  prepared DSP.TPTSVF.Runtime.prepareFromSnapshotFloat
  [DSP.TPTSVF.Snapshot.bandPassMagnitudeSquaredFloat as MagnitudeSquared]
  as TPTSVFBandPass to "build/lean/generated/TPTSVFBandPass.hpp"

#generate_cpp_sampled_filter_generic DSP.TPTSVF.Runtime.processBandPass DSP.TPTSVF.Runtime.prepareCoefficients
  prepared DSP.TPTSVF.Runtime.prepareFromSnapshot
  [DSP.TPTSVF.Snapshot.bandPassMagnitudeSquared as MagnitudeSquared]
  as TPTSVFBandPassGeneric to "build/lean/generated/TPTSVFBandPassGeneric.hpp"

#generate_cpp_sampled_filter DSP.TPTSVF.Runtime.processNotchFloat DSP.TPTSVF.Runtime.prepareCoefficientsFloat
  prepared DSP.TPTSVF.Runtime.prepareFromSnapshotFloat
  [DSP.TPTSVF.Snapshot.notchMagnitudeSquaredFloat as MagnitudeSquared]
  as TPTSVFNotch to "build/lean/generated/TPTSVFNotch.hpp"

#generate_cpp_sampled_filter_generic DSP.TPTSVF.Runtime.processNotch DSP.TPTSVF.Runtime.prepareCoefficients
  prepared DSP.TPTSVF.Runtime.prepareFromSnapshot
  [DSP.TPTSVF.Snapshot.notchMagnitudeSquared as MagnitudeSquared]
  as TPTSVFNotchGeneric to "build/lean/generated/TPTSVFNotchGeneric.hpp"

#generate_cpp_sampled_filter DSP.TPTSVF.Runtime.processAllPassFloat DSP.TPTSVF.Runtime.prepareCoefficientsFloat
  prepared DSP.TPTSVF.Runtime.prepareFromSnapshotFloat
  [DSP.TPTSVF.Snapshot.allPassMagnitudeSquaredFloat as MagnitudeSquared]
  as TPTSVFAllPass to "build/lean/generated/TPTSVFAllPass.hpp"

#generate_cpp_sampled_filter_generic DSP.TPTSVF.Runtime.processAllPass DSP.TPTSVF.Runtime.prepareCoefficients
  prepared DSP.TPTSVF.Runtime.prepareFromSnapshot
  [DSP.TPTSVF.Snapshot.allPassMagnitudeSquared as MagnitudeSquared]
  as TPTSVFAllPassGeneric to "build/lean/generated/TPTSVFAllPassGeneric.hpp"
