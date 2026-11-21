import DSP.SmoothRandomLFO
import DSP.FMA.Algebra
import Mathlib.Analysis.Calculus.ContDiff.Deriv
import Mathlib.Analysis.Calculus.Deriv.Add
import Mathlib.Analysis.Calculus.Deriv.Mul
import Mathlib.Analysis.Calculus.Deriv.Pow
import Mathlib.Topology.Piecewise
import Mathlib.Tactic.FieldSimp
import Mathlib.Tactic.LinearCombination
import Mathlib.Tactic.Positivity
import Mathlib.Tactic.Ring

namespace DSP.SmoothRandomLFO

open Filter Set
open scoped Topology

noncomputable section

def atTime (p : Cubic ℝ) (start width t : ℝ) : ℝ :=
  evaluate p ((t - start) / width)

def velocity (p : Cubic ℝ) (width phase : ℝ) : ℝ :=
  (3 * p.a * phase ^ 2 + 2 * p.b * phase + p.c) / width

def acceleration (p : Cubic ℝ) (width phase : ℝ) : ℝ :=
  (6 * p.a * phase + 2 * p.b) / width ^ 2

private theorem hasDerivAt_time (p : Cubic ℝ) (start width t : ℝ) :
    HasDerivAt (atTime p start width) (velocity p width ((t - start) / width)) t := by
  have hu := (HasDerivAt.sub_const start (hasDerivAt_id t)).div_const width
  have h := HasDerivAt.add_const p.d
    ((((hu.pow 3).const_mul p.a).add ((hu.pow 2).const_mul p.b)).add (hu.const_mul p.c))
  convert! h using 1
  · funext x
    simp [atTime, evaluate]
    ring
  · norm_num [velocity, div_eq_mul_inv]
    ring

private theorem hasDerivAt_velocity (p : Cubic ℝ) (start width t : ℝ) :
    HasDerivAt (fun x => velocity p width ((x - start) / width))
      (acceleration p width ((t - start) / width)) t := by
  have hu := (HasDerivAt.sub_const start (hasDerivAt_id t)).div_const width
  have h := (HasDerivAt.add_const p.c
    (((hu.pow 2).const_mul (3 * p.a)).add (hu.const_mul (2 * p.b)))).div_const width
  convert! h using 1
  norm_num [acceleration, div_eq_mul_inv, mul_inv_rev]
  ring

private def jet (s : Segment ℝ) (phase : ℝ) : ℝ × ℝ × ℝ :=
  (evaluate (prepare s) phase, velocity (prepare s) s.width phase,
    acceleration (prepare s) s.width phase)

private theorem jet_left (s : Segment ℝ) (hw : s.width ≠ 0) :
    jet s 0 =
      (s.p1 + (s.previous - s.width) * s.slope + s.width ^ 2 * s.leftCurve,
        3 * (s.slope - s.width * s.leftCurve), 6 * s.leftCurve) := by
  simp only [jet, evaluate, velocity, acceleration, prepare, fma_eq]
  congr 1
  · ring
  · congr 1 <;> field_simp [hw] <;> ring

private theorem jet_right (s : Segment ℝ) (hw : s.width ≠ 0) :
    jet s 1 =
      (s.p1 + (s.previous + 2 * s.width) * s.slope + s.width ^ 2 * s.rightCurve,
        3 * (s.slope + s.width * s.rightCurve), 6 * s.rightCurve) := by
  simp only [jet, evaluate, velocity, acceleration, prepare, fma_eq]
  congr 1
  · ring
  · congr 1 <;> field_simp [hw] <;> ring

private theorem jets_match (s : Segment ℝ) (point span : ℝ)
    (hp : 0 < s.previous) (hw : 0 < s.width) (hn : 0 < s.next) :
    jet s 1 = jet (s.advance point span) 0 := by
  rw [jet_right s hw.ne', jet_left _ hn.ne']
  have hp2 : s.p1 + (s.previous + s.width + s.next) * s.slope = s.p2 := by
    have h : s.previous + s.width + s.next ≠ 0 := by positivity
    simp only [Segment.slope]
    field_simp [h]
    ring
  have hslope : s.slope + (s.width + s.next) * s.rightCurve = s.rightSlope := by
    have h : s.width + s.next ≠ 0 := by positivity
    simp only [Segment.rightCurve]
    field_simp [h]
    ring
  change (_, _, _) = (s.p2 + (s.width - s.next) * s.rightSlope + s.next ^ 2 * s.rightCurve,
    3 * (s.rightSlope - s.next * s.rightCurve), 6 * s.rightCurve)
  exact Prod.ext (by linear_combination hp2 + (s.width - s.next) * hslope)
    (Prod.ext (by linear_combination 3 * hslope) rfl)

