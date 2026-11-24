import DSP.Resynthesis.SpectrumStorage

namespace DSP.Resynthesis.FrameStorage

open SpectrumStorage (Lane Bin forLanes)

abbrev FrameIndex := Fin 16384

structure Measurement (R : Type) where
  magnitude : R
  phase : R

structure Scene (R P : Type) where
  spectrum : SpectrumStorage.Scene R P
  forward : Bool → Lane → DFT R 4096
  inverse : (Lane → DFT R 4096) → Lane → FrameIndex → R
  window : FrameIndex → R
  draw : StateM Nat R
  rng : Nat
  dft : Lane → DFT R 4096
  frame : Lane → FrameIndex → R
  prevPhase : Lane → BinTable R 4096
  slew : Lane → R

variable {R P : Type}

def fft (previous : Bool) : StateM (Scene R P) PUnit :=
  fun state => (⟨⟩, {state with dft := state.forward previous})

def ifft [OfNat R 0] : StateM (Scene R P) PUnit := fun state =>
  let spectrum := fun lane =>
    (⟨fun (k : Fin 2048) => if h : 0 < k.val then
      (state.spectrum.output lane).get ⟨k.val - 1, by have := k.isLt; omega⟩
      else ⟨0, 0⟩⟩ : DFT R 4096)
  (⟨⟩, {state with frame := state.inverse spectrum})

def measurement (lane : Lane) (bin : Bin) : StateM (Scene R P) (Measurement R) := fun state =>
  let value := (state.dft lane).coeff bin.coefficient
  (⟨state.spectrum.math.magnitude value, state.spectrum.math.phase value⟩, state)

def previousPhase (lane : Lane) (bin : Bin) : StateM (Scene R P) R :=
  fun state => ((state.prevPhase lane).get bin, state)


def current (lane : Lane) (bin : Bin) : StateM (Scene R P) (Measurement R) := fun state =>
  let input := state.spectrum.input lane
  (⟨input.magnitude.get bin, input.phase.get bin⟩, state)

def setPrevious (lane : Lane) (bin : Bin) (value : Measurement R) : StateM (Scene R P) PUnit := fun state =>
  let input := state.spectrum.input lane
  let updated := Function.update state.spectrum.input lane
    {input with prevMagnitude := input.prevMagnitude.set bin value.magnitude}
  (⟨⟩, {state with
    spectrum.input := updated
    prevPhase := Function.update state.prevPhase lane ((state.prevPhase lane).set bin value.phase)})

def setCurrent (lane : Lane) (bin : Bin) (value : Measurement R) (omega : P) :
    StateM (Scene R P) PUnit := fun state =>
  let input := state.spectrum.input lane
  let updated := {input with
    magnitude := input.magnitude.set bin value.magnitude
    phase := input.phase.set bin value.phase
    omega := input.omega.set bin omega}
  (⟨⟩, {state with spectrum.input := Function.update state.spectrum.input lane updated})

def magnitude (lane : Lane) (bin : Bin) : StateM (Scene R P) R :=
  fun state => ((state.spectrum.magnitudes lane).get bin, state)

def setMagnitude (lane : Lane) (bin : Bin) (value : R) : StateM (Scene R P) PUnit := fun state =>
  let magnitudes := Function.update state.spectrum.magnitudes lane ((state.spectrum.magnitudes lane).set bin value)
  (⟨⟩, {state with spectrum.magnitudes := magnitudes})

def slewCutoff (lane : Lane) : StateM (Scene R P) R :=
  fun state => (state.slew lane, state)

def prepareSlew (lane : Lane) : StateM (Scene R P) PUnit :=
  fun state => (⟨⟩, {state with slew := Function.update state.slew lane (state.spectrum.controls lane).slewUp})

def slewCoefficient (cutoff : R) : StateM (Scene R P) R :=
  fun state => (state.spectrum.math.slewCoefficient cutoff, state)

def getSample (lane : Lane) (sample : FrameIndex) : StateM (Scene R P) R :=
  fun state => (state.frame lane sample, state)

def putSample (lane : Lane) (sample : FrameIndex) (value : R) : StateM (Scene R P) PUnit := fun state =>
  let frame := Function.update state.frame lane (Function.update (state.frame lane) sample value)
  (⟨⟩, {state with frame})

def window (sample : FrameIndex) : StateM (Scene R P) R :=
  fun state => (state.window sample, state)

def analyzeGraph [OfNat R 0] [LT R] [LE R] [DecidableLT R] [DecidableLE R]
    [Max R] [Sub R] [OfNat P 0] [Add P] [Mul P] (lane : Lane) : StateM (Scene R P) PUnit := fun state =>
  let math := state.spectrum.math
  let queue := PVDR.binaryQueue (fun (a b : R) => decide (a < b))
  let result :=
    (PVDR.analyze math.widen (math.natPhase 1024) math.thresholds queue state.draw
      (state.spectrum.gates lane) (state.spectrum.input lane)).run state.rng
  (⟨⟩, {state with
    spectrum.gates := Function.update state.spectrum.gates lane result.1.1
    spectrum.graph := Function.update state.spectrum.graph lane result.1.2
    rng := result.2})

