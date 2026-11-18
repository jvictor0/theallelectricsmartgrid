import DSP.Analysis.Asymptotic
import Mathlib.Analysis.Asymptotics.SpecificAsymptotics
import Mathlib.Algebra.Field.GeomSum

/-! Long-time measurements for nonlinear real-valued DSP. A coherent Fourier
coefficient retains phase and rejects distinct frequencies; it is a projection,
not a norm of the complete output. Mean-square/RMS measurements retain the
energy of harmonics and autonomous oscillations. Neither definition asserts
that the relevant limits exist for the adaptive ladder. -/

noncomputable section
namespace DSP.ResponseMeasurement
open Filter Finset
open scoped Topology

def average (signal : ℕ → ℂ) (count : ℕ) : ℂ :=
  (count : ℝ)⁻¹ • ∑ n ∈ range count, signal n

/-- z is exp(i*omega). The factor two accounts for the conjugate component of
a real sinusoid. DC and Nyquist require a different normalization. -/
def coherent (z : ℂ) (signal : ℕ → ℝ) (count : ℕ) : ℂ :=
  2 * average (fun n => (signal n : ℂ) * star z ^ n) count

def normalizedCoherent (amplitude : ℝ) (z : ℂ) (signal : ℕ → ℝ) (count : ℕ) : ℂ :=
  coherent z signal count / (amplitude : ℂ)

def meanSquare (signal : ℕ → ℝ) (count : ℕ) : ℝ :=
  (count : ℝ)⁻¹ * ∑ n ∈ range count, signal n ^ 2

def rms (signal : ℕ → ℝ) (count : ℕ) : ℝ := Real.sqrt (meanSquare signal count)

/-- For a positive peak-amplitude cosine stimulus, this is total output RMS
divided by input RMS. Harmonics and other oscillations remain in this measure. -/
def normalizedRms (amplitude : ℝ) (signal : ℕ → ℝ) (count : ℕ) : ℝ :=
  Real.sqrt 2 / amplitude * rms signal count

def HasMeanSquare (signal : ℕ → ℝ) (power : ℝ) : Prop :=
  Tendsto (meanSquare signal) atTop (𝓝 power)

/-- The inner (time) limit at a fixed positive stimulus amplitude. -/
def HasCoherentResponse (z : ℂ) (signal : ℕ → ℝ) (response : ℂ) : Prop :=
  Tendsto (coherent z signal) atTop (𝓝 response)

/-- Time is taken to infinity FIRST. The second limit approaches zero
amplitude from above. A waveform-convergence theorem alone does not supply it. -/
def HasSmallSignalResponse (z : ℂ) (output : ℝ → ℕ → ℝ) (response : ℂ) : Prop :=
  ∃ settled : ℝ → ℂ,
    (∀ amplitude, 0 < amplitude → HasCoherentResponse z (output amplitude) (settled amplitude)) ∧
    Tendsto (fun amplitude => settled amplitude / (amplitude : ℂ)) (𝓝[>] 0) (𝓝 response)

/-- Companion energy measurement. A finite coherent response alone could hide
an oscillation at a different frequency, so it is insufficient for a gain bound. -/
def HasSmallSignalPowerGain (output : ℝ → ℕ → ℝ) (gainSquared : ℝ) : Prop :=
  ∃ settledPower : ℝ → ℝ,
    (∀ amplitude, 0 < amplitude → HasMeanSquare (output amplitude) (settledPower amplitude)) ∧
    Tendsto (fun amplitude => 2 * settledPower amplitude / amplitude ^ 2)
      (𝓝[>] 0) (𝓝 gainSquared)

/-- Decaying startup transients disappear in the long-time average. -/
theorem average_tendsto_zero (signal : ℕ → ℂ) (h : Tendsto signal atTop (𝓝 0)) :
    Tendsto (average signal) atTop (𝓝 0) := h.cesaro_smul

/-- A finite-window leakage bound, valid even for a noninteger number of
periods. Distinct digital frequencies mean z != 1 after demodulation. -/
theorem average_mode_bound (z : ℂ) (hz : ‖z‖ = 1) (hne : z ≠ 1) (count : ℕ) :
    ‖average (fun n => z ^ n) count‖ ≤ (count : ℝ)⁻¹ * (2 / ‖z - 1‖) := by
  unfold average
  rw [norm_smul, Real.norm_of_nonneg (inv_nonneg.mpr (Nat.cast_nonneg count)), geom_sum_eq hne,
    norm_div]
  apply mul_le_mul_of_nonneg_left _ (inv_nonneg.mpr (Nat.cast_nonneg count))
  apply div_le_div_of_nonneg_right _ (norm_nonneg _)
  calc
    ‖z ^ count - 1‖ ≤ ‖z ^ count‖ + ‖(1 : ℂ)‖ := norm_sub_le _ _
    _ = 2 := by simp [norm_pow, hz]; norm_num

theorem average_mode_tendsto_zero (z : ℂ) (hz : ‖z‖ = 1) (hne : z ≠ 1) :
    Tendsto (average (fun n => z ^ n)) atTop (𝓝 0) := by
  apply tendsto_zero_iff_norm_tendsto_zero.mpr
  have h := (tendsto_inv_atTop_nhds_zero_nat (𝕜 := ℝ)).mul_const (2 / ‖z - 1‖)
  simp only [zero_mul] at h
  exact squeeze_zero (fun n => norm_nonneg _) (average_mode_bound z hz hne) h

theorem unit_mul_star (z : ℂ) (hz : ‖z‖ = 1) : z * star z = 1 := by
  rw [Complex.star_def, Complex.mul_conj, Complex.normSq_eq_norm_sq, hz]
  norm_num

