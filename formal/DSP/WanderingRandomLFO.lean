import DSP.SmoothRandomLFO
import DSP.Nonlinear
import DSP.Codegen.Control

namespace DSP.WanderingRandomLFO

open SmoothRandomLFO

/-- Audio precision is independent of the double-precision knot clock. -/
class SampleValue (R : Type) where
  fromDouble : Float → R

instance : SampleValue Float := ⟨id⟩
instance : SampleValue Float32 := ⟨Float.toFloat32⟩

def randomStep : UInt64 := 0x9e3779b97f4a7c15
def randomMix1 : UInt64 := 0xbf58476d1ce4e5b9
def randomMix2 : UInt64 := 0x94d049bb133111eb
def speedSeedMask : UInt64 := 0xdecafe8ad14b783c

def frameSamples : Nat := 32

/-- 250 milliseconds at 192 kHz. -/
def speedSmoothingSamples : Float := 48000

def speedAlpha : Float := Float.exp (-((Float.ofNat frameSamples) / speedSmoothingSamples))

/-- SplitMix64 with an open-interval uniform draw from the high 52 bits. -/
def drawUnit (seed : UInt64) : Float × UInt64 :=
  let seed := seed + randomStep
  let bits := (seed ^^^ (seed >>> 30)) * randomMix1
  let bits := (bits ^^^ (bits >>> 27)) * randomMix2
  let bits := bits ^^^ (bits >>> 31)
  (((bits >>> 12).toFloat + 0.5) / 4503599627370496, seed)

def drawPoint (seed : UInt64) : Float × UInt64 :=
  let (u, next) := drawUnit seed
  (2 * u - 1, next)

structure Stream (R : Type) where
  private mk ::
  private segment : Segment Float
  private cubic : Cubic R
  private phase : Float
  private phaseIncrement : Float
  private lambdaSamples : Float
  private frameRemaining : Fin (frameSamples + 1)
  private seed : UInt64
  private speedSeed : UInt64

namespace Stream

def castCubic {R : Type} [SampleValue R] (p : Cubic Float) : Cubic R :=
  ⟨SampleValue.fromDouble p.a, SampleValue.fromDouble p.b,
    SampleValue.fromDouble p.c, SampleValue.fromDouble p.d⟩

/-- Uniform knots in spline phase, with a smoothed exponential clock in sample time. -/
def init {R : Type} [SampleValue R] (lambdaSamples : Float) (seed : UInt64) : Stream R :=
  let rate := maxValue 1 lambdaSamples
  let speedSeed := seed ^^^ speedSeedMask
  let (p0, seed) := drawPoint seed
  let (p1, seed) := drawPoint seed
  let (p2, seed) := drawPoint seed
  let (p3, seed) := drawPoint seed
  let segment := ⟨p0, p1, p2, p3, 1, 1, 1, 1, 1⟩
  ⟨segment, castCubic (prepare segment), 0, 1 / rate, rate, 0, seed, speedSeed⟩

def value {R : Type} [SampleValue R] [FMA R] (state : Stream R) : R :=
  evaluate state.cubic (SampleValue.fromDouble state.phase)

private def advanceWrapped {R : Type} [SampleValue R] (state : Stream R) : Stream R :=
  let (point, seed) := drawPoint state.seed
  let segment := state.segment.advance point 1
  { state with segment, cubic := castCubic (prepare segment), phase := state.phase - 1, seed }

private def prepareSpeed {R : Type} (state : Stream R) : Stream R :=
  let (u, speedSeed) := drawUnit state.speedSeed
  let rawDraw := -u.log / state.lambdaSamples
  let draw := minValue 1 rawDraw
  let alpha := speedAlpha
  { state with
    phaseIncrement := smoothVelocity alpha state.phaseIncrement draw
    frameRemaining := ⟨frameSamples, Nat.lt_succ_self _⟩
    speedSeed := speedSeed }

private def advancePhase {R : Type} (state : Stream R) (count : Nat) : Stream R :=
  { state with
    phase := state.phase + state.phaseIncrement * Float.ofNat count
    frameRemaining := ⟨state.frameRemaining.val - count,
      Nat.lt_of_le_of_lt (Nat.sub_le ..) state.frameRemaining.isLt⟩ }

/-- Advance the phase clock in chunks ending at the 32-sample update boundaries. -/
private def advanceM {R S : Type} (stream : S → Stream R) (replace : S → Stream R → S)
    (refresh wrap : StateM S PUnit) (samples : Nat) : StateM S PUnit := do
  let initial : Fin (samples + 1) := ⟨samples, Nat.lt_succ_self _⟩
  let _ ← foldUntilM samples (fun remaining => do
    if remaining.val = 0 then return .done remaining
    if (stream (← get)).frameRemaining.val = 0 then refresh
    let count := min remaining.val (stream (← get)).frameRemaining.val
    modify fun state => replace state ((stream state).advancePhase count)
    let _ ← foldUntilM count (fun (_ : PUnit) => do
      if (stream (← get)).phase < 1 then return .done ⟨⟩
      wrap
      return .yield ⟨⟩) ⟨⟩
    return .yield ⟨remaining.val - count,
      Nat.lt_of_le_of_lt (Nat.sub_le ..) remaining.isLt⟩) initial
  return ⟨⟩

def advance {R : Type} [SampleValue R] (state : Stream R) (samples : Nat) : Stream R :=
  ((advanceM id (fun _ next => next) (modify prepareSpeed) (modify advanceWrapped) samples).run state).2

