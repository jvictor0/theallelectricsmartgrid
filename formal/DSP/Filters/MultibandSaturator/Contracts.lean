import DSP.Filters.MultibandSaturator.ResponseProofs
import DSP.Nonlinear.SaturatingSum

namespace DSP.MultibandSaturator

section Processing
variable {n : Nat} [NeZero n] {R : Type} [OfNat R 0] [OfNat R 2]
  [Add R] [Sub R] [Neg R] [Mul R] [FMA R]

theorem runState_splitter (mono : Bool) (mix sat : R → R) (s : State n R)
    (u : ℕ → Sample R) (k : ℕ) :
    (Processor.runState (stepWith mono mix sat) u s k).splitter =
      Step.states LinkwitzRileySplitter.step u s.splitter k := by
  induction k with
  | zero => rfl
  | succ k ih =>
    change LinkwitzRileySplitter.step (Processor.runState (stepWith mono mix sat) u s k).splitter (u k) = _
    rw [ih]
    rfl

theorem runState_gains (mono : Bool) (mix sat : R → R) (s : State n R)
    (u : ℕ → Sample R) (k : ℕ) :
    (Processor.runState (stepWith mono mix sat) u s k).bandGain = s.bandGain ∧
    (Processor.runState (stepWith mono mix sat) u s k).masterGain = s.masterGain := by
  induction k with
  | zero => exact ⟨rfl,rfl⟩
  | succ k ih => exact ih

end Processing

noncomputable section
open scoped BigOperators

theorem step_output_eq {n : Nat} [NeZero n] (s : State n ℝ) (x : ℝ) :
    (step s ⟨x⟩).2.master.after = SaturatingSum.apply s.bandGain s.masterGain
      (LinkwitzRileySplitter.step s.splitter ⟨x⟩).bands := by
  simp [step,stepWith,StateSpace.snapshot_values,sum_eq_sum,SaturatingSum.apply_eq,mul_comm]

theorem output_eq {n : Nat} [NeZero n] (s : State n ℝ) (u : ℕ → ℝ) (k : ℕ) :
    (Processor.run step (fun i => ⟨u i⟩) s k).master.after =
      SaturatingSum.apply s.bandGain s.masterGain
        (Step.states LinkwitzRileySplitter.step (fun i => ⟨u i⟩) s.splitter (k+1)).bands := by
  have hg := runState_gains false id (Tanh.tanh : ℝ → ℝ) s (fun i => ⟨u i⟩) k
  have hs := runState_splitter false id (Tanh.tanh : ℝ → ℝ) s (fun i => ⟨u i⟩) k
  change (step (Processor.runState step (fun i => ⟨u i⟩) s k) ⟨u k⟩).2.master.after = _
  rw [step_output_eq]
  change SaturatingSum.apply (Processor.runState (stepWith false id Tanh.tanh) _ s k).bandGain
    (Processor.runState (stepWith false id Tanh.tanh) _ s k).masterGain
    (LinkwitzRileySplitter.step (Processor.runState (stepWith false id Tanh.tanh) _ s k).splitter ⟨u k⟩).bands = _
  rw [hg.1,hg.2,hs]
  rfl

theorem prepared_splitter {n : Nat} (spec : LogicalSpec n ℝ) (s : State n ℝ) :
    (computeCoefficients spec s).splitter = LinkwitzRileySplitter.computeCoefficients spec.splitter s.splitter := by
  exact LinkwitzRileySplitter.install_prepare spec.splitter s.splitter

theorem prepared_snapshot {n : Nat} (spec : LogicalSpec n ℝ) :
    (prepare spec).snapshot = ⟨LinkwitzRileySplitter.makeSnapshot spec.splitter,spec.bandGain,spec.masterGain⟩ := by
  simp [prepare,PreparedSpec.snapshot,LinkwitzRileySplitter.prepare_snapshot,StateSpace.snapshot_values]

end
end DSP.MultibandSaturator
