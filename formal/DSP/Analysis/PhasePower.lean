import DSP.Analysis.PhaseLocking
import Mathlib.Analysis.Complex.Poisson

/-! Power of the stationary continuous averaged phase flow. The running-phase
density is the normalized inverse phase velocity, a Poisson kernel. Its first
moment is computed exactly below. This is NOT a convergence theorem for the
discrete ladder, its discrete phase map, or an identification of a sampled time
average with this phase average. The continuous slow-flow approximation and
the omitted driven amplitude response each need separate error bounds. -/

noncomputable section
namespace DSP.PhasePower
open Complex Metric

/-- Positive detuning convention. Negative detuning conjugates this moment. -/
def runningMoment (c : ℝ) : ℂ := -(c : ℂ) * Complex.I

def velocity (c : ℝ) (z : ℂ) : ℝ := 1 + c^2 + 2*c*z.im

def density (c : ℝ) (z : ℂ) : ℝ := (1-c^2) / velocity c z

theorem moment_norm_sq (c : ℝ) : ‖runningMoment c‖^2 = c^2 := by
  simp [runningMoment, Complex.norm_real, Real.norm_eq_abs, sq_abs]

theorem velocity_pos (c : ℝ) (hc : 0 ≤ c) (hc1 : c < 1) (z : ℂ) (hz : ‖z‖ = 1) :
    0 < velocity c z := by
  have hi := Complex.abs_im_le_norm z
  rw [hz] at hi
  have hlow := (abs_le.mp hi).1
  have hs : 0 < (1-c)^2 := sq_pos_of_pos (by linarith)
  unfold velocity
  nlinarith

theorem density_pos (c : ℝ) (hc : 0 ≤ c) (hc1 : c < 1) (z : ℂ) (hz : ‖z‖ = 1) :
    0 < density c z := by
  apply div_pos _ (velocity_pos c hc hc1 z hz)
  nlinarith

theorem constant_flux (c : ℝ) (hc : 0 ≤ c) (hc1 : c < 1) (z : ℂ) (hz : ‖z‖ = 1) :
    density c z * velocity c z = 1-c^2 := by
  exact div_mul_cancel₀ _ (ne_of_gt (velocity_pos c hc hc1 z hz))

theorem density_eq_poisson (c : ℝ) (z : ℂ) (hz : ‖z‖ = 1) :
    density c z = poissonKernel 0 (runningMoment c) z := by
  have hd : ‖z - runningMoment c‖^2 = velocity c z := by
    have hzsq : z.re^2 + z.im^2 = 1 := by
      simp only [pow_two]
      rw [← Complex.normSq_apply, Complex.normSq_eq_norm_sq, hz]; norm_num
    rw [← Complex.normSq_eq_norm_sq]
    simp [runningMoment, Complex.normSq_apply, velocity]
    nlinarith
  simp only [poissonKernel, sub_zero, hz, one_pow, moment_norm_sq, hd]
  rfl

theorem moment_mem_ball (c : ℝ) (hc : 0 ≤ c) (hc1 : c < 1) :
    runningMoment c ∈ ball (0 : ℂ) 1 := by
  simpa [mem_ball, dist_zero_right, runningMoment, norm_mul, Complex.norm_real,
    Real.norm_eq_abs, abs_of_nonneg hc] using hc1

/-- The positive inverse-velocity density is normalized over a full circle. -/
theorem density_normalized (c : ℝ) (hc : 0 ≤ c) (hc1 : c < 1) :
    Real.circleAverage (fun z => (density c z : ℂ)) 0 1 = 1 := by
  have hf : DiffContOnCl ℂ (fun _ : ℂ => (1 : ℂ)) (ball 0 1) := diffContOnCl_const
  have h := hf.circleAverage_poissonKernel_smul (moment_mem_ball c hc hc1)
  calc
    _ = Real.circleAverage (poissonKernel 0 (runningMoment c) • (fun _ : ℂ => (1 : ℂ))) 0 1 := by
      apply Real.circleAverage_congr_sphere
      intro z hz
      have hn : ‖z‖ = 1 := by simpa [mem_sphere, dist_zero_right] using hz
      simp [density_eq_poisson c z hn, Complex.real_smul]
    _ = 1 := h

