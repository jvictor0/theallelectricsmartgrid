import DSP.Channels.Arithmetic
import DSP.DelayBuffer.Cubic
import DSP.Filters.DelayKernels
import DSP.WanderingRandomLFO

namespace DSP

/-- A Schroeder stage with a spline generator for each channel and an
interleaved delay history. -/
structure WanderingSchroederAllPassGeneric (capacity channels : Nat) (R : Type) where
  private mk ::
  private history : DelayBuffer capacity (Fin channels → R)
  private base : Fin channels → Nat
  private gain : Fin channels → R
  private depth : Nat
  private lfo : Vector (WanderingRandomLFO.FramedStream R) channels
  private output : Fin channels → R

namespace WanderingSchroederAllPassGeneric

structure WanderSpec where
  depth : Nat
  lambdaSamples : Float

variable {capacity channels : Nat} {R : Type}

def init [OfNat R 0] [WanderingRandomLFO.SampleValue R] [Sub R] [Div R] [FMA R]
    (tap : Fin channels → Nat) (gain : Fin channels → R)
    (spec : WanderSpec) (seed : Fin channels → UInt64)
    (_fits : ∀ ch, DelayBuffer.CubicOffsetFits capacity (tap ch) spec.depth)
    (positive : 0 < capacity) :
    WanderingSchroederAllPassGeneric capacity channels R :=
  ⟨DelayBuffer.empty positive, fun ch => tap ch - spec.depth, gain, spec.depth,
    Vector.ofFn (fun ch => WanderingRandomLFO.FramedStream.init spec.lambdaSamples (seed ch)),
    fun _ => 0⟩

def uniform [OfNat R 0] [WanderingRandomLFO.SampleValue R] [Sub R] [Div R] [FMA R]
    (tap : Nat) (gain : Fin channels → R) (spec : WanderSpec) (seed : Fin channels → UInt64)
    (fits : DelayBuffer.CubicOffsetFits capacity tap spec.depth) :
    WanderingSchroederAllPassGeneric capacity channels R :=
  init (fun _ => tap) gain spec seed (fun _ => fits) (by have := fits.2; omega)

def y (filter : WanderingSchroederAllPassGeneric capacity channels R) : Fin channels → R :=
  filter.output

/-- Limit the finite-precision spline to its control point range before
converting its bipolar output to an offset from the shortest tap. -/
def offsetSamples {V : Type} [OfNat V 1] [Add V] [Neg V] [Mul V] [IfLess V]
    (depth spline : V) : V :=
  depth * (1 + maxValue (-1) (minValue 1 spline))

def step [OfNat R 0] [OfNat R 1] [OfNat R 2] [OfNat R 3] [OfNat R 4] [OfNat R 5]
    [OfScientific R] [Add R] [Sub R] [Neg R] [Mul R] [Div R] [FMA R] [IfLess R]
    [DelayMath R] [DelayBuffer.SampleIndex R] [WanderingRandomLFO.SampleValue R]
    (filter : WanderingSchroederAllPassGeneric capacity channels R) (input : Sample (Fin channels → R)) :
    WanderingSchroederAllPassGeneric capacity channels R :=
  let next := filter.lfo.map WanderingRandomLFO.FramedStream.step
  let offset := fun ch : Fin channels =>
    offsetSamples (DelayBuffer.SampleIndex.ofNat filter.depth) next[ch].2
  let delayed := filter.history.readCubicOffsets filter.base offset
  let result := Vector.ofFn fun ch =>
    DelayKernels.allPassStep ⟨filter.gain ch, 0, 0⟩ ⟨input.x ch, delayed ch⟩
  let written := Vector.ofFn fun (ch : Fin channels) => result[ch].written
  let output := Vector.ofFn fun (ch : Fin channels) => result[ch].y
  { filter with
    lfo := next.map Prod.fst
    history := filter.history.push (fun ch => written[ch])
    output := fun ch => output[ch] }

end WanderingSchroederAllPassGeneric

inductive WanderingSchroederAllPassCascade (channels : Nat) (R : Type) : List Nat → Type where
  | passthrough (output : Fin channels → R) : WanderingSchroederAllPassCascade channels R []
  | series {capacity : Nat} {rest : List Nat}
      (stage : WanderingSchroederAllPassGeneric capacity channels R)
      (next : WanderingSchroederAllPassCascade channels R rest) :
      WanderingSchroederAllPassCascade channels R (capacity :: rest)

namespace WanderingSchroederAllPassCascade

variable {channels : Nat} {R : Type}

def y : {capacities : List Nat} → WanderingSchroederAllPassCascade channels R capacities → Fin channels → R
  | _, .passthrough output => output
  | _, .series _ rest => rest.y

def step [OfNat R 0] [OfNat R 1] [OfNat R 2] [OfNat R 3] [OfNat R 4] [OfNat R 5]
    [OfScientific R] [Add R] [Sub R] [Neg R] [Mul R] [Div R] [FMA R] [IfLess R]
    [DelayMath R] [DelayBuffer.SampleIndex R] [WanderingRandomLFO.SampleValue R] :
    {capacities : List Nat} → WanderingSchroederAllPassCascade channels R capacities →
    Sample (Fin channels → R) → WanderingSchroederAllPassCascade channels R capacities
  | _, .passthrough _, input => .passthrough input.x
  | _, .series stage rest, input =>
    let next := stage.step input
    .series next (rest.step ⟨next.y⟩)

end WanderingSchroederAllPassCascade
end DSP
