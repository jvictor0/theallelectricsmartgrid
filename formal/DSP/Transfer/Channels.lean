import DSP.Transfer.Bilinear
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

/-- A decimal literal is the same value in every channel, like `Vector::Splat`. -/
instance {ι K : Type} [OfScientific K] : OfScientific (ι → K) :=
  ⟨fun m s e _ => OfScientific.ofScientific m s e⟩

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

end Channels
end DSP
