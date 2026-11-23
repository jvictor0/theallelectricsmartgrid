import Mathlib.Analysis.Fourier.AddCircleMulti

/-! Exact Fourier coordinates for continuous waveforms with one or more phases.
The phase increment is in cycles per sample. A second phase can represent an
independent oscillation; no synchronization or convergence is assumed.
All coefficients are retained. Finite harmonic truncation is a separate step. -/

noncomputable section
namespace DSP.Harmonics
open MeasureTheory UnitAddTorus
open scoped ComplexConjugate

local instance : MeasureSpace UnitAddCircle := ⟨AddCircle.haarAddCircle⟩
local instance : Measure.IsAddHaarMeasure (volume : Measure UnitAddCircle) :=
  inferInstanceAs (Measure.IsAddHaarMeasure AddCircle.haarAddCircle)
local instance : IsProbabilityMeasure (volume : Measure UnitAddCircle) :=
  inferInstanceAs (IsProbabilityMeasure AddCircle.haarAddCircle)

variable {d : Type*} [Fintype d]

abbrev Phase (d : Type*) := UnitAddTorus d
abbrev Waveform (d : Type*) := C(Phase d, ℝ)
abbrev Spectrum (d : Type*) := (d → ℤ) → ℂ

def complexify (f : Waveform d) : C(Phase d, ℂ) :=
  ⟨fun θ => (f θ : ℂ), Complex.continuous_ofReal.comp f.continuous⟩

def coeff (f : Waveform d) : Spectrum d := mFourierCoeff (complexify f)

def delay (ω : Phase d) (f : Waveform d) : Waveform d :=
  ⟨fun θ => f (θ - ω), f.continuous.comp (continuous_id.sub continuous_const)⟩

/-- Exact multiplier for a one-sample delay, including every mixed harmonic. -/
def delayFactor (ω : Phase d) (k : d → ℤ) : ℂ := mFourier (-k) ω

theorem character_add (k : d → ℤ) (θ φ : Phase d) :
    mFourier k (θ + φ) = mFourier k θ * mFourier k φ := by
  simp only [mFourier, ContinuousMap.coe_mk, Pi.add_apply, fourier_apply,
    smul_add, AddCircle.toCircle_add, Circle.coe_mul, Finset.prod_mul_distrib]

theorem complex_coeff_injective :
    Function.Injective (fun f : C(Phase d, ℂ) => mFourierCoeff f) := by
  intro f g h
  apply ContinuousMap.toLp_injective (p := 2) (𝕜 := ℂ) volume
  apply mFourierBasis.repr.injective
  ext k
  simp only [mFourierBasis_repr, mFourierCoeff_toLp]
  exact congr_fun h k

theorem coeff_injective : Function.Injective (coeff (d := d)) := by
  intro f g h
  have hc := complex_coeff_injective h
  ext θ
  exact Complex.ofReal_injective (congrArg (fun z : C(Phase d, ℂ) => z θ) hc)

theorem complex_coeff_add (f g : C(Phase d, ℂ)) (k : d → ℤ) :
    mFourierCoeff (f + g) k = mFourierCoeff f k + mFourierCoeff g k := by
  simp only [mFourierCoeff, ContinuousMap.add_apply, smul_eq_mul, mul_add]
  apply integral_add <;>
    exact (by fun_prop : Continuous _).integrable_of_hasCompactSupport (HasCompactSupport.of_compactSpace _)

theorem complex_coeff_mul (a : ℂ) (f : C(Phase d, ℂ)) (k : d → ℤ) :
    mFourierCoeff (a • f) k = a * mFourierCoeff f k := by
  simp only [mFourierCoeff, ContinuousMap.smul_apply, smul_eq_mul]
  simp_rw [mul_left_comm _ a]
  exact integral_const_mul a _

@[simp] theorem coeff_add (f g : Waveform d) (k : d → ℤ) :
    coeff (f + g) k = coeff f k + coeff g k := by
  have h : complexify (f + g) = complexify f + complexify g := by
    ext θ; simp [complexify]
  rw [coeff, h]
  exact complex_coeff_add _ _ _

@[simp] theorem coeff_smul (a : ℝ) (f : Waveform d) (k : d → ℤ) :
    coeff (a • f) k = (a : ℂ) * coeff f k := by
  have h : complexify (a • f) = (a : ℂ) • complexify f := by
    ext θ; simp [complexify]
  rw [coeff, h]
  exact complex_coeff_mul _ _ _

@[simp] theorem coeff_neg (f : Waveform d) (k : d → ℤ) :
    coeff (-f) k = -coeff f k := by
  simpa using coeff_smul (-1) f k

@[simp] theorem coeff_sub (f g : Waveform d) (k : d → ℤ) :
    coeff (f - g) k = coeff f k - coeff g k := by
  rw [sub_eq_add_neg, coeff_add, coeff_neg, sub_eq_add_neg]

