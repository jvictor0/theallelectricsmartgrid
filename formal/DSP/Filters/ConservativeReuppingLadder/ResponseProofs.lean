import DSP.Filters.ConservativeReuppingLadder.Response
import DSP.Filters.ConservativeReuppingLadder.Convergence
import DSP.Response.Proofs

noncomputable section
namespace DSP.ConservativeReuppingLadder
open Channels

def Snapshot.response (s : Snapshot ℝ) (q : ℂ) : ℂ :=
  ((1+(s.k:ℂ))*(s.slope:ℂ)^4) /
    ((1+(s.memoryRatio:ℂ)*(1-q))^4+(s.k:ℂ)*(s.slope:ℂ)^3*q)

theorem snapshot_response (s : ConservativeReuppingLadder ℝ) (ha : s.alpha ≠ 0) (q : ℂ) :
    (snapshot s).response q =
      transfer (s.alpha:ℂ) ((NormalizedTanh.slopeZero s.saturation:ℝ):ℂ) (s.k:ℂ) q := by
  have ha' : (s.alpha:ℂ) ≠ 0 := by exact_mod_cast ha
  have he : 1+((1-(s.alpha:ℂ))/(s.alpha:ℂ))*(1-q) =
      (1-(1-(s.alpha:ℂ))*q)/(s.alpha:ℂ) := by field_simp; ring
  simp only [snapshot, Snapshot.response, Complex.ofReal_div, Complex.ofReal_sub, Complex.ofReal_one, he]
  rw [div_pow]
  have hd : (1-(1-(s.alpha:ℂ))*q)^4/(s.alpha:ℂ)^4+
      (s.k:ℂ)*((NormalizedTanh.slopeZero s.saturation:ℝ):ℂ)^3*q =
      ((1-(1-(s.alpha:ℂ))*q)^4+(s.k:ℂ)*((NormalizedTanh.slopeZero s.saturation:ℝ):ℂ)^3*(s.alpha:ℂ)^4*q)/(s.alpha:ℂ)^4 := by
    field_simp
  rw [hd, div_div_eq_mul_div]
  rfl

/-- Arithmetic identity for the prepared-frequency kernel, with no executable
complex arithmetic. It uses the same unit delay as the transfer theorem. -/
theorem Snapshot.magnitudeSquared_eq_normSq (s : Snapshot ℝ) (frequency : ℝ) :
    s.magnitudeSquared (ResponseFrequency.ofCyclesPerSample frequency) =
      Complex.normSq (s.response (ResponseFrequency.unitDelay frequency)) := by
  let h := Real.sin (Real.pi*frequency)
  let c := Real.cos (Real.pi*frequency)
  have hq : ResponseFrequency.unitDelay frequency = (⟨1-2*h*h, -(2*h*c)⟩:ℂ) := by
    rw [← ResponseFrequency.ofCyclesPerSample_delay]
    rfl
  rw [hq, Snapshot.response, Complex.normSq_div]
  unfold Snapshot.magnitudeSquared
  rw [Response.ratio_eq _ _ (mul_self_nonneg _)]
  simp only [Response.normSquared, fma_eq, ResponseFrequency.ofCyclesPerSample,
    ResponseFrequency.fromHalfAngle]
  congr 1
  · simp [Complex.normSq_apply, pow_succ]
    ring
  · simp [Complex.normSq_apply, pow_succ, h, c]
    ring

theorem prepareCoefficients_state {R : Type} [OfNat R 1] [OfNat R 2] [OfScientific R]
    [Add R] [Sub R] [Mul R] [Div R] [IfLess R] [Trig R] [SquareRoot R] [Tanh R]
    (spec : LogicalSpec R) (s : ConservativeReuppingLadder R) :
    (prepareCoefficients spec s).1 = computeCoefficients spec s := rfl

theorem prepareCoefficients_snapshot {R : Type} [OfNat R 1] [OfNat R 2] [OfScientific R]
    [Add R] [Sub R] [Mul R] [Div R] [IfLess R] [Trig R] [SquareRoot R] [Tanh R]
    (spec : LogicalSpec R) (s : ConservativeReuppingLadder R) :
    (prepareCoefficients spec s).2 = snapshot (computeCoefficients spec s) := rfl

theorem prepared_magnitudeSquared_eq_transfer (spec : LogicalSpec ℝ)
    (s : ConservativeReuppingLadder ℝ) (hc : 0 < spec.cutoff) (frequency : ℝ) :
    let f := computeCoefficients spec s
    (prepareCoefficients spec s).2.magnitudeSquared (ResponseFrequency.ofCyclesPerSample frequency) =
      Complex.normSq (transfer (f.alpha:ℂ) ((NormalizedTanh.slopeZero f.saturation:ℝ):ℂ)
        (f.k:ℂ) (ResponseFrequency.unitDelay frequency)) := by
  rw [prepareCoefficients_snapshot, Snapshot.magnitudeSquared_eq_normSq, snapshot_response]
  exact ne_of_gt (ReuppingLadder.prepareTuning_properties spec.cutoff hc).1

theorem magnitudeSquared_channel {n : ℕ} (s : Snapshot (Fin n → ℝ))
    (f : ResponseFrequency (Fin n → ℝ)) (i : Fin n) :
    s.magnitudeSquared f i = (channel i s).magnitudeSquared (channel i f) := rfl

theorem snapshot_channel {n : ℕ} (s : ConservativeReuppingLadder (Fin n → ℝ)) (i : Fin n) :
    channel i (snapshot s) = snapshot (channel i s) := rfl

theorem unitDelay_eq_fourier_inv (frequency : ℝ) :
    ResponseFrequency.unitDelay frequency =
      (fourier 1 (frequency : ResponseMeasurement.Phase))⁻¹ := by
  rw [fourier_coe_apply, ← Complex.exp_neg]
  unfold ResponseFrequency.unitDelay
  congr 1
  push_cast
  ring

/-- The exact response whose power the UI emits is the time-first limit of
the actual nonlinear processor, not merely a separate tangent formula. -/
theorem prepared_ui_smallSignal (spec : LogicalSpec ℝ) (s : ConservativeReuppingLadder ℝ)
    (hc : 0 < spec.cutoff) (hr0 : 0 ≤ spec.resonance) (hr1 : spec.resonance ≤ 1)
    (hg : 0 < spec.gain) (frequency : ℝ)
    (hfrequency : star (fourier 1 (frequency : ResponseMeasurement.Phase)) *
      star (fourier 1 (frequency : ResponseMeasurement.Phase)) ≠ 1) :
    let f := computeCoefficients spec s
    ResponseMeasurement.HasSmallSignalResponse (fourier 1 (frequency : ResponseMeasurement.Phase))
      (fun epsilon n => (trajectory (reset f)
        (fun i => epsilon*((fourier 1 (frequency : ResponseMeasurement.Phase))^i).re) (n+1)).output)
      ((prepareCoefficients spec s).2.response (ResponseFrequency.unitDelay frequency)) := by
  dsimp only
  rw [prepareCoefficients_snapshot, snapshot_response _
    (ne_of_gt (ReuppingLadder.prepareTuning_properties spec.cutoff hc).1), unitDelay_eq_fourier_inv]
  exact prepared_hasSmallSignalResponse spec s hc hr0 hr1 hg _ hfrequency

end DSP.ConservativeReuppingLadder
