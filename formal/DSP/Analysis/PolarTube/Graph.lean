import DSP.Analysis.PolarTube.Lift
import DSP.Analysis.AttractingGraph.Circle

/-! Connect the actual coordinate map to the graph-transform phase theorem.
The quantitative bounds are still inputs: this does not certify a numerical
proposal. Alignment supplies the continuous degree-one lift; the slope and
fiber bounds supply bijectivity along every admissible graph. -/

noncomputable section
namespace DSP.PolarTube.Chart
open Metric
open scoped NNReal

def phaseBounds (chart : Chart) (f : State → State) (radius : ℝ) (L : ℝ≥0)
    (hp : Continuous chart.profile) (hf : Continuous f)
    (ha : ∀ θ r, r ∈ closedBall (0:Normal) radius →
      0 < (chart.nextHead f θ r).re*θ.cos+(chart.nextHead f θ r).im*θ.sin)
    (a : ℝ) (b A : ℝ≥0) (hA : 0 < A) (hgap : (A:ℝ)+b*L ≤ a)
    (hlower : ∀ r ∈ closedBall (0:Normal) radius, ∀ θ φ, θ ≤ φ →
      a*(φ-θ) ≤ chart.phaseLift f φ r-chart.phaseLift f θ r)
    (hfiber : ∀ θ r s, r ∈ closedBall (0:Normal) radius → s ∈ closedBall (0:Normal) radius →
      |chart.phaseLift f θ r-chart.phaseLift f θ s| ≤ b*dist r s) :
    AttractingGraph.PhaseBounds (0:Normal) radius L where
  realPhase := chart.phaseLift f
  continuous := chart.phaseLift_continuousOn f hp hf _ (fun _ hx => ha _ _ hx)
  period := chart.phaseLift_period f
  a := a
  b := b
  A := A
  positive := hA
  gap := hgap
  lower := hlower
  fiber := hfiber

theorem phase_eq_of_lift (chart : Chart) (f : State → State) {radius : ℝ} {L : ℝ≥0}
    (bounds : AttractingGraph.PhaseBounds (0:Normal) radius L)
    (hlift : bounds.realPhase = chart.phaseLift f)
    (ha : ∀ θ r, r ∈ closedBall (0:Normal) radius →
      0 < (chart.nextHead f θ r).re*θ.cos+(chart.nextHead f θ r).im*θ.sin)
    (θ : Real.Angle) (r : Normal) (hr : r ∈ closedBall (0:Normal) radius) :
    bounds.phase θ r = (chart.coordinateStep f (θ,r)).1 := by
  have hne : chart.nextHead f θ r ≠ 0 := by
    intro hz
    have h := ha θ r hr
    simp only [hz, Complex.zero_re, Complex.zero_im, zero_mul, zero_add] at h
    exact (lt_irrefl 0) h
  change (bounds.realPhase θ.toReal r:Real.Angle) = _
  rw [hlift]
  simpa only [Real.Angle.coe_toReal] using chart.phaseLift_agrees f θ.toReal r
    (by simpa only [Real.Angle.coe_toReal] using hne)

/-- A graph certificate proved for these coordinates describes iterations of
the original state map, not merely an unrelated map with similar bounds. -/
theorem certificate_iterate_commutes (chart : Chart) (f : State → State)
    {radius : ℝ} {L : ℝ≥0} (cert : AttractingGraph.Certificate (B := Real.Angle) (0:Normal) radius L)
    (heq : ∀ x : Coordinates, x.2 ∈ closedBall (0:Normal) radius →
      cert.step x = chart.coordinateStep f x)
    (x : Coordinates) (hx : x.2 ∈ closedBall (0:Normal) radius) (n : ℕ) :
    chart.decode (cert.step^[n] x) = f^[n] (chart.decode x) := by
  have hmem : ∀ k, (cert.step^[k] x).2 ∈ closedBall (0:Normal) radius := by
    intro k
    induction k with
    | zero => exact hx
    | succ k ih =>
      simpa only [Function.iterate_succ_apply', AttractingGraph.Certificate.step] using
        cert.transverse_range (cert.step^[k] x).1 (cert.step^[k] x).2 ih
  induction n with
  | zero => rfl
  | succ n ih =>
    rw [Function.iterate_succ_apply', heq _ (hmem n), chart.coordinateStep_commutes,
      ih, Function.iterate_succ_apply']

end DSP.PolarTube.Chart
