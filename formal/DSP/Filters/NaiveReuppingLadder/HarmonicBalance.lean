import DSP.Analysis.Harmonics
import DSP.Filters.NaiveReuppingLadder.Contracts
import Mathlib.Analysis.SpecialFunctions.Trigonometric.DerivHyp

/-! Exact, untruncated harmonic balance for the detector-disabled ladder.
Waveforms may have any finite number of phases. This includes input-locked
responses and candidate input/oscillator tori without asserting their existence
or attraction. The nonlinear Fourier coefficients are those of the FULL
normalized tanh waveform, not a slope approximation or a truncated polynomial.
The implementation bridge below uses the existing `NaiveReuppingLadder.step`. -/

noncomputable section
namespace DSP.NaiveReuppingLadder.HarmonicBalance
open DSP.Harmonics

variable {d : Type*} [Fintype d]

structure Orbit (d : Type*) where
  stage1 : Waveform d
  stage2 : Waveform d
  stage3 : Waveform d
  stage4 : Waveform d

def saturation (c : NormalizedTanh.Coefficients ℝ) : C(ℝ, ℝ) where
  toFun := NormalizedTanh.apply c
  continuous_toFun := by
    change Continuous (fun x : ℝ => Real.tanh (x * c.gain) / c.tanhGain)
    simp_rw [Real.tanh_eq_sinh_div_cosh]
    exact ((Real.continuous_sinh.comp (continuous_id.mul_const _)).div
      (Real.continuous_cosh.comp (continuous_id.mul_const _))
      (fun x => ne_of_gt (Real.cosh_pos _))).div_const _

def saturated (s : NaiveReuppingLadder ℝ) (f : Waveform d) : Waveform d :=
  (saturation s.saturation).comp f

def output (s : NaiveReuppingLadder ℝ) (q : Orbit d) : Waveform d :=
  (s.baseFeedback + 1) • saturated s q.stage4

/-- The nonlinear spectral operation reconstructs the complete signal before
applying tanh. Replacing this with tanh of each coefficient would be incorrect. -/
theorem saturated_from_spectrum (s : NaiveReuppingLadder ℝ) (f : Waveform d)
    (h : Summable (coeff f)) (θ : Phase d) :
    saturated s f θ = NormalizedTanh.apply s.saturation (synthesize (coeff f) θ).re := by
  rw [synthesize_coeff f h]
  rfl

/-- Current-stage equations; feedback alone reads the previous fourth stage. -/
def Equations (s : NaiveReuppingLadder ℝ) (ω : Phase d) (u : Waveform d)
    (q : Orbit d) : Prop :=
  q.stage1 = (1-s.alpha) • delay ω q.stage1 +
    s.alpha • (u - s.baseFeedback • delay ω q.stage4) ∧
  q.stage2 = (1-s.alpha) • delay ω q.stage2 + s.alpha • saturated s q.stage1 ∧
  q.stage3 = (1-s.alpha) • delay ω q.stage3 + s.alpha • saturated s q.stage2 ∧
  q.stage4 = (1-s.alpha) • delay ω q.stage4 + s.alpha • saturated s q.stage3

def denominator (α : ℝ) (ω : Phase d) (k : d → ℤ) : ℂ :=
  1 - (1 - (α : ℂ)) * delayFactor ω k

def onePoleResponse (α : ℝ) (ω : Phase d) (k : d → ℤ) : ℂ :=
  (α : ℂ) / denominator α ω k

