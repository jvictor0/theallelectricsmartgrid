import DSP.Filters.SchroederAllPass.Filter

noncomputable section

namespace DSP
open Bilinear

namespace SchroederAllPassCascade

variable {channels : Nat} {R : Type}

def AtRest [OfNat R 0] : {capacities : List Nat} → SchroederAllPassCascade channels R capacities → Prop
  | _, .passthrough _ => True
  | _, .series first rest => first.AtRest ∧ rest.AtRest

def response {K : Type} [Field K] (gainMap : R → K) (q : K) :
    {capacities : List Nat} → SchroederAllPassCascade channels R capacities → Fin channels → K
  | _, .passthrough _ => fun _ => 1
  | _, .series first rest => fun ch => rest.response gainMap q ch * first.response gainMap q ch

theorem realizes {K : Type} [Field K] {capacities : List Nat}
    (chain : SchroederAllPassCascade channels K capacities) (rest : chain.AtRest) :
    Channels.Realizes step chain y (response const delay chain) := by
  induction chain with
  | passthrough _ =>
    intro ch u
    have identity (s : SchroederAllPassCascade channels K []) (x : Sample (Fin channels → K)) :
        (s.step x).y = x.x := by cases s; rfl
    simp only [Step.states_succ, identity, response, one_mul]
  | @series capacity capacities first tail ih =>
    let next : Step (SchroederAllPassCascade channels K (capacity :: capacities))
        (Sample (Fin channels → K)) := step
    let headStage : SchroederAllPassCascade channels K (capacity :: capacities) →
        SchroederAllPassFilterGeneric capacity channels K := fun | .series stage _ => stage
    let tailStages : SchroederAllPassCascade channels K (capacity :: capacities) →
        SchroederAllPassCascade channels K capacities := fun | .series _ stages => stages
    have headStep (s : SchroederAllPassCascade channels K (capacity :: capacities))
        (x : Sample (Fin channels → K)) : headStage (next s x) = (headStage s).step x := by
      cases s
      rfl
    have tailStep (s : SchroederAllPassCascade channels K (capacity :: capacities))
        (x : Sample (Fin channels → K)) :
        tailStages (next s x) = (tailStages s).step ⟨(headStage (next s x)).y⟩ := by
      cases s
      rfl
    have headTransfer : Channels.Realizes next (.series first tail) (fun s => (headStage s).y)
        (first.response const delay) :=
      Channels.Realizes.project headStage headStep (first.realizes rest.1)
    have tailTransfer : Channels.Realizes step (tailStages (.series first tail)) y
        (tail.response const delay) := ih rest.2
    have combined := Channels.Realizes.cascade tailStages (fun s => (headStage s).y) tailStep
      headTransfer tailTransfer
    have readout : (fun s : SchroederAllPassCascade channels K (capacity :: capacities) =>
        (tailStages s).y) = y := by
      funext s
      cases s
      rfl
    rw [readout] at combined
    simpa only [response] using combined

def StableGains (ch : Fin channels) :
    {capacities : List Nat} → SchroederAllPassCascade channels ℝ capacities → Prop
  | _, .passthrough _ => True
  | _, .series first tail => |first.gain ch| < 1 ∧ StableGains ch tail

theorem response_unit {capacities : List Nat} (chain : SchroederAllPassCascade channels ℝ capacities)
    (ch : Fin channels) (q : ℂ) (unit : Complex.normSq q = 1)
    (bounded : StableGains ch chain) :
    Complex.normSq (chain.response (fun g : ℝ => (g : ℂ)) q ch) = 1 := by
  induction chain with
  | passthrough _ => simp [response]
  | series first tail ih =>
    rw [response, Complex.normSq_mul, ih bounded.2, first.response_unit ch q bounded.1 unit, one_mul]

end SchroederAllPassCascade

namespace ParallelSchroederAllPassCascades

variable {shortCapacities longCapacities : List Nat} {channels : Nat} {R : Type}

def response {K : Type} [Field R] [Field K] (gainMap : R → K) (q : K)
    (bank : ParallelSchroederAllPassCascades shortCapacities longCapacities channels R)
    (ch : Fin channels) : K :=
  gainMap (1 / 2) * (bank.short.response gainMap q ch + bank.long.response gainMap q ch)

theorem realizes {K : Type} [Field K]
    (bank : ParallelSchroederAllPassCascades shortCapacities longCapacities channels K)
    (shortRest : bank.short.AtRest) (longRest : bank.long.AtRest) :
    Channels.Realizes step bank y (response const delay bank) :=
  Channels.Realizes.average
    (Channels.Realizes.project short (fun _ _ => rfl) (bank.short.realizes shortRest))
    (Channels.Realizes.project long (fun _ _ => rfl) (bank.long.realizes longRest))

end ParallelSchroederAllPassCascades
end DSP
