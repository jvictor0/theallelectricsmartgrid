import DSP.Resynthesis.PVDR
import DSP.IfLess

namespace DSP.Resynthesis

structure PitchRatio where
  numerator : Int
  denominator : Int
  deriving DecidableEq

def PitchRatio.unity : PitchRatio := ⟨1, 1⟩

structure Controls (R : Type) where
  shift : PitchRatio
  fade : R
  unisonGain : R
  unisonDetune : R
  slewUp : R

structure GrainInput (R : Type) (n channels : ℕ) where
  previous : PackedBuffer R n channels
  current : PackedBuffer R n channels
  controls : Fin channels → Controls R

structure ChannelState (R P : Type) (n : ℕ) where
  gates : BinTable GateState n
  magnitude : BinTable R n
  phase : Fin 3 → BinTable P n

abbrev State (R P : Type) (n channels : ℕ) := Fin channels → ChannelState R P n

def ChannelState.clear {R P : Type} {n : ℕ} [OfNat R 0] [OfNat P 0] : ChannelState R P n :=
  ⟨BinTable.ofFn (fun _ => .low), BinTable.ofFn (fun _ => 0), fun _ => BinTable.ofFn (fun _ => 0)⟩

def clearChannel {R P : Type} {n channels : ℕ} [OfNat R 0] [OfNat P 0]
    (state : State R P n channels) (lane : Fin channels) : State R P n channels :=
  fun i => if i = lane then ChannelState.clear else state i

/-- The native full clear keeps the stored slew magnitudes. -/
def clear {R P : Type} {n channels : ℕ} [OfNat P 0]
    (state : State R P n channels) : State R P n channels :=
  fun lane => { state lane with
    gates := BinTable.ofFn (fun _ => .low), phase := fun _ => BinTable.ofFn (fun _ => 0) }

/-- Operations with dedicated exact and native interpretations. Phases and
frequencies are in turns and turns per host sample. -/
structure GrainMath (R P : Type) where
  widen : R → P
  natSample : Nat → R
  natPhase : Nat → P
  intPhase : Int → P
  phaseFloor : P → P
  phaseRound : P → Int
  sqrt : R → R
  magnitude : ComplexValue R → R
  phase : ComplexValue R → R
  polar : R → P → ComplexValue R
  thresholds : R → R × R
  slewCoefficient : R → R
  hann : {m : ℕ} → Buffer R m → Buffer R m

section Kernels

def slewMagnitude {R : Type} [Add R] [Sub R] [Mul R] [IfLess R]
    (source target alpha : R) : R :=
  IfLess.ifLess source target (source + (target - source) * alpha) target

def slewBins {R : Type} {count : ℕ} [Add R] [Sub R] [Mul R] [IfLess R]
    (alpha : R) (previous target : Vector R count) : Vector R count :=
  Vector.ofFn fun bin => slewMagnitude (previous.get bin) (target.get bin) alpha

def principalArgument {P : Type} [Sub P] [OfNat P 1] [OfScientific P] [IfLess P]
    (phase floorPhase : P) : P :=
  let wrapped := phase - floorPhase
  IfLess.ifLess 0.5 wrapped (wrapped - 1) wrapped

def phaseResidual {P : Type} [Sub P] [Mul P] (center hop phase previous : P) : P :=
  phase - previous - center * hop

def correctedFrequency {P : Type} [Add P] [Div P] (center hop wrapped : P) : P :=
  wrapped / hop + center

def instantaneousFrequency {P : Type} [Add P] [Sub P] [Mul P] [Div P]
    [OfNat P 1] [OfScientific P] [IfLess P]
    (floor : P → P) (center hop phase previous : P) : P :=
  let residual := phaseResidual center hop phase previous
  correctedFrequency center hop (principalArgument residual (floor residual))

def leaderPhase {P : Type} [Add P] [Mul P] (triggered : Bool)
    (phase analysis delta detune : P) : P :=
  if triggered then analysis else phase + delta * detune

def followerPhase {P : Type} [Add P] (root delta : P) : P := root + delta

def fadeWeight {R : Type} [Mul R] (fade : R) : R := fade * fade * fade * fade

end Kernels

section Grain