/-- Every integer multi-index is present. A finite solver enforces only a
projection of these equations and must account for its remaining residual. -/
def Balance (s : NaiveReuppingLadder ℝ) (ω : Phase d) (u : Waveform d)
    (q : Orbit d) : Prop :=
  (∀ k, denominator s.alpha ω k * coeff q.stage1 k =
    (s.alpha : ℂ) * (coeff u k - (s.baseFeedback : ℂ) * delayFactor ω k * coeff q.stage4 k)) ∧
  (∀ k, denominator s.alpha ω k * coeff q.stage2 k =
    (s.alpha : ℂ) * coeff (saturated s q.stage1) k) ∧
  (∀ k, denominator s.alpha ω k * coeff q.stage3 k =
    (s.alpha : ℂ) * coeff (saturated s q.stage2) k) ∧
  (∀ k, denominator s.alpha ω k * coeff q.stage4 k =
    (s.alpha : ℂ) * coeff (saturated s q.stage3) k)

theorem onePole_equation_iff (α : ℝ) (ω : Phase d) (x v : Waveform d) :
    x = (1-α) • delay ω x + α • v ↔
      ∀ k, denominator α ω k * coeff x k = (α : ℂ) * coeff v k := by
  rw [eq_iff_coeff]
  simp only [coeff_add, coeff_smul, coeff_delay, Complex.ofReal_sub, Complex.ofReal_one]
  apply forall_congr'
  intro k
  unfold denominator
  constructor <;> intro h <;> linear_combination h

/-- Full harmonic balance is equivalent to the nonlinear waveform recurrence,
not merely a necessary condition or a finite-harmonic approximation. -/
theorem equations_iff_balance (s : NaiveReuppingLadder ℝ) (ω : Phase d)
    (u : Waveform d) (q : Orbit d) : Equations s ω u q ↔ Balance s ω u q := by
  unfold Equations Balance
  simp only [onePole_equation_iff, coeff_sub, coeff_smul, coeff_delay]
  simp only [mul_assoc]

theorem output_coeff (s : NaiveReuppingLadder ℝ) (q : Orbit d) (k : d → ℤ) :
    coeff (output s q) k = ((s.baseFeedback : ℂ) + 1) * coeff (saturated s q.stage4) k := by
  simp [output]

/-- Replacing only audio histories is a proof device, not another processor. -/
def atPhase (s : NaiveReuppingLadder ℝ) (q : Orbit d) (θ : Phase d) : NaiveReuppingLadder ℝ :=
  {s with
    stage1 := q.stage1 θ
    stage2 := q.stage2 θ
    stage3 := q.stage3 θ
    stage4 := q.stage4 θ}

def AudioAt (q : Orbit d) (θ : Phase d) (s : NaiveReuppingLadder ℝ) : Prop :=
  s.stage1 = q.stage1 θ ∧ s.stage2 = q.stage2 θ ∧
    s.stage3 = q.stage3 θ ∧ s.stage4 = q.stage4 θ

theorem feedback_disabled (s : NaiveReuppingLadder ℝ) (h : s.dampingScale = 0) :
    effectiveFeedback s = s.baseFeedback := by
  simp [effectiveFeedback, h]

