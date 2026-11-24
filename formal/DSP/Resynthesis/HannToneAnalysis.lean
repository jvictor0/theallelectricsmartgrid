import DSP.Resynthesis.HannToneForward
import DSP.Resynthesis.HannTonePhase

namespace DSP.Resynthesis

private theorem get_ofFn {A : Type} {n : ℕ} (f : ActiveBin n → A) (b : ActiveBin n) :
    (BinTable.ofFn f).get b = f b := by
  simp [BinTable.ofFn, BinTable.get]

private theorem hannSpectrum_magnitude {n : ℕ} {amplitude : ℝ}
    (positive : 0 < amplitude) (phase : ℝ) (bin : ℕ) (b : ActiveBin n) :
    exactMath.magnitude ((hannSinusoidSpectrum amplitude bin phase).coeff b.coefficient) =
      if b.coefficient.val = bin then amplitude / 4
      else if b.coefficient.val + 1 = bin ∨ b.coefficient.val = bin + 1 then amplitude / 8
      else 0 := by
  unfold hannSinusoidSpectrum
  rw [exactMagnitude_polar]
  split_ifs <;> first
  | exact abs_of_nonneg (by positivity)
  | rw [neg_div, abs_neg, abs_of_nonneg (by positivity)]

/-- Two consecutive captures of an interior Hann tone have the same three
magnitudes, recover the carrier frequency, and preserve the side signs under
phase locking to the carrier. -/
theorem analysisInput_hannSinusoid {n : ℕ} [NeZero n] {amplitude : ℝ}
    (positive : 0 < amplitude) (phase start : ℝ) (bin : ℕ)
    (hlo : 1 < bin) (hhi : bin + 1 < n / 2) :
    let frequency := (bin : ℝ) / n
    let input := analysisInput exactMath
      (fft (hannSinusoid (n := n) amplitude bin (frequency * (start - ↑(n / 4)) + phase)))
      (fft (hannSinusoid (n := n) amplitude bin (frequency * start + phase)))
    let magnitude := fun b : ActiveBin n =>
      if b.coefficient.val = bin then amplitude / 4
      else if b.coefficient.val + 1 = bin ∨ b.coefficient.val = bin + 1 then amplitude / 8
      else 0
    (∀ b, input.prevMagnitude.get b = magnitude b) ∧
    (∀ b, input.magnitude.get b = magnitude b) ∧
    (∀ center : ActiveBin n, center.coefficient.val = bin → input.omega.get center = frequency) ∧
    (∀ center : ActiveBin n, center.coefficient.val = bin →
      input.phase.get center = exactMath.phase (exactMath.polar (amplitude / 4)
        (frequency * start + phase))) ∧
    (∀ center side : ActiveBin n, center.coefficient.val = bin →
      (side.coefficient.val + 1 = bin ∨ side.coefficient.val = bin + 1) → ∀ q,
      exactMath.polar (amplitude / 8) (q + (input.phase.get side - input.phase.get center)) =
        exactMath.polar (-amplitude / 8) q) := by
  dsimp only
  rw [fft_hannSinusoid _ _ bin hlo hhi, fft_hannSinusoid _ _ bin hlo hhi]
  refine ⟨?_, ?_, ?_, ?_, ?_⟩
  · intro b
    simpa only [analysisInput, get_ofFn] using hannSpectrum_magnitude positive _ bin b
  · intro b
    simpa only [analysisInput, get_ofFn] using hannSpectrum_magnitude positive _ bin b
  · intro center atCenter
    have hop_ne : (↑(n / 4) : ℝ) ≠ 0 := by
      exact_mod_cast (show n / 4 ≠ 0 by omega)
    simp only [analysisInput, get_ofFn, hannSinusoidSpectrum, atCenter, if_true]
    exact instantaneousFrequency_stationary_polar (by positivity : 0 < amplitude / 4)
      hop_ne ((bin : ℝ) / n) phase start
  · intro center atCenter
    simp only [analysisInput, get_ofFn, hannSinusoidSpectrum, atCenter, if_true]
  · intro center side atCenter adjacent q
    have notCenter : side.coefficient.val ≠ bin := by omega
    simp only [analysisInput, get_ofFn, hannSinusoidSpectrum, atCenter, if_true,
      if_neg notCenter, if_pos adjacent]
    exact exactPolar_hann_follower positive _ q

end DSP.Resynthesis