variable {R P Q RNG : Type} {n m channels : ℕ}
variable [Add R] [Sub R] [Mul R] [Div R] [OfNat R 0] [OfNat R 1] [OfNat R 2] [OfNat R 3]
  [OfScientific R] [LT R] [LE R] [DecidableLT R] [DecidableLE R] [Max R] [IfLess R]
variable [Add P] [Sub P] [Mul P] [Div P] [Neg P] [OfNat P 0] [OfNat P 1]
  [OfScientific P] [LT P] [DecidableLT P] [IfLess P]

def analysisInput (math : GrainMath R P) (previous current : DFT R n) : PVDRInput R P n :=
  let currentPhase := BinTable.ofFn fun bin => math.phase (current.coeff bin.coefficient)
  let omega := BinTable.ofFn fun bin =>
    let center := math.widen (math.natSample bin.coefficient.val / math.natSample n)
    instantaneousFrequency math.phaseFloor center (math.natPhase (n / 4))
      (math.widen (currentPhase.get bin)) (math.widen (math.phase (previous.coeff bin.coefficient)))
  ⟨BinTable.ofFn (fun bin => math.magnitude (previous.coeff bin.coefficient)),
    BinTable.ofFn (fun bin => math.magnitude (current.coeff bin.coefficient)), currentPhase, omega⟩

def slewMagnitudes (math : GrainMath R P) (cutoff : R) (previous target : BinTable R n) :
    BinTable R n :=
  let alpha := if (0.5 : R) ≤ cutoff then 1 else math.slewCoefficient cutoff
  ⟨slewBins alpha previous.values target.values⟩

def fixupPhases (math : GrainMath R P) (detune : R) (input : PVDRInput R P n)
    (gates : BinTable GateState n) (graph : PVDRGraph P n) (phases : BinTable P n) : BinTable P n :=
  graph.order.foldl (fun phases bin =>
    let link := graph.link.get bin
    let next := if link.root = bin then
      leaderPhase (gates.get bin == .triggered) (phases.get bin) (math.widen (input.phase.get bin))
        link.delta (math.widen detune)
    else followerPhase (phases.get link.root) link.delta
    phases.set bin next) phases

def detuneFor (unisonDetune : R) (oscillator : Fin 3) : R :=
  if oscillator.val = 0 then 1 else if oscillator.val = 1 then unisonDetune
  else 1 / unisonDetune

def oscillatorDetune (controls : Controls R) (oscillator : Fin 3) : R :=
  detuneFor controls.unisonDetune oscillator

def gainFor (sqrt : R → R) (unisonGain : R) (oscillator : Fin 3) : R :=
  if oscillator.val = 0 then
    sqrt (1 - 2 * unisonGain * unisonGain / 3)
  else unisonGain / sqrt 3

def oscillatorGain (math : GrainMath R P) (controls : Controls R) (oscillator : Fin 3) : R :=
  gainFor math.sqrt controls.unisonGain oscillator

def addCoefficient (spectrum : BinTable (ComplexValue R) n) (bin : ActiveBin n)
    (coefficient : ComplexValue R) : BinTable (ComplexValue R) n :=
  spectrum.set bin (ComplexValue.add (spectrum.get bin) coefficient)

def accumulateDry (math : GrainMath R P) (gain : R) (magnitude : BinTable R n)
    (phase : BinTable P n) (spectrum : BinTable (ComplexValue R) n) : BinTable (ComplexValue R) n :=
  (List.finRange (n / 2 - 1)).foldl (fun spectrum bin =>
    addCoefficient spectrum bin (math.polar (gain * magnitude.get bin) (phase.get bin))) spectrum

def clampShiftedBin (n : ℕ) (rounded : Int) : Int := min (max 1 rounded) (Int.ofNat n - 1)

def shiftedBin (math : GrainMath R P) (omega : P) : Int :=
  clampShiftedBin n (math.phaseRound (omega * math.natPhase n))

def acceptedBinWith (widen : R → P) (natSample : Nat → R) (natPhase : Nat → P)
    (omega : P) (target : Int) : Option (ActiveBin n) :=
  if h : 0 < target ∧ target < Int.ofNat (n / 2) then
    let bin : ActiveBin n := ⟨target.toNat - 1, by
      have cast : (target.toNat : Int) = target := Int.toNat_of_nonneg (Int.le_of_lt h.1)
      have upper : target.toNat < n / 2 := Int.ofNat_lt.mp (cast ▸ h.2)
      have positive : 0 < target.toNat := Int.lt_toNat.mpr h.1
      omega⟩
    let center := widen (natSample bin.coefficient.val / natSample n)
    let difference := omega - center
    let limit := (0.5 : P) / natPhase (n / 4)
    if limit < difference ∨ limit < -difference then none else some bin
  else none

