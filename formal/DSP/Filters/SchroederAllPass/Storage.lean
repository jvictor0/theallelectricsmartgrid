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

/-- Four separately updated clocks for a quad processing stage. -/
structure QuadClock (Clock : Type) where
  lane0 : Clock
  lane1 : Clock
  lane2 : Clock
  lane3 : Clock

namespace QuadClock

def initializeLanes {Clock : Type} (start : Fin 4 → StateM Clock PUnit) :
    StateM (QuadClock Clock) PUnit := do
  focus QuadClock.lane0 (fun s lane0 => { s with lane0 }) (start 0)
  focus QuadClock.lane1 (fun s lane1 => { s with lane1 }) (start 1)
  focus QuadClock.lane2 (fun s lane2 => { s with lane2 }) (start 2)
  focus QuadClock.lane3 (fun s lane3 => { s with lane3 }) (start 3)

def nextValue {R Clock : Type} [DelayBuffer.Gather R] (next : StateM Clock R) :
    StateM (QuadClock Clock) R := do
  let lane0 ← focus QuadClock.lane0 (fun s lane0 => { s with lane0 }) next
  let lane1 ← focus QuadClock.lane1 (fun s lane1 => { s with lane1 }) next
  let lane2 ← focus QuadClock.lane2 (fun s lane2 => { s with lane2 }) next
  let lane3 ← focus QuadClock.lane3 (fun s lane3 => { s with lane3 }) next
  pure (DelayBuffer.Gather.gather ![lane0, lane1, lane2, lane3])

end QuadClock

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
    [DelayMath R] [DelayBuffer.SampleIndex R] [DelayBuffer.SampleAges R] [DelayBuffer.Gather R]
    (spec : TapSpec) (next : StateM Clock R) (input : R) : StateM (Storage capacity R Clock) R := do
  let spline ← focus Storage.clock (fun s clock => { s with clock }) next
  let offset := WanderingSchroederAllPassGeneric.offsetSamples (DelayBuffer.SampleIndex.ofWord spec.depth) spline
  let (rounded, fraction) := DelayBuffer.fractionalPosition offset
  -- The fit bounds leave three newer samples after each lane's oldest tap.
  let base := DelayBuffer.Gather.gather ![
    DelayBuffer.SampleIndex.ofWord (spec.first - spec.depth),
    DelayBuffer.SampleIndex.ofWord (spec.second - spec.depth),
    DelayBuffer.SampleIndex.ofWord (spec.third - spec.depth),
    DelayBuffer.SampleIndex.ofWord (spec.fourth - spec.depth)]
  let oldest := DelayBuffer.SampleAges.toAges (base + rounded)
  let delayed ← focus Storage.history (fun s history => { s with history })
    (DelayBuffer.readCubicAges oldest fraction)
  let result := DelayKernels.allPassStep ⟨SquiggleBoyQuadReverb.allPassGain, 0, 0⟩ ⟨input, delayed⟩
  focus Storage.history (fun s history => { s with history }) (DelayBuffer.pushFrame result.written)
  return result.y

end DSP.WanderingSchroederAllPass
