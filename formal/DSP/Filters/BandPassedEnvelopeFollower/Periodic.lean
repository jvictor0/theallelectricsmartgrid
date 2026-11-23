import DSP.Filters.BandPassedEnvelopeFollower.Convergence
import DSP.Analysis.PeriodicOrbit

/-! Periodic initialization of the two tuned Q=1 bandpasses and their power smoother.
Index zero is the history before the first sample. -/

noncomputable section
namespace DSP.BandPassedEnvelopeFollower.Periodic
open Filter TPTSVF.Convergence
open scoped Topology

/-- Each complex coordinate is an invertible pair of real integrator memories. -/
abbrev Memory := ℂ × ℂ × ℝ

def memory (s : BandPassedEnvelopeFollower ℝ) : Memory :=
  (coordinate s.stage1, coordinate s.stage2, s.power)

def withCoordinate (s : TPTSVF ℝ) (v : ℂ) : TPTSVF ℝ :=
  {s with ic1 := firstState v, ic2 := 2 * v.im / Real.sqrt 3}

theorem coordinate_withCoordinate (s : TPTSVF ℝ) (v : ℂ) :
    coordinate (withCoordinate s v) = v := by
  have h : Real.sqrt 3 ≠ 0 := (Real.sqrt_pos.mpr (by norm_num)).ne'
  apply Complex.ext <;> dsimp [coordinate, withCoordinate, firstState] <;> field_simp
  ring

def install (s : BandPassedEnvelopeFollower ℝ) (m : Memory) : BandPassedEnvelopeFollower ℝ :=
  {s with
    stage1 := withCoordinate s.stage1 m.1, stage2 := withCoordinate s.stage2 m.2.1
    power := m.2.2}

@[simp] theorem memory_install (s : BandPassedEnvelopeFollower ℝ) (m : Memory) :
    memory (install s m) = m := by simp [memory, install, coordinate_withCoordinate]

def modalStep (g : ℝ) (v : ℂ) (x : ℝ) : ℂ := pole g * v + drive g * (x : ℂ)

def tap (v w : ℂ) : ℝ := (firstState w + firstState v) / 2

def advance (g a : ℝ) (m : Memory) (x : ℝ) : Memory :=
  let v := modalStep g m.1 x
  let w := modalStep g m.2.1 (tap m.1 v)
  let y := tap m.2.1 w
  (v, w, (1-a)*m.2.2 + a*y^2)

def Tuned (s : BandPassedEnvelopeFollower ℝ) (g a : ℝ) : Prop :=
  s.stage1.Tuned g ∧ s.stage1.k = 1 ∧ s.stage2.Tuned g ∧ s.stage2.k = 1 ∧ s.powerAlpha = a

theorem tuned_prepared (s : BandPassedEnvelopeFollower ℝ) (g a : ℝ) (hg : 0 ≤ g) :
    Tuned (computeCoefficients ⟨g,a⟩ s) g a :=
  ⟨TPTSVF.prepareFromSnapshot_tuned ⟨g,1⟩ _ hg (by norm_num), rfl,
   TPTSVF.prepareFromSnapshot_tuned ⟨g,1⟩ _ hg (by norm_num), rfl, rfl⟩

theorem tuned_step (s : BandPassedEnvelopeFollower ℝ) (g a x : ℝ) (h : Tuned s g a) :
    Tuned (s.step ⟨x⟩) g a :=
  ⟨⟨h.1.solve,h.1.a2,h.1.a3⟩,h.2.1,⟨h.2.2.1.solve,h.2.2.1.a2,h.2.2.1.a3⟩,h.2.2.2⟩

theorem tuned_install (s : BandPassedEnvelopeFollower ℝ) (g a : ℝ) (m : Memory) (h : Tuned s g a) :
    Tuned (install s m) g a :=
  ⟨⟨h.1.solve,h.1.a2,h.1.a3⟩,h.2.1,⟨h.2.2.1.solve,h.2.2.1.a2,h.2.2.1.a3⟩,h.2.2.2⟩

theorem memory_step (s : BandPassedEnvelopeFollower ℝ) (g a x : ℝ) (h : Tuned s g a) :
    memory (s.step ⟨x⟩) = advance g a (memory s) x := by
  obtain ⟨h1,k1,h2,k2,ha⟩ := h
  have hv := step_coordinate g s.stage1 h1 k1 x
  have hw := step_coordinate g s.stage2 h2 k2 (s.stage1.step ⟨x⟩).bandPass
  have hb := bandPass_step s.stage1 x
  have hy := bandPass_step s.stage2 (s.stage1.step ⟨x⟩).bandPass
  simp only [memory, step, TPTSVF4Pole.stepBandPass, advance, modalStep, tap, OnePoleZOHLowPass.relax]
  rw [hy, hw, hb, hv, ha]
  congr 2
  ring

