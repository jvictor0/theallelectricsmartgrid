import DSP.Filters.SquiggleBoyQuadReverb.Loop
import DSP.StateFocus

namespace DSP.WanderingRandomLFO

def FramedStream.initialize {R : Type} [SampleValue R] [Sub R] [Div R] [FMA R]
    (lambdaSamples : Float) (seed : UInt64) : StateM (FramedStream R) PUnit :=
  set (FramedStream.init (R := R) lambdaSamples seed)

def Stream.initialize {R : Type} [SampleValue R] [FMA R]
    (lambdaSamples : Float) (seed : UInt64) : StateM (Stream R) PUnit :=
  set (Stream.init (R := R) lambdaSamples seed)

def FramedStream.nextValue {R : Type} [SampleValue R] [Sub R] [Div R] [FMA R] :
    StateM (FramedStream R) R :=
  modifyGet fun state => let (next, value) := state.step; (value, next)

def Stream.nextValue {R : Type} [SampleValue R] [FMA R] : StateM (Stream R) R :=
  modifyGet fun state => let (next, value) := state.step; (value, next)

end DSP.WanderingRandomLFO

namespace DSP.WanderingSchroederAllPass

structure TapSpec where
  depth : UInt64
  first : UInt64
  second : UInt64
  third : UInt64
  fourth : UInt64

instance {R : Type} [DelayBuffer.SampleIndex R] : DelayBuffer.SampleIndex (Fin 4 → R) :=
  ⟨fun n _ => DelayBuffer.SampleIndex.ofNat n, fun x => DelayBuffer.SampleIndex.toNat (x 0)⟩
instance {R : Type} [WanderingRandomLFO.SampleValue R] : WanderingRandomLFO.SampleValue (Fin 4 → R) :=
  ⟨fun x _ => WanderingRandomLFO.SampleValue.fromDouble x⟩

structure Storage (capacity : Nat) (R Clock : Type) where
  history : DelayBuffer capacity R
  clock : Clock

def initializeStage {capacity : Nat} {R Clock : Type} [OfNat R 0] (start : StateM Clock PUnit) :
    StateM (Storage capacity R Clock) PUnit := do
  focus Storage.history (fun s history => { s with history }) DelayBuffer.clear
  focus Storage.clock (fun s clock => { s with clock }) start

def step {capacity : Nat} {R Clock : Type} [OfNat R 0] [OfNat R 1] [OfNat R 2] [OfNat R 3]
    [OfNat R 4] [OfNat R 5] [OfScientific R] [Add R] [Sub R] [Neg R] [Mul R] [FMA R] [IfLess R]
    [DelayMath R] [DelayBuffer.SampleIndex R] [DelayBuffer.Gather R]
    (spec : TapSpec) (next : StateM Clock R) (input : R) : StateM (Storage capacity R Clock) R := do
  let spline ← focus Storage.clock (fun s clock => { s with clock }) next
  let offset := WanderingSchroederAllPassGeneric.offsetSamples (DelayBuffer.SampleIndex.ofWord spec.depth) spline
  let (rounded, fraction) := DelayBuffer.fractionalPosition offset
  let whole := DelayBuffer.SampleIndex.toWord rounded
  -- Each lane starts at its oldest cubic sample. The tap bounds leave room for three newer samples.
  let first := spec.first + whole - spec.depth
  let second := spec.second + whole - spec.depth
  let third := spec.third + whole - spec.depth
  let fourth := spec.fourth + whole - spec.depth
  let shared := if spec.first = spec.second ∧ spec.first = spec.third ∧ spec.first = spec.fourth then
    true else false
  let delayed ← focus Storage.history (fun s history => { s with history })
    (if shared then
      DelayBuffer.readCubicAt DelayBuffer.readWrappedAge UInt64.sub first fraction
    else
      DelayBuffer.readCubicAges (DelayBuffer.QuadAges.pack first second third fourth) fraction)
  let result := DelayKernels.allPassStep ⟨SquiggleBoyQuadReverb.allPassGain, 0, 0⟩ ⟨input, delayed⟩
  focus Storage.history (fun s history => { s with history }) (DelayBuffer.pushFrame result.written)
  return result.y

end DSP.WanderingSchroederAllPass
