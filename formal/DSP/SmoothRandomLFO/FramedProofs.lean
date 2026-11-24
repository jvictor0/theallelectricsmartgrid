import DSP.SmoothRandomLFO.Proofs
import DSP.WanderingRandomLFO
import Mathlib.Analysis.Calculus.Deriv.MeanValue
import Mathlib.Analysis.Complex.ExponentialBounds
import Mathlib.MeasureTheory.Measure.MeasureSpace
import Mathlib.Tactic.Linarith
import Mathlib.Tactic.NormNum

namespace DSP.SmoothRandomLFO

open Set MeasureTheory
open DSP.WanderingRandomLFO (frameSamples)
open scoped BigOperators ENNReal

noncomputable section

/-- Secant slope and the exact-arithmetic interpretation of the framed LFO. -/
def frameSlope (f : ℝ → ℝ) (a b : ℝ) : ℝ := (f b - f a) / (b - a)

def frameValue (f : ℝ → ℝ) (a b t : ℝ) : ℝ := f a + (t - a) * frameSlope f a b

theorem frameValue_hasDerivAt (f : ℝ → ℝ) (a b t : ℝ) :
    HasDerivAt (frameValue f a b) (frameSlope f a b) t := by
  have slope := ((hasDerivAt_id t).sub_const a).mul_const (frameSlope f a b)
  change HasDerivAt (fun x => f a + (x - a) * frameSlope f a b) (frameSlope f a b) t
  simpa only [id_eq, one_mul] using slope.const_add (f a)

private theorem exists_velocity_eq_frameSlope {f v : ℝ → ℝ} {a b : ℝ}
    (hab : a < b) (hv : ∀ x ∈ Icc a b, HasDerivAt f (v x) x) :
    ∃ x ∈ Ioo a b, v x = frameSlope f a b :=
  exists_hasDerivAt_eq_slope f v hab
    (fun x hx => (hv x hx).continuousAt.continuousWithinAt)
    (fun x hx => hv x ⟨hx.1.le, hx.2.le⟩)

theorem abs_frameSlope_le {f v : ℝ → ℝ} {a b M : ℝ}
    (hab : a < b) (hv : ∀ x ∈ Icc a b, HasDerivAt f (v x) x)
    (bound : ∀ x ∈ Icc a b, |v x| ≤ M) : |frameSlope f a b| ≤ M := by
  obtain ⟨x, hx, slope⟩ := exists_velocity_eq_frameSlope hab hv
  rw [← slope]
  exact bound x ⟨hx.1.le, hx.2.le⟩

private theorem velocity_distance {v accel : ℝ → ℝ} {a b M x y : ℝ}
    (ha : ∀ t ∈ Icc a b, HasDerivAt v (accel t) t)
    (bound : ∀ t ∈ Icc a b, |accel t| ≤ M) (hM : 0 ≤ M)
    (hx : x ∈ Icc a b) (hy : y ∈ Icc a b) :
    |v y - v x| ≤ M * (b - a) := by
  have hlip := (convex_Icc a b).norm_image_sub_le_of_norm_hasDerivWithin_le
    (fun t ht => (ha t ht).hasDerivWithinAt)
    (fun t ht => by simpa [Real.norm_eq_abs] using bound t ht) hx hy
  rw [Real.norm_eq_abs, Real.norm_eq_abs] at hlip
  apply hlip.trans (mul_le_mul_of_nonneg_left ?_ hM)
  exact abs_le.mpr ⟨by linarith [hx.2, hy.1], by linarith [hy.2, hx.1]⟩

