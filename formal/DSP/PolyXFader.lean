import DSP.Filters.OnePoleZOHLowPass
import DSP.Codegen.Packed

namespace DSP.PolyXFader

abbrev MaxLoops := 6
abbrev maxMappedMult : Nat := 16

/-- The nominal range of the exponential Mult control. -/
def MappedMult {R : Type} [LE R] [OfNat R 1] [OfNat R maxMappedMult] (mult : R) : Prop :=
  1 ≤ mult ∧ mult ≤ OfNat.ofNat maxMappedMult

def fractional {R : Type} [Sub R] (floor : R → R) (phase : R) : R :=
  phase - floor phase

def minOf {R : Type} [IfLess R] (a b : R) : R := IfLess.ifLess a b a b

def maxOf {R : Type} [IfLess R] (a b : R) : R := IfLess.ifLess a b b a

def clamp {R : Type} [IfLess R] (lo hi x : R) : R := maxOf lo (minOf hi x)

/-- The native lowering samples these two curves with the shared interpolated table.
The clamp applies to the table position, while the linear morph uses the original ramp. -/
def curvesFloat32 (ramp : Float32) : Float32 × Float32 :=
  let x := (clamp 0 1 ramp).toFloat
  let sine := (1 - Float.cos (3.141592653589793 * x)) * 0.5
  let t := 8 * (2 * sine - 1)
  let t2 := t * t
  let hard := (clamp (-1) 1 (t * (27 + t2) / (27 + 9 * t2)) + 1) * 0.5
  (sine.toFloat32, hard.toFloat32)

def curvesPacked {n : Nat} (ramp : Packed Float32 n) : Packed Float32 n × Packed Float32 n :=
  (⟨fun lane => (curvesFloat32 (ramp.lane lane)).1⟩,
   ⟨fun lane => (curvesFloat32 (ramp.lane lane)).2⟩)

structure MorphWeights (R : Type) where
  linear : R
  sine : R
  hard : R

def morphWeights {R : Type} [IfLess R] [OfNat R 0] [OfNat R 1] [OfNat R 2]
    [Sub R] [Mul R] (shape : R) : MorphWeights R :=
  let a := clamp 0 1 (shape * 2)
  let b := clamp 0 1 (shape * 2 - 1)
  ⟨1 - a, a - b, b⟩

def morph {R : Type} [Add R] [Mul R] (curves : R → R × R) (weights : MorphWeights R) (ramp : R) : R :=
  let (sine, hard) := curves ramp
  ramp * weights.linear + sine * weights.sine + hard * weights.hard

def shapeFloat32 (shape ramp : Float32) : Float32 :=
  let weights := morphWeights shape
  morph curvesFloat32 weights ramp

structure MorphControl where
  shape : Float32

def shapeRamp (control : MorphControl) (ramp : Float32) : Float32 :=
  shapeFloat32 control.shape ramp

structure Waveform (R : Type) where
  phaseShift : R
  mult : R
  attackFrac : R
  shape : R

structure PreparedMultiplier (R : Type) where
  mult : R
  floorMult : R
  partialAmp : R
  inversePartial : R

structure PreparedWaveform (P R : Type) where
  phaseShift : P
  multiplier : PreparedMultiplier R
  morph : MorphWeights R
  attack : R
  inverseAttack : R
  inverseDecay : R

def prepareMultiplierWith {R : Type} [OfNat R 0] [OfNat R 1] [IfLess R] [Sub R] [Div R]
    (floor : R → R) (mult : R) : PreparedMultiplier R :=
  let floorMult := floor mult
  let partialAmp := mult - floorMult
  ⟨mult, floorMult, partialAmp, 1 / IfLess.ifLess 0 partialAmp partialAmp 1⟩

def prepareWith {P R : Type} [OfNat R 0] [OfNat R 1] [OfScientific R]
    [IfLess R] [OfNat R 2] [Sub R] [Div R] [Mul R]
    (floor : R → R) (widen : R → P) (control : Waveform R) : PreparedWaveform P R :=
  let attack := clamp 0.001 0.999 control.attackFrac
  ⟨widen control.phaseShift, prepareMultiplierWith floor control.mult, morphWeights control.shape,
    attack, 1 / attack, 1 / (1 - attack)⟩

def shapePreparedWith {P R : Type} [OfScientific P] [Add P] [Sub P]
    [OfNat R 1] [IfLess R] [Add R] [Sub R] [Mul R]
    (floorPhase : P → P) (floorRamp : R → R) (narrow : P → R) (curves : R → R × R)
    (p : PreparedWaveform P R) (loopPhase : P) : R :=
  let m := p.multiplier
  let t := narrow (fractional floorPhase (loopPhase + p.phaseShift + 0.75)) * m.mult
  let position := IfLess.ifLess m.floorMult t
    ((t - m.floorMult) * m.inversePartial) (fractional floorRamp t)
  let amplitude := IfLess.ifLess m.floorMult t m.partialAmp 1
  let rise := position * p.inverseAttack
  let decay := 1 - (position - p.attack) * p.inverseDecay
  amplitude * morph curves p.morph (IfLess.ifLess p.attack position decay rise)