def synthesize [Add R] [Sub R] [Mul R] [Div R] [OfNat R 0] [OfNat R 1] [OfNat R 2]
    [OfNat R 3] [Max R] [Add P] [Sub P] [Mul P] [Div P] [Neg P]
    [OfScientific P] [LT P] [DecidableLT P] (count : Fin 5) : StateM (Scene R P) PUnit := fun state =>
  let math := state.spectrum.math
  let next := (SpectrumStorage.synthesize math.widen math.natSample math.natPhase math.sqrt count).run state.spectrum
  (next.1, {state with spectrum := next.2})

def clearAnalysisMagnitude [OfNat R 0] (lane : Lane) : StateM (Scene R P) PUnit := fun state =>
  let input := state.spectrum.input lane
  let updated := {input with magnitude := BinTable.ofFn fun _ => 0}
  (⟨⟩, {state with spectrum.input := Function.update state.spectrum.input lane updated})

def clearPreviousMagnitude [OfNat R 0] (lane : Lane) : StateM (Scene R P) PUnit := fun state =>
  let input := state.spectrum.input lane
  let updated := {input with prevMagnitude := BinTable.ofFn fun _ => 0}
  (⟨⟩, {state with spectrum.input := Function.update state.spectrum.input lane updated})

def clearAnalysisPhase [OfNat R 0] (lane : Lane) : StateM (Scene R P) PUnit := fun state =>
  let input := state.spectrum.input lane
  let updated := {input with phase := BinTable.ofFn fun _ => 0}
  (⟨⟩, {state with spectrum.input := Function.update state.spectrum.input lane updated})

def clearPreviousPhase [OfNat R 0] (lane : Lane) : StateM (Scene R P) PUnit := fun state =>
  let phases := Function.update state.prevPhase lane (BinTable.ofFn fun _ => 0)
  (⟨⟩, {state with prevPhase := phases})

def clearOmega [OfNat P 0] (lane : Lane) : StateM (Scene R P) PUnit := fun state =>
  let input := state.spectrum.input lane
  let updated := {input with omega := BinTable.ofFn fun _ => 0}
  (⟨⟩, {state with spectrum.input := Function.update state.spectrum.input lane updated})

def clearOscillatorPhase [OfNat P 0] (lane : Lane) (osc : Fin 3) : StateM (Scene R P) PUnit := fun state =>
  let phases := Function.update (state.spectrum.phases lane) osc (BinTable.ofFn fun _ => 0)
  (⟨⟩, {state with spectrum.phases := Function.update state.spectrum.phases lane phases})

def clearGates (lane : Lane) : StateM (Scene R P) PUnit := fun state =>
  let gates := Function.update state.spectrum.gates lane (BinTable.ofFn fun _ => .low)
  (⟨⟩, {state with spectrum.gates := gates})

def clearOrder (lane : Lane) : StateM (Scene R P) PUnit := fun state =>
  let graph := {state.spectrum.graph lane with order := []}
  (⟨⟩, {state with spectrum.graph := Function.update state.spectrum.graph lane graph})

def clearMagnitude [OfNat R 0] (lane : Lane) : StateM (Scene R P) PUnit := fun state =>
  let magnitudes := Function.update state.spectrum.magnitudes lane (BinTable.ofFn fun _ => 0)
  (⟨⟩, {state with spectrum.magnitudes := magnitudes})

def setSlewCutoff (lane : Lane) (cutoff : R) : StateM (Scene R P) PUnit := fun state =>
  (⟨⟩, {state with slew := Function.update state.slew lane cutoff})

def clearLane [OfNat R 0] [OfNat P 0] (lane : Lane) : StateM (Scene R P) PUnit := do
  clearAnalysisMagnitude lane
  clearPreviousMagnitude lane
  clearAnalysisPhase lane
  clearPreviousPhase lane
  clearOmega lane
  let _ ← Fin.foldlM 3 (fun _ osc => clearOscillatorPhase lane osc) (⟨⟩ : PUnit)
  clearGates lane
  clearOrder lane

def forBins {S : Type} (step : Bin → StateM S PUnit) : StateM S PUnit :=
  Fin.foldlM 2047 (fun _ bin => step bin) ⟨⟩

def primeCoefficients (count : Fin 5) : StateM (Scene R P) PUnit :=
  forLanes count fun lane => forBins fun bin => do
    let value ← measurement lane bin
    setPrevious lane bin value

def copyCoefficients (count : Fin 5) : StateM (Scene R P) PUnit :=
  forLanes count fun lane => forBins fun bin => do
    let value ← current lane bin
    setPrevious lane bin value

