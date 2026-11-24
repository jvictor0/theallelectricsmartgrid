import DSP.Resynthesis.HannTone
import Mathlib.Tactic.FieldSimp
import Mathlib.Tactic.Ring

namespace DSP.Resynthesis

private theorem polar_complex (radius phase : ℝ) :
    (⟨(exactMath.polar radius phase).real, (exactMath.polar radius phase).imag⟩ : ℂ) =
      (radius : ℂ) * (Complex.cos ((2 * Real.pi * phase : ℝ) : ℂ) +
        Complex.sin ((2 * Real.pi * phase : ℝ) : ℂ) * Complex.I) := by
  apply Complex.ext <;>
    simp only [exactMath, cos2pi_real, sin2pi_real, Complex.mul_re, Complex.mul_im,
      Complex.add_re, Complex.add_im, Complex.ofReal_re, Complex.ofReal_im, Complex.I_re,
      Complex.I_im, Complex.cos_ofReal_re, Complex.cos_ofReal_im, Complex.sin_ofReal_re,
      Complex.sin_ofReal_im] <;> ring

theorem exactMagnitude_polar (radius phase : ℝ) :
    exactMath.magnitude (exactMath.polar radius phase) = |radius| := by
  change ‖(⟨(exactMath.polar radius phase).real, (exactMath.polar radius phase).imag⟩ : ℂ)‖ = _
  rw [polar_complex, norm_mul, Complex.norm_cos_add_sin_mul_I, mul_one, Complex.norm_real, Real.norm_eq_abs]

theorem exactPhase_polar_pos {radius : ℝ} (positive : 0 < radius) (phase : ℝ) :
    ∃ turns : ℤ, exactMath.phase (exactMath.polar radius phase) = phase + (turns : ℝ) := by
  refine ⟨⌊(Real.pi - 2 * Real.pi * phase) / (2 * Real.pi)⌋, ?_⟩
  change Complex.arg (⟨(exactMath.polar radius phase).real, (exactMath.polar radius phase).imag⟩ : ℂ) /
    (2 * Real.pi) = _
  rw [polar_complex]
  apply (div_eq_iff Real.two_pi_pos.ne').2
  calc
    _ = 2 * Real.pi * phase + 2 * Real.pi * ↑⌊(Real.pi - 2 * Real.pi * phase) / (2 * Real.pi)⌋ :=
      (sub_eq_iff_eq_add.mp (Complex.arg_mul_cos_add_sin_mul_I_sub positive _)).trans (add_comm _ _)
    _ = _ := by ring

theorem exactPolar_add_int (radius phase : ℝ) (turns : ℤ) :
    exactMath.polar radius (phase + (turns : ℝ)) = exactMath.polar radius phase := by
  have angle : 2 * Real.pi * (phase + (turns : ℝ)) =
      2 * Real.pi * phase + (turns : ℝ) * (2 * Real.pi) := by ring
  simp [exactMath, angle, Real.cos_add_int_mul_two_pi, Real.sin_add_int_mul_two_pi]

theorem exactPolar_add_half (radius phase : ℝ) :
    exactMath.polar radius (phase + 1 / 2) = exactMath.polar (-radius) phase := by
  have angle : 2 * Real.pi * (phase + 1 / 2) = 2 * Real.pi * phase + Real.pi := by ring
  simp only [exactMath, cos2pi_real, sin2pi_real, angle, Real.cos_add_pi, Real.sin_add_pi, mul_neg, neg_mul]

/-- A follower of a Hann-windowed tone keeps its negative side coefficient
when the leader is given a new phase. -/
theorem exactPolar_hann_follower {amplitude : ℝ} (positive : 0 < amplitude)
    (inputPhase outputPhase : ℝ) :
    exactMath.polar (amplitude / 8) (outputPhase +
      (exactMath.phase (exactMath.polar (-amplitude / 8) inputPhase) -
        exactMath.phase (exactMath.polar (amplitude / 4) inputPhase))) =
      exactMath.polar (-amplitude / 8) outputPhase := by
  obtain ⟨rootTurns, root⟩ := exactPhase_polar_pos (by positivity : 0 < amplitude / 4) inputPhase
  obtain ⟨sideTurns, side⟩ := exactPhase_polar_pos (by positivity : 0 < amplitude / 8)
    (inputPhase + 1 / 2)
  rw [exactPolar_add_half, ← neg_div] at side
  rw [root, side]
  have phase : outputPhase + (inputPhase + 1 / 2 + (sideTurns : ℝ) -
      (inputPhase + (rootTurns : ℝ))) = (outputPhase + 1 / 2) + ((sideTurns - rootTurns : ℤ) : ℝ) := by
    push_cast
    ring
  rw [phase, exactPolar_add_int, exactPolar_add_half, ← neg_div]

/-- The phase-deviation estimator removes the analysis start time and both
principal-argument wraps for a tone at the center of the analysis bin. -/
theorem instantaneousFrequency_stationary_polar {radius hop : ℝ} (positive : 0 < radius)
    (hop_ne : hop ≠ 0) (frequency phase start : ℝ) :
    instantaneousFrequency exactMath.phaseFloor frequency hop
      (exactMath.phase (exactMath.polar radius (frequency * start + phase)))
      (exactMath.phase (exactMath.polar radius (frequency * (start - hop) + phase))) = frequency := by
  obtain ⟨now, hnow⟩ := exactPhase_polar_pos positive (frequency * start + phase)
  obtain ⟨prev, hprev⟩ := exactPhase_polar_pos positive (frequency * (start - hop) + phase)
  rw [hnow, hprev]
  have residual : phaseResidual frequency hop (frequency * start + phase + (now : ℝ))
      (frequency * (start - hop) + phase + (prev : ℝ)) = ((now - prev : ℤ) : ℝ) := by
    unfold phaseResidual
    push_cast
    ring
  rw [instantaneousFrequency, residual]
  simp [exactMath, principalArgument, IfLess.ifLess, correctedFrequency, hop_ne]
  norm_num

end DSP.Resynthesis