/-- Exact coherent moment of the stationary running-phase density. -/
theorem running_mean (c : ℝ) (hc : 0 ≤ c) (hc1 : c < 1) :
    Real.circleAverage (fun z => (density c z : ℂ) * z) 0 1 = runningMoment c := by
  have hf : DiffContOnCl ℂ (fun z : ℂ => z) (ball 0 1) := differentiable_id.diffContOnCl
  have h := hf.circleAverage_poissonKernel_smul (moment_mem_ball c hc hc1)
  calc
    _ = Real.circleAverage (poissonKernel 0 (runningMoment c) • (fun z : ℂ => z)) 0 1 := by
      apply Real.circleAverage_congr_sphere
      intro z hz
      have hn : ‖z‖ = 1 := by simpa [mem_sphere, dist_zero_right] using hz
      simp [density_eq_poisson c z hn, Complex.real_smul]
    _ = runningMoment c := h

/-- All positive Fourier moments of the first-order running-phase model.
A quadratic amplitude observer needs moments beyond the coherent first one. -/
theorem running_moment_pow (c : ℝ) (hc : 0 ≤ c) (hc1 : c < 1) (n : ℕ) :
    Real.circleAverage (fun z => (density c z : ℂ)*z^n) 0 1 = (runningMoment c)^n := by
  have hf : DiffContOnCl ℂ (fun z : ℂ => z^n) (ball 0 1) :=
    (differentiable_id.pow n).diffContOnCl
  have h := hf.circleAverage_poissonKernel_smul (moment_mem_ball c hc hc1)
  calc
    _ = Real.circleAverage (poissonKernel 0 (runningMoment c) • (fun z : ℂ => z^n)) 0 1 := by
      apply Real.circleAverage_congr_sphere
      intro z hz
      have hn : ‖z‖ = 1 := by simpa [mem_sphere, dist_zero_right] using hz
      simp [density_eq_poisson c z hn, Complex.real_smul]
    _ = (runningMoment c)^n := h

/-- The rotated first-order driven observer contains a constant term and a
second phase harmonic. Averaging it requires both first and second moments. -/
theorem driven_observer (z y0 yc ys : ℂ) (A : ℝ) (hz : ‖z‖ = 1) :
    z*(y0+(A:ℂ)*(yc*z.re+ys*z.im)) =
      y0*z+(A:ℂ)/2*((yc-Complex.I*ys)*z^2+yc+Complex.I*ys) := by
  have hu : z*star z = 1 := by
    rw [Complex.star_def, Complex.mul_conj, Complex.normSq_eq_norm_sq, hz]
    norm_num
  rw [Complex.re_eq_add_conj, Complex.im_eq_sub_conj]
  have hi : (Complex.I : ℂ)⁻¹ = -Complex.I := by simp
  simp only [div_eq_mul_inv, mul_inv_rev, hi, Complex.star_def] at *
  linear_combination ((A:ℂ)/2*(yc+Complex.I*ys))*hu

/-- Quadratic waveform correction after rotation into the input phase.
Only moments 1, 3 and -1 are needed for its coherent output. -/
theorem quadratic_observer (z ycc yss ycs : ℂ) (hz : ‖z‖ = 1) :
    z*(ycc*(z.re:ℂ)^2+yss*(z.im:ℂ)^2+ycs*z.re*z.im) =
      (ycc*(z^3+2*z+star z)+yss*(-z^3+2*z-star z)+
        ycs*(z^3-star z)/Complex.I)/4 := by
  have hu : z*star z = 1 := by
    rw [Complex.star_def, Complex.mul_conj, Complex.normSq_eq_norm_sq, hz]
    norm_num
  rw [Complex.re_eq_add_conj, Complex.im_eq_sub_conj]
  have hi : (Complex.I : ℂ)⁻¹ = -Complex.I := by simp
  simp only [div_eq_mul_inv, mul_inv_rev, hi, Complex.star_def] at *
  linear_combination (norm := ring_nf) (ycc*(2*z+star z)+yss*(2*z-star z)+Complex.I*ycs*star z)/4*hu
  simp [Complex.I_sq]
  ring

def detuningRatio (c : ℝ) : ℝ := (1+c^2)/(2*c)

/-- The actual Adler velocity, up to a positive constant factor. -/
theorem velocity_ratio (c : ℝ) (hc : c ≠ 0) (z : ℂ) :
    velocity c z = 2*c*(detuningRatio c + z.im) := by
  unfold velocity detuningRatio
  field_simp

theorem detuning_gap (c : ℝ) (hc : c ≠ 0) :
    detuningRatio c - 1 = (1-c)^2/(2*c) := by
  unfold detuningRatio
  field_simp
  ring