omit [Fintype d] in
/-- The waveform equations are exactly the audio part of the existing step,
with `dampingScale = 0`. Detector histories need not be periodic or frozen. -/
theorem equations_iff_step (s : NaiveReuppingLadder ℝ) (hd : s.dampingScale = 0)
    (ω : Phase d) (u : Waveform d) (q : Orbit d) :
    Equations s ω u q ↔ ∀ θ,
      AudioAt q θ (step (atPhase s q (θ - ω)) ⟨u θ⟩) := by
  simp only [Equations, ContinuousMap.ext_iff, ContinuousMap.add_apply,
    ContinuousMap.smul_apply, smul_eq_mul, delay, ContinuousMap.coe_mk,
    ContinuousMap.sub_apply, saturated, ContinuousMap.comp_apply, saturation]
  simp only [AudioAt, step, atPhase, effectiveFeedback, hd, zero_mul, zero_add, div_one,
    OnePoleZOHLowPass.onePoleStep_eq, fma_eq]
  constructor
  · rintro ⟨h1,h2,h3,h4⟩ θ
    have e1 := h1 θ
    have e2 := h2 θ
    have e3 := h3 θ
    have e4 := h4 θ
    have a1 : s.alpha * (u θ - s.baseFeedback * q.stage4 (θ-ω)) +
        (1-s.alpha)*q.stage1 (θ-ω) = q.stage1 θ := by linarith
    rw [a1]
    have a2 : s.alpha * NormalizedTanh.apply s.saturation (q.stage1 θ) +
        (1-s.alpha)*q.stage2 (θ-ω) = q.stage2 θ := by linarith
    rw [a2]
    have a3 : s.alpha * NormalizedTanh.apply s.saturation (q.stage2 θ) +
        (1-s.alpha)*q.stage3 (θ-ω) = q.stage3 θ := by linarith
    rw [a3]
    exact ⟨rfl,rfl,rfl,by linarith⟩
  · intro h
    have hs (θ : Phase d) := h θ
    refine ⟨?_,?_,?_,?_⟩ <;> intro θ
    · linarith [(hs θ).1]
    · have h2 := (hs θ).2.1
      rw [(hs θ).1] at h2
      linarith
    · have h2 := (hs θ).2.1
      have h3 := (hs θ).2.2.1
      rw [h2] at h3
      linarith
    · have h3 := (hs θ).2.2.1
      have h4 := (hs θ).2.2.2
      rw [h3] at h4
      linarith

theorem balance_iff_step (s : NaiveReuppingLadder ℝ) (hd : s.dampingScale = 0)
    (ω : Phase d) (u : Waveform d) (q : Orbit d) :
    Balance s ω u q ↔ ∀ θ, AudioAt q θ (step (atPhase s q (θ-ω)) ⟨u θ⟩) := by
  rw [← equations_iff_balance, equations_iff_step s hd]

theorem denominator_ne_zero (α : ℝ) (hα : 0 < α) (hα1 : α ≤ 1)
    (ω : Phase d) (k : d → ℤ) : denominator α ω k ≠ 0 := by
  intro h
  have he : (1-(α : ℂ))*delayFactor ω k = 1 := by
    unfold denominator at h
    linear_combination -h
  have hn := congrArg norm he
  rw [norm_mul, delayFactor_norm, mul_one, norm_one] at hn
  have hc : (1-(α : ℂ)) = ((1-α : ℝ) : ℂ) := by simp
  rw [hc, Complex.norm_real, Real.norm_eq_abs, abs_of_nonneg (by linarith)] at hn
  linarith

theorem divide_mode (α : ℝ) (hα : 0 < α) (hα1 : α ≤ 1) (ω : Phase d)
    (k : d → ℤ) (x v : ℂ) :
    denominator α ω k * x = (α : ℂ) * v ↔ x = onePoleResponse α ω k * v := by
  have hd := denominator_ne_zero α hα hα1 ω k
  unfold onePoleResponse
  constructor <;> intro h <;> field_simp [hd] at * <;> linear_combination h

/-- Familiar per-harmonic one-pole form, with exact nonlinear coefficients.
The extra delay factor on feedback survives division by the one-pole denominator. -/
theorem balance_iff_response (s : NaiveReuppingLadder ℝ) (hα : 0 < s.alpha)
    (hα1 : s.alpha ≤ 1) (ω : Phase d) (u : Waveform d) (q : Orbit d) :
    Balance s ω u q ↔
      (∀ k, coeff q.stage1 k = onePoleResponse s.alpha ω k *
        (coeff u k - (s.baseFeedback : ℂ) * delayFactor ω k * coeff q.stage4 k)) ∧
      (∀ k, coeff q.stage2 k = onePoleResponse s.alpha ω k * coeff (saturated s q.stage1) k) ∧
      (∀ k, coeff q.stage3 k = onePoleResponse s.alpha ω k * coeff (saturated s q.stage2) k) ∧
      (∀ k, coeff q.stage4 k = onePoleResponse s.alpha ω k * coeff (saturated s q.stage3) k) := by
  simp only [Balance, divide_mode s.alpha hα hα1]

