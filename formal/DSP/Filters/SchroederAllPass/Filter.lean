import Init.Data.Vector.Lemmas
import DSP.Channels.Arithmetic
import DSP.Filters.DelayChannelsProofs
import DSP.Transfer.Channels

namespace DSP

/-- A vector Schroeder stage with a single interleaved ring and a tap per channel. -/
structure SchroederAllPassFilterGeneric (capacity channels : Nat) (R : Type) where
  private mk ::
  private coefficients : SchroederAllPass.Channels.Coefficients capacity (Fin channels) R
  private state : SchroederAllPass.State capacity (Fin channels → R)

namespace SchroederAllPassFilterGeneric

variable {capacity channels : Nat} {R : Type}

def init [OfNat R 0]
    (delay : Fin channels → Nat) (valid : ∀ ch, 0 < delay ch ∧ delay ch ≤ capacity)
    (gain : Fin channels → R) (positive : 0 < capacity) :
    SchroederAllPassFilterGeneric capacity channels R :=
  let age := fun ch => DelayBuffer.ofDelay (delay ch) (valid ch).1 (valid ch).2
  ⟨⟨age, gain⟩, ⟨DelayBuffer.empty positive, fun _ => 0⟩⟩

/-- Initialize every channel at the same positive tap with independent gains. -/
def uniform [OfNat R 0] (delay : Nat) (positive : 0 < delay) (fits : delay ≤ capacity)
    (gain : Fin channels → R) : SchroederAllPassFilterGeneric capacity channels R :=
  init (fun _ => delay) (fun _ => ⟨positive, fits⟩) gain (Nat.lt_of_lt_of_le positive fits)

def gain (filter : SchroederAllPassFilterGeneric capacity channels R) : Fin channels → R :=
  filter.coefficients.gain

def y (filter : SchroederAllPassFilterGeneric capacity channels R) : Fin channels → R :=
  filter.state.y

def step [Neg R] [FMA R] (filter : SchroederAllPassFilterGeneric capacity channels R)
    (input : Sample (Fin channels → R)) : SchroederAllPassFilterGeneric capacity channels R :=
  let state := SchroederAllPass.Channels.stepWithStorage Vector.ofFn Vector.get
    filter.coefficients filter.state input
  { filter with state }

noncomputable section

def AtRest [OfNat R 0] (filter : SchroederAllPassFilterGeneric capacity channels R) : Prop :=
  ∀ ch, (SchroederAllPass.Channels.project ch filter.state).buffer.history = fun _ => 0

@[simp] theorem init_atRest [OfNat R 0]
    (delay : Fin channels → Nat) (valid : ∀ ch, 0 < delay ch ∧ delay ch ≤ capacity)
    (gain : Fin channels → R) (positive : 0 < capacity) :
    (init delay valid gain positive).AtRest := by
  intro ch
  funext age
  simp only [init, SchroederAllPass.Channels.project, DelayBuffer.history, DelayBuffer.read_map]
  simp [DelayBuffer.empty, DelayBuffer.read]

@[simp] theorem uniform_atRest [OfNat R 0] (delay : Nat) (positive : 0 < delay)
    (fits : delay ≤ capacity) (gain : Fin channels → R) : (uniform delay positive fits gain).AtRest :=
  init_atRest _ _ _ _

def response {K : Type} [Field K] (gainMap : R → K) (q : K)
    (filter : SchroederAllPassFilterGeneric capacity channels R) (ch : Fin channels) : K :=
  SchroederAllPass.response (gainMap (filter.coefficients.gain ch)) q
    ((filter.coefficients.age ch).val + 1)

private theorem unpack_packed (values : Fin channels → R) :
    Vector.get (Vector.ofFn values) = values := by
  funext ch
  simp [Vector.get]

theorem realizes {K : Type} [Field K] (filter : SchroederAllPassFilterGeneric capacity channels K)
    (rest : filter.AtRest) : Channels.Realizes step filter y (response Bilinear.const Bilinear.delay filter) := by
  intro ch u
  let input : Nat → Sample (Fin channels → K) := fun n => ⟨u n⟩
  have hc (n : Nat) : (Step.states step input filter n).coefficients = filter.coefficients := by
    induction n with
    | zero => rfl
    | succ n ih => exact ih
  have hs (n : Nat) : (Step.states step input filter n).state =
      Step.states (SchroederAllPass.Channels.step filter.coefficients) input filter.state n := by
    induction n with
    | zero => rfl
    | succ n ih =>
      simp only [Step.states_succ, step, hc, ih, SchroederAllPass.Channels.step,
        SchroederAllPass.Channels.stepWithStorage, unpack_packed, id_eq]
  change Bilinear.ser (fun n => (Step.states step input filter (n + 1)).state.y ch) = _
  simp only [hs]
  exact SchroederAllPass.Channels.realizes filter.coefficients filter.state u ch (rest ch)

theorem response_unit (filter : SchroederAllPassFilterGeneric capacity channels ℝ)
    (ch : Fin channels) (q : ℂ) (gain : |filter.gain ch| < 1)
    (unit : Complex.normSq q = 1) :
    Complex.normSq (filter.response (fun g : ℝ => (g : ℂ)) q ch) = 1 :=
  SchroederAllPass.response_unit _ _ _ gain unit

end

end SchroederAllPassFilterGeneric

/-- A serial chain with each stage's ring capacity fixed by its position in the type. -/
inductive SchroederAllPassCascade (channels : Nat) (R : Type) : List Nat → Type where
  | passthrough (y : Fin channels → R) : SchroederAllPassCascade channels R []
  | series {capacity : Nat} {capacities : List Nat}
      (first : SchroederAllPassFilterGeneric capacity channels R)
      (rest : SchroederAllPassCascade channels R capacities) :
      SchroederAllPassCascade channels R (capacity :: capacities)

namespace SchroederAllPassCascade

variable {channels : Nat} {R : Type}

def y : {capacities : List Nat} → SchroederAllPassCascade channels R capacities → Fin channels → R
  | _, .passthrough x => x
  | _, .series _ rest => rest.y

def step [Neg R] [FMA R] : {capacities : List Nat} →
    SchroederAllPassCascade channels R capacities → Sample (Fin channels → R) →
    SchroederAllPassCascade channels R capacities
  | _, .passthrough _, input => .passthrough input.x
  | _, .series first rest, input =>
      let next := first.step input
      .series next (rest.step ⟨next.y⟩)

end SchroederAllPassCascade

/-- Two serial all-pass branches that see the same input and mix at equal gain. -/
structure ParallelSchroederAllPassCascades (shortCapacities longCapacities : List Nat)
    (channels : Nat) (R : Type) where
  short : SchroederAllPassCascade channels R shortCapacities
  long : SchroederAllPassCascade channels R longCapacities

namespace ParallelSchroederAllPassCascades

variable {shortCapacities longCapacities : List Nat} {channels : Nat} {R : Type}

def mixOutputs {V : Type} [Add V] [Div V] [OfNat V 2] (short long : V) : V :=
  (short + long) / 2

def y [Add R] [Div R] [OfNat R 2]
    (bank : ParallelSchroederAllPassCascades shortCapacities longCapacities channels R) :
    Fin channels → R :=
  fun channel => mixOutputs (bank.short.y channel) (bank.long.y channel)

def step [Neg R] [FMA R]
    (bank : ParallelSchroederAllPassCascades shortCapacities longCapacities channels R)
    (input : Sample (Fin channels → R)) :
    ParallelSchroederAllPassCascades shortCapacities longCapacities channels R :=
  ⟨bank.short.step input, bank.long.step input⟩

end ParallelSchroederAllPassCascades
end DSP