theorem detuning_gt_one (c : ℝ) (hc : 0 < c) (hc1 : c < 1) :
    1 < detuningRatio c := by
  have h := detuning_gap c (ne_of_gt hc)
  have hp : 0 < (1-c)^2/(2*c) := div_pos (sq_pos_of_pos (by linarith)) (by positivity)
  linarith

/-- Stable form of `r - sqrt(r*r-1)` for positive running detuning. -/
def runningCoherence (r : ℝ) : ℝ := 1/(r + Real.sqrt (r^2-1))

theorem runningCoherence_eq (r : ℝ) (hr : 1 < r) :
    runningCoherence r = r - Real.sqrt (r^2-1) := by
  have hp : 0 ≤ r^2-1 := by nlinarith
  have hd : r + Real.sqrt (r^2-1) ≠ 0 := by
    have hs := Real.sqrt_nonneg (r^2-1)
    linarith
  unfold runningCoherence
  apply (div_eq_iff hd).mpr
  nlinarith [Real.sq_sqrt hp]

/-- Every running detuning r>1 has the positive inverse-velocity density
used above; the closed form does not require solving another equation. -/
theorem running_parameters (r : ℝ) (hr : 1 < r) :
    0 < runningCoherence r ∧ runningCoherence r < 1 ∧
      detuningRatio (runningCoherence r) = r := by
  have hs := Real.sqrt_nonneg (r^2-1)
  have hd : 0 < r + Real.sqrt (r^2-1) := by linarith
  have hc : 0 < runningCoherence r := one_div_pos.mpr hd
  have hc1 : runningCoherence r < 1 := (div_lt_one hd).mpr (by linarith)
  refine ⟨hc, hc1, ?_⟩
  unfold detuningRatio
  apply (div_eq_iff (ne_of_gt (by positivity : 0 < 2*runningCoherence r))).mpr
  rw [runningCoherence_eq r hr]
  nlinarith [Real.sq_sqrt (by nlinarith : 0 ≤ r^2-1)]

theorem coherence_at_ratio (c : ℝ) (hc : 0 < c) (hc1 : c < 1) :
    runningCoherence (detuningRatio c) = c := by
  have hn : c ≠ 0 := ne_of_gt hc
  have he : (detuningRatio c)^2-1 = ((1-c^2)/(2*c))^2 := by
    unfold detuningRatio; field_simp; ring
  have hp : 0 ≤ (1-c^2)/(2*c) := div_nonneg (by nlinarith) (by positivity)
  rw [runningCoherence, he, Real.sqrt_sq_eq_abs, abs_of_nonneg hp]
  unfold detuningRatio
  field_simp
  ring

/-- Squared coherence in the averaged model, including its locked branch.
The caller must supply a positive locking width before forming |delta|/width. -/
def coherencePower (r : ℝ) : ℝ :=
  if |r| ≤ 1 then 1 else (runningCoherence |r|)^2

def lockedMoment (r : ℝ) : ℂ := -(Real.sqrt (1-r^2) : ℂ) - (r : ℂ)*Complex.I

theorem locked_balance (r : ℝ) : r + (lockedMoment r).im = 0 := by
  simp [lockedMoment]

/-- A locked phase has unit coherence. Its stable branch has negative real
part; at the boundary the restoring derivative vanishes. -/
theorem locked_norm_sq (r : ℝ) (hr : |r| ≤ 1) : ‖lockedMoment r‖^2 = 1 := by
  have hp : 0 ≤ 1-r^2 := by
    have h := abs_le.mp hr
    nlinarith [sq_nonneg (r-1), sq_nonneg (r+1)]
  rw [← Complex.normSq_eq_norm_sq]
  simp [lockedMoment, Complex.normSq_apply]
  nlinarith [Real.sq_sqrt hp]

theorem power_at_ratio (c : ℝ) (hc : 0 < c) (hc1 : c < 1) :
    coherencePower (detuningRatio c) = c^2 := by
  have hr := detuning_gt_one c hc hc1
  simp [coherencePower, abs_of_pos (by linarith : 0 < detuningRatio c),
    not_le.mpr hr, coherence_at_ratio c hc hc1]

/-- Exact conditioning on approach to a locking edge. The denominator tends
to zero, so a uniform linear error bound across that edge cannot be inferred. -/
theorem edge_conditioning (c : ℝ) (hc : 0 < c) (hc1 : c < 1) :
    (1-coherencePower (detuningRatio c))/(detuningRatio c-1) = 2*c*(1+c)/(1-c) := by
  rw [power_at_ratio c hc hc1, detuning_gap c (ne_of_gt hc)]
  have hn : 1-c ≠ 0 := ne_of_gt (by linarith)
  field_simp
  ring