/-- A one-sample advance consumes one tick of the bounded knot clock. -/
private def tick {R : Type} [SampleValue R] [FMA R]
    (refresh wrap : StateM (Stream R) PUnit) : StateM (Stream R) R := do
  let output := (← get : Stream R).value
  let remaining := (← get : Stream R).frameRemaining.val
  (if remaining = 0 then refresh else pure ⟨⟩)
  modify fun state => state.advancePhase 1
  let phase := (← get : Stream R).phase
  (if phase < 1 then pure ⟨⟩ else wrap)
  return output

def step {R : Type} [SampleValue R] [FMA R] (state : Stream R) : Stream R × R :=
  let (value, next) := (tick (modify prepareSpeed) (modify advanceWrapped)).run state
  (next, value)

end Stream

/-- Linear reconstruction between spline values evaluated on a sample grid. -/
structure FramedStream (R : Type) where
  private mk ::
  private endpoint : Stream R
  private left : R
  private right : R
  private slope : R
  private position : Fin (frameSamples + 1)

namespace FramedStream

private def initial {R : Type} [SampleValue R] [FMA R]
    (lambdaSamples : Float) (seed : UInt64) : FramedStream R :=
  let endpoint := Stream.init lambdaSamples seed
  let first := endpoint.value
  ⟨endpoint, first, first, SampleValue.fromDouble 0, 0⟩

private def wrapEndpoint {R : Type} [SampleValue R] : StateM (FramedStream R) PUnit :=
  modify fun state => { state with endpoint := state.endpoint.advanceWrapped }

private def renew {R : Type} [SampleValue R] [Sub R] [Div R] [FMA R]
    (wrap : StateM (FramedStream R) PUnit) : StateM (FramedStream R) PUnit := do
  Stream.advanceM FramedStream.endpoint (fun state endpoint => { state with endpoint })
    (modify fun state => { state with endpoint := state.endpoint.prepareSpeed }) wrap frameSamples
  modify fun state =>
    let left := state.right
    let right := state.endpoint.value
    { state with
      left
      right
      slope := (right - left) / SampleValue.fromDouble (Float.ofNat frameSamples)
      position := 0 }

def init {R : Type} [SampleValue R] [Sub R] [Div R] [FMA R]
    (lambdaSamples : Float) (seed : UInt64) : FramedStream R :=
  ((renew wrapEndpoint).run (initial lambdaSamples seed)).2

private def tick {R : Type} [SampleValue R] [FMA R]
    (renew : StateM (FramedStream R) PUnit) : StateM (FramedStream R) R := do
  let position := (← get : FramedStream R).position
  let index : Fin frameSamples ← if h : position.val < frameSamples then pure ⟨position.val, h⟩
    else do
      renew
      pure ⟨0, by decide⟩
  modifyGet fun state =>
    (FMA.fma state.slope (SampleValue.fromDouble (Float.ofNat index.val)) state.left,
      { state with position := ⟨index.val + 1, Nat.succ_lt_succ index.isLt⟩ })

/-- The executable model and generated framed processor share the same state action. -/
def step {R : Type} [SampleValue R] [Sub R] [Div R] [FMA R]
    (state : FramedStream R) : FramedStream R × R :=
  let (value, next) := (tick (renew wrapEndpoint)).run state
  (next, value)

end FramedStream

structure Initial where
  lambdaSamples : Float
  seed : UInt64

def initialize32 (input : Initial) : StateM (Stream Float32) PUnit :=
  set (Stream.init (R := Float32) input.lambdaSamples input.seed)
def initialize64 (input : Initial) : StateM (Stream Float) PUnit :=
  set (Stream.init (R := Float) input.lambdaSamples input.seed)

def refresh32 (_ : PUnit.{1}) : StateM (Stream Float32) PUnit := modify Stream.prepareSpeed
def refresh64 (_ : PUnit.{1}) : StateM (Stream Float) PUnit := modify Stream.prepareSpeed
def wrap32 (_ : PUnit.{1}) : StateM (Stream Float32) PUnit := modify Stream.advanceWrapped
def wrap64 (_ : PUnit.{1}) : StateM (Stream Float) PUnit := modify Stream.advanceWrapped

def process32 (_ : PUnit.{1}) := Stream.tick (refresh32 ⟨⟩) (wrap32 ⟨⟩)
def process64 (_ : PUnit.{1}) := Stream.tick (refresh64 ⟨⟩) (wrap64 ⟨⟩)

def wrapFramed32 (_ : PUnit.{1}) := FramedStream.wrapEndpoint (R := Float32)
def wrapFramed64 (_ : PUnit.{1}) := FramedStream.wrapEndpoint (R := Float)
def renewFrame32 (_ : PUnit.{1}) := FramedStream.renew (wrapFramed32 ⟨⟩)
def renewFrame64 (_ : PUnit.{1}) := FramedStream.renew (wrapFramed64 ⟨⟩)

def initializeFramed32 (input : Initial) : StateM (FramedStream Float32) PUnit := do
  set (FramedStream.initial (R := Float32) input.lambdaSamples input.seed)
  renewFrame32 ⟨⟩

def initializeFramed64 (input : Initial) : StateM (FramedStream Float) PUnit := do
  set (FramedStream.initial (R := Float) input.lambdaSamples input.seed)
  renewFrame64 ⟨⟩

def processFramed32 (_ : PUnit.{1}) := FramedStream.tick (renewFrame32 ⟨⟩)
def processFramed64 (_ : PUnit.{1}) := FramedStream.tick (renewFrame64 ⟨⟩)

end DSP.WanderingRandomLFO