def prepare := prepareWith Float32.floor Float32.toFloat

def shapePrepared := shapePreparedWith Float.floor Float32.floor Float.toFloat32 curvesFloat32

def shapeSingle (control : Waveform Float32) (loopPhase : Float) : Float32 :=
  let prepared := prepare control
  shapePrepared prepared loopPhase

abbrev Lanes (n : Nat) := Packed Float32 n
abbrev Wide (n : Nat) := Packed Float n

structure Input (lanes : Nat) where
  size : Fin (MaxLoops + 1)
  masterPhase : Float
  phaseTravel : Float
  maxSlewCycles : Float
  slewDoublings : Fin 3
  slewStepScale : Float
  cycleRatio : Vector Float MaxLoops
  phaseShift : Lanes lanes
  mult : Lanes lanes
  minWaveMultLimit : Float32
  waveMultLimits : Vector Float32 MaxLoops
  attackFrac : Lanes lanes
  shape : Lanes lanes
  slope : Lanes lanes
  center : Lanes lanes
  externalWeights : Vector (Lanes lanes) MaxLoops
  trig : Lanes lanes
  shFade : Lanes lanes
  amplitude : Lanes lanes

/-- Exact range contract for callers supplying mapped Mult values. -/
def Input.HasMappedMultipliers {lanes : Nat} (input : Input lanes) : Prop :=
  ∀ lane, MappedMult (input.mult.lane lane)

structure State (lanes : Nat) where
  size : Fin (MaxLoops + 1)
  slope : Lanes lanes
  center : Lanes lanes
  totalWeight : Lanes lanes
  weights : Vector (Lanes lanes) MaxLoops
  values : Vector (Lanes lanes) MaxLoops
  mixOutput : Wide lanes
  holdAlpha : Float32
  heldValue : Lanes lanes
  heldOutput : Lanes lanes
  rawOutput : Lanes lanes
  output : Lanes lanes

def activeIndex (count : Fin (MaxLoops + 1)) (i : Fin count.val) : Fin MaxLoops :=
  ⟨i.val, Nat.lt_of_lt_of_le i.isLt (Nat.le_of_lt_succ count.isLt)⟩

def weight {n : Nat} (size index : Nat) (slope center : Lanes n) : Lanes n :=
  let distance := Packed.abs32 (Packed.splat (Float32.ofNat index) - center)
  let tapered := maxOf 0 (1 - maxOf 0 (distance - 0.25) * slope)
  -- Both edge rules apply for a one-loop mix.
  let first := if Nat.beq index 0 then IfLess.ifLess center 0.25 1 tapered else tapered
  if Nat.beq index (size - 1) then
    IfLess.ifLess center (Packed.splat (Float32.ofNat size) - 0.25) first 1
  else first

def updateWeights {lanes : Nat} (input : Input lanes) : StateM (State lanes) PUnit := do
  let before ← get
  if !Nat.beq input.size.val before.size.val || Packed.anyDifferent input.slope before.slope ||
      Packed.anyDifferent input.center before.center then
    let count := input.size
    let size := Packed.splat (Float32.ofNat count.val)
    let center := input.center * size
    let preSlope := input.slope * size
    let slope := IfLess.ifLess preSlope 0.5 2 (1 / maxOf preSlope 0.5)
    let total ← Fin.foldlM count.val (fun sum i => do
      let index := activeIndex count i
      let weights := weight count.val index.val slope center
      modify fun s => { s with weights := s.weights.set index.val weights index.isLt }
      return sum + weights
    ) 0
    modify fun s => { s with
      size := count, slope := input.slope, center := input.center, totalWeight := total }

structure Mix (lanes : Nat) where
  signal : Lanes lanes
  maxSpeed : Wide lanes

def waveMultiplier {R : Type} [IfLess R] (requested limit : R) : R :=
  minOf requested limit

def includeLoopSpeed {R : Type} [OfNat R 0] [IfLess R] (weight speed fastest : R) : R :=
  IfLess.ifLess 0 weight (maxOf fastest speed) fastest