/-- Matching coefficients plus audio histories. The detector histories remain
unconstrained: they still evolve in `step`, but cannot affect disabled feedback. -/
def Matches (s : NaiveReuppingLadder ℝ) (q : Orbit d) (θ : Phase d)
    (t : NaiveReuppingLadder ℝ) : Prop :=
  t.alpha = s.alpha ∧ t.saturation = s.saturation ∧
    t.baseFeedback = s.baseFeedback ∧ t.dampingScale = 0 ∧ AudioAt q θ t

omit [Fintype d] in
theorem equations_step_matches (s : NaiveReuppingLadder ℝ) (hd : s.dampingScale = 0)
    (ω : Phase d) (u : Waveform d) (q : Orbit d) (he : Equations s ω u q)
    (θ : Phase d) (t : NaiveReuppingLadder ℝ) (ht : Matches s q θ t) :
    Matches s q (θ+ω) (step t ⟨u (θ+ω)⟩) := by
  rcases ht with ⟨ha,hc,hk,hd',h1,h2,h3,h4⟩
  refine ⟨ha,hc,hk,hd',?_⟩
  have h := (equations_iff_step s hd ω u q).mp he (θ+ω)
  simpa only [AudioAt, step, atPhase, add_sub_cancel_right, effectiveFeedback,
    hd, hd', zero_mul, zero_add, div_one, ha, hc, hk, h1, h2, h3, h4] using h

/-- Initialize ON the candidate orbit. This is not a reset-to-orbit attraction
claim. The first input advances the phase once, matching the state-step indexing. -/
def trajectory (s : NaiveReuppingLadder ℝ) (ω : Phase d) (u : Waveform d)
    (q : Orbit d) (θ : Phase d) : ℕ → NaiveReuppingLadder ℝ :=
  Step.states step (fun n => ⟨u (θ+(n+1) • ω)⟩) (atPhase s q θ)

theorem balance_trajectory (s : NaiveReuppingLadder ℝ) (hd : s.dampingScale = 0)
    (ω : Phase d) (u : Waveform d) (q : Orbit d) (hb : Balance s ω u q)
    (θ : Phase d) (n : ℕ) : Matches s q (θ+n • ω) (trajectory s ω u q θ n) := by
  induction n with
  | zero => exact ⟨rfl,rfl,rfl,hd,by simp [AudioAt, trajectory, atPhase]⟩
  | succ n ih =>
    have h := equations_step_matches s hd ω u q ((equations_iff_balance s ω u q).mpr hb)
      (θ+n • ω) _ ih
    simpa only [trajectory, Step.states_succ, succ_nsmul, add_assoc] using h

theorem balance_trajectory_output (s : NaiveReuppingLadder ℝ) (hd : s.dampingScale = 0)
    (ω : Phase d) (u : Waveform d) (q : Orbit d) (hb : Balance s ω u q)
    (θ : Phase d) (n : ℕ) :
    (trajectory s ω u q θ (n+1)).output = output s q (θ+(n+1) • ω) := by
  have h := balance_trajectory s hd ω u q hb θ n
  have hnext := (balance_trajectory s hd ω u q hb θ (n+1)).2.2.2.2.2.2.2
  change (step (trajectory s ω u q θ n) _).output = _
  change NormalizedTanh.apply (trajectory s ω u q θ n).saturation
      (trajectory s ω u q θ (n+1)).stage4 *
      (effectiveFeedback (trajectory s ω u q θ n)+1) = _
  rw [h.2.1, feedback_disabled _ h.2.2.2.1, h.2.2.1, hnext]
  simp only [output, saturated, saturation, ContinuousMap.comp_apply,
    ContinuousMap.coe_mk, ContinuousMap.smul_apply, smul_eq_mul]
  ring

end DSP.NaiveReuppingLadder.HarmonicBalance