/-- No finite Lipschitz constant controls coherent power at a locking edge.
This is a property of the reduced model itself, before ladder errors enter. -/
theorem edge_not_lipschitz (L : ℝ) (hL : 0 ≤ L) :
    ∃ r : ℝ, 1 < r ∧
      L * |r-1| < |coherencePower r - coherencePower 1| := by
  let c := (L+1)/(L+2)
  have hd : 0 < L+2 := by linarith
  have hc : 0 < c := div_pos (by linarith) hd
  have hc1 : c < 1 := (div_lt_one hd).mpr (by linarith)
  have hr := detuning_gt_one c hc hc1
  have he : 2*c*(1+c)/(1-c) = 2*(L+1)*(2*L+3)/(L+2) := by
    dsimp [c]
    field_simp
    ring
  have hlarge : L < 2*c*(1+c)/(1-c) := by
    rw [he]
    apply (lt_div_iff₀ hd).mpr
    nlinarith [sq_nonneg L]
  rw [← edge_conditioning c hc hc1] at hlarge
  have hbound := (lt_div_iff₀ (sub_pos.mpr hr)).mp hlarge
  refine ⟨detuningRatio c, hr, ?_⟩
  rw [abs_of_pos (sub_pos.mpr hr), power_at_ratio c hc hc1]
  have hunit : coherencePower 1 = 1 := by norm_num [coherencePower]
  rw [hunit, abs_of_nonpos (by nlinarith : c^2-1 ≤ 0)]
  rw [power_at_ratio c hc hc1] at hbound
  linarith

/-- Multiplying an output harmonic's complex peak amplitude by its coherent
moment multiplies its power by the squared magnitude of that moment. -/
theorem coherent_power_factor (peak moment : ℂ) :
    ‖peak*moment‖^2/2 = (‖peak‖^2/2)*‖moment‖^2 := by
  rw [norm_mul]
  ring

/-- Oscillator and driven response at the same frequency must be added as
complex amplitudes before taking power; the cross term can cancel a peak. -/
theorem coherent_power_sum (oscillator driven : ℂ) :
    ‖oscillator+driven‖^2 = ‖oscillator‖^2 + ‖driven‖^2 +
      2*(oscillator * star driven).re := by
  simp only [← Complex.normSq_eq_norm_sq, Complex.normSq_apply,
    Complex.add_re, Complex.add_im, Complex.mul_re, Complex.star_def,
    Complex.conj_re, Complex.conj_im]
  ring

/-- The rigid-waveform model preserves all-phase total power under rotation.
This must not be confused with time averaging a nonuniform phase trajectory. -/
theorem rotated_total_power {d : Type*} [Fintype d]
    (f : DSP.Harmonics.Waveform d) (ω : DSP.Harmonics.Phase d) :
    DSP.Harmonics.phasePower (DSP.Harmonics.delay ω f) = DSP.Harmonics.phasePower f := by
  rw [DSP.Harmonics.phasePower_eq_spectrum, DSP.Harmonics.phasePower_eq_spectrum]
  apply tsum_congr
  intro k
  rw [DSP.Harmonics.coeff_delay, norm_mul, DSP.Harmonics.delayFactor_norm, one_mul]

/-- A waveform/coherent-amplitude error has a quantitative power consequence.
The amplitude error must be established separately by a dynamical argument. -/
theorem complex_power_error (a b : ℂ) (ε : ℝ) (h : ‖a-b‖ ≤ ε) :
    |‖a‖^2-‖b‖^2| ≤ ε*(2*‖b‖+ε) := by
  have he : 0 ≤ ε := (norm_nonneg _).trans h
  have hd : |‖a‖-‖b‖| ≤ ε := (abs_norm_sub_norm_le a b).trans h
  have ha : ‖a‖ ≤ ‖b‖+ε := by linarith [(abs_le.mp hd).2]
  rw [show ‖a‖^2-‖b‖^2 = (‖a‖-‖b‖)*(‖a‖+‖b‖) by ring, abs_mul,
    abs_of_nonneg (add_nonneg (norm_nonneg a) (norm_nonneg b))]
  nlinarith [norm_nonneg a, norm_nonneg b, abs_nonneg (‖a‖-‖b‖)]

end DSP.PhasePower