/-- Inactive loops preserve the previous diagnostic value. This cache does not
feed the mix. The cutoff depends on the geometric weights, including when
an external weight mutes a geometrically active loop. -/
def mix {lanes : Nat} (input : Input lanes) (limitMults : Bool) : StateM (State lanes) (Mix lanes) := do
  let prepared := prepareWith Packed.floor32 Packed.widen
    ⟨input.phaseShift, input.mult, input.attackFrac, input.shape⟩
  let count := input.size
  let sums ← Fin.foldlM count.val (fun acc i => do
    let state ← get
    let index := activeIndex count i
    let weights := state.weights.get index
    if !Packed.anyDifferent weights 0 then
      return acc
    let ratio := input.cycleRatio.get index
    let mult := if limitMults then waveMultiplier input.mult (Packed.splat (input.waveMultLimits.get index))
      else input.mult
    let multiplier := if limitMults && Packed.anyDifferent mult input.mult then
      prepareMultiplierWith Packed.floor32 mult
    else prepared.multiplier
    let loopPhase := Packed.splat (input.masterPhase * ratio)
    let values := shapePreparedWith Packed.floor64 Packed.floor32 Packed.narrow curvesPacked
      { prepared with multiplier } loopPhase
    let ext := input.externalWeights.get index
    let speed := if limitMults then Packed.splat ratio * Packed.widen mult else Packed.splat ratio
    modify fun s => { s with values := s.values.set index.val values index.isLt }
    return ⟨acc.signal + values * weights * ext,
      includeLoopSpeed (Packed.widen weights) speed acc.maxSpeed⟩
  ) (⟨0, 0⟩ : Mix lanes)
  let total := (← get).totalWeight
  let divisor := IfLess.ifLess 0 total total 1
  -- A uniform multiplier factors out of the maximum when no loop clips.
  let maxSpeed := if limitMults then sums.maxSpeed else sums.maxSpeed * Packed.widen input.mult
  return ⟨sums.signal / divisor, maxSpeed⟩

def slewCycles {R : Type} [OfNat R 8] [Mul R] [IfLess R]
    (limit speed travel : R) : R :=
  minOf (8 * speed * travel) limit

/-- The degree-six Taylor polynomial for `1 - exp(-omega)`. The cutoff cap
puts the whole executable interval below `omega = 0.3`. -/
def slewAlpha {R : Type} [OfNat R 1] [OfNat R 2] [OfNat R 6]
    [OfNat R 24] [OfNat R 120] [OfNat R 720] [Div R] [Neg R] [Mul R] [FMA R]
    (omega : R) : R :=
  let p := FMA.fma (-(1 / 720)) omega (1 / 120)
  let p := FMA.fma p omega (-(1 / 24))
  let p := FMA.fma p omega (1 / 6)
  let p := FMA.fma p omega (-(1 / 2))
  omega * FMA.fma p omega 1

/-- Compose the decay for two equal intervals. At the production rate the
interval already fits the polynomial's range, so no composition is needed. -/
def doubleSlewAlpha {R : Type} [OfNat R 2] [Sub R] [Mul R] (alpha : R) : R :=
  alpha * (2 - alpha)

structure SlewResult (R : Type) where
  latched : R
  held : R
  raw : R
  output : R

def holdAndBlend {R : Type} [OfNat R 0] [OfNat R 1] [OfScientific R] [IfLess R]
    [Add R] [Sub R] [Mul R] [FMA R] (trigger live latched held alpha fade amplitude : R) : SlewResult R :=
  let latched := IfLess.ifLess 0 trigger live latched
  let held := OnePoleZOHLowPass.onePoleStep alpha latched held
  let raw := held * fade + live * (1 - fade)
  ⟨latched, held, raw, raw * amplitude + (1 - amplitude) * 0.5⟩

def filterMix {lanes : Nat} (input : Input lanes) (mixed : Mix lanes) :
    StateM (State lanes) PUnit := do
  let state ← get
  let cycles := slewCycles (Packed.splat input.maxSlewCycles) mixed.maxSpeed (Packed.splat input.phaseTravel)
  let stepCycles := Packed.narrow (cycles * Packed.splat input.slewStepScale)
  let stepAlpha := slewAlpha (stepCycles * 6.283185307179586)
  let alpha := Packed.widen (Fin.foldl input.slewDoublings.val (fun a _ => doubleSlewAlpha a) stepAlpha)
  let live := OnePoleZOHLowPass.onePoleStep alpha (Packed.widen mixed.signal) state.mixOutput
  let values := holdAndBlend input.trig (Packed.narrow live) state.heldValue
    state.heldOutput (Packed.splat state.holdAlpha) input.shFade input.amplitude
  set { state with
    mixOutput := live,
    heldValue := values.latched, heldOutput := values.held, rawOutput := values.raw, output := values.output }

def process {lanes : Nat} (input : Input lanes) : StateM (State lanes) PUnit := do
  updateWeights input
  if !Nat.beq input.size.val 0 then
    let limitMults := Packed.anyDifferent (waveMultiplier input.mult (Packed.splat input.minWaveMultLimit))
      input.mult
    if limitMults then
      filterMix input (← mix input true)
    else
      filterMix input (← mix input false)

/-- The proof-side entry point for mapped controls uses the same DSP as the
raw Float32 exports, which can also receive rounded or directly supplied values. -/
def processMapped {lanes : Nat} (input : Input lanes) (_ : input.HasMappedMultipliers) :
    StateM (State lanes) PUnit :=
  process input

def processMono := @process 1
def processTrio := @process 3
def processQuad := @process 4

end DSP.PolyXFader