def frequency [Div R] [Add P] [Sub P] [Mul P] [Div P] [OfNat P 1] [OfScientific P] [IfLess P]
    (widen : R → P) (natSample : Nat → R) (natPhase : Nat → P) (floor : P → P)
    (bin : Bin) (phase previous : R) : P :=
  let center := widen (natSample bin.coefficient.val / natSample 4096)
  instantaneousFrequency floor center (natPhase 1024) (widen phase) (widen previous)

def analyzeCoefficients [OfScientific R] [OfNat R 1] [LE R] [DecidableLE R] [Div R]
    [Sub R] [Add R] [Mul R] [IfLess R]
    [Add P] [Sub P] [Mul P] [Div P] [OfNat P 1] [OfScientific P] [IfLess P]
    (widen : R → P) (natSample : Nat → R) (natPhase : Nat → P) (floor : P → P) (count : Fin 5) :
    StateM (Scene R P) PUnit :=
  forLanes count fun lane => do
    let cutoff ← slewCutoff lane
    let alpha ← if (0.5 : R) ≤ cutoff then pure 1 else slewCoefficient cutoff
    forBins fun bin => do
      let value ← measurement lane bin
      let previous ← previousPhase lane bin
      setCurrent lane bin value (frequency widen natSample natPhase floor bin value.phase previous)
      let old ← magnitude lane bin
      setMagnitude lane bin (slewMagnitude old value.magnitude alpha)

/-- The coefficient-analysis prefix of the process stage. -/
def analyzeFrame [OfScientific R] [OfNat R 1] [LE R] [DecidableLE R] [Div R]
    [Sub R] [Add R] [Mul R] [IfLess R]
    [Add P] [Sub P] [Mul P] [Div P] [OfNat P 1] [OfScientific P] [IfLess P]
    (widen : R → P) (natSample : Nat → R) (natPhase : Nat → P) (floor : P → P)
    (count : Fin 5) : StateM (Scene R P) PUnit := do
  forLanes count prepareSlew
  fft true
  primeCoefficients count
  fft false
  analyzeCoefficients widen natSample natPhase floor count

def applyWindow [Mul R] (count : Fin 5) (frameSize : Fin 16385) : StateM (Scene R P) PUnit :=
  Fin.foldlM frameSize.val (fun _ i => do
    let sample : FrameIndex := ⟨i.val, by have := i.isLt; have := frameSize.isLt; omega⟩
    let weight ← window sample
    forLanes count fun lane => do
      let value ← getSample lane sample
      putSample lane sample (value * weight)) ⟨⟩

inductive Stage where
  | process | synthesize | analyze | prime | primeSpectrum | measure | copy | updateFrequency
  | clear | clearChannel | renderCurrent

structure Input where
  stage : Stage
  count : Fin 5
  frameSize : Fin 16385
  lane : Lane
  bin : Bin

def run [OfScientific R] [OfNat R 0] [OfNat R 1] [OfNat R 2] [OfNat R 3]
    [LE R] [DecidableLE R] [LT R] [DecidableLT R] [Max R]
    [Div R] [Sub R] [Add R] [Mul R] [IfLess R]
    [OfNat P 0] [OfNat P 1] [Add P] [Sub P] [Mul P] [Div P] [Neg P]
    [OfScientific P] [LT P] [DecidableLT P] [IfLess P]
    (widen : R → P) (natSample : Nat → R) (natPhase : Nat → P) (floor : P → P)
    (input : Input) : StateM (Scene R P) PUnit := do
  let count := input.count
  let measure := analyzeCoefficients widen natSample natPhase floor count
  match input.stage with
  | .process =>
    analyzeFrame widen natSample natPhase floor count
    forLanes count analyzeGraph
    synthesize count
    ifft
    applyWindow count input.frameSize
  | .synthesize => synthesize count
  | .analyze =>
    fft false
    measure
    forLanes count analyzeGraph
  | .prime =>
    fft true
    primeCoefficients count
  | .primeSpectrum => primeCoefficients count
  | .measure => measure
  | .copy => copyCoefficients count
  | .updateFrequency => forLanes count fun lane => do
    let value ← current lane input.bin
    let previous ← previousPhase lane input.bin
    setCurrent lane input.bin value (frequency widen natSample natPhase floor input.bin value.phase previous)
  | .clear => forLanes count clearLane
  | .clearChannel =>
    clearLane input.lane
    clearMagnitude input.lane
    setSlewCutoff input.lane 0
  | .renderCurrent =>
    forLanes count prepareSlew
    fft false
    measure
    forLanes count analyzeGraph
    synthesize count
    ifft
    applyWindow count input.frameSize

def run32 := run (R := Float32) Float32.toFloat Float32.ofNat Float.ofNat Float.floor
def run64 := run (R := Float) id Float.ofNat Float.ofNat Float.floor

end DSP.Resynthesis.FrameStorage
