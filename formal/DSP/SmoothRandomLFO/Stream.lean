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
def speedSeedMask : UInt64 := 0xdecafe8ad14b783c

def frameSamples : Nat := 32

/-- 250 milliseconds at 192 kHz. -/
def speedSmoothingSamples : Float := 48000

def speedAlpha : Float := Float.exp (-(frameSamples.toFloat / speedSmoothingSamples))

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
  private frameRemaining : Nat
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

private def settle {R : Type} [SampleValue R] (state : Stream R) (fuel : Nat) : Stream R :=
  match fuel with
  | 0 => state
  | count + 1 =>
    if state.phase < 1 then state
    else settle state.advanceWrapped count

private def prepareSpeed {R : Type} (state : Stream R) : Stream R :=
  let (u, speedSeed) := drawUnit state.speedSeed
  let draw := minValue 1 (-u.log / state.lambdaSamples)
  { state with
    phaseIncrement := smoothVelocity speedAlpha state.phaseIncrement draw
    frameRemaining := frameSamples
    speedSeed := speedSeed }

private def advanceFrames {R : Type} [SampleValue R] (state : Stream R) (samples : Nat) :
    Nat → Stream R
  | 0 => state
  | fuel + 1 =>
    if samples = 0 then state else
      let state := if state.frameRemaining = 0 then state.prepareSpeed else state
      let count := min samples state.frameRemaining
      let advanced := { state with
        phase := state.phase + state.phaseIncrement * count.toFloat
        frameRemaining := state.frameRemaining - count }
      advanceFrames (settle advanced count) (samples - count) fuel

/-- Advance at the current speed up to the next 32-sample update boundary. -/
def advance {R : Type} [SampleValue R] (state : Stream R) (samples : Nat) : Stream R :=
  advanceFrames state samples samples

def step {R : Type} [SampleValue R] [FMA R] (state : Stream R) : Stream R × R :=
  (state.advance 1, state.value)

end Stream

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
