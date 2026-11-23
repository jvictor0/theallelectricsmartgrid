import Mathlib.Topology.MetricSpace.Contracting
import Mathlib.Analysis.Calculus.MeanValue

/-! A local, a-posteriori existence AND attraction criterion for a block map.
The residual and derivative bounds must hold on the entire stated ball. A
pointwise eigenvalue calculation or a finite grid check is not such a bound.
No assumption about uniqueness of attractors outside this ball is made. -/

noncomputable section
namespace DSP.AttractingBlock
open Filter Metric
open scoped Topology NNReal

variable {E : Type*} [MetricSpace E]

/-- A residual plus uniform contraction makes the candidate ball invariant. -/
theorem mapsTo_ball (f : E → E) (center : E) (radius : ℝ) (K : ℝ≥0)
    (hr : 0 ≤ radius) (hl : LipschitzOnWith K f (closedBall center radius))
    (he : dist (f center) center + K*radius ≤ radius) :
    Set.MapsTo f (closedBall center radius) (closedBall center radius) := by
  intro x hx
  have hc : center ∈ closedBall center radius := mem_closedBall_self hr
  calc
    dist (f x) center ≤ dist (f x) (f center) + dist (f center) center := dist_triangle _ _ _
    _ ≤ K * dist x center + dist (f center) center := by
      gcongr; exact hl.dist_le_mul x hx center hc
    _ ≤ K * radius + dist (f center) center := by gcongr; exact hx
    _ ≤ radius := by linarith

/-- The fixed point exists, is unique IN THE BALL, attracts every initial
point in that ball, and has an a-posteriori distance bound from the candidate.
Here `f` may be the actual composition of many sample steps. -/
theorem exists_attracting_fixedPoint [CompleteSpace E] (f : E → E) (center : E) (radius : ℝ) (K : ℝ≥0)
    (hr : 0 ≤ radius) (hk : K < 1)
    (hl : LipschitzOnWith K f (closedBall center radius))
    (he : dist (f center) center + K*radius ≤ radius) :
    ∃ p ∈ closedBall center radius, f p = p ∧
      (∀ x ∈ closedBall center radius, f x = x → x = p) ∧
      (∀ x ∈ closedBall center radius,
        Tendsto (fun n => f^[n] x) atTop (𝓝 p)) ∧
      dist center p ≤ dist center (f center)/(1-K) := by
  have hm := mapsTo_ball f center radius K hr hl he
  let B := closedBall center radius
  have hc : center ∈ B := mem_closedBall_self hr
  letI : Nonempty B := ⟨⟨center, hc⟩⟩
  letI : CompleteSpace B := isClosed_closedBall.completeSpace_coe
  let g : B → B := fun x => ⟨f x, hm x.property⟩
  have hg : ContractingWith K g := ⟨hk, LipschitzWith.of_dist_le_mul
    (fun x y => hl.dist_le_mul x x.property y y.property)⟩
  let p := hg.fixedPoint g
  have hp : g p = p := hg.fixedPoint_isFixedPt
  have hi (x : B) (n : ℕ) : ((g^[n] x : B) : E) = f^[n] x := by
    induction n with
    | zero => rfl
    | succ n ih => simp only [Function.iterate_succ_apply', g, ih]
  refine ⟨p, p.property, congrArg Subtype.val hp, ?_, ?_, ?_⟩
  · intro x hx hfx
    have hfix : g ⟨x,hx⟩ = ⟨x,hx⟩ := Subtype.ext hfx
    exact congrArg Subtype.val (hg.fixedPoint_unique hfix)
  · intro x hx
    have h := (continuous_subtype_val.tendsto p).comp (hg.tendsto_iterate_fixedPoint ⟨x,hx⟩)
    simpa only [Function.comp_def, hi] using h
  · exact hg.dist_fixedPoint_le ⟨center,hc⟩

/-- Explicit geometric attraction rate, measured once per block. -/
theorem iterate_error (f : E → E) (center : E) (radius : ℝ) (K : ℝ≥0)
    (hr : 0 ≤ radius) (hl : LipschitzOnWith K f (closedBall center radius))
    (he : dist (f center) center + K*radius ≤ radius)
    (p x : E) (hp : p ∈ closedBall center radius) (hfix : f p = p)
    (hx : x ∈ closedBall center radius) (n : ℕ) :
    dist (f^[n] x) p ≤ (K:ℝ)^n * dist x p := by
  have hm := mapsTo_ball f center radius K hr hl he
  induction n with
  | zero => simp
  | succ n ih =>
    rw [Function.iterate_succ_apply']
    calc
      dist (f (f^[n] x)) p = dist (f (f^[n] x)) (f p) := by rw [hfix]
      _ ≤ K * dist (f^[n] x) p := hl.dist_le_mul _ (hm.iterate n hx) _ hp
      _ ≤ K * ((K:ℝ)^n * dist x p) := mul_le_mul_of_nonneg_left ih K.coe_nonneg
      _ = _ := by rw [pow_succ]; ring

/-- A contraction checked in convenient coordinates gives a fixed point and
attraction for the original block map. The intertwining identity must hold
exactly; a numerical change-of-basis fit alone does not supply it. -/
theorem exists_conjugate [CompleteSpace E] {F : Type*} [MetricSpace F]
    (g : E → E) (f : F → F) (embed : E → F) (hc : Continuous embed)
    (hcomm : ∀ x, embed (g x) = f (embed x))
    (center : E) (radius : ℝ) (K : ℝ≥0) (hr : 0 ≤ radius) (hk : K < 1)
    (hl : LipschitzOnWith K g (closedBall center radius))
    (he : dist (g center) center+K*radius ≤ radius) :
    ∃ p ∈ closedBall center radius, f (embed p) = embed p ∧
      (∀ x ∈ closedBall center radius, Tendsto (fun n => f^[n] (embed x)) atTop (𝓝 (embed p))) ∧
      dist center p ≤ dist center (g center)/(1-K) := by
  obtain ⟨p, hp, hfix, _, ht, herr⟩ := exists_attracting_fixedPoint g center radius K hr hk hl he
  have hi (x : E) (n : ℕ) : embed (g^[n] x) = f^[n] (embed x) := by
    induction n with
    | zero => rfl
    | succ n ih => rw [Function.iterate_succ_apply', hcomm, ih, Function.iterate_succ_apply']
  refine ⟨p, hp, ?_, ?_, herr⟩
  · rw [← hcomm, hfix]
  · intro x hx
    simpa only [Function.comp_def, hi] using (hc.tendsto p).comp (ht x hx)

/-- A derivative enclosure on a convex ball discharges the Lipschitz part
of the criterion. This is a uniform enclosure, not a derivative at the center. -/
theorem lipschitz_of_derivative {F : Type*} [NormedAddCommGroup F] [NormedSpace ℝ F]
    (f : F → F) (df : F → F →L[ℝ] F) (center : F) (radius : ℝ) (K : ℝ≥0)
    (hd : ∀ x ∈ closedBall center radius, HasFDerivAt f (df x) x)
    (hb : ∀ x ∈ closedBall center radius, ‖df x‖₊ ≤ K) :
    LipschitzOnWith K f (closedBall center radius) :=
  (convex_closedBall center radius).lipschitzOnWith_of_nnnorm_hasFDerivWithin_le
    (fun x hx => (hd x hx).hasFDerivWithinAt) hb

end DSP.AttractingBlock