/-- The reconstructed slope differs from the spline velocity by at most the
acceleration bound times the frame length. It also bounds the endpoint
errors of the one-sided slopes. -/
theorem frameSlope_error_le {f v accel : ℝ → ℝ} {a b M t : ℝ}
    (hab : a < b) (ht : t ∈ Icc a b)
    (hv : ∀ x ∈ Icc a b, HasDerivAt f (v x) x)
    (ha : ∀ x ∈ Icc a b, HasDerivAt v (accel x) x)
    (bound : ∀ x ∈ Icc a b, |accel x| ≤ M) (hM : 0 ≤ M) :
    |frameSlope f a b - v t| ≤ M * (b - a) := by
  obtain ⟨x, hx, slope⟩ := exists_velocity_eq_frameSlope hab hv
  rw [← slope]
  exact velocity_distance ha bound hM ht ⟨hx.1.le, hx.2.le⟩

/-- Interpolation value error on one frame, including frames that contain a
smooth spline knot. -/
theorem frameValue_error_le {f v accel : ℝ → ℝ} {a b M t : ℝ}
    (hab : a < b) (ht : t ∈ Icc a b)
    (hv : ∀ x ∈ Icc a b, HasDerivAt f (v x) x)
    (ha : ∀ x ∈ Icc a b, HasDerivAt v (accel x) x)
    (bound : ∀ x ∈ Icc a b, |accel x| ≤ M) (hM : 0 ≤ M) :
    |frameValue f a b t - f t| ≤ M * (b - a) ^ 2 / 4 := by
  rcases ht.1.eq_or_lt with h | hat
  · subst t
    simp only [frameValue, sub_self, zero_mul, add_zero, abs_zero]
    positivity
  rcases ht.2.eq_or_lt with h | htb
  · subst t
    have hEq : frameValue f a b b = f b := by
      unfold frameValue frameSlope
      field_simp [ne_of_gt (sub_pos.mpr hab)]
      ring
    rw [hEq, sub_self, abs_zero]
    positivity
  obtain ⟨x, hx, hleft⟩ := exists_velocity_eq_frameSlope hat
    (fun u hu => hv u ⟨hu.1, hu.2.trans ht.2⟩)
  obtain ⟨y, hy, hright⟩ := exists_velocity_eq_frameSlope htb
    (fun u hu => hv u ⟨ht.1.trans hu.1, hu.2⟩)
  have hs : |v y - v x| ≤ M * (b - a) :=
    velocity_distance ha bound hM ⟨hx.1.le, hx.2.le.trans ht.2⟩ ⟨ht.1.trans hy.1.le, hy.2.le⟩
  have identity : frameValue f a b t - f t =
      ((t - a) * (b - t) / (b - a)) * (frameSlope f t b - frameSlope f a t) := by
    unfold frameValue frameSlope
    field_simp [ne_of_gt (sub_pos.mpr hab), ne_of_gt (sub_pos.mpr hat), ne_of_gt (sub_pos.mpr htb)]
    ring
  have coeff : 0 ≤ (t - a) * (b - t) / (b - a) :=
    div_nonneg (mul_nonneg (sub_nonneg.mpr ht.1) (sub_nonneg.mpr ht.2)) (sub_nonneg.mpr hab.le)
  rw [identity, abs_mul, abs_of_nonneg coeff, ← hright, ← hleft]
  calc
    _ ≤ (t - a) * (b - t) / (b - a) * (M * (b - a)) :=
      mul_le_mul_of_nonneg_left hs coeff
    _ = M * ((t - a) * (b - t)) := by field_simp [ne_of_gt (sub_pos.mpr hab)]
    _ ≤ M * ((b - a) ^ 2 / 4) :=
      mul_le_mul_of_nonneg_left (by nlinarith [sq_nonneg (t - a - (b - t))]) hM
    _ = _ := by ring

