import DSP.Analysis.AttractingGraph
import DSP.Analysis.Coboundary

/-! Discrete measurement certificates on a trapped phase/transverse tube.
The correction uses the actual phase update, including transverse feedback.
These bounds do not replace the map by a continuous averaged flow. -/

noncomputable section
namespace DSP.AttractingGraph.Certificate
open Metric
open scoped NNReal
variable {B F E : Type*} [MetricSpace B] [CompactSpace B] [Nonempty B]
  [MetricSpace F] [CompleteSpace F] [NormedAddCommGroup E] [NormedSpace ℝ E]
variable {center : F} {radius : ℝ} {L : ℝ≥0}
variable (cert : Certificate (B := B) center radius L)

omit [CompactSpace B] [Nonempty B] [CompleteSpace F] [NormedSpace ℝ E] in
/-- Lift a correction fitted on the center of the tube to its whole width.
The term involving b accounts for the transverse change in NEXT phase. -/
theorem tube_residual (observe : B → F → E) (V : B → E) (mean : E)
    (phaseL observationL : ℝ≥0) (hV : LipschitzWith phaseL V)
    (ho : ∀ θ r, r ∈ closedBall center radius →
      ‖observe θ r-observe θ center‖ ≤ observationL*dist r center)
    (error : ℝ) (he : ∀ θ,
      ‖observe θ center-mean-(V θ-V (cert.phase θ center))‖ ≤ error)
    (θ : B) (r : F) (hr : r ∈ closedBall center radius) :
    ‖observe θ r-mean-(V θ-V (cert.phase θ r))‖ ≤
      error+(observationL+phaseL*cert.b)*radius := by
  have hf := cert.phase_fiber θ r center hr (mem_closedBall_self cert.radius_nonneg)
  have hv : ‖V (cert.phase θ r)-V (cert.phase θ center)‖ ≤ phaseL*(cert.b*radius) := by
    rw [← dist_eq_norm]
    exact (hV.dist_le_mul _ _).trans (mul_le_mul_of_nonneg_left
      (hf.trans (mul_le_mul_of_nonneg_left hr cert.b.coe_nonneg)) phaseL.coe_nonneg)
  have hob : ‖observe θ r-observe θ center‖ ≤ observationL*radius :=
    (ho θ r hr).trans (mul_le_mul_of_nonneg_left hr observationL.coe_nonneg)
  have hid : observe θ r-mean-(V θ-V (cert.phase θ r)) =
      (observe θ center-mean-(V θ-V (cert.phase θ center)))+
      (observe θ r-observe θ center)+(V (cert.phase θ r)-V (cert.phase θ center)) := by abel
  rw [hid]
  have ht := (norm_add_le _ _).trans (add_le_add ((norm_add_le _ _).trans (add_le_add (he θ) hob)) hv)
  simpa only [NNReal.coe_add, NNReal.coe_mul, add_mul, mul_assoc, add_assoc] using ht

omit [CompactSpace B] [Nonempty B] [CompleteSpace F] in
/-- Once the tube checks hold, every trajectory in it has this finite-window
measurement bound. The observable may be a complex phasor or total power. -/
theorem average_bound (observe : B → F → E) (V : B → E) (mean : E)
    (phaseL observationL : ℝ≥0) (hV : LipschitzWith phaseL V)
    (ho : ∀ θ r, r ∈ closedBall center radius →
      ‖observe θ r-observe θ center‖ ≤ observationL*dist r center)
    (error M : ℝ) (he : ∀ θ,
      ‖observe θ center-mean-(V θ-V (cert.phase θ center))‖ ≤ error)
    (hp : ∀ θ, ‖V θ‖ ≤ M) (x : B × F) (hx : x.2 ∈ closedBall center radius)
    (N : ℕ) (hN : 0 < N) :
    ‖DSP.Coboundary.average (fun n => observe (cert.step^[n] x).1 (cert.step^[n] x).2) N-mean‖ ≤
      error+(observationL+phaseL*cert.b)*radius+2*M/(N:ℝ) := by
  exact DSP.Coboundary.orbit_bound cert.step {x | x.2 ∈ closedBall center radius}
    (fun x hx => cert.transverse_range x.1 x.2 hx)
    (fun x => observe x.1 x.2) (fun x => V x.1) mean
    (error+(observationL+phaseL*cert.b)*radius) M
    (fun x hx => cert.tube_residual observe V mean phaseL observationL hV ho error he x.1 x.2 hx)
    (fun x _ => hp x.1) x hx N hN

end DSP.AttractingGraph.Certificate
