import Lean

namespace DSP.Codegen

open Lean

/-- A native implementation of a `StateM` storage or numeric primitive.
The method acts on the single borrowed state and returns its observation. -/
structure StateBinding where
  source : Name
  method : String
  arguments : Nat
  generated : Option (Name × Name) := none

private def native (source : Name) (method : String) (arguments : Nat) : StateBinding :=
  ⟨source, method, arguments, none⟩

/-- The storage and numeric allowance for compiled StateM actions. Entries
marked generated must resolve to two emitted Lean procedures, one per sample
precision. Their native methods only adapt the storage view and call that code. -/
def stateBindings : Array StateBinding := #[
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
  ⟨`DSP.Resynthesis.FrameStorage.analyzeGraph, "AnalyzeGraph", 1, some
    (`DSP.Resynthesis.PVDR.Storage.analyze32, `DSP.Resynthesis.PVDR.Storage.analyze64)⟩,
  ⟨`DSP.Resynthesis.FrameStorage.synthesize, "Synthesize", 1, some
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
  native `DSP.Resynthesis.FrameStorage.setSlewCutoff "SetSlewCutoff" 2]

end DSP.Codegen