private def affineRun {F : Type} [Field F] (b : F) (v : ℕ → F) (x : F) :=
  Step.states (fun h u => b*h + u) v x

private theorem affine_superpose {F : Type} [Field F] (b : F) (v : ℕ → F) (x : F) (n : ℕ) :
    affineRun b v x n = b^n*x + affineRun b v 0 n := by
  induction n with
  | zero => simp [affineRun]
  | succ n ih =>
    change b * affineRun b v x n + _ = _ + (b * affineRun b v 0 n + _)
    rw [ih, pow_succ]
    ring

/-- The forced history after one period from rest divided by one minus the pole
raised to that period gives the initial history of the exact repeating solution. -/
def affineOrbit {F : Type} [Field F] (b : F) (v : ℕ → F) (N : ℕ) :=
  affineRun b v (affineRun b v 0 N / (1-b^N))

theorem affine_period {F : Type} [Field F] (b : F) (v : ℕ → F) (N : ℕ)
    (hu : Function.Periodic v N) (h : 1-b^N ≠ 0) :
    Function.Periodic (affineOrbit b v N) N := by
  intro n
  rw [Nat.add_comm]
  apply Step.states_periodic_of_fixed
  · intro i; simpa [Nat.add_comm] using hu i
  · change affineRun b v _ N = _
    rw [affine_superpose]
    field_simp
    ring

theorem affineOrbit_step {F : Type} [Field F] (b : F) (v : ℕ → F) (N n : ℕ) :
    affineOrbit b v N (n+1) = b*affineOrbit b v N n + v n := rfl

/-- All three initial histories are finite one-period evaluations, in signal order. -/
def orbit (g a : ℝ) (u : ℕ → ℝ) (N : ℕ) (n : ℕ) : Memory :=
  let v := affineOrbit (pole g) (fun i => drive g * (u i : ℂ)) N
  let w := affineOrbit (pole g) (fun i => drive g * (tap (v i) (v (i+1)) : ℂ)) N
  let p := affineOrbit (1-a) (fun i => a * tap (w i) (w (i+1)) ^ 2) N
  (v n, w n, p n)

theorem orbit_step (g a : ℝ) (u : ℕ → ℝ) (N n : ℕ) :
    orbit g a u N (n+1) = advance g a (orbit g a u N n) (u n) := rfl

theorem orbit_periodic (g a : ℝ) (hg : 0 < g) (ha : 0 < a) (ha1 : a ≤ 1)
    (u : ℕ → ℝ) (N : ℕ) (hN : 0 < N) (hu : Function.Periodic u N) :
    Function.Periodic (orbit g a u N) N := by
  have hp : 1 - pole g ^ N ≠ 0 := by
    intro h
    have hn := congrArg norm (sub_eq_zero.mp h)
    rw [norm_one, norm_pow] at hn
    exact (pow_lt_one₀ (norm_nonneg _) (pole_norm_lt_one g hg) (Nat.ne_of_gt hN)).ne' hn
  have he : 1 - (1-a)^N ≠ 0 :=
    (sub_pos.mpr (pow_lt_one₀ (by linarith) (by linarith) (Nat.ne_of_gt hN))).ne'
  let v := affineOrbit (pole g) (fun i => drive g * (u i : ℂ)) N
  let w := affineOrbit (pole g) (fun i => drive g * (tap (v i) (v (i+1)) : ℂ)) N
  have hv : Function.Periodic v N := affine_period _ _ _ (fun i => by rw [hu i]) hp
  have hw : Function.Periodic w N := by
    apply affine_period _ _ _ _ hp
    intro i
    change drive g * (tap (v (i+N)) (v (i+N+1)) : ℂ) = _
    rw [hv i, show i+N+1 = i+1+N by omega, hv (i+1)]
  have hq := affine_period (1-a) (fun i => a * tap (w i) (w (i+1)) ^ 2) N (by
    intro i
    change a * tap (w (i+N)) (w (i+N+1)) ^ 2 = _
    rw [hw i, show i+N+1 = i+1+N by omega, hw (i+1)]) he
  intro i
  exact Prod.ext (hv i) (Prod.ext (hw i) (hq i))

end DSP.BandPassedEnvelopeFollower.Periodic
