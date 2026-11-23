import DSP.Analysis.Harmonics
import Mathlib.Analysis.SpecialFunctions.Trigonometric.Inverse

/-! Algebra of the averaged scalar phase model, not a validity theorem for
phase reduction of a nonlinear processor. An attracting orbit, its asymptotic
phase derivative, and a controlled averaging remainder must be supplied
separately before these equations describe that processor. -/

noncomputable section
namespace DSP.PhaseLocking

open Filter
open scoped Topology

variable {E : Type*} [NormedAddCommGroup E] [NormedSpace ℝ E]

/-- Differentiating a locally lifted invariant phase gives the discrete
adjoint equation. This assumes the phase coordinate exists and is smooth. -/
theorem invariant_phase_adjoint (step : E → E) (phase : E → ℝ) (q : E)
    (omega : ℝ) (jacobian : E →L[ℝ] E) (z zNext : E →L[ℝ] ℝ)
    (hStep : HasFDerivAt step jacobian q)
    (hPhase : HasFDerivAt phase z q)
    (hNext : HasFDerivAt phase zNext (step q))
    (hInvariant : (fun x => phase (step x)) =ᶠ[𝓝 q] (fun x => phase x + omega)) :
    zNext.comp jacobian = z := by
  exact (hNext.comp q hStep).unique
    ((hPhase.add_const omega).congr_of_eventuallyEq hInvariant)

/-- The first-order phase kick from one input sample is `Z(next) * B`.
Again, the phase derivative is an explicit hypothesis, not an assumed theorem
about existence or stability of a ladder orbit. -/
theorem phase_kick_derivative (inputStep : ℝ → E) (phase : E → ℝ)
    (input : ℝ) (b : E) (zNext : E →L[ℝ] ℝ)
    (hInput : HasDerivAt inputStep b input)
    (hPhase : HasFDerivAt phase zNext (inputStep input)) :
    HasDerivAt (fun u => phase (inputStep u)) (zNext b) input :=
  hPhase.comp_hasDerivAt input hInput

/-- Phase mismatch update after sinusoidal averaging, in radians per sample.
The phase origin absorbs the argument of the sensitivity's Fourier mode. -/
def drift (detuning width phase : ℝ) : ℝ := detuning + width * Real.sin phase

/-- Exact existence criterion inside the scalar averaged model. At the
boundary this allows a marginal fixed point; it does not assert attraction. -/
theorem equilibrium_iff (detuning width : ℝ) (hw : 0 ≤ width) :
    (∃ phase, drift detuning width phase = 0) ↔ |detuning| ≤ width := by
  constructor
  · rintro ⟨phase,h⟩
    have hd : detuning = -(width * Real.sin phase) := by
      unfold drift at h; linarith
    rw [hd, abs_neg, abs_mul, abs_of_nonneg hw]
    nlinarith [Real.abs_sin_le_one phase]
  · intro h
    rcases eq_or_lt_of_le hw with hw0 | hwpos
    · have hd : detuning = 0 := by
        rw [← hw0] at h
        exact abs_eq_zero.mp (le_antisymm h (abs_nonneg _))
      exact ⟨0, by simp [drift, hd]⟩
    · have hr : |(-detuning)/width| ≤ 1 := by
        rw [abs_div, abs_neg, abs_of_pos hwpos]
        exact (div_le_one hwpos).mpr h
      refine ⟨Real.arcsin (-detuning/width), ?_⟩
      rw [drift, Real.sin_arcsin (abs_le.mp hr).1 (abs_le.mp hr).2]
      field_simp
      ring

/-- Peak input amplitude is a parameter. `sensitivity` is the COMPLEX Fourier
coefficient of phase kick per unit input, not its doubled peak phasor. -/
def halfWidth (harmonic : ℕ) (amplitude : ℝ) (sensitivity : ℂ) : ℝ :=
  harmonic * |amplitude| * ‖sensitivity‖

theorem halfWidth_nonneg (harmonic : ℕ) (amplitude : ℝ) (sensitivity : ℂ) :
    0 ≤ halfWidth harmonic amplitude sensitivity := by
  unfold halfWidth
  positivity

theorem harmonic_equilibrium_iff (harmonic : ℕ) (oscillator input amplitude : ℝ)
    (sensitivity : ℂ) :
    (∃ phase, drift (harmonic*oscillator-input)
      (halfWidth harmonic amplitude sensitivity) phase = 0) ↔
    |harmonic*oscillator-input| ≤ harmonic * |amplitude| * ‖sensitivity‖ :=
  equilibrium_iff _ _ (halfWidth_nonneg _ _ _)

end DSP.PhaseLocking
