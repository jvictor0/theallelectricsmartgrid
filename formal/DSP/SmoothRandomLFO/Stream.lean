import DSP.SmoothRandomLFO
import DSP.Nonlinear

namespace DSP.SmoothRandomLFO

/-- Audio precision is independent of the double-precision knot clock. -/
class SampleValue (R : Type) where
  fromDouble : Float → R

instance : SampleValue Float := ⟨id⟩
instance : SampleValue Float32 := ⟨Float.toFloat32⟩

def randomStep : UInt64 := 0x9e3779b97f4a7c15
def randomMix1 : UInt64 := 0xbf58476d1ce4e5b9
def randomMix2 : UInt64 := 0x94d049bb133111eb

/-- SplitMix64 with an open-interval uniform draw from the high 52 bits. -/
def drawUnit (seed : UInt64) : Float × UInt64 :=
  let seed := seed + randomStep
  let bits := (seed ^^^ (seed >>> 30)) * randomMix1
  let bits := (bits ^^^ (bits >>> 27)) * randomMix2
  let bits := bits ^^^ (bits >>> 31)
  (((bits >>> 12).toFloat + 0.5) / 4503599627370496, seed)

def minIncrement : Float := 1 / 281474976710656

/-- Rate is the exponential parameter for the increment in cycles per sample.
The one-sample minimum span allows at most one knot advance per step. -/
def drawWidth (rate : Float) (seed : UInt64) : Float × UInt64 :=
  let (u, seed) := drawUnit seed
  let increment := -u.log / rate
  (1 / minValue 1 (maxValue minIncrement increment), seed)

def drawPoint (seed : UInt64) : Float × UInt64 :=
  let (u, next) := drawUnit seed
  (2 * u - 1, next)

structure Stream (R : Type) where
  private mk ::
  private segment : Segment Float
  private cubic : Cubic R
  private elapsed : Float
  private phaseIncrement : Float
  private rate : Float
  private seed : UInt64

namespace Stream

def castCubic {R : Type} [SampleValue R] (p : Cubic Float) : Cubic R :=
  ⟨SampleValue.fromDouble p.a, SampleValue.fromDouble p.b,
    SampleValue.fromDouble p.c, SampleValue.fromDouble p.d⟩

/-- Lambda is the exponential parameter for velocity in cycles per sample.
The mean velocity draw is `1 / lambdaSamples` cycles per sample. -/
def init {R : Type} [SampleValue R] (lambdaSamples : Float) (seed : UInt64) : Stream R :=
  let rate := lambdaSamples
  let (p0, seed) := drawPoint seed
  let (p1, seed) := drawPoint seed
  let (p2, seed) := drawPoint seed
  let (p3, seed) := drawPoint seed
  let (beforePrevious, seed) := drawWidth rate seed
  let (previous, seed) := drawWidth rate seed
  let (width, seed) := drawWidth rate seed
  let (next, seed) := drawWidth rate seed
  let (afterNext, seed) := drawWidth rate seed
  let segment := ⟨p0, p1, p2, p3, beforePrevious, previous, width, next, afterNext⟩
  ⟨segment, castCubic (prepare segment), 0, 1 / width, rate, seed⟩

def value {R : Type} [SampleValue R] [FMA R] (state : Stream R) : R :=
  evaluate state.cubic (SampleValue.fromDouble (state.elapsed * state.phaseIncrement))

private def advanceWrapped {R : Type} [SampleValue R] (state : Stream R) : Stream R :=
  let (point, seed) := drawPoint state.seed
  let (span, seed) := drawWidth state.rate seed
  let segment := state.segment.advance point span
  ⟨segment, castCubic (prepare segment), state.elapsed - state.segment.width,
    1 / segment.width, state.rate, seed⟩

private def settle {R : Type} [SampleValue R] (state : Stream R) (fuel : Nat) : Stream R :=
  match fuel with
  | 0 => state
  | count + 1 =>
    if state.elapsed < state.segment.width then state
    else settle state.advanceWrapped count

/-- Move the knot clock directly to a future sample. Each span is at least
one sample wide, so at most `samples` knots are crossed. -/
def advance {R : Type} [SampleValue R] (state : Stream R) (samples : Nat) : Stream R :=
  settle { state with elapsed := state.elapsed + samples.toFloat } samples

def step {R : Type} [SampleValue R] [FMA R] (state : Stream R) : Stream R × R :=
  (state.advance 1, state.value)

end Stream

def frameSamples : Nat := 32

/-- Linear reconstruction between spline values evaluated on a sample grid. -/
structure FramedStream (R : Type) where
  private mk ::
  private endpoint : Stream R
  private left : R
  private right : R
  private slope : R
  private position : Nat

namespace FramedStream

private def prepareFrame {R : Type} [SampleValue R] [Sub R] [Div R] [FMA R]
    (state : Stream R) (left : R) : FramedStream R :=
  let endpoint := state.advance frameSamples
  let right := endpoint.value
  ⟨endpoint, left, right, (right - left) / SampleValue.fromDouble frameSamples.toFloat, 0⟩

def init {R : Type} [SampleValue R] [Sub R] [Div R] [FMA R]
    (lambdaSamples : Float) (seed : UInt64) : FramedStream R :=
  let state := Stream.init lambdaSamples seed
  prepareFrame state state.value

def step {R : Type} [SampleValue R] [Sub R] [Div R] [FMA R]
    (state : FramedStream R) : FramedStream R × R :=
  let state := if state.position = frameSamples then prepareFrame state.endpoint state.right else state
  let value := FMA.fma state.slope (SampleValue.fromDouble state.position.toFloat) state.left
  ({ state with position := state.position + 1 }, value)

end FramedStream
end DSP.SmoothRandomLFO
