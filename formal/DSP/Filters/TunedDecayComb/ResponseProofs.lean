import DSP.Filters.TunedDecayComb.Response
import DSP.Filters.TunedDecayComb.Realization
import DSP.Filters.TunedDecayComb.Stability
import DSP.ComplexValue.Proofs

noncomputable section
namespace DSP.TunedDecayComb
variable {capacity : ℕ}

namespace Snapshot

def map {R S : Type} (f : R → S) (s : Snapshot capacity R) : Snapshot capacity S :=
  ⟨s.first.map f, s.second.map f, s.mix.map f⟩

def response {K : Type} [Field K] (s : Snapshot capacity K) (q : K) : K :=
  match s.mix with
  | none => settledResponse s.first q
  | some m => mixedResponse s.first s.second m q

theorem lowpass_value (c : Coefficients capacity ℝ) (q : ComplexValue ℝ) :
    (lowpass c q).value = lowpassResponse (c.map Complex.ofReal) q.value := by
  cases hb : c.bypass <;>
    simp [lowpass, lowpassResponse, lowpassDenominator, Coefficients.map, hb, pow_two]

theorem loop_value (c : Coefficients capacity ℝ) (q low : ComplexValue ℝ)
    (hl : low.value = lowpassResponse (c.map Complex.ofReal) q.value) :
    (loop c q low).value = returnResponse (c.map Complex.ofReal) q.value := by
  simp only [loop, ComplexValue.value_mul, ComplexValue.value_scale,
    ComplexValue.value_div, ComplexValue.value_add, ComplexValue.value_ofReal,
    ComplexValue.value_pow, hl]
  simp only [returnResponse, NormalizedAllPass.response, Coefficients.map, Complex.ofReal_one]
  ring

theorem evaluate_value (s : Snapshot capacity ℝ) (f : ResponseFrequency ℝ) :
    (s.evaluate f).1.value / (s.evaluate f).2.value =
      (s.map Complex.ofReal).response f.delay := by
  change _ = (s.map Complex.ofReal).response f.zInverse.value
  have hl (c : Coefficients capacity ℝ) :=
    loop_value c f.zInverse (lowpass c f.zInverse) (lowpass_value c f.zInverse)
  cases hm : s.mix <;>
    simp [evaluate, response, map, hm, lowpass_value, hl, settledResponse, mixedResponse]

theorem complexResponse_eq (s : Snapshot capacity ℝ) (f : ResponseFrequency ℝ) :
    (s.complexResponse f).value = (s.map Complex.ofReal).response f.delay := by
  simp only [complexResponse, ComplexValue.value_div, evaluate_value]

theorem magnitudeSquared_eq_normSq (s : Snapshot capacity ℝ) (f : ResponseFrequency ℝ) :
    s.magnitudeSquared f = Complex.normSq ((s.map Complex.ofReal).response f.delay) := by
  rw [← evaluate_value, Complex.normSq_div]
  change Response.ratio (s.evaluate f).1.normSquared (s.evaluate f).2.normSquared = _
  rw [Response.ratio_eq _ _ (by rw [ComplexValue.normSquared_value]; exact Complex.normSq_nonneg _)]
  simp only [ComplexValue.normSquared_value]

theorem frequency_response (s : Snapshot capacity ℝ) (f : ℝ) :
    s.magnitudeSquared (ResponseFrequency.ofCyclesPerSample f) =
      Complex.normSq ((s.map Complex.ofReal).response (ResponseFrequency.unitDelay f)) := by
  rw [magnitudeSquared_eq_normSq, ResponseFrequency.ofCyclesPerSample_delay]

/-- The captured response is realized by arbitrary input streams from rest
when its controls are held fixed. It is not a transfer for an entire retune. -/
theorem realizes {K : Type} [Field K] (s : Snapshot capacity K)
    (hc₀ : Prepared s.first) (hc₁ : Prepared s.second)
    (state : State capacity K) (h0 : state.AtRest) :
    Bilinear.Realizes (step s.first s.second s.mix) state (·.y)
      ((s.map Bilinear.const).response Bilinear.delay) := by
  cases hm : s.mix with
  | none => simpa [map, response, hm] using realizes_settled s.first s.second hc₀ state h0
  | some m => simpa [map, response, hm] using realizes_mixed s.first s.second m hc₀ hc₁ state h0

end Snapshot

theorem Controller.snapshot_frozen_step {R : Type} [OfNat R 0] [OfNat R 2]
    [Add R] [Sub R] [Mul R] (c : Controller capacity R) (x : Sample R) :
    step c.snapshot.first c.snapshot.second c.snapshot.mix c.state x =
      step c.current c.next (if c.active then some c.mix else none) c.state x := rfl

/-- Capturing a response is independent of audio history and of queued targets.
Queued targets become audible only after the controller begins their fade. -/
theorem Controller.snapshot_request {R : Type} (c : Controller capacity R)
    (desired : Coefficients capacity R) : (c.request desired).snapshot = c.snapshot := rfl

end DSP.TunedDecayComb
