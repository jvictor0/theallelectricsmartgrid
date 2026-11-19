import DSP.Filters.OnePoleZOHLowPass
import DSP.Filters.OnePoleZOHLowPass.Response
import DSP.Filters.Coupled
import DSP.IfLess

namespace DSP.Tests

structure SwapState where
  left : Float
  right : Float

structure SwapInput where
  value : Float
  reset : Bool

structure SwapOutput where
  oldLeft : Float
  sum : Float

def swapProcessor (state : SwapState) (input : SwapInput) : SwapState × SwapOutput :=
  if input.reset then
    ({ left := input.value, right := state.left },
      { oldLeft := state.left, sum := (state.left + state.right) + 0.1 })
  else
    ({ left := state.right, right := state.left },
      { oldLeft := state.left, sum := (state.left + input.value) * state.right })

structure GainState where
  gain : Float32

structure GainInput where
  x : Float32

structure GainOutput where
  y : Float32
  same : Bool

def gainProcessor (state : GainState) (input : GainInput) : GainState × GainOutput :=
  let divisor := if input.x < state.gain then 0.1 else 2.0
  (state, { y := -(input.x / divisor) + state.gain, same := input.x == state.gain })

structure UnsupportedState where
  count : Nat

def unsupportedField (state : UnsupportedState) (input : GainInput) : UnsupportedState × GainOutput :=
  (state, { y := input.x, same := false })

def unsupportedExtern (state : DSP.OnePoleZOHLowPass Float) (input : DSP.Sample Float) :
    DSP.OnePoleZOHLowPass Float :=
  { state with y := input.x.sin }

unsafe def unsupportedUnsafe (state : GainState) (input : GainInput) : GainState × GainOutput :=
  (state, { y := input.x, same := true })

def wrongState (state : GainState) (input : GainInput) : GainInput × GainOutput :=
  (input, { y := state.gain, same := false })

def wrongStateOnly (_state : GainState) (input : GainInput) : GainInput := input

def replacementOne : Nat := 2

@[implemented_by replacementOne] def declaredOne : Nat := 1

def unsupportedLiteral (state : DSP.OnePoleZOHLowPass Float) (_input : DSP.Sample Float) :
    DSP.OnePoleZOHLowPass Float :=
  { state with y := Float.ofNat declaredOne }

def nativeDecision (b : Bool) : Decidable (b = true) := Bool.decEq b true

@[implemented_by nativeDecision] def alternativeDecision (b : Bool) : Decidable (b = true) :=
  Bool.decEq b true

def unsupportedDecision (state : SwapState) (input : SwapInput) : SwapState × SwapOutput :=
  @ite (SwapState × SwapOutput) (input.reset = true) (alternativeDecision input.reset)
    (state, { oldLeft := state.left, sum := input.value })
    (state, { oldLeft := state.right, sum := input.value })

def unsupportedStaticCount (state : GainState) (input : GainInput) : GainState × GainOutput :=
  (state, { y := StateSpace.orderedSum declaredOne (fun _ => input.x), same := false })

structure OversizedState where
  q : Fin 257 → Float

def unsupportedSize (_state : OversizedState) (input : DSP.Sample Float) :
    OversizedState := { q := fun _ => input.x }

structure ArithmeticState (R : Type) where
  left : R
  right : R

def scalarPreparationResult {R : Type} (input : R) : R := input

def arrayPreparation {R : Type} (input : Fin 2 → R) : Sample R := ⟨input 0⟩

/-- A standalone FMA keeps its three operands independent in rounding tests. -/
def fmaProcessor {R : Type} [FMA R] (state : ArithmeticState R) (input : Sample R) :
    ArithmeticState R × Sample R :=
  (state, ⟨FMA.fma state.left input.x state.right⟩)

def fmaProcessorFloat := fmaProcessor (R := Float)
def fmaProcessorFloat32 := fmaProcessor (R := Float32)

/-- Exercise simultaneous assignments, old-state output, per-channel selection,
negation, division and a decimal just above a Float32 rounding midpoint. -/
def arithmeticProcessor {R : Type} [Add R] [Div R] [Neg R] [OfScientific R] [IfLess R]
    (state : ArithmeticState R) (input : Sample R) : ArithmeticState R × Sample R :=
  ({ left := state.right, right := state.left },
    ⟨IfLess.ifLess input.x state.left (-(state.left / 0.1))
      (state.right + 1.0000000596046447753906250000000000000000000001)⟩)

def unsupportedGenericOrder {R : Type} [LT R] [DecidableLT R]
    (state : ArithmeticState R) (input : Sample R) : ArithmeticState R :=
  if input.x < state.left then { state with left := input.x } else state

def unsupportedGenericLiteral {R : Type} [OfScientific R]
    (state : ArithmeticState R) (_input : Sample R) : ArithmeticState R :=
  { state with left := OfScientific.ofScientific declaredOne true 1 }

def wrongGenericState {R : Type} (_state : ArithmeticState R) (input : Sample R) : Sample R :=
  input

def zohStepFloat32 := OnePoleZOHLowPass.step (R := Float32)
def zohMagnitudeSquaredFloat32 := OnePoleZOHLowPass.Snapshot.magnitudeSquared (R := Float32)

/-- An unrelated kernel uses every frequency field and the same shared input
type. Distinct expressions exercise record-field access during generation. -/
def frequencyProbe {R : Type} [Add R] [Mul R] (s : ArithmeticState R)
    (frequency : ResponseFrequency R) : R :=
  (s.left * frequency.cyclesPerSample + s.right * frequency.sinHalf) +
    (frequency.cosHalf + frequency.zInverse.real * frequency.zInverse.imag)

def frequencyProbeFloat := frequencyProbe (R := Float)
def frequencyProbeFloat32 := frequencyProbe (R := Float32)

def functionMixedNumbers (s : Sample Float) (_frequency : ResponseFrequency Float32) : Float := s.x

structure UnsupportedRecord (R : Type) where
  value : R
  count : Nat

/-- Compile both unused arguments, with no arithmetic temporaries to return. -/
def constantFunction {R : Type} [OfNat R 1] (_snapshot : ArithmeticState R) (_input : R) : R := 1

structure FunctionSnapshot (R : Type) where
  input : R

/-- A member containing `input` must not hide the unused numeric argument. -/
def snapshotFunction {R : Type} (s : FunctionSnapshot R) (_input : R) : R := s.input

def functionWithoutSnapshot (a b : Float) : Float := a + b

def functionRecordInput (snapshot : GainState) (input : GainInput) : Float32 :=
  snapshot.gain + input.x

def functionRecordResult (snapshot : GainState) (_input : Float32) : GainState := snapshot

def functionExtern (_snapshot : GainState) (input : Float32) : Float32 := input.sin

def functionWrongArity (input : Float) : Float := input

def functionNoArguments : Float := 1

end DSP.Tests