/-- Projecting a real sinusoid leaves its complex amplitude and a finite-window
conjugate-frequency leakage term. Frequencies at 0/pi are excluded later. -/
theorem coherent_sinusoid (z amplitude : ℂ) (hz : ‖z‖ = 1) (count : ℕ) (hc : count ≠ 0) :
    coherent z (fun n => (amplitude * z ^ n).re) count =
      amplitude + star amplitude * average (fun n => (star z * star z) ^ n) count := by
  have he : ∀ n : ℕ, 2 * (((amplitude * z ^ n).re : ℝ) : ℂ) * star z ^ n =
      amplitude + star amplitude * (star z * star z) ^ n := by
    intro n
    rw [Complex.re_eq_add_conj]
    simp only [map_mul, map_pow, starRingEnd_apply]
    have hp : z ^ n * star z ^ n = 1 := by rw [← mul_pow, unit_mul_star z hz, one_pow]
    rw [mul_pow]
    linear_combination amplitude * hp
  have hn : (count : ℂ) ≠ 0 := by exact_mod_cast hc
  simp only [coherent, average, Complex.real_smul, Complex.ofReal_inv, Complex.ofReal_natCast]
  calc
    _ = (count : ℂ)⁻¹ * ∑ n ∈ range count,
        (2 * (((amplitude * z ^ n).re : ℝ) : ℂ) * star z ^ n) := by
      simp_rw [mul_assoc (2 : ℂ), ← Finset.mul_sum]
      ring
    _ = _ := by
      simp only [he, Finset.sum_add_distrib, Finset.sum_const, card_range,
        nsmul_eq_mul, ← Finset.mul_sum]
      field_simp

theorem coherent_sinusoid_tendsto (z amplitude : ℂ) (hz : ‖z‖ = 1)
    (hfrequency : star z * star z ≠ 1) :
    HasCoherentResponse z (fun n => (amplitude * z ^ n).re) amplitude := by
  have hn : ‖star z * star z‖ = 1 := by simp [hz]
  have h := ((average_mode_tendsto_zero _ hn hfrequency).const_mul (star amplitude)).const_add amplitude
  simp only [mul_zero, add_zero] at h
  apply h.congr'
  filter_upwards [eventually_ge_atTop 1] with count hc
  exact (coherent_sinusoid z amplitude hz count (by omega)).symm

theorem coherent_sub (z : ℂ) (signal reference : ℕ → ℝ) (count : ℕ) :
    coherent z (fun n => signal n - reference n) count =
      coherent z signal count - coherent z reference count := by
  simp only [coherent, average, Complex.ofReal_sub, sub_mul, Finset.sum_sub_distrib, smul_sub, mul_sub]

/-- Decaying waveform errors also vanish after demodulation and averaging. -/
theorem coherent_error_tendsto_zero (z : ℂ) (hz : ‖z‖ = 1)
    (error : ℕ → ℝ) (h : Tendsto error atTop (𝓝 0)) :
    Tendsto (coherent z error) atTop (𝓝 0) := by
  have he : Tendsto (fun n => (error n : ℂ) * star z ^ n) atTop (𝓝 0) := by
    apply tendsto_zero_iff_norm_tendsto_zero.mpr
    simpa [norm_mul, norm_pow, hz] using h.norm
  change Tendsto (fun count => (2 : ℂ) * average
    (fun n => (error n : ℂ) * star z ^ n) count) atTop (𝓝 0)
  simpa only [mul_zero] using (average_tendsto_zero _ he).const_mul 2

theorem HasCoherentResponse.of_tracking {z : ℂ} (hz : ‖z‖ = 1)
    {signal reference : ℕ → ℝ} {response : ℂ}
    (hr : HasCoherentResponse z reference response)
    (h : Tendsto (fun n => signal n - reference n) atTop (𝓝 0)) :
    HasCoherentResponse z signal response := by
  have he := coherent_error_tendsto_zero z hz _ h
  change Tendsto (fun count => coherent z (fun n => signal n - reference n) count) _ _ at he
  simp only [coherent_sub] at he
  have hs := he.add hr
  simpa only [HasCoherentResponse, sub_add_cancel, zero_add] using hs

/-- A sinusoid plus a decaying transient has the expected coherent response. -/
theorem coherent_of_sinusoid_tracking (z amplitude : ℂ) (hz : ‖z‖ = 1)
    (hfrequency : star z * star z ≠ 1) (signal : ℕ → ℝ)
    (h : Tendsto (fun n => signal n - (amplitude * z ^ n).re) atTop (𝓝 0)) :
    HasCoherentResponse z signal amplitude :=
  (coherent_sinusoid_tendsto z amplitude hz hfrequency).of_tracking hz h

/-- Calibration: when every positive-amplitude experiment tracks H times its
stimulus, both nested limits exist and give H. Used below for the real detector
bandpasses; not asserted for the nonlinear adaptive ladder. -/
theorem smallSignal_of_tracking (z H : ℂ) (hz : ‖z‖ = 1)
    (hfrequency : star z * star z ≠ 1) (output : ℝ → ℕ → ℝ)
    (h : ∀ amplitude, 0 < amplitude → Tendsto
      (fun n => output amplitude n - (H * (amplitude : ℂ) * z ^ n).re) atTop (𝓝 0)) :
    HasSmallSignalResponse z output H := by
  refine ⟨fun amplitude => H * (amplitude : ℂ), ?_, ?_⟩
  · intro amplitude ha
    exact coherent_of_sinusoid_tracking z _ hz hfrequency _ (h amplitude ha)
  · apply tendsto_const_nhds.congr'
    filter_upwards [self_mem_nhdsWithin] with amplitude ha
    have hn : (amplitude : ℂ) ≠ 0 := by exact_mod_cast ne_of_gt ha
    simp [hn]

end DSP.ResponseMeasurement
