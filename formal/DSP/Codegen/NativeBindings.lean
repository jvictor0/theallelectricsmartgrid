import Lean
import DSP.ChannelMean
import DSP.DelayBuffer.Quad
import DSP.DelayMath
import DSP.Nonlinear
import DSP.PolyXFader

namespace DSP.Codegen

open Lean

/-- A native implementation of a `StateM` storage or numeric primitive.
The method acts on the single borrowed state and returns its observation. -/
structure StateBinding where
  source : Name
  method : String
  arguments : Nat
  generated : Option (Name × Name) := none

/-- Template parameters for a native storage representation of packed samples. -/
inductive StorageParameter where
  | number
  | channels
  | natural (argument : Nat)
  | boolean (value : Bool)

structure StorageBinding where
  source : Name
  cppName : String
  header : String
  sampleArgument : Nat
  parameters : Array StorageParameter

structure RecordExport where
  cppName : String
  header : String

inductive NumericLowering where
  | trig (method : String)
  | expDecay
  | exp2
  | floor
  | phaseTurns
  | decay
  | tanh
  | sqrt
  | channelMean
  | ifLess
  | fma
  | libm (method result : String)
  | curvePair (header method : String)

inductive PackedStorageLowering where
  | sampleWord (fromWord : Bool)
  | sampleAges
  | packAges
  | broadcastLane
  | gather

inductive PackedArithmeticLowering where
  | splat
  | binary (operator : String)
  | neg
  | member (method : String)
  | method (name : String)
  | convert (number : String)
  | curvePair (header method : String)

inductive ValueLowering where
  | numeric (operation : NumericLowering)
  | packedStorage (operation : PackedStorageLowering)
  | packedArithmetic (operation : PackedArithmeticLowering) (arguments : Nat)
      (instances : Option (Name × Name))

inductive NativeBinding where
  | action (binding : StateBinding)
  | storage (binding : StorageBinding)
  | record (source : Name) (target : RecordExport)
  | value (source : Name) (lowering : ValueLowering)
  /-- Correspond to the fields of an arithmetic class in declaration order. -/
  | arithmetic (source : Name) (fields : Array NumericLowering)

private def native (source : Name) (method : String) (arguments : Nat) : NativeBinding :=
  .action ⟨source, method, arguments, none⟩

private def numeric (source : Name) (operation : NumericLowering) : NativeBinding :=
  .value source (.numeric operation)

/-- The optional pair names the standard Float and Float32 instances,
respectively, required by the native operation. -/
private def packed (source : Name) (operation : PackedArithmeticLowering) (arguments : Nat)
    (instances : Option (Name × Name) := none) : NativeBinding :=
  .value source (.packedArithmetic operation arguments instances)