/-- The velocity jump at a frame boundary is bounded by the sum of the
two endpoint slope errors. -/
theorem frameSlope_jump_le {f v accel : ℝ → ℝ} {a b c M : ℝ}
    (hab : a < b) (hbc : b < c)
    (hv : ∀ x ∈ Icc a c, HasDerivAt f (v x) x)
    (ha : ∀ x ∈ Icc a c, HasDerivAt v (accel x) x)
    (bound : ∀ x ∈ Icc a c, |accel x| ≤ M) (hM : 0 ≤ M) :
    |frameSlope f b c - frameSlope f a b| ≤ M * (c - a) := by
  have left : Icc a b ⊆ Icc a c := Icc_subset_Icc le_rfl hbc.le
  have right : Icc b c ⊆ Icc a c := Icc_subset_Icc hab.le le_rfl
  have hl := frameSlope_error_le hab ⟨hab.le, le_rfl⟩
    (fun x hx => hv x (left hx)) (fun x hx => ha x (left hx)) (fun x hx => bound x (left hx)) hM
  have hr := frameSlope_error_le hbc ⟨le_rfl, hbc.le⟩
    (fun x hx => hv x (right hx)) (fun x hx => ha x (right hx)) (fun x hx => bound x (right hx)) hM
  calc
    _ = |(frameSlope f b c - v b) + (v b - frameSlope f a b)| := by congr 1; ring
    _ ≤ |frameSlope f b c - v b| + |v b - frameSlope f a b| := abs_add_le _ _
    _ ≤ M * (c - b) + M * (b - a) := by rw [abs_sub_comm (v b)]; exact add_le_add hr hl
    _ = _ := by ring

/-- Bounds on the control points and the five knot spans supporting a segment. -/
structure SegmentBounded (s : Segment ℝ) (L : ℝ) : Prop where
  p0 : |s.p0| ≤ 1
  p1 : |s.p1| ≤ 1
  p2 : |s.p2| ≤ 1
  p3 : |s.p3| ≤ 1
  beforePrevious : L ≤ s.beforePrevious
  previous : L ≤ s.previous
  width : L ≤ s.width
  next : L ≤ s.next
  afterNext : L ≤ s.afterNext

private theorem abs_slope_le {x y a b c L : ℝ} (hL : 0 < L)
    (hx : |x| ≤ 1) (hy : |y| ≤ 1) (ha : L ≤ a) (hb : L ≤ b) (hc : L ≤ c) :
    |(x - y) / (a + b + c)| ≤ 2 / (3 * L) := by
  rw [abs_div, abs_of_pos (by linarith : 0 < a + b + c)]
  apply div_le_div_of_nonneg_right (show |x - y| ≤ 2 from (abs_sub x y).trans (by linarith))
    (by linarith : 0 ≤ a + b + c) |>.trans
  exact div_le_div_of_nonneg_left (by norm_num) (by positivity) (by linarith)

