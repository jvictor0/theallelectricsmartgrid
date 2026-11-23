import DSP.Analysis.AttractingGraph
import DSP.Analysis.CircleLift

/-! Construct the graph theorem's phase equivalences from a real degree-one
lift. A graph is only assumed Lipschitz: we do not differentiate it. Its
transverse feedback reduces the real phase separation by at most b*L. -/

noncomputable section
namespace DSP.AttractingGraph
open Metric
open scoped NNReal
variable {F : Type*} [MetricSpace F]

structure PhaseBounds (center : F) (radius : ℝ) (L : ℝ≥0) where
  realPhase : ℝ → F → ℝ
  continuous : ContinuousOn (fun x : ℝ × F => realPhase x.1 x.2)
    {x | x.2 ∈ closedBall center radius}
  period : ∀ θ r, realPhase (θ+2*Real.pi) r = realPhase θ r+2*Real.pi
  a : ℝ
  b : ℝ≥0
  A : ℝ≥0
  positive : 0 < A
  gap : (A:ℝ)+b*L ≤ a
  lower : ∀ r ∈ closedBall center radius, ∀ θ φ, θ ≤ φ →
    a*(φ-θ) ≤ realPhase φ r-realPhase θ r
  fiber : ∀ θ r s, r ∈ closedBall center radius → s ∈ closedBall center radius →
    |realPhase θ r-realPhase θ s| ≤ b*dist r s

namespace PhaseBounds
variable {center : F} {radius : ℝ} {L : ℝ≥0} (bounds : PhaseBounds center radius L)

def phase (θ : Real.Angle) (r : F) : Real.Angle := bounds.realPhase θ.toReal r

theorem phase_fiber (θ : Real.Angle) (r s : F)
    (hr : r ∈ closedBall center radius) (hs : s ∈ closedBall center radius) :
    dist (bounds.phase θ r) (bounds.phase θ s) ≤ bounds.b*dist r s :=
  (CircleLift.angle_dist_le _ _).trans (bounds.fiber θ.toReal r s hr hs)

/-- Feedback from a Lipschitz graph costs b*L even if the graph has corners. -/
theorem graph_lower (h : Graph (B := Real.Angle) center radius L) (θ φ : ℝ) (hθφ : θ ≤ φ) :
    (bounds.A:ℝ)*(φ-θ) ≤ bounds.realPhase φ (h.val (φ:Real.Angle))-
      bounds.realPhase θ (h.val (θ:Real.Angle)) := by
  have hp := bounds.lower (h.val (θ:Real.Angle)) (h.property.2 _) θ φ hθφ
  have hf := bounds.fiber φ (h.val (φ:Real.Angle)) (h.val (θ:Real.Angle))
    (h.property.2 _) (h.property.2 _)
  have hh := h.property.1.dist_le_mul (φ:Real.Angle) (θ:Real.Angle)
  have ha : dist (φ:Real.Angle) (θ:Real.Angle) ≤ φ-θ := by
    simpa only [abs_of_nonneg (sub_nonneg.mpr hθφ)] using CircleLift.angle_dist_le φ θ
  have hd : dist (h.val (φ:Real.Angle)) (h.val (θ:Real.Angle)) ≤ (L:ℝ)*(φ-θ) :=
    hh.trans (mul_le_mul_of_nonneg_left ha L.coe_nonneg)
  have hb := mul_le_mul_of_nonneg_left hd bounds.b.coe_nonneg
  have hg := mul_le_mul_of_nonneg_right bounds.gap (sub_nonneg.mpr hθφ)
  have hm := (abs_le.mp hf).1
  nlinarith

def graphLift (h : Graph (B := Real.Angle) center radius L) : CircleLift.Increasing bounds.A where
  realMap θ := bounds.realPhase θ (h.val (θ:Real.Angle))
  continuous := bounds.continuous.comp_continuous
    (continuous_id.prodMk (h.val.continuous.comp Real.Angle.continuous_coe))
    (fun θ => h.property.2 (θ:Real.Angle))
  period θ := by
    simp only [Real.Angle.coe_add, Real.Angle.coe_two_pi, add_zero]
    exact bounds.period θ _
  positive := bounds.positive
  lower := bounds.graph_lower h

def equiv (h : Graph (B := Real.Angle) center radius L) : Real.Angle ≃ Real.Angle :=
  (bounds.graphLift h).equiv

theorem equiv_eq (h : Graph (B := Real.Angle) center radius L) (θ : Real.Angle) :
    bounds.equiv h θ = bounds.phase θ (h.val θ) := by
  change (↑(bounds.realPhase θ.toReal (h.val (θ.toReal:Real.Angle))):Real.Angle) = _
  rw [Real.Angle.coe_toReal]
  rfl

theorem separation (h : Graph (B := Real.Angle) center radius L) (θ φ : Real.Angle) :
    (bounds.A:ℝ)*dist θ φ ≤ dist (bounds.equiv h θ) (bounds.equiv h φ) :=
  (bounds.graphLift h).circleMap_separation θ φ

/-- The original graph-transform certificate can now be built without
assuming a bijection or separation along every graph. They follow from the
real phase bounds above. Transverse bounds remain explicit obligations. -/
def toCertificate (transverse : Real.Angle → F → F) (hr : 0 ≤ radius) (c d : ℝ≥0)
    (ht : ∀ θ φ r s, r ∈ closedBall center radius → s ∈ closedBall center radius →
      dist (transverse θ r) (transverse φ s) ≤ c*dist θ φ+d*dist r s)
    (hi : ∀ θ r, r ∈ closedBall center radius → transverse θ r ∈ closedBall center radius)
    (hcone : (c+d*L)/bounds.A ≤ L)
    (hsmall : d+(c+d*L)*bounds.b/bounds.A < 1) : Certificate (B := Real.Angle) center radius L where
  phase := bounds.phase
  transverse := transverse
  radius_nonneg := hr
  A := bounds.A
  b := bounds.b
  c := c
  d := d
  A_pos := bounds.positive
  phase_fiber := bounds.phase_fiber
  transverse_lip := ht
  transverse_range := hi
  phase_equiv := bounds.equiv
  phase_equiv_eq := bounds.equiv_eq
  phase_separation := bounds.separation
  cone := hcone
  small := hsmall

end PhaseBounds
end DSP.AttractingGraph
