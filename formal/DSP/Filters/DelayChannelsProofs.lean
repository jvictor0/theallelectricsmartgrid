import DSP.Filters.DelayChannels
import DSP.DelayBuffer.History
import DSP.Filters.TunedDecayComb.ResponseProofs
import DSP.Filters.SchroederAllPass.Proofs

namespace DSP

namespace TunedDecayComb.Channels
variable {capacity : ℕ} {ι R : Type} [Add R] [Sub R] [Mul R] [OfNat R 0] [OfNat R 2]

/-- The complete kernel projects exactly, including independent per-channel
delays and bypass choices and one shared ring head/crossfade schedule. -/
theorem project_step (c₀ c₁ : ι → Coefficients capacity R) (mix : Option R)
    (s : State capacity ι R) (x : Sample (ι → R)) (ch : ι) :
    project ch (step c₀ c₁ mix s x) =
      TunedDecayComb.step (c₀ ch) (c₁ ch) mix (project ch s) ⟨x.x ch⟩ := by
  cases mix <;> simp [project, step, TunedDecayComb.step, DelayBuffer.read_map, DelayBuffer.push_map]

theorem project_states (c₀ c₁ : ι → Coefficients capacity R) (mix : Option R)
    (s : State capacity ι R) (u : ℕ → ι → R) (ch : ι) (n : ℕ) :
    project ch (Step.states (step c₀ c₁ mix) (fun n => ⟨u n⟩) s n) =
      Step.states (TunedDecayComb.step (c₀ ch) (c₁ ch) mix) (fun n => ⟨u n ch⟩) (project ch s) n := by
  induction n with
  | zero => rfl
  | succ n ih => simp only [Step.states, project_step, ih]

end TunedDecayComb.Channels

namespace SchroederAllPass.Channels
variable {capacity : ℕ} {ι R : Type} [Neg R] [FMA R]

theorem project_step (c : Coefficients capacity ι R) (s : State capacity (ι → R))
    (x : Sample (ι → R)) (ch : ι) :
    project ch (step c s x) = SchroederAllPass.step ⟨c.age ch, c.gain ch⟩ (project ch s) ⟨x.x ch⟩ := by
  simp [project, step, stepWithStorage, SchroederAllPass.step, DelayBuffer.read_map, DelayBuffer.push_map]

theorem project_states (c : Coefficients capacity ι R) (s : State capacity (ι → R))
    (u : ℕ → ι → R) (ch : ι) (n : ℕ) :
    project ch (Step.states (step c) (fun n => ⟨u n⟩) s n) =
      Step.states (SchroederAllPass.step ⟨c.age ch, c.gain ch⟩) (fun n => ⟨u n ch⟩) (project ch s) n := by
  induction n with
  | zero => rfl
  | succ n ih => simp only [Step.states, project_step, ih]

end SchroederAllPass.Channels

noncomputable section
open Bilinear

namespace TunedDecayComb.Channels

/-- Every channel realizes its own scalar transfer, despite sharing the ring
head and having independently chosen integer delays and bypass settings. -/
theorem realizes {capacity : ℕ} {ι K : Type} [Field K]
    (c₀ c₁ : ι → Coefficients capacity K) (mix : Option K)
    (s : State capacity ι K) (u : ℕ → ι → K) (ch : ι)
    (hc₀ : Prepared (c₀ ch)) (hc₁ : Prepared (c₁ ch))
    (h0 : (project ch s).AtRest) :
    ser (fun n => (Step.states (step c₀ c₁ mix) (fun n => ⟨u n⟩) s (n + 1)).y ch) =
      ((Snapshot.mk (c₀ ch) (c₁ ch) mix).map const).response delay * ser (fun n => u n ch) := by
  have h := Snapshot.realizes ⟨c₀ ch, c₁ ch, mix⟩ hc₀ hc₁ (project ch s) h0 (fun n => u n ch)
  change ser (fun n => (Step.states (TunedDecayComb.step (c₀ ch) (c₁ ch) mix)
    (fun n => ⟨u n ch⟩) (project ch s) (n + 1)).y) = _ at h
  rw [← h]
  apply congrArg ser
  funext n
  exact congrArg TunedDecayComb.State.y (project_states c₀ c₁ mix s u ch (n + 1))

end TunedDecayComb.Channels

namespace SchroederAllPass.Channels

theorem realizes {capacity : ℕ} {ι K : Type} [Field K]
    (c : Coefficients capacity ι K) (s : State capacity (ι → K))
    (u : ℕ → ι → K) (ch : ι) (h0 : (project ch s).buffer.history = fun _ => 0) :
    ser (fun n => (Step.states (step c) (fun n => ⟨u n⟩) s (n + 1)).y ch) =
      SchroederAllPass.response (const (c.gain ch)) delay ((c.age ch).val + 1) * ser (fun n => u n ch) := by
  have h := SchroederAllPass.realizes ⟨c.age ch, c.gain ch⟩ (project ch s) h0 (fun n => u n ch)
  change ser (fun n => (Step.states (SchroederAllPass.step ⟨c.age ch, c.gain ch⟩)
    (fun n => ⟨u n ch⟩) (project ch s) (n + 1)).y) = _ at h
  rw [← h]
  apply congrArg ser
  funext n
  exact congrArg SchroederAllPass.State.y (project_states c s u ch (n + 1))

end SchroederAllPass.Channels
end
end DSP