/-- The two segment functions are expressed in sample time relative to their shared knot. -/
def join (left right : ℝ → ℝ) : ℝ → ℝ := (Iio 0).piecewise left right

private theorem hasDerivAt_join {l r dl dr : ℝ → ℝ} (x : ℝ)
    (hl : ∀ t, HasDerivAt l (dl t) t) (hr : ∀ t, HasDerivAt r (dr t) t)
    (hv : l 0 = r 0) (hd : dl 0 = dr 0) :
    HasDerivAt (join l r) (join dl dr x) x := by
  rcases lt_trichotomy x 0 with hx | rfl | hx
  · rw [show join dl dr x = dl x by simp [join, hx]]
    exact (hl x).congr_of_eventuallyEq (by
      filter_upwards [gt_mem_nhds hx] with y hy
      simp [join, hy])
  · have left : HasDerivWithinAt (join l r) (dr 0) (Iio 0) 0 := by
      rw [← hd]
      exact (hl 0).hasDerivWithinAt.congr (by intro y hy; simp [join, mem_Iio.mp hy])
        (by simpa [join] using hv.symm)
    have right : HasDerivWithinAt (join l r) (dr 0) (Ici 0) 0 :=
      (hr 0).hasDerivWithinAt.congr
        (by intro y hy; simp [join, not_lt.mpr (mem_Ici.mp hy)]) (by simp [join])
    have hunion : Iio (0 : ℝ) ∪ Ici 0 = univ := by rw [← compl_Iio, union_compl_self]
    simpa [hunion, join] using left.union right
  · rw [show join dl dr x = dr x by simp [join, hx.not_gt]]
    exact (hr x).congr_of_eventuallyEq (by
      filter_upwards [lt_mem_nhds hx] with y hy
      simp [join, hy.not_gt])

/-- Sliding the window by one random point and span gives a C² join for
arbitrary control values and positive knot spans. The derivatives here are
with respect to sample time, so the two local phases may run at different rates. -/
theorem contDiff_join (s : Segment ℝ) (point span : ℝ)
    (hp : 0 < s.previous) (hw : 0 < s.width) (hn : 0 < s.next) :
    ContDiff ℝ 2 (join (atTime (prepare s) (-s.width) s.width)
      (atTime (prepare (s.advance point span)) 0 s.next)) := by
  let p := prepare s
  let q := prepare (s.advance point span)
  let l := atTime p (-s.width) s.width
  let r := atTime q 0 s.next
  let dl := fun t => velocity p s.width ((t - -s.width) / s.width)
  let dr := fun t => velocity q s.next ((t - 0) / s.next)
  let ddl := fun t => acceleration p s.width ((t - -s.width) / s.width)
  let ddr := fun t => acceleration q s.next ((t - 0) / s.next)
  have hj : (l 0, dl 0, ddl 0) = (r 0, dr 0, ddr 0) := by
    simpa [jet, l, r, dl, dr, ddl, ddr, atTime, p, q, Segment.advance, hw.ne'] using
      jets_match s point span hp hw hn
  have hv := congrArg Prod.fst hj
  have hd := congrArg (fun j => j.2.1) hj
  have hdd := congrArg (fun j => j.2.2) hj
  have h₁ t : HasDerivAt (join l r) (join dl dr t) t :=
    hasDerivAt_join t (hasDerivAt_time p _ _) (hasDerivAt_time q _ _) hv hd
  have h₂ t : HasDerivAt (join dl dr) (join ddl ddr t) t :=
    hasDerivAt_join t (hasDerivAt_velocity p _ _) (hasDerivAt_velocity q _ _) hd hdd
  change ContDiff ℝ (1 + 1) (join l r)
  rw [contDiff_succ_iff_deriv]
  refine ⟨fun t => (h₁ t).differentiableAt, by simp, ?_⟩
  rw [show deriv (join l r) = join dl dr from funext (fun t => (h₁ t).deriv)]
  rw [contDiff_one_iff_deriv]
  refine ⟨fun t => (h₂ t).differentiableAt, ?_⟩
  rw [show deriv (join dl dr) = join ddl ddr from funext (fun t => (h₂ t).deriv)]
  apply Continuous.piecewise
  · intro t ht
    rw [frontier_Iio, mem_singleton_iff] at ht
    simpa [ht] using hdd
  · unfold ddl acceleration
    fun_prop
  · unfold ddr acceleration
    fun_prop

end

end DSP.SmoothRandomLFO
