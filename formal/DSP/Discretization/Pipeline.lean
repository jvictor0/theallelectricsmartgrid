import DSP.Discretization.ZOH
import DSP.Transfer.Delay

namespace DSP.StateSpace

variable {n : ℕ}

/-- The complete zero-state chain, sampled at the beginning of each hold. -/
theorem zoh_iir_sampledOutput (s : StateSpace n ℝ) (T : ℝ) (hT : 0 ≤ T)
    (u : ℕ → ℝ) (q : s.HeldRealization T u (fun _ => 0)) (k : ℕ) :
    Processor.run (IIR.processor (s.zoh T).toIIR) u IIR.zeroState k =
      s.observe (q.sample k) (u k) := by
  rw [toIIR_correct]
  exact (s.zoh_sampledOutput T hT u _ q k).symm

theorem runState_observeNext {R : Type} [CommRing R] (s : StateSpace n R)
    (u : ℕ → R) (initial : Fin n → R) :
    Processor.runState s.observeNext.processor u initial = Processor.runState s.processor u initial := by
  funext k
  induction k with
  | zero => rfl
  | succ k ih =>
    simp only [Processor.runState_succ, processor, snapshot_values]
    change s.advance _ _ = s.advance _ _
    rw [ih]

/-- Endpoint output uses the interval's held input, including direct feedthrough. -/
theorem zoh_iir_endpointOutput (s : StateSpace n ℝ) (T : ℝ) (hT : 0 ≤ T)
    (u : ℕ → ℝ) (q : s.HeldRealization T u (fun _ => 0)) (k : ℕ) :
    Processor.run (IIR.processor (s.zoh T).observeNext.toIIR) u IIR.zeroState k =
      s.observe (q.segment k T) (u k) := by
  rw [toIIR_correct, Processor.run, observeNext_equiv, processorNext, snapshot_values, runState_observeNext]
  rw [← s.zoh_sampledState T hT u _ q]
  exact (s.zoh_endpointOutput T hT u _ q k).symm

end DSP.StateSpace