/-- The native storage and numeric allowlist. Actions marked generated must
resolve to two emitted Lean procedures, one per sample precision. -/
def nativeBindings : Array NativeBinding := #[
  .arithmetic ``Trig #[.trig "Sin2pi", .trig "Cos2pi", .trig "TanPi"],
  .arithmetic ``ExpDecay #[.expDecay],
  .arithmetic ``DelayMath #[.exp2, .floor, .phaseTurns, .decay],
  .arithmetic ``Tanh #[.tanh],
  .arithmetic ``SquareRoot #[.sqrt],
  .arithmetic ``ChannelMean #[.channelMean],
  .arithmetic ``IfLess #[.ifLess],
  .arithmetic ``FMA #[.fma],
  numeric ``Trig.sinFloat (.trig "Sin2pi"),
  numeric ``Trig.sinFloat32 (.trig "Sin2pi"),
  numeric ``Trig.cosFloat (.trig "Cos2pi"),
  numeric ``Trig.cosFloat32 (.trig "Cos2pi"),
  numeric ``Trig.tanFloat (.trig "TanPi"),
  numeric ``Trig.tanFloat32 (.trig "TanPi"),
  numeric ``ExpDecay.float .expDecay,
  numeric ``ExpDecay.float32 .expDecay,
  numeric ``DelayMath.exp2Float .exp2,
  numeric ``DelayMath.exp2Float32 .exp2,
  numeric ``DelayMath.floorFloat .floor,
  numeric ``DelayMath.floorFloat32 .floor,
  numeric ``DelayMath.phaseFloat .phaseTurns,
  numeric ``DelayMath.phaseFloat32 .phaseTurns,
  numeric ``DelayMath.decayFloat .decay,
  numeric ``DelayMath.decayFloat32 .decay,
  numeric ``FMA.float .fma,
  numeric ``FMA.float32 .fma,
  numeric ``Float.sqrt .sqrt,
  numeric ``Float32.sqrt .sqrt,
  numeric ``Float.floor (.libm "floor" "double"),
  numeric ``Float32.floor (.libm "floor" "float"),
  numeric ``DSP.PolyXFader.curvesFloat32 (.curvePair "PolyXFaderTables.hpp" "PolyXFaderTables::Evaluate"),
  packed ``DSP.Packed.splat .splat 1,
  packed ``DSP.Packed.add (.binary "+") 2 (some (``instAddFloat, ``instAddFloat32)),
  packed ``DSP.Packed.sub (.binary "-") 2 (some (``instSubFloat, ``instSubFloat32)),
  packed ``DSP.Packed.mul (.binary "*") 2 (some (``instMulFloat, ``instMulFloat32)),
  packed ``DSP.Packed.div (.binary "/") 2 (some (``instDivFloat, ``instDivFloat32)),
  packed ``DSP.Packed.neg .neg 1 (some (``instNegFloat, ``instNegFloat32)),
  packed ``DSP.Packed.anyDifferent (.binary "!=") 2 (some (``instBEqFloat, ``instBEqFloat32)),
  packed ``DSP.Packed.abs32 (.member "Abs") 1,
  packed ``DSP.Packed.floor32 (.member "Floor") 1,
  packed ``DSP.Packed.floor64 (.member "Floor") 1,
  packed ``DSP.Packed.widen (.convert "double") 1,
  packed ``DSP.Packed.narrow (.convert "float") 1,
  packed ``DSP.Packed.ifLess (.method "IfLess") 4 (some (``DSP.instIfLessFloat, ``DSP.instIfLessFloat32)),
  packed ``DSP.Packed.fma (.method "FusedMultiplyAdd") 3 (some (``DSP.instFMAFloat, ``DSP.instFMAFloat32)),
  packed ``DSP.PolyXFader.curvesPacked (.curvePair "PolyXFaderTables.hpp" "PolyXFaderTables::Evaluate") 1,
  numeric ``Float.ceil (.libm "ceil" "double"),
  numeric ``Float.isFinite (.libm "isfinite" "bool"),
  numeric ``Float.exp (.libm "exp" "double"),
  numeric ``Float.log (.libm "log" "double"),
  numeric ``Float.cos (.libm "cos" "double"),
  numeric ``Float.tan (.libm "tan" "double"),
  .value ``DelayBuffer.SampleIndex.ofWord (.packedStorage (.sampleWord true)),
  .value ``DelayBuffer.SampleIndex.toWord (.packedStorage (.sampleWord false)),
  .value ``DelayBuffer.SampleAges.toAges (.packedStorage .sampleAges),
  .value ``DelayBuffer.QuadAges.pack (.packedStorage .packAges),
  .value ``DelayBuffer.BroadcastLane.lane (.packedStorage .broadcastLane),
  .value ``DelayBuffer.Gather.gather (.packedStorage .gather),
  .record `DSP.ResponseFrequency ⟨"ResponseFrequencyGeneric", "ResponseFrequency.hpp"⟩,
  .storage ⟨`DSP.DelayBuffer, "CircularBufferGeneric", "CircularBuffer.hpp", 1,
    #[.number, .channels, .natural 0]⟩,
  .storage ⟨`DSP.WanderingRandomLFO.Stream, "PackedWanderingRandomLFO", "WanderingRandomLFO.hpp", 0,
    #[.number, .channels, .boolean false]⟩,
  .storage ⟨`DSP.WanderingRandomLFO.FramedStream, "PackedWanderingRandomLFO", "WanderingRandomLFO.hpp", 0,
    #[.number, .channels, .boolean true]⟩,
  native `DSP.DelayBuffer.readWrappedAge "ReadWrappedAge" 1,
  native `DSP.DelayBuffer.readAgeOffsets "ReadAgeOffsets" 2,
  native `DSP.DelayBuffer.pushFrame "Push" 1,
  native `DSP.DelayBuffer.clear "Clear" 0,
  .action ⟨`DSP.WanderingRandomLFO.FramedStream.initialize, "Initialize", 2, some
    (`DSP.WanderingRandomLFO.initializeFramed32, `DSP.WanderingRandomLFO.initializeFramed64)⟩,
  .action ⟨`DSP.WanderingRandomLFO.Stream.initialize, "Initialize", 2, some
    (`DSP.WanderingRandomLFO.initialize32, `DSP.WanderingRandomLFO.initialize64)⟩,
  .action ⟨`DSP.WanderingRandomLFO.FramedStream.nextValue, "NextSpline", 0, some
    (`DSP.WanderingRandomLFO.processFramed32, `DSP.WanderingRandomLFO.processFramed64)⟩,
  .action ⟨`DSP.WanderingRandomLFO.Stream.nextValue, "NextSpline", 0, some
    (`DSP.WanderingRandomLFO.process32, `DSP.WanderingRandomLFO.process64)⟩,
  native `DSP.Resynthesis.PVDR.Storage.magnitude "Magnitude" 1,
  native `DSP.Resynthesis.PVDR.Storage.previousMagnitude "PreviousMagnitude" 1,
  native `DSP.Resynthesis.PVDR.Storage.phase "Phase" 1,
  native `DSP.Resynthesis.PVDR.Storage.omega "Omega" 1,
  native `DSP.Resynthesis.PVDR.Storage.gate "Gate" 1,
  native `DSP.Resynthesis.PVDR.Storage.setGate "SetGate" 2,
  native `DSP.Resynthesis.PVDR.Storage.computed "Computed" 1,
  native `DSP.Resynthesis.PVDR.Storage.link "Link" 1,
  native `DSP.Resynthesis.PVDR.Storage.put "Put" 2,
  native `DSP.Resynthesis.PVDR.Storage.push "Push" 1,
  native `DSP.Resynthesis.PVDR.Storage.pop "Pop" 0,
  native `DSP.Resynthesis.PVDR.Storage.nextPhase "NextPhase" 0,
  native `DSP.Resynthesis.SpectrumStorage.controls "Controls" 1,
  native `DSP.Resynthesis.SpectrumStorage.graphSize "GraphSize" 1,
  native `DSP.Resynthesis.SpectrumStorage.visitAt "VisitAt" 2,
  native `DSP.Resynthesis.SpectrumStorage.gate "Gate" 2,
  native `DSP.Resynthesis.SpectrumStorage.analysisPhase "AnalysisPhase" 2,
  native `DSP.Resynthesis.SpectrumStorage.omega "Omega" 2,
  native `DSP.Resynthesis.SpectrumStorage.setOmega "SetOmega" 3,
  native `DSP.Resynthesis.SpectrumStorage.synthesisMagnitude "SynthesisMagnitude" 2,
  native `DSP.Resynthesis.SpectrumStorage.phase "Phase" 3,
  native `DSP.Resynthesis.SpectrumStorage.setPhase "SetPhase" 4,
  native `DSP.Resynthesis.SpectrumStorage.clearOutput "ClearOutput" 0,
  native `DSP.Resynthesis.SpectrumStorage.coefficient "Coefficient" 2,
  native `DSP.Resynthesis.SpectrumStorage.setCoefficient "SetCoefficient" 3,
  native `DSP.Resynthesis.SpectrumStorage.polar "Polar" 2,
  native `DSP.Resynthesis.SpectrumStorage.round "Round" 1,
  native `DSP.Resynthesis.SpectrumStorage.ratio "Ratio" 1,
  native `DSP.Resynthesis.SpectrumStorage.drive "Drive" 1,
  native `DSP.Resynthesis.FrameStorage.fft "FFT" 1,
  native `DSP.Resynthesis.FrameStorage.ifft "IFFT" 0,
  native `DSP.Resynthesis.FrameStorage.measurement "Measurement" 2,
  native `DSP.Resynthesis.FrameStorage.previousPhase "PreviousPhase" 2,
  native `DSP.Resynthesis.FrameStorage.current "Current" 2,
  native `DSP.Resynthesis.FrameStorage.setPrevious "SetPrevious" 3,
  native `DSP.Resynthesis.FrameStorage.setCurrent "SetCurrent" 4,
  native `DSP.Resynthesis.FrameStorage.magnitude "Magnitude" 2,
  native `DSP.Resynthesis.FrameStorage.setMagnitude "SetMagnitude" 3,
  native `DSP.Resynthesis.FrameStorage.slewCutoff "SlewCutoff" 1,
  native `DSP.Resynthesis.FrameStorage.prepareSlew "PrepareSlew" 1,
  native `DSP.Resynthesis.FrameStorage.slewCoefficient "SlewCoefficient" 1,
  native `DSP.Resynthesis.FrameStorage.getSample "GetSample" 2,
  native `DSP.Resynthesis.FrameStorage.putSample "PutSample" 3,
  native `DSP.Resynthesis.FrameStorage.window "Window" 1,
  .action ⟨`DSP.Resynthesis.FrameStorage.analyzeGraph, "AnalyzeGraph", 1, some
    (`DSP.Resynthesis.PVDR.Storage.analyze32, `DSP.Resynthesis.PVDR.Storage.analyze64)⟩,
  .action ⟨`DSP.Resynthesis.FrameStorage.synthesize, "Synthesize", 1, some
    (`DSP.Resynthesis.SpectrumStorage.synthesize32, `DSP.Resynthesis.SpectrumStorage.synthesize64)⟩,
  native `DSP.Resynthesis.FrameStorage.clearAnalysisMagnitude "ClearAnalysisMagnitude" 1,
  native `DSP.Resynthesis.FrameStorage.clearPreviousMagnitude "ClearPreviousMagnitude" 1,
  native `DSP.Resynthesis.FrameStorage.clearAnalysisPhase "ClearAnalysisPhase" 1,
  native `DSP.Resynthesis.FrameStorage.clearPreviousPhase "ClearPreviousPhase" 1,
  native `DSP.Resynthesis.FrameStorage.clearOmega "ClearOmega" 1,
  native `DSP.Resynthesis.FrameStorage.clearOscillatorPhase "ClearOscillatorPhase" 2,
  native `DSP.Resynthesis.FrameStorage.clearGates "ClearGates" 1,
  native `DSP.Resynthesis.FrameStorage.clearOrder "ClearOrder" 1,
  native `DSP.Resynthesis.FrameStorage.clearMagnitude "ClearMagnitude" 1,
  native `DSP.Resynthesis.FrameStorage.setSlewCutoff "SetSlewCutoff" 2,
  native `PhaseTime.InverseRuntime.Storage.beginRevision "BeginWrite" 0,
  native `PhaseTime.InverseRuntime.Storage.endRevision "EndWrite" 0,
  native `PhaseTime.InverseRuntime.Storage.capacityMinusOne "CapacityMinusOne" 0,
  native `PhaseTime.InverseRuntime.Storage.gridResolution "GridResolution" 0,
  native `PhaseTime.InverseRuntime.Storage.base "LoadBase" 1,
  native `PhaseTime.InverseRuntime.Storage.fallback "LoadFallback" 1,
  native `PhaseTime.InverseRuntime.Storage.cut "LoadCut" 1,
  native `PhaseTime.InverseRuntime.Storage.startDelta "LoadStartDelta" 1,
  native `PhaseTime.InverseRuntime.Storage.endDelta "LoadEndDelta" 1,
  native `PhaseTime.InverseRuntime.Storage.seed "LoadSeed" 0,
  native `PhaseTime.InverseRuntime.Storage.frontier "LoadFrontier" 0,
  native `PhaseTime.InverseRuntime.Storage.previous "LoadPrevious" 0,
  native `PhaseTime.InverseRuntime.Storage.highest "LoadHighest" 0,
  native `PhaseTime.InverseRuntime.Storage.hasForward "HasForward" 0,
  native `PhaseTime.InverseRuntime.Storage.descending "IsDescending" 0,
  native `PhaseTime.InverseRuntime.Storage.sameSlot "SameSlot" 2,
  native `PhaseTime.InverseRuntime.Storage.clearSeed "ClearSeed" 0,
  native `PhaseTime.InverseRuntime.Storage.storeSeed "StoreSeed" 2,
  native `PhaseTime.InverseRuntime.Storage.storeFrontier "StoreFrontier" 2,
  native `PhaseTime.InverseRuntime.Storage.storePrevious "StoreObservation" 2,
  native `PhaseTime.InverseRuntime.Storage.storeViewPrevious "StorePreviousObservation" 2,
  native `PhaseTime.InverseRuntime.Storage.storeHighest "StoreHighest" 1,
  native `PhaseTime.InverseRuntime.Storage.storeOldest "StoreOldest" 1,
  native `PhaseTime.InverseRuntime.Storage.storeDirection "StoreDirection" 2,
  native `PhaseTime.InverseRuntime.Storage.markDescending "MarkDescending" 0,
  native `PhaseTime.InverseRuntime.Storage.incrementIslands "IncrementIslands" 0,
  native `PhaseTime.InverseRuntime.Storage.clearCell "ClearIndex" 1,
  native `PhaseTime.InverseRuntime.Storage.putCell "PutCell" 4,
  native `PhaseTime.InverseRuntime.Storage.putFallback "PutFallback" 2,
  native `PhaseTime.InverseRuntime.Storage.putReplacement "PutReplacement" 3,
  native `PhaseTime.InverseRuntime.Storage.putEndDelta "PutEndDelta" 3]

def stateBindings : Array StateBinding := nativeBindings.filterMap fun
  | .action binding => some binding
  | _ => none

def storageBindings : Array StorageBinding := nativeBindings.filterMap fun
  | .storage binding => some binding
  | _ => none

def nativeRecords : NameMap RecordExport := nativeBindings.foldl (init := ∅) fun records entry =>
  match entry with
  | .record source record => records.insert source record
  | _ => records

def valueLowerings : NameMap ValueLowering := nativeBindings.foldl (init := ∅) fun bindings entry =>
  match entry with
  | .value source lowering => bindings.insert source lowering
  | _ => bindings

def arithmeticLowerings : NameMap (Array NumericLowering) := nativeBindings.foldl (init := ∅) fun bindings entry =>
  match entry with
  | .arithmetic source fields => bindings.insert source fields
  | _ => bindings

end DSP.Codegen
