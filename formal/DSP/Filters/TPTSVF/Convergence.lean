import DSP.Analysis.Asymptotic
import DSP.Filters.Capture.Proofs

/-! Time-domain settling of the Q=1 SVFs used by the bandpass detector.
The complex coordinate below is a proof device, not a second implementation.
Its recurrence is proved directly from the existing step. -/

noncomputable section
namespace DSP.TPTSVF.Convergence
open Filter
open scoped Topology

def denominator (g : ℝ) : ℝ := 1 + g + g ^ 2

theorem denominator_pos (g : ℝ) : 0 < denominator g := by
  unfold denominator
  nlinarith [sq_nonneg (g + 1 / 2)]

def coordinate (s : TPTSVF ℝ) : ℂ :=
  ⟨s.ic1 + s.ic2 / 2, Real.sqrt 3 / 2 * s.ic2⟩

def pole (g : ℝ) : ℂ :=
  ⟨(1 - g ^ 2) / denominator g, Real.sqrt 3 * g / denominator g⟩

def drive (g : ℝ) : ℂ :=
  ⟨(2 * g + g ^ 2) / denominator g, Real.sqrt 3 * g ^ 2 / denominator g⟩

theorem pole_normSq (g : ℝ) :
    Complex.normSq (pole g) = (1 - g + g ^ 2) / denominator g := by
  have hd := ne_of_gt (denominator_pos g)
  have hs : Real.sqrt 3 ^ 2 = 3 := Real.sq_sqrt (by norm_num)
  simp only [pole, Complex.normSq_apply]
  field_simp
  unfold denominator
  nlinarith [hs]

theorem pole_norm_lt_one (g : ℝ) (hg : 0 < g) : ‖pole g‖ < 1 := by
  have h : Complex.normSq (pole g) < 1 := by
    rw [pole_normSq, div_lt_one (denominator_pos g)]
    unfold denominator
    linarith
  rw [Complex.normSq_eq_norm_sq] at h
  nlinarith [norm_nonneg (pole g)]

