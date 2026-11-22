import DSP.Channels.Arithmetic
import DSP.DelayBuffer.Cubic
import DSP.Filters.DelayKernels
import DSP.SmoothRandomLFO.Stream

namespace DSP

/-- A Schroeder stage with a single fractional offset shared by its channels.
Each stage owns both its spline generator and its interleaved history. -/
structure WanderingSchroederAllPassGeneric (capacity channels : Nat) (R : Type) where
  private mk ::
  private history : DelayBuffer capacity (Fin channels → R)
  private base : Fin channels → Nat
  private gain : Fin channels → R
  private depth : Nat
  private lfo : Option (SmoothRandomLFO.FramedStream R)
  private output : Fin channels → R

namespace WanderingSchroederAllPassGeneric

structure WanderSpec where
  depth : Nat
  lambdaSamples : Float

variable {capacity channels : Nat} {R : Type}

def init [OfNat R 0] [SmoothRandomLFO.SampleValue R] [Sub R] [Div R] [FMA R]
    (tap : Fin channels → Nat) (gain : Fin channels → R)
    (spec : WanderSpec) (seed : UInt64)
    (_fits : ∀ ch, DelayBuffer.CubicOffsetFits capacity (tap ch) spec.depth)
    (positive : 0 < capacity) :
    WanderingSchroederAllPassGeneric capacity channels R :=
  ⟨DelayBuffer.empty positive, fun ch => tap ch - spec.depth, gain, spec.depth,
    if spec.depth = 0 then none else some (SmoothRandomLFO.FramedStream.init spec.lambdaSamples seed),
    fun _ => 0⟩

def uniform [OfNat R 0] [SmoothRandomLFO.SampleValue R] [Sub R] [Div R] [FMA R]
    (tap : Nat) (gain : Fin channels → R) (spec : WanderSpec) (seed : UInt64)
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
    [DelayMath R] [DelayBuffer.SampleIndex R] [SmoothRandomLFO.SampleValue R]
    (filter : WanderingSchroederAllPassGeneric capacity channels R) (input : Sample (Fin channels → R)) :
    WanderingSchroederAllPassGeneric capacity channels R :=
  let (lfo, delayed) := match filter.lfo with
    | none =>
      let positive := Nat.zero_lt_of_lt filter.history.head.isLt
      (none, filter.history.readChannels fun ch =>
        ⟨(filter.base ch + capacity - 1) % capacity, Nat.mod_lt _ positive⟩)
    | some lfo =>
      let (next, value) := lfo.step
      let offset := offsetSamples (DelayBuffer.SampleIndex.ofNat filter.depth) value
      (some next, filter.history.readCubicOffset filter.base offset)
  let result := Vector.ofFn fun ch =>
    DelayKernels.allPassStep ⟨filter.gain ch, 0, 0⟩ ⟨input.x ch, delayed ch⟩
  let written := Vector.ofFn fun (ch : Fin channels) => result[ch].written
  let output := Vector.ofFn fun (ch : Fin channels) => result[ch].y
  { filter with
    lfo
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
    [DelayMath R] [DelayBuffer.SampleIndex R] [SmoothRandomLFO.SampleValue R] :
    {capacities : List Nat} → WanderingSchroederAllPassCascade channels R capacities →
    Sample (Fin channels → R) → WanderingSchroederAllPassCascade channels R capacities
  | _, .passthrough _, input => .passthrough input.x
  | _, .series stage rest, input =>
    let next := stage.step input
    .series next (rest.step ⟨next.y⟩)

end WanderingSchroederAllPassCascade
end DSP
