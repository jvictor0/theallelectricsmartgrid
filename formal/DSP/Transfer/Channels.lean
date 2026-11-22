import DSP.Transfer.Bilinear
import DSP.Channels.Arithmetic
import Mathlib.Tactic.DeriveTraversable

/-!
Multi-channel filters with pointwise arithmetic, the model of the app's
`MultiChannelNumber`. A filter record `F R` instantiated at `R = Fin n → K`
carries n independent channels. Reading channel i is a functor map, and a
filter step written once for any arithmetic commutes with it. Each channel then
realizes the scalar transfer of its own coefficients, so every scalar
`Realizes` theorem lifts to n channels without new analysis.
-/

namespace DSP

deriving instance Functor for Sample

namespace Channels

open Bilinear

variable {n : ℕ} {K : Type}

/-- Channel i of a multi-channel record: every scalar is read at i. -/
def channel {F : Type → Type} [Functor F] (i : Fin n) (s : F (Fin n → K)) : F K :=
  (fun v => v i) <$> s

/-- Stepping n channels and then reading channel i equals stepping channel i
alone. -/
def Natural {F : Type → Type} [Functor F] (multi : Step (F (Fin n → K)) (Sample (Fin n → K)))
    (scalar : Step (F K) (Sample K)) : Prop :=
  ∀ i s x, channel i (multi s x) = scalar (channel i s) (channel i x)

variable [Field K]

/-- Channel i of the stored field realizes H i for channel i of every input. -/
def Realizes {S : Type} (p : Step S (Sample (Fin n → K))) (initial : S) (f : S → Fin n → K)
    (H : Fin n → Stream K) : Prop :=
  ∀ i (u : ℕ → Fin n → K),
    ser (fun k => f (Step.states p (fun k => ⟨u k⟩) initial (k + 1)) i) =
      H i * ser (fun k => u k i)

/-- A natural step realizes, channel by channel, the scalar transfers of its
channels. The scalar theorems precede the field equation so that they fix the
scalar field before `rfl` checks it. -/
theorem realizes {F : Type → Type} [Functor F]
    {multi : Step (F (Fin n → K)) (Sample (Fin n → K))}
    {scalar : Step (F K) (Sample K)} (hn : Natural multi scalar) {initial : F (Fin n → K)}
    {fs : F K → K} {H : Fin n → Stream K}
    (h : ∀ i, Bilinear.Realizes scalar (channel i initial) fs (H i))
    {fm : F (Fin n → K) → Fin n → K} (hf : ∀ i s, fs (channel i s) = fm s i) :
    Realizes multi initial fm H := by
  intro i u
  have hs : (fun k => fm (Step.states multi (fun k => ⟨u k⟩) initial (k + 1)) i) =
      response scalar (fun k => u k i) (channel i initial) fs := by
    funext k
    rw [← hf, Step.states_map multi scalar (channel i) (fun _ x => channel i x) (hn i)]
    rfl
  rw [hs]
  exact h i _

/-- A branch that receives the same vector input has the same channel transfers. -/
theorem Realizes.project {S T : Type}
    {p : Step S (Sample (Fin n → K))} {q : Step T (Sample (Fin n → K))}
    (π : S → T) (h : ∀ s x, π (p s x) = q (π s) x)
    {initial : S} {f : T → Fin n → K} {H : Fin n → Stream K}
    (hq : Realizes q (π initial) f H) : Realizes p initial (fun s => f (π s)) H := by
  intro i u
  have hs := Step.states_map p q π (fun _ x => x) h (fun k => ⟨u k⟩) initial
  simpa only [hs] using hq i u

/-- Driving a second independent-channel processor with the first's output
multiplies their transfers in each channel. -/
theorem Realizes.cascade {S T : Type}
    {p : Step S (Sample (Fin n → K))} {q : Step T (Sample (Fin n → K))}
    (π : S → T) (d : S → Fin n → K)
    (h : ∀ s x, π (p s x) = q (π s) ⟨d (p s x)⟩)
    {initial : S} {f : T → Fin n → K} {G H : Fin n → Stream K}
    (hd : Realizes p initial d G) (hq : Realizes q (π initial) f H) :
    Realizes p initial (fun s => f (π s)) (fun i => H i * G i) := by
  intro i u
  let v := fun k => d (Step.states p (fun j => ⟨u j⟩) initial (k + 1))
  have hs := Step.states_map p q π (fun s x => ⟨d (p s x)⟩) h (fun k => ⟨u k⟩) initial
  have hf : (fun k => f (π (Step.states p (fun j => ⟨u j⟩) initial (k + 1))) i) =
      fun k => f (Step.states q (fun j => ⟨v j⟩) (π initial) (k + 1)) i := by
    funext k
    rw [hs]
    rfl
  rw [hf, hq i v, hd i u, mul_assoc]

/-- Equal mixing adds the complex transfers before taking their magnitude. -/
theorem Realizes.average {S : Type} {p : Step S (Sample (Fin n → K))}
    {initial : S} {f g : S → Fin n → K} {F G : Fin n → Stream K}
    (hf : Realizes p initial f F) (hg : Realizes p initial g G) :
    Realizes p initial (fun s i => (f s i + g s i) / 2)
      (fun i => const (1 / 2 : K) * (F i + G i)) := by
  intro i u
  let s := fun k => Step.states p (fun j => ⟨u j⟩) initial (k + 1)
  change ser (fun k => (f (s k) i + g (s k) i) / 2) = _
  simp only [div_eq_mul_inv, one_mul] at *
  rw [show (fun k => (f (s k) i + g (s k) i) * (2 : K)⁻¹) =
    (fun k => (2 : K)⁻¹ * (f (s k) i + g (s k) i)) from funext fun k => mul_comm _ _,
    ser_mul, ser_add, hf i u, hg i u, ← add_mul, mul_assoc]

end Channels
end DSP