def acceptedBin (math : GrainMath R P) (omega : P) (target : Int) : Option (ActiveBin n) :=
  acceptedBinWith math.widen math.natSample math.natPhase omega target

def accumulateShifted (math : GrainMath R P) (gain : R) (shift : PitchRatio)
    (omega : BinTable P n) (magnitude : BinTable R n) (phase : BinTable P n)
    (graph : PVDRGraph P n) (spectrum : BinTable (ComplexValue R) n) : BinTable (ComplexValue R) n :=
  let ratio := math.intPhase shift.numerator / math.intPhase shift.denominator
  graph.order.foldl (fun spectrum bin =>
    let link := graph.link.get bin
    if link.small then spectrum
    else
      let rootOmega := omega.get link.root * ratio
      let rootTarget := shiftedBin (n := n) math rootOmega
      let rootPhase := phase.get link.root * ratio
      let (target, angle) := if link.root = bin then (rootTarget, rootPhase)
        else (rootTarget - Int.ofNat link.root.coefficient.val + Int.ofNat bin.coefficient.val,
          followerPhase rootPhase link.delta)
      match acceptedBin math rootOmega target with
      | none => spectrum
      | some binOut => addCoefficient spectrum binOut (math.polar (gain * magnitude.get bin) angle)) spectrum

def synthesizeSpectrum (math : GrainMath R P) (controls : Controls R) (omega : BinTable P n)
    (magnitude : BinTable R n) (phase : Fin 3 → BinTable P n) (graph : PVDRGraph P n) : DFT R n :=
  let fade := fadeWeight controls.fade
  let bins := (List.finRange 3).foldl (fun spectrum oscillator =>
    let gain := oscillatorGain math controls oscillator
    let spectrum := accumulateDry math (max 0 (1 - fade) * gain) magnitude (phase oscillator) spectrum
    let shiftedGain := max 0 fade * gain
    if controls.shift = .unity then accumulateDry math shiftedGain magnitude (phase oscillator) spectrum
    else accumulateShifted math shiftedGain controls.shift omega magnitude (phase oscillator) graph spectrum)
    (BinTable.ofFn fun _ => ComplexValue.ofReal 0)
  ⟨fun k => if h : 0 < k.val then bins.get ⟨k.val - 1, by have hk := k.isLt; omega⟩
    else ComplexValue.ofReal 0⟩

def processSpectrum (math : GrainMath R P) (queue : PVDR.Queue R Q n)
    (nextPhase : StateM RNG R) (state : ChannelState R P n)
    (control : Controls R) (previous current : DFT R n) : StateM RNG (ChannelState R P n × DFT R n) := do
  let input := analysisInput math previous current
  let magnitude := slewMagnitudes math control.slewUp state.magnitude input.magnitude
  let (gates, graph) ← PVDR.analyze math.widen (math.natPhase (n / 4)) math.thresholds queue nextPhase
    state.gates input
  let phase := fun oscillator => fixupPhases math (oscillatorDetune control oscillator) input gates graph
    (state.phase oscillator)
  let spectrum := synthesizeSpectrum math control input.omega magnitude phase graph
  return (⟨gates, magnitude, phase⟩, spectrum)

def process (fourier : Fourier R n m channels) (math : GrainMath R P) (queue : PVDR.Queue R Q n)
    (nextPhase : StateM RNG R)
    (state : State R P n channels) (input : GrainInput R n channels) :
    StateM RNG (State R P n channels × PackedBuffer R m channels) := do
  let previous := fourier.fft input.previous
  let current := fourier.fft input.current
  let results ← Vector.ofFnM fun lane =>
    processSpectrum math queue nextPhase (state lane) (input.controls lane) (previous lane) (current lane)
  let frame := fourier.ifft (fun lane => results[lane].2)
  return (fun lane => results[lane].1, fun lane => math.hann (frame lane))

end Grain
end DSP.Resynthesis