/-- Exact modal recurrence, including the original FMA evaluation interpreted
over the reals. Preparation changes coefficients but preserves both histories. -/
theorem prepared_step_coordinate (g : ℝ) (s : TPTSVF ℝ) (input : ℝ) :
    coordinate (step (prepareFromSnapshot ⟨g, 1⟩ s).1 ⟨input⟩) =
      pole g * coordinate s + drive g * (input : ℂ) := by
  have hd := ne_of_gt (denominator_pos g)
  have hs : Real.sqrt 3 ^ 2 = 3 := Real.sq_sqrt (by norm_num)
  have hd' : g * (g + 1) + 1 = denominator g := by unfold denominator; ring
  apply Complex.ext
  · simp only [coordinate, step, prepareFromSnapshot, fma_eq, pole, drive,
      Complex.add_re, Complex.mul_re, Complex.ofReal_re, Complex.ofReal_im, hd']
    field_simp
    unfold denominator
    rw [hs]
    ring
  · simp only [coordinate, step, prepareFromSnapshot, fma_eq, pole, drive,
      Complex.add_im, Complex.mul_im, Complex.ofReal_re, Complex.ofReal_im, hd']
    field_simp
    unfold denominator
    ring

theorem tuned_eq_prepared (g : ℝ) (s : TPTSVF ℝ) (ht : s.Tuned g) (hk : s.k = 1) :
    s = (prepareFromSnapshot ⟨g, 1⟩ s).1 := by
  have ha : s.a1 = 1 / (g * (g + 1) + 1) := by
    apply (eq_div_iff (by nlinarith [sq_nonneg (g + 1 / 2)] : g * (g + 1) + 1 ≠ 0)).mpr
    have h := ht.solve
    rw [hk] at h
    nlinarith [h]
  obtain ⟨_, h2, h3⟩ := ht
  cases s
  simp_all [prepareFromSnapshot, fma_eq]

theorem step_coordinate (g : ℝ) (s : TPTSVF ℝ) (ht : s.Tuned g) (hk : s.k = 1) (input : ℝ) :
    coordinate (step s ⟨input⟩) = pole g * coordinate s + drive g * (input : ℂ) := by
  conv_lhs => rw [tuned_eq_prepared g s ht hk]
  exact prepared_step_coordinate g s input

def firstState (z : ℂ) : ℝ := z.re - z.im / Real.sqrt 3

theorem firstState_coordinate (s : TPTSVF ℝ) : firstState (coordinate s) = s.ic1 := by
  have hs : Real.sqrt 3 ≠ 0 := ne_of_gt (Real.sqrt_pos.mpr (by norm_num))
  unfold firstState coordinate
  dsimp
  field_simp
  ring

theorem firstState_sub (z w : ℂ) : firstState (z - w) = firstState z - firstState w := by
  simp only [firstState, Complex.sub_re, Complex.sub_im]
  ring

theorem firstState_tendsto {z : ℕ → ℂ} (h : Tendsto z atTop (𝓝 0)) :
    Tendsto (fun n => firstState (z n)) atTop (𝓝 0) := by
  have hre := (Complex.continuous_re.tendsto 0).comp h
  have him := (Complex.continuous_im.tendsto 0).comp h
  simpa [firstState] using hre.sub (him.div_const (Real.sqrt 3))

theorem bandPass_step (s : TPTSVF ℝ) (input : ℝ) :
    (step s ⟨input⟩).bandPass =
      (firstState (coordinate (step s ⟨input⟩)) + firstState (coordinate s)) / 2 := by
  rw [firstState_coordinate, firstState_coordinate]
  simp only [step, fma_eq]
  ring

theorem unit_sub_pole_ne_zero (g : ℝ) (hg : 0 < g) (z : ℂ) (hz : ‖z‖ = 1) :
    z - pole g ≠ 0 := by
  intro h
  have hp := pole_norm_lt_one g hg
  rw [← sub_eq_zero.mp h, hz] at hp
  exact lt_irrefl 1 hp

/-- A particular solution for a real sinusoid Re(amplitude*z^n), including
both conjugate frequencies. This is not a complex input to the real DSP. -/
def reference (g : ℝ) (amplitude z : ℂ) (n : ℕ) : ℂ :=
  drive g * amplitude / (2 * (z - pole g)) * z ^ n +
    drive g * star amplitude / (2 * (star z - pole g)) * (star z) ^ n

theorem reference_step (g : ℝ) (amplitude z : ℂ)
    (hz : z - pole g ≠ 0) (hc : star z - pole g ≠ 0) (n : ℕ) :
    reference g amplitude z (n + 1) = pole g * reference g amplitude z n +
      drive g * (((amplitude * z ^ n).re : ℝ) : ℂ) := by
  rw [Complex.re_eq_add_conj]
  simp only [reference, pow_succ, map_mul, map_pow, starRingEnd_apply]
  field_simp
  ring

def observation : ℂ := ⟨1, (Real.sqrt 3)⁻¹⟩

theorem firstState_pair (v w z : ℂ) :
    firstState (v * z + w * star z) = ((observation * v + star (observation * w)) * z).re := by
  simp only [firstState, observation, Complex.star_def, Complex.add_re, Complex.add_im,
    Complex.mul_re, Complex.mul_im, Complex.conj_re, Complex.conj_im]
  ring

/-- Actual steady bandpass gain, derived from the stable state recurrence. -/
def response (g : ℝ) (z : ℂ) : ℂ :=
  observation * (drive g * (z + 1) / (4 * (z - pole g))) +
    star (observation * (drive g * (star z + 1) / (4 * (star z - pole g))))

theorem observation_drive_sum (g : ℝ) :
    observation * drive g + star (observation * drive g) = (4 * g / denominator g : ℝ) := by
  have hd := ne_of_gt (denominator_pos g)
  have hs : Real.sqrt 3 ≠ 0 := ne_of_gt (Real.sqrt_pos.mpr (by norm_num))
  apply Complex.ext
  · simp [observation, drive]
    field_simp
    ring
  · simp
    ring

theorem observation_drive_cross (g : ℝ) :
    (observation * drive g) * star (pole g) + star (observation * drive g) * pole g =
      (4 * g / denominator g : ℝ) := by
  have hd := ne_of_gt (denominator_pos g)
  have hs : Real.sqrt 3 ≠ 0 := ne_of_gt (Real.sqrt_pos.mpr (by norm_num))
  have hs2 : Real.sqrt 3 ^ 2 = 3 := Real.sq_sqrt (by norm_num)
  apply Complex.ext
  · simp [observation, drive, pole]
    field_simp
    unfold denominator
    ring_nf
    simp only [hs2]
    ring
  · simp [observation, drive, pole]
    ring

theorem pole_sum (g : ℝ) : pole g + star (pole g) = (2 * (1 - g ^ 2) / denominator g : ℝ) := by
  apply Complex.ext
  · change (1 - g ^ 2) / denominator g + (1 - g ^ 2) / denominator g =
      2 * (1 - g ^ 2) / denominator g
    ring
  · change Real.sqrt 3 * g / denominator g + -(Real.sqrt 3 * g / denominator g) = 0
    ring

theorem pole_product (g : ℝ) : pole g * star (pole g) = ((1 - g + g ^ 2) / denominator g : ℝ) := by
  rw [Complex.star_def, Complex.mul_conj, pole_normSq]

/-- The state-based sinusoidal response has exactly the existing SVF transfer
in homogeneous bilinear coordinates. This closes the spectral/time-domain link. -/
theorem response_eq_transfer (g : ℝ) (hg : 0 < g) (z : ℂ) (hz : ‖z‖ = 1) :
    response g z = (⟨(g : ℂ), 1⟩ : Snapshot ℂ).bandPassTransfer (z - 1) (z + 1) := by
  have hd : (denominator g : ℂ) ≠ 0 := by exact_mod_cast ne_of_gt (denominator_pos g)
  have h1 := unit_sub_pole_ne_zero g hg z hz
  have hc := unit_sub_pole_ne_zero g hg (star z) (by simpa using hz)
  have h2 : z - star (pole g) ≠ 0 := by simpa only [star_sub, star_star] using star_ne_zero.mpr hc
  have hnum : (observation * drive g) * (z - star (pole g)) +
      (z - pole g) * star (observation * drive g) =
        (4 * g / denominator g : ℝ) * (z - 1) := by
    linear_combination z * observation_drive_sum g - observation_drive_cross g
  have hden : (z - 1) ^ 2 + (g : ℂ) * (z - 1) * (z + 1) + (g : ℂ) ^ 2 * (z + 1) ^ 2 =
      (denominator g : ℂ) * ((z - pole g) * (z - star (pole g))) := by
    have he : (z - pole g) * (z - star (pole g)) =
        z ^ 2 - (pole g + star (pole g)) * z + pole g * star (pole g) := by ring
    rw [he, pole_sum, pole_product]
    push_cast
    field_simp
    unfold denominator
    push_cast
    ring
  have hr : response g z = (z + 1) / 4 *
      ((observation * drive g) / (z - pole g) + star (observation * drive g) / (z - star (pole g))) := by
    simp only [response, star_mul, star_div₀, star_add, star_sub, star_star, star_ofNat, star_one]
    simp only [div_eq_mul_inv, mul_inv_rev]
    ring
  rw [hr, div_add_div _ _ h1 h2, hnum]
  simp only [Snapshot.bandPassTransfer, Snapshot.weightedTransfer, Response.svf]
  have hden' : (z - 1) * (z - 1) + 1 * ((g : ℂ) * (z + 1)) * (z - 1) +
      (g : ℂ) * (z + 1) * ((g : ℂ) * (z + 1)) =
      (denominator g : ℂ) * ((z - pole g) * (z - star (pole g))) := by
    convert hden using 1
    ring
  rw [hden']
  push_cast
  field_simp
  ring

theorem reference_output (g : ℝ) (amplitude z : ℂ) (n : ℕ) :
    (firstState (reference g amplitude z (n + 1)) +
      firstState (reference g amplitude z n)) / 2 =
      (response g z * amplitude * z ^ n).re := by
  have he : (reference g amplitude z (n + 1) + reference g amplitude z n) / 2 =
      (drive g * (z + 1) / (4 * (z - pole g)) * amplitude) * z ^ n +
      (drive g * (star z + 1) / (4 * (star z - pole g)) * star amplitude) * star (z ^ n) := by
    simp only [reference, pow_succ, star_pow, div_eq_mul_inv, mul_inv_rev]
    ring
  have hf : (firstState (reference g amplitude z (n + 1)) +
      firstState (reference g amplitude z n)) / 2 =
      firstState ((reference g amplitude z (n + 1) + reference g amplitude z n) / 2) := by
    simp only [firstState, Complex.div_ofNat_re, Complex.div_ofNat_im, Complex.add_re, Complex.add_im]
    ring
  rw [hf, he, firstState_pair]
  congr 1
  simp only [response, star_mul, star_star]
  ring

theorem trajectory_tuned (g : ℝ) (s : TPTSVF ℝ) (ht : s.Tuned g) (input : ℕ → ℝ) (n : ℕ) :
    (trajectory s input n).Tuned g := by
  obtain ⟨hk, h1, h2, h3⟩ := states_coefficients s input n
  exact ⟨by rw [h1, hk]; exact ht.solve, by rw [h2, h1]; exact ht.a2,
    by rw [h3, h2]; exact ht.a3⟩

/-- Arbitrary initial histories and any input tending to a sinusoid have the
same asymptotic bandpass output. The convergence hypothesis is on the INPUT,
so this theorem composes directly across both detector stages. -/
theorem bandPass_tendsto (g : ℝ) (hg : 0 < g) (s : TPTSVF ℝ)
    (ht : s.Tuned g) (hk : s.k = 1) (input : ℕ → ℝ) (amplitude z : ℂ)
    (hz : ‖z‖ = 1)
    (hu : Tendsto (fun n => input n - (amplitude * z ^ n).re) atTop (𝓝 0)) :
    Tendsto (fun n => (trajectory s input (n + 1)).bandPass -
      (response g z * amplitude * z ^ n).re) atTop (𝓝 0) := by
  have hzc : ‖star z‖ = 1 := by simpa using hz
  have hd := unit_sub_pole_ne_zero g hg z hz
  have hc := unit_sub_pole_ne_zero g hg (star z) hzc
  let error := fun n => coordinate (trajectory s input n) - reference g amplitude z n
  have he : Tendsto error atTop (𝓝 0) := by
    apply Asymptotic.stable_recurrence (pole g) (pole_norm_lt_one g hg) error
      (fun n => drive g * ((input n - (amplitude * z ^ n).re : ℝ) : ℂ))
    · intro n
      dsimp [error]
      change coordinate (step (trajectory s input n) ⟨input n⟩) - _ = _
      rw [step_coordinate g _ (trajectory_tuned g s ht input n)
        (by rw [(states_coefficients s input n).1, hk]), reference_step g amplitude z hd hc]
      push_cast
      ring
    · have h := (Complex.continuous_ofReal.tendsto 0).comp hu
      simpa using h.const_mul (drive g)
  have h0 := firstState_tendsto he
  have h1 := h0.comp (tendsto_add_atTop_nat 1)
  have hout := (h1.add h0).div_const 2
  simp only [zero_add, zero_div] at hout
  apply hout.congr
  intro n
  change (firstState (error (n + 1)) + firstState (error n)) / 2 = _
  dsimp [error]
  rw [firstState_sub, firstState_sub, ← reference_output]
  change _ = (step (trajectory s input n) ⟨input n⟩).bandPass - _
  rw [bandPass_step]
  ring

end DSP.TPTSVF.Convergence
