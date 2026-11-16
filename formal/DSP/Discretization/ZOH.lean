import DSP.StateSpace.Analog

namespace DSP.StateSpace

open LinearFlow

variable {n : ℕ}

noncomputable def zoh (s : StateSpace n ℝ) (T : ℝ) : StateSpace n ℝ where
  A := LinearMap.toMatrix' (flow s.operator T).toLinearMap
  B := inputGain s.operator s.B T
  C := s.C
  D := s.D

theorem zoh_advance (s : StateSpace n ℝ) (T : ℝ) (q : Fin n → ℝ) (u : ℝ) :
    (s.zoh T).advance q u = flow s.operator T q + u • inputGain s.operator s.B T := by
  rw [advance_eq]
  simp only [zoh, LinearMap.toMatrix'_mulVec, ContinuousLinearMap.coe_coe]
  congr 1
  funext i
  simp [mul_comm]

theorem zoh_interval (s : StateSpace n ℝ) (T : ℝ) (hT : 0 ≤ T) (u : ℝ)
    (q : ℝ → Fin n → ℝ) (hc : ContinuousOn q (Set.Icc 0 T))
    (hd : ∀ t ∈ Set.Ioo 0 T, HasDerivAt q (s.operator (q t) + u • s.B) t) :
    q T = (s.zoh T).advance (q 0) u := by
  rw [zoh_advance]
  exact held_interval s.operator s.B (q 0) u T hT q hc rfl hd

theorem zoh_sampledState (s : StateSpace n ℝ) (T : ℝ) (hT : 0 ≤ T)
    (u : ℕ → ℝ) (initial : Fin n → ℝ) (q : s.HeldRealization T u initial) :
    q.sample = Processor.runState (s.zoh T).processor u initial := by
  apply Processor.runState_unique _ _ _ _ q.starts
  intro k
  simp only [processor, snapshot_values]
  exact (q.joins k).symm.trans (s.zoh_interval T hT (u k) (q.segment k)
    (q.continuous k) (q.derivative k))

noncomputable def heldRealization (s : StateSpace n ℝ) (T : ℝ) (u : ℕ → ℝ)
    (initial : Fin n → ℝ) : s.HeldRealization T u initial where
  segment k := heldTrajectory s.operator s.B (Processor.runState (s.zoh T).processor u initial k) (u k)
  starts := by simp
  continuous k := (heldTrajectory_continuous _ _ _ _).continuousOn
  derivative k t _ := heldTrajectory_deriv _ _ _ _ _
  joins k := by
    simp only [heldTrajectory_zero, Processor.runState_succ, processor, snapshot_values]
    exact (s.zoh_advance T _ _).symm

theorem heldRealization_unique (s : StateSpace n ℝ) (T : ℝ) (hT : 0 ≤ T)
    (u : ℕ → ℝ) (initial : Fin n → ℝ) (q r : s.HeldRealization T u initial)
    (k : ℕ) (t : ℝ) (ht : t ∈ Set.Icc 0 T) : q.segment k t = r.segment k t := by
  have hi : q.segment k 0 = r.segment k 0 :=
    congrFun ((s.zoh_sampledState T hT u initial q).trans
      (s.zoh_sampledState T hT u initial r).symm) k
  apply interval_unique s.operator (q.segment k) (r.segment k) (fun _ => u k • s.B) t ht.1
    ((q.continuous k).mono (Set.Icc_subset_Icc le_rfl ht.2))
    ((r.continuous k).mono (Set.Icc_subset_Icc le_rfl ht.2))
    (fun x hx => q.derivative k x ⟨hx.1, lt_of_lt_of_le hx.2 ht.2⟩)
    (fun x hx => r.derivative k x ⟨hx.1, lt_of_lt_of_le hx.2 ht.2⟩) hi

theorem zoh_sampledOutput (s : StateSpace n ℝ) (T : ℝ) (hT : 0 ≤ T)
    (u : ℕ → ℝ) (initial : Fin n → ℝ) (q : s.HeldRealization T u initial) (k : ℕ) :
    s.observe (q.sample k) (u k) = Processor.run (s.zoh T).processor u initial k := by
  rw [s.zoh_sampledState T hT u initial q]
  rfl

theorem zoh_endpointOutput (s : StateSpace n ℝ) (T : ℝ) (hT : 0 ≤ T)
    (u : ℕ → ℝ) (initial : Fin n → ℝ) (q : s.HeldRealization T u initial) (k : ℕ) :
    s.observe (q.segment k T) (u k) =
      s.observe ((s.zoh T).advance (q.sample k) (u k)) (u k) := by
  rw [s.zoh_interval T hT (u k) (q.segment k) (q.continuous k) (q.derivative k)]
  rfl

end DSP.StateSpace