theorem coeff_delay (ω : Phase d) (f : Waveform d) (k : d → ℤ) :
    coeff (delay ω f) k = delayFactor ω k * coeff f k := by
  unfold coeff mFourierCoeff
  simp only [complexify, delay, ContinuousMap.coe_mk, smul_eq_mul]
  rw [← integral_add_right_eq_self (fun θ : Phase d =>
    mFourier (-k) θ * (f (θ - ω) : ℂ)) ω]
  simp only [add_sub_cancel_right, character_add]
  simp_rw [mul_right_comm _ (mFourier (-k) ω), mul_comm _ (mFourier (-k) ω)]
  exact integral_const_mul _ _

/-- Equality of continuous waveforms is equivalent to equality at ALL modes. -/
theorem eq_iff_coeff (f g : Waveform d) : f = g ↔ ∀ k, coeff f k = coeff g k := by
  constructor
  · intro h; subst g; intro k; rfl
  · exact fun h => coeff_injective (funext h)

theorem character_norm (k : d → ℤ) (θ : Phase d) : ‖mFourier k θ‖ = 1 := by
  simp only [mFourier, ContinuousMap.coe_mk, norm_prod, fourier_apply,
    Circle.norm_coe, Finset.prod_const_one]

@[simp] theorem delayFactor_norm (ω : Phase d) (k : d → ℤ) : ‖delayFactor ω k‖ = 1 :=
  character_norm _ _

theorem complex_coeff_mode (m k : d → ℤ) :
    mFourierCoeff (mFourier m) k = if k = m then 1 else 0 := by
  classical
  rw [← mFourierCoeff_toLp, ← mFourierBasis_repr]
  change mFourierBasis.repr (mFourierLp 2 m) k = _
  rw [← congr_fun coe_mFourierBasis m, HilbertBasis.repr_self]
  simp only [lp.single_apply, Pi.single_apply]

/-- A peak phasor: real part of `z * exp(i phase)`, not a Fourier coefficient.
Its positive-frequency coefficient is `z/2` when the carrier is nonzero. -/
def realMode (z : ℂ) (m : d → ℤ) : Waveform d :=
  ⟨fun θ => (z * mFourier m θ).re, by fun_prop⟩

theorem complexify_realMode (z : ℂ) (m : d → ℤ) :
    complexify (realMode z m) =
      (z/2) • mFourier m + (conj z/2) • mFourier (-m) := by
  ext θ
  simp only [complexify, realMode, ContinuousMap.coe_mk, Complex.re_eq_add_conj,
    ContinuousMap.add_apply, ContinuousMap.smul_apply, smul_eq_mul,
    map_mul, mFourier_neg]
  ring

theorem coeff_realMode (z : ℂ) (m k : d → ℤ) :
    coeff (realMode z m) k =
      (z/2) * (if k = m then 1 else 0) + (conj z/2) * (if k = -m then 1 else 0) := by
  classical
  rw [coeff, complexify_realMode, complex_coeff_add, complex_coeff_mul,
    complex_coeff_mul, complex_coeff_mode, complex_coeff_mode]

/-- Negative modes of a real waveform carry no independent unknowns. -/
theorem coeff_conj (f : Waveform d) (k : d → ℤ) :
    coeff f (-k) = conj (coeff f k) := by
  unfold coeff mFourierCoeff
  rw [← integral_conj]
  apply integral_congr_ae
  filter_upwards [] with θ
  simp [complexify, smul_eq_mul, mFourier_neg]

/-- All-phase correlation with a sinusoid selects exactly its Fourier mode.
This is the averaging identity used for an oscillator's phase sensitivity. -/
theorem realMode_correlation (f : Waveform d) (z : ℂ) (m : d → ℤ) :
    (∫ θ, f θ * realMode z m θ) = (conj z * coeff f m).re := by
  unfold coeff mFourierCoeff
  simp only [smul_eq_mul]
  rw [← integral_const_mul]
  rw [← RCLike.re_eq_complex_re]
  rw [← integral_re (𝕜 := ℂ) (by
    exact (by fun_prop : Continuous (fun θ =>
      conj z * (mFourier (-m) θ • complexify f θ))).integrable_of_hasCompactSupport
        (HasCompactSupport.of_compactSpace _))]
  apply integral_congr_ae
  filter_upwards [] with θ
  simp [realMode, complexify, mFourier_neg, Complex.mul_re]
  ring

/-- A real cosine of arbitrary peak amplitude at an integer combination of
the phases. Its initial phase is selected when sampling the waveform. -/
def stimulus (amplitude : ℝ) (m : d → ℤ) : Waveform d :=
  ⟨fun θ => amplitude * (mFourier m θ).re, by fun_prop⟩

theorem complexify_stimulus (amplitude : ℝ) (m : d → ℤ) :
    complexify (stimulus amplitude m) =
      ((amplitude : ℂ)/2) • (mFourier m + mFourier (-m)) := by
  ext θ
  simp only [complexify, stimulus, ContinuousMap.coe_mk, Complex.ofReal_mul,
    Complex.re_eq_add_conj, ContinuousMap.smul_apply, ContinuousMap.add_apply,
    smul_eq_mul, mFourier_neg]
  ring