/-- The derivative control values are bounded by twice the inverse of the
minimum span. The quadratic spline blends them with nonnegative weights. -/
theorem abs_spline_velocity_le {s : Segment ℝ} {L u : ℝ} (hL : 0 < L)
    (span : SegmentBounded s L) (hu : u ∈ Icc (0 : ℝ) 1) :
    |velocity (prepare s) s.width u| ≤ 2 / L := by
  have hw := hL.trans_le span.width
  have hp := hL.trans_le span.previous
  have hn := hL.trans_le span.next
  let l := s.width / (s.previous + s.width) * (1 - u) ^ 2
  let r := s.width / (s.width + s.next) * u ^ 2
  have hl : 0 ≤ l := by dsimp [l]; positivity
  have hr : 0 ≤ r := by dsimp [r]; positivity
  have hl' : l ≤ (1 - u) ^ 2 := by
    simpa [l] using mul_le_mul_of_nonneg_right
      ((div_le_one (by positivity : 0 < s.previous + s.width)).mpr (by linarith))
      (sq_nonneg (1 - u))
  have hr' : r ≤ u ^ 2 := by
    simpa [r] using mul_le_mul_of_nonneg_right
      ((div_le_one (by positivity : 0 < s.width + s.next)).mpr (by linarith))
      (sq_nonneg u)
  have hm : 0 ≤ 1 - l - r := by
    nlinarith [mul_nonneg hu.1 (sub_nonneg.mpr hu.2)]
  have identity : velocity (prepare s) s.width u =
      3 * (l * s.leftSlope + (1 - l - r) * s.slope + r * s.rightSlope) := by
    unfold velocity prepare l r Segment.leftCurve Segment.rightCurve
    dsimp
    field_simp [hw.ne', (show 0 < s.previous + s.width by positivity).ne',
      (show 0 < s.width + s.next by positivity).ne']
    ring
  have left := abs_slope_le hL span.p1 span.p0 span.beforePrevious span.previous span.width
  have mid := abs_slope_le hL span.p2 span.p1 span.previous span.width span.next
  have right := abs_slope_le hL span.p3 span.p2 span.width span.next span.afterNext
  rw [identity, abs_mul, abs_of_nonneg (by norm_num : (0 : ℝ) ≤ 3)]
  calc
    _ ≤ 3 * (|l * s.leftSlope| + |(1 - l - r) * s.slope| + |r * s.rightSlope|) :=
      mul_le_mul_of_nonneg_left ((abs_add_le _ _).trans (add_le_add_left (abs_add_le _ _) _))
        (by norm_num)
    _ ≤ 3 * (l * (2 / (3 * L)) + (1 - l - r) * (2 / (3 * L)) + r * (2 / (3 * L))) := by
      rw [abs_mul l, abs_mul (1 - l - r), abs_mul r,
        abs_of_nonneg hl, abs_of_nonneg hm, abs_of_nonneg hr]
      exact mul_le_mul_of_nonneg_left (add_le_add
        (add_le_add (mul_le_mul_of_nonneg_left left hl) (mul_le_mul_of_nonneg_left mid hm))
        (mul_le_mul_of_nonneg_left right hr)) (by norm_num)
    _ = _ := by field_simp; ring

theorem abs_join_velocity_le {s : Segment ℝ} {point span L t : ℝ} (hL : 0 < L)
    (bounds : SegmentBounded s L) (hpoint : |point| ≤ 1) (hspan : L ≤ span)
    (ht : t ∈ Icc (-s.width) s.next) :
    |joinVelocity s point span t| ≤ 2 / L := by
  by_cases left : t < 0
  · simp [joinVelocity, join, left]
    apply abs_spline_velocity_le hL bounds
    have hw := hL.trans_le bounds.width
    exact ⟨div_nonneg (by linarith [ht.1]) hw.le, (div_le_iff₀ hw).mpr (by linarith)⟩
  · simp [joinVelocity, join, left]
    have nextBounds : SegmentBounded (s.advance point span) L :=
      ⟨bounds.p1, bounds.p2, bounds.p3, hpoint, bounds.previous, bounds.width,
        bounds.next, bounds.afterNext, hspan⟩
    have hn := hL.trans_le bounds.next
    exact abs_spline_velocity_le hL nextBounds
      ⟨div_nonneg (le_of_not_gt left) hn.le, (div_le_iff₀ hn).mpr (by linarith [ht.2])⟩

theorem abs_join_frameSlope_le {s : Segment ℝ} {point span L a b : ℝ}
    (hL : 0 < L) (bounds : SegmentBounded s L) (hpoint : |point| ≤ 1) (hspan : L ≤ span)
    (hab : a < b) (within : Icc a b ⊆ Icc (-s.width) s.next) :
    |frameSlope (joinValue s point span) a b| ≤ 2 / L := by
  obtain ⟨hv, _⟩ := join_hasDerivs s point span (hL.trans_le bounds.previous)
    (hL.trans_le bounds.width) (hL.trans_le bounds.next)
  exact abs_frameSlope_le hab (fun t _ => hv t)
    (fun t ht => abs_join_velocity_le hL bounds hpoint hspan (within ht))

/-- At any phase of a prepared cubic, normalized acceleration is bounded by
four divided by the squared minimum knot span. -/
theorem abs_spline_acceleration_le {s : Segment ℝ} {L u : ℝ} (hL : 0 < L)
    (span : SegmentBounded s L) (hu : u ∈ Icc (0 : ℝ) 1) :
    |acceleration (prepare s) s.width u| ≤ 4 / L ^ 2 := by
  have hw := hL.trans_le span.width
  have hleft := abs_slope_le hL span.p1 span.p0 span.beforePrevious span.previous span.width
  have hmid := abs_slope_le hL span.p2 span.p1 span.previous span.width span.next
  have hright := abs_slope_le hL span.p3 span.p2 span.width span.next span.afterNext
  have curve {a b x y : ℝ} (ha : |a| ≤ 2 / (3 * L)) (hb : |b| ≤ 2 / (3 * L))
      (hx : L ≤ x) (hy : L ≤ y) : |(a - b) / (x + y)| ≤ 2 / (3 * L ^ 2) := by
    rw [abs_div, abs_of_pos (by linarith : 0 < x + y)]
    calc
      _ ≤ (2 / (3 * L) + 2 / (3 * L)) / (x + y) :=
        div_le_div_of_nonneg_right ((abs_sub a b).trans (add_le_add ha hb)) (by linarith)
      _ ≤ (2 / (3 * L) + 2 / (3 * L)) / (2 * L) :=
        div_le_div_of_nonneg_left (by positivity) (by positivity) (by linarith)
      _ = _ := by field_simp [hL.ne']; ring
  have hl := curve hmid hleft span.previous span.width
  have hr := curve hright hmid span.width span.next
  have identity : acceleration (prepare s) s.width u =
      6 * ((1 - u) * s.leftCurve + u * s.rightCurve) := by
    unfold acceleration prepare
    dsimp
    field_simp [hw.ne']
    ring
  rw [identity, abs_mul, abs_of_pos (by norm_num : (0 : ℝ) < 6)]
  calc
    _ ≤ 6 * (|(1 - u) * s.leftCurve| + |u * s.rightCurve|) :=
      mul_le_mul_of_nonneg_left (abs_add_le _ _) (by norm_num)
    _ ≤ 6 * ((1 - u) * (2 / (3 * L ^ 2)) + u * (2 / (3 * L ^ 2))) := by
      rw [abs_mul, abs_mul, abs_of_nonneg (sub_nonneg.mpr hu.2), abs_of_nonneg hu.1]
      apply mul_le_mul_of_nonneg_left _ (by norm_num)
      exact add_le_add (mul_le_mul_of_nonneg_left hl (sub_nonneg.mpr hu.2))
        (mul_le_mul_of_nonneg_left hr hu.1)
    _ = _ := by field_simp [hL.ne']; ring

theorem abs_join_acceleration_le {s : Segment ℝ} {point span L t : ℝ} (hL : 0 < L)
    (bounds : SegmentBounded s L) (hpoint : |point| ≤ 1) (hspan : L ≤ span)
    (ht : t ∈ Icc (-s.width) s.next) :
    |joinAcceleration s point span t| ≤ 4 / L ^ 2 := by
  by_cases left : t < 0
  · simp [joinAcceleration, join, left]
    apply abs_spline_acceleration_le hL bounds
    have hw := hL.trans_le bounds.width
    exact ⟨div_nonneg (by linarith [ht.1]) hw.le,
      (div_le_iff₀ hw).mpr (by linarith)⟩
  · simp [joinAcceleration, join, left]
    have nextBounds : SegmentBounded (s.advance point span) L :=
      ⟨bounds.p1, bounds.p2, bounds.p3, hpoint, bounds.previous, bounds.width,
        bounds.next, bounds.afterNext, hspan⟩
    have hn := hL.trans_le bounds.next
    exact abs_spline_acceleration_le hL nextBounds
      ⟨div_nonneg (le_of_not_gt left) hn.le, (div_le_iff₀ hn).mpr (by linarith [ht.2])⟩

/-- Two adjacent 32-sample frames of the 128-sample delay modulation.
The bounds are in delay samples, delay samples per engine sample, and delay
samples per engine sample squared. The interval can straddle a spline knot. -/
theorem reverb_frame_bounds {s : Segment ℝ} {point span start : ℝ}
    (bounds : SegmentBounded s 50000) (hpoint : |point| ≤ 1) (hspan : 50000 ≤ span)
    (within : Icc start (start + 2 * (frameSamples : ℝ)) ⊆ Icc (-s.width) s.next) :
    let f := joinValue s point span
    let v := joinVelocity s point span
    let a := joinAcceleration s point span
    let middle := start + (frameSamples : ℝ)
    let finish := start + 2 * (frameSamples : ℝ)
    (∀ t ∈ Icc start middle,
      |128 * (frameValue f start middle t - f t)| ≤ 0.0000524288 ∧
      |128 * (frameSlope f start middle - v t)| ≤ 0.0000065536) ∧
    |128 * (frameSlope f middle finish - frameSlope f start middle)| ≤ 0.0000131072 ∧
    (∀ t ∈ Icc start finish, |128 * a t| ≤ 0.0000002048) := by
  dsimp only
  obtain ⟨hv, ha⟩ := join_hasDerivs s point span (by linarith [bounds.previous])
    (by linarith [bounds.width]) (by linarith [bounds.next])
  let M : ℝ := 4 / (50000 : ℝ) ^ 2
  have hM : 0 ≤ M := by positivity
  have hframe : (frameSamples : ℝ) = 32 := by norm_num [frameSamples]
  rw [hframe] at within ⊢
  have combined (t : ℝ) (ht : t ∈ Icc start (start + 2 * 32)) :
      |joinAcceleration s point span t| ≤ M :=
    abs_join_acceleration_le (by norm_num) bounds hpoint hspan (within ht)
  have first (t : ℝ) (ht : t ∈ Icc start (start + 32)) :
      |joinAcceleration s point span t| ≤ M :=
    combined t ⟨ht.1, by linarith [ht.2]⟩
  have hab : start < start + 32 := by linarith
  have hbc : start + 32 < start + 2 * 32 := by linarith
  refine ⟨fun t ht => ⟨?_, ?_⟩, ?_, fun t ht => ?_⟩
  · have err := frameValue_error_le hab ht (fun t _ => hv t) (fun t _ => ha t) first hM
    rw [abs_mul, abs_of_pos (by norm_num : (0 : ℝ) < 128)]
    calc
      _ ≤ 128 * (M * (start + 32 - start) ^ 2 / 4) :=
        mul_le_mul_of_nonneg_left err (by norm_num)
      _ = _ := by rw [add_sub_cancel_left]; norm_num [M]
  · have err := frameSlope_error_le hab ht (fun t _ => hv t) (fun t _ => ha t) first hM
    rw [abs_mul, abs_of_pos (by norm_num : (0 : ℝ) < 128)]
    calc
      _ ≤ 128 * (M * (start + 32 - start)) :=
        mul_le_mul_of_nonneg_left err (by norm_num)
      _ = _ := by rw [add_sub_cancel_left]; norm_num [M]
  · have err := frameSlope_jump_le hab hbc (fun t _ => hv t) (fun t _ => ha t) combined hM
    rw [abs_mul, abs_of_pos (by norm_num : (0 : ℝ) < 128)]
    calc
      _ ≤ 128 * (M * (start + 2 * 32 - start)) :=
        mul_le_mul_of_nonneg_left err (by norm_num)
      _ = _ := by rw [add_sub_cancel_left]; norm_num [M]
  · rw [abs_mul, abs_of_pos (by norm_num : (0 : ℝ) < 128)]
    calc
      _ ≤ 128 * M := mul_le_mul_of_nonneg_left (combined t ht) (by norm_num)
      _ = _ := by norm_num [M]

/-- Inverse-transform exponential draw in cycles per sample. -/
def velocityDraw (lambdaSamples u : ℝ) : ℝ := -Real.log u / lambdaSamples

theorem velocityDraw_gt_iff {lambdaSamples L u : ℝ} (hRate : 0 < lambdaSamples)
    (hu : 0 < u) :
    1 / L < velocityDraw lambdaSamples u ↔ u < Real.exp (-lambdaSamples / L) := by
  rw [velocityDraw, lt_div_iff₀ hRate, ← Real.log_lt_log_iff hu (Real.exp_pos _), Real.log_exp]
  rw [show 1 / L * lambdaSamples = lambdaSamples / L by ring, neg_div]
  constructor <;> intro h <;> linarith

/-- A finite-horizon failure bound for the minimum span condition. Uniform
open-interval marginals suffice, even when the draws are dependent. -/
theorem fastDraw_probability_le {Ω : Type*} [MeasurableSpace Ω] (μ : Measure Ω)
    {n : ℕ} (u : Fin n → Ω → ℝ) {lambdaSamples L : ℝ}
    (hRate : 0 < lambdaSamples) (hL : 0 < L)
    (positive : ∀ i ω, 0 < u i ω)
    (uniform : ∀ i t, 0 ≤ t → t ≤ 1 → μ {ω | u i ω < t} ≤ ENNReal.ofReal t) :
    μ (⋃ i, {ω | 1 / L < velocityDraw lambdaSamples (u i ω)}) ≤
      (n : ℝ≥0∞) * ENNReal.ofReal (Real.exp (-lambdaSamples / L)) := by
  let q := Real.exp (-lambdaSamples / L)
  calc
    _ ≤ ∑ i : Fin n, μ {ω | 1 / L < velocityDraw lambdaSamples (u i ω)} := measure_iUnion_fintype_le _ _
    _ ≤ ∑ _ : Fin n, ENNReal.ofReal q := by
      apply Finset.sum_le_sum
      intro i _
      have setEq : {ω | 1 / L < velocityDraw lambdaSamples (u i ω)} = {ω | u i ω < q} := by
        ext ω
        exact velocityDraw_gt_iff hRate (positive i ω)
      rw [setEq]
      exact uniform i q (Real.exp_pos _).le (Real.exp_le_one_iff.mpr (by
        rw [neg_div]
        exact neg_nonpos.mpr (div_nonneg hRate.le hL.le)))
    _ = _ := by simp [q, nsmul_eq_mul]

/-- The exact real interpretation of the open 52-bit grid also meets the
minimum-span condition deterministically at the reverb settings. -/
theorem reverb_grid_velocity_le {u : ℝ} (hu : (2 ^ 53 : ℝ)⁻¹ ≤ u) :
    velocityDraw 1920000 u ≤ 1 / 50000 := by
  have hlo := Real.log_le_log (by positivity : 0 < (2 ^ 53 : ℝ)⁻¹) hu
  rw [Real.log_inv, Real.log_pow] at hlo
  norm_num at hlo
  unfold velocityDraw
  rw [div_le_iff₀ (by norm_num : (0 : ℝ) < 1920000)]
  linarith [Real.log_two_lt_d9]

/-- Knot span of the exponential-velocity comparison clock in exact arithmetic. -/
def idealSpan (lambdaSamples u : ℝ) : ℝ :=
  1 / min 1 (max ((2 ^ 48 : ℝ)⁻¹) (velocityDraw lambdaSamples u))

theorem span_ge_of_velocity_le {lambdaSamples L u : ℝ} (hL : 0 < L)
    (floorFits : (2 ^ 48 : ℝ)⁻¹ ≤ 1 / L) (speed : velocityDraw lambdaSamples u ≤ 1 / L) :
    L ≤ idealSpan lambdaSamples u := by
  have pos : 0 < min 1 (max ((2 ^ 48 : ℝ)⁻¹) (velocityDraw lambdaSamples u)) :=
    lt_min (by norm_num) ((by positivity : 0 < (2 ^ 48 : ℝ)⁻¹).trans_le (le_max_left _ _))
  apply (le_div_iff₀ pos).mpr
  calc
    _ ≤ L * (1 / L) := mul_le_mul_of_nonneg_left
      ((min_le_right _ _).trans (max_le floorFits speed)) hL.le
    _ = _ := by field_simp

theorem reverb_grid_span_ge {u : ℝ} (hu : (2 ^ 53 : ℝ)⁻¹ ≤ u) :
    50000 ≤ idealSpan 1920000 u :=
  span_ge_of_velocity_le (by norm_num) (by norm_num) (reverb_grid_velocity_le hu)

theorem reverb_open52_span_ge (bits : ℕ) :
    50000 ≤ idealSpan 1920000 (((bits : ℝ) + 1 / 2) / (2 ^ 52 : ℝ)) := by
  apply reverb_grid_span_ge
  calc
    _ = (1 / 2 : ℝ) / 2 ^ 52 := by norm_num
    _ ≤ _ := div_le_div_of_nonneg_right (by linarith [Nat.cast_nonneg (α := ℝ) bits])
      (by positivity)

/-- Finite-horizon bound on the probability that any clamped span is too short. -/
theorem shortSpan_probability_le {Ω : Type*} [MeasurableSpace Ω] (μ : Measure Ω)
    {n : ℕ} (u : Fin n → Ω → ℝ) {lambdaSamples L : ℝ}
    (hRate : 0 < lambdaSamples) (hL : 0 < L) (floorFits : (2 ^ 48 : ℝ)⁻¹ ≤ 1 / L)
    (positive : ∀ i ω, 0 < u i ω)
    (uniform : ∀ i t, 0 ≤ t → t ≤ 1 → μ {ω | u i ω < t} ≤ ENNReal.ofReal t) :
    μ (⋃ i, {ω | idealSpan lambdaSamples (u i ω) < L}) ≤
      (n : ℝ≥0∞) * ENNReal.ofReal (Real.exp (-lambdaSamples / L)) := by
  have subset : (⋃ i, {ω | idealSpan lambdaSamples (u i ω) < L}) ⊆
      (⋃ i, {ω | 1 / L < velocityDraw lambdaSamples (u i ω)}) := by
    intro ω hω
    obtain ⟨i, hi⟩ := mem_iUnion.mp hω
    apply mem_iUnion.mpr
    exact ⟨i, lt_of_not_ge fun slow => (not_lt_of_ge (span_ge_of_velocity_le hL floorFits slow)) hi⟩
  exact (measure_mono subset).trans (fastDraw_probability_le μ u hRate hL positive uniform)

theorem reverb_short_span_probability_le {Ω : Type*} [MeasurableSpace Ω] (μ : Measure Ω)
    {n : ℕ} (u : Fin n → Ω → ℝ)
    (positive : ∀ i ω, 0 < u i ω)
    (uniform : ∀ i t, 0 ≤ t → t ≤ 1 → μ {ω | u i ω < t} ≤ ENNReal.ofReal t) :
    μ (⋃ i, {ω | idealSpan 1920000 (u i ω) < 50000}) ≤
      (n : ℝ≥0∞) * ENNReal.ofReal ((2 ^ 55 : ℝ)⁻¹) := by
  have expBound : Real.exp (-(1920000 : ℝ) / 50000) ≤ (2 ^ 55 : ℝ)⁻¹ := by
    apply (Real.log_le_log_iff (Real.exp_pos _) (by positivity)).mp
    rw [Real.log_exp, Real.log_inv, Real.log_pow]
    norm_num
    linarith [Real.log_two_lt_d9]
  calc
    _ ≤ (n : ℝ≥0∞) * ENNReal.ofReal (Real.exp (-1920000 / 50000)) :=
      shortSpan_probability_le μ u (by norm_num) (by norm_num) (by norm_num) positive uniform
    _ ≤ _ := mul_le_mul_right (ENNReal.ofReal_le_ofReal expBound) _

end
end DSP.SmoothRandomLFO
