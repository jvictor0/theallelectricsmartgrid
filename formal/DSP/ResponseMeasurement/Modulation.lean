import DSP.ResponseMeasurement

/-! Coefficient ripple and coherent response. A zero-mean coefficient ripple
can contribute at the stimulus frequency through multiplication. -/

noncomputable section
namespace DSP.ResponseMeasurement
open Filter Finset
open scoped Topology

theorem coherent_add (z : ℂ) (left right : ℕ → ℝ) (count : ℕ) :
    coherent z (fun n => left n + right n) count = coherent z left count + coherent z right count := by
  simp only [coherent, average, Complex.ofReal_add, add_mul, Finset.sum_add_distrib, smul_add, mul_add]

/-- The split is exact; nothing in this equality sets its second term to zero. -/
theorem coherent_feedback_split (z : ℂ) (center : ℝ) (wobble signal : ℕ → ℝ) (count : ℕ) :
    coherent z (fun n => (center + wobble n) * signal n) count =
      (center : ℂ) * coherent z signal count + coherent z (fun n => wobble n * signal n) count := by
  simp only [add_mul, coherent_add, coherent_scale]

theorem coherent_mode (z mode amplitude : ℂ) (count : ℕ) :
    coherent z (fun n => (amplitude * mode ^ n).re) count =
      amplitude * average (fun n => (mode * star z) ^ n) count +
        star amplitude * average (fun n => (star mode * star z) ^ n) count := by
  have he (n : ℕ) : 2 * (((amplitude * mode ^ n).re : ℝ) : ℂ) * star z ^ n =
      amplitude * (mode * star z) ^ n + star amplitude * (star mode * star z) ^ n := by
    rw [Complex.re_eq_add_conj]
    simp only [map_mul, map_pow, starRingEnd_apply, mul_pow]
    ring
  simp only [coherent, average, Complex.real_smul]
  calc
    _ = (count : ℝ)⁻¹ * ∑ n ∈ range count,
        (2 * (((amplitude * mode ^ n).re : ℝ) : ℂ) * star z ^ n) := by
      simp_rw [mul_assoc (2 : ℂ), ← Finset.mul_sum]
      ring
    _ = _ := by
      simp only [he, Finset.sum_add_distrib, ← Finset.mul_sum]
      ring

theorem coherent_off_frequency (z mode amplitude : ℂ) (hz : ‖z‖ = 1) (hm : ‖mode‖ = 1)
    (hforward : mode * star z ≠ 1) (hbackward : star mode * star z ≠ 1) :
    HasCoherentResponse z (fun n => (amplitude * mode ^ n).re) 0 := by
  have hf := (average_mode_tendsto_zero (mode * star z) (by simp [hz, hm]) hforward).const_mul amplitude
  have hb := (average_mode_tendsto_zero (star mode * star z) (by simp [hz, hm]) hbackward).const_mul (star amplitude)
  change Tendsto (fun count => coherent z (fun n => (amplitude * mode ^ n).re) count) atTop (𝓝 0)
  simpa only [HasCoherentResponse, coherent_mode, mul_zero, add_zero] using hf.add hb

theorem re_product (u v : ℂ) : u.re * v.re = ((u * v).re + (u * star v).re) / 2 := by
  simp only [Complex.mul_re, Complex.star_def, Complex.conj_re, Complex.conj_im]
  ring

/-- Multiplication by a second-harmonic coefficient produces a fundamental
correction C*conj(A)/2 as well as the third harmonic C*A/2. -/
theorem modulated_tone_decomposition (z A C : ℂ) (center : ℝ) (hz : ‖z‖ = 1) (n : ℕ) :
    (center + (C * (z * z) ^ n).re) * (A * z ^ n).re =
      (((center : ℂ) * A + C * star A / 2) * z ^ n).re +
        ((C * A / 2) * (z * z * z) ^ n).re := by
  have hp : z ^ n * star z ^ n = 1 := by rw [← mul_pow, unit_mul_star z hz, one_pow]
  have he : (C * (z * z) ^ n) * (A * z ^ n) + (C * (z * z) ^ n) * star (A * z ^ n) =
      (C * A) * (z * z * z) ^ n + (C * star A) * z ^ n := by
    simp only [mul_pow, star_mul, star_pow]
    linear_combination C * star A * z ^ n * hp
  rw [add_mul, re_product, ← Complex.add_re, he, Complex.add_re]
  simp only [add_mul, div_mul_eq_mul_div, Complex.add_re, Complex.div_ofNat_re,
    mul_assoc, Complex.mul_re, Complex.ofReal_re, Complex.ofReal_im, zero_mul, sub_zero]
  ring

/-- The integral of the wobble term generally survives. The hypotheses
exclude aliases of the third harmonic onto the measured frequency. -/
theorem coherent_modulated_tone (z A C : ℂ) (center : ℝ) (hz : ‖z‖ = 1)
    (hfrequency : star z * star z ≠ 1)
    (hforward : (z * z * z) * star z ≠ 1)
    (hbackward : star (z * z * z) * star z ≠ 1) :
    HasCoherentResponse z
      (fun n => (center + (C * (z * z) ^ n).re) * (A * z ^ n).re)
      ((center : ℂ) * A + C * star A / 2) := by
  have hf := coherent_sinusoid_tendsto z ((center : ℂ) * A + C * star A / 2) hz hfrequency
  have ht := coherent_off_frequency z (z * z * z) (C * A / 2) hz
    (by simp [hz]) hforward hbackward
  have h := hf.add ht
  simp only [add_zero] at h
  apply h.congr
  intro count
  rw [← coherent_add]
  congr 2
  funext n
  exact (modulated_tone_decomposition z A C center hz n).symm

end DSP.ResponseMeasurement