/-- No chosen amplitude, odd-harmonic assumption, or missing conjugate mode.
At zero carrier the two contributions coincide and give the correct DC value. -/
theorem stimulus_coeff (amplitude : ℝ) (m k : d → ℤ) :
    coeff (stimulus amplitude m) k = ((amplitude : ℂ)/2) *
      ((if k = m then 1 else 0) + (if k = -m then 1 else 0)) := by
  classical
  rw [coeff, complexify_stimulus, complex_coeff_mul, complex_coeff_add,
    complex_coeff_mode, complex_coeff_mode]

theorem character_nsmul (k : d → ℤ) (ω : Phase d) (n : ℕ) :
    mFourier k (n • ω) = (mFourier k ω) ^ n := by
  induction n with
  | zero => simp only [zero_nsmul, pow_zero, mFourier, ContinuousMap.coe_mk,
      Pi.zero_apply, fourier_eval_zero, Finset.prod_const_one]
  | succ n ih => rw [succ_nsmul, character_add, ih, pow_succ]

/-- The stimulus frequency is the selected integer combination of phase
increments. Its peak amplitude is a free real parameter. -/
theorem stimulus_sample (amplitude : ℝ) (m : d → ℤ) (θ ω : Phase d) (n : ℕ) :
    stimulus amplitude m (θ+n • ω) =
      amplitude * (mFourier m θ * (mFourier m ω)^n).re := by
  simp only [stimulus, ContinuousMap.coe_mk, character_add, character_nsmul]

def synthesize (c : Spectrum d) (θ : Phase d) : ℂ :=
  ∑' k, c k * mFourier k θ

/-- Absolutely summable coefficients reconstruct the full waveform pointwise;
no finite-harmonic or small-input approximation is involved. -/
theorem synthesize_coeff (f : Waveform d) (h : Summable (coeff f)) (θ : Phase d) :
    synthesize (coeff f) θ = (f θ : ℂ) := by
  exact (hasSum_mFourier_series_apply_of_summable h θ).tsum_eq

/-- Average over ALL phases. This is not automatically the time average of
a sampled orbit: rational rotations and resonant multi-indices can alias. -/
def phasePower (f : Waveform d) : ℝ := ∫ θ, f θ ^ 2

theorem phasePower_eq_spectrum (f : Waveform d) :
    phasePower f = ∑' k, ‖coeff f k‖ ^ 2 := by
  have h := (hasSum_sq_mFourierCoeff ((complexify f).toLp 2 volume ℂ)).tsum_eq
  simp only [mFourierCoeff_toLp] at h
  change phasePower f = ∑' k, ‖mFourierCoeff (complexify f) k‖ ^ 2
  rw [h]
  unfold phasePower
  apply integral_congr_ae
  filter_upwards [(complexify f).coeFn_toLp (p := 2) (𝕜 := ℂ) volume] with θ hθ
  rw [hθ]
  simp [complexify, Complex.norm_real, Real.norm_eq_abs, sq_abs]

/-- Full coefficient residuals retain exactly the phase-averaged squared
waveform residual. This applies to a finite solver's omitted equations too. -/
theorem residual_power (f g : Waveform d) :
    phasePower (f-g) = ∑' k, ‖coeff f k - coeff g k‖ ^ 2 := by
  rw [phasePower_eq_spectrum]
  simp only [coeff_sub]

/-- Solving the retained equations leaves exactly the omitted residual
energy. A residual bound alone is NOT a trajectory-error or attraction bound. -/
theorem omitted_residual_energy (r : Waveform d) (kept : Finset (d → ℤ))
    (h : ∀ k ∈ kept, coeff r k = 0) :
    phasePower r = ∑' k, if k ∈ kept then 0 else ‖coeff r k‖ ^ 2 := by
  classical
  rw [phasePower_eq_spectrum]
  apply tsum_congr
  intro k
  by_cases hk : k ∈ kept
  · simp [hk, h k hk]
  · simp [hk]

/-- A uniform waveform error bounds every Fourier coefficient, without any
assumption about the dynamics or the number of harmonics kept by a solver. -/
theorem coeff_error_le (f g : Waveform d) (ε : ℝ)
    (h : ∀ θ, |f θ-g θ| ≤ ε) (k : d → ℤ) :
    ‖coeff f k-coeff g k‖ ≤ ε := by
  rw [← coeff_sub]
  unfold coeff mFourierCoeff
  have hn : ∀ θ : Phase d, ‖mFourier (-k) θ • complexify (f-g) θ‖ ≤ ε := by
    intro θ
    simpa only [norm_smul, character_norm, one_mul, complexify, ContinuousMap.coe_mk,
      ContinuousMap.sub_apply, Complex.norm_real, Real.norm_eq_abs] using h θ
  simpa using norm_integral_le_of_norm_le_const (μ := volume) (Filter.Eventually.of_forall hn)

end DSP.Harmonics
