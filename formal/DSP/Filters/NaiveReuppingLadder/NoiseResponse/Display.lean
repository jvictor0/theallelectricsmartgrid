import Mathlib.Analysis.Complex.ExponentialBounds
import Mathlib.Analysis.SpecialFunctions.Log.Basic
import Mathlib.Tactic

noncomputable section
namespace NoiseDisplay
open Finset

def clampDb (a : ℝ) := max (-60) a
def displayError (a b : ℝ) := |clampDb a - clampDb b|
def powerFloor : ℝ := (10:ℝ)^(-6:ℤ)
def db (p : ℝ) := (10/Real.log 10)*Real.log (max powerFloor p)

theorem floor_pos : 0 < powerFloor := by unfold powerFloor; positivity
theorem floor_db : (10/Real.log 10)*Real.log powerFloor = -60 := by
  rw [powerFloor,Real.log_zpow]
  have h := (Real.log_pos (by norm_num : (1:ℝ) < 10)).ne'
  push_cast
  field_simp
  ring

theorem hidden {a b : ℝ} (ha : a ≤ -60) (hb : b ≤ -60) : displayError a b = 0 := by
  simp [displayError,clampDb,max_eq_left ha,max_eq_left hb]

theorem threshold_crossing {a b : ℝ} (ha : a ≤ -60) (hb : -60 ≤ b) :
    displayError a b = b+60 := by
  simp only [displayError,clampDb,max_eq_left ha,max_eq_right hb]
  rw [abs_of_nonpos (by linarith)];ring

theorem db_mono : Monotone db := by
  intro a b h
  exact mul_le_mul_of_nonneg_left
    (Real.log_le_log (floor_pos.trans_le (le_max_left _ _)) (max_le_max_left _ h))
    (div_nonneg (by norm_num) (Real.log_pos (by norm_num)).le)

theorem silent_power {p : ℝ} (h : p ≤ powerFloor) : db p = -60 := by
  simpa [db,max_eq_left h] using floor_db

theorem db_eq_display {p : ℝ} (hp : 0 < p) :
    db p = clampDb ((10/Real.log 10)*Real.log p) := by
  by_cases h : p ≤ powerFloor
  · rw [silent_power h,clampDb,max_eq_left]
    rw [←floor_db];exact mul_le_mul_of_nonneg_left (Real.log_le_log hp h) (by positivity)
  · have h' := le_of_not_ge h
    have hm := Real.log_le_log floor_pos h'
    unfold db clampDb
    rw [max_eq_right h',max_eq_right]
    rw [←floor_db];exact mul_le_mul_of_nonneg_left hm (by positivity)

/-- A bound stated directly on the computed picture, including a physical band that can be silent. -/
theorem band_bracket {lo p hi dry fit : ℝ} (hd : 0 < dry) (hl : lo ≤ p) (hu : p ≤ hi) :
    |db (p/dry)-clampDb fit| ≤ max |db (lo/dry)-clampDb fit| |db (hi/dry)-clampDb fit| := by
  have l := db_mono (div_le_div_of_nonneg_right hl hd.le)
  have u := db_mono (div_le_div_of_nonneg_right hu hd.le)
  apply abs_le.mpr
  constructor
  · linarith [neg_le_abs (db (lo/dry)-clampDb fit),le_max_left |db (lo/dry)-clampDb fit| |db (hi/dry)-clampDb fit|]
  · linarith [le_abs_self (db (hi/dry)-clampDb fit),le_max_right |db (lo/dry)-clampDb fit| |db (hi/dry)-clampDb fit|]

/-- The binary32 guard in PowerToDb is below the floor of PathDrawer.NormalizeDb.
    Comparing y coordinates is exactly comparing clipped dB, with height/120 as scale
    in the real-arithmetic drawing equations. -/
def powerToDb (p : ℝ) :=
  (10/Real.log 10)*Real.log (max (14411519/144115188075855872 : ℝ) p)

theorem path_floor (p : ℝ) : clampDb (powerToDb p) = db p := by
  let literal : ℝ := 14411519/144115188075855872
  have hl : 0 < literal := by norm_num [literal]
  have hguard : literal ≤ powerFloor := by norm_num [literal,powerFloor]
  let q := max literal p
  have hq : 0 < q := hl.trans_le (le_max_left ..)
  change clampDb ((10/Real.log 10)*Real.log q) = db p
  rw [←db_eq_display hq]
  unfold db
  rw [show max powerFloor q = max powerFloor p by
    dsimp [q];rw [←max_assoc,max_eq_left hguard]]

def screenY (top height gain : ℝ) :=
  top + height*(1-max 0 ((gain+60)/60)/2)

theorem drawn_error (top height a b : ℝ) (hh : 0 ≤ height) :
    |screenY top height a - screenY top height b| = height/120*displayError a b := by
  have normalize (x : ℝ) : max 0 ((x+60)/60) = (clampDb x+60)/60 := by
    unfold clampDb
    by_cases hx : x ≤ -60
    · rw [max_eq_left hx,max_eq_left (by linarith : (x+60)/60 ≤ 0)]; norm_num
    · rw [max_eq_right (le_of_not_ge hx),max_eq_right (by linarith : 0 ≤ (x+60)/60)]
  unfold screenY displayError
  rw [normalize a,normalize b,
    show top + height*(1-(clampDb a+60)/60/2) - (top + height*(1-(clampDb b+60)/60/2))
      = -(height/120*(clampDb a-clampDb b)) by ring,abs_neg,abs_mul,
    abs_of_nonneg (by positivity : 0 ≤ height/120)]

/-- The sampled scope connects adjacent heights with a straight path. An endpoint
    screen-error certificate survives every intermediate horizontal position. -/
theorem line_error {a b c d t E : ℝ} (ht : 0 ≤ t) (hu : t ≤ 1)
    (hl : |a-c| ≤ E) (hr : |b-d| ≤ E) :
    |((1-t)*a+t*b)-((1-t)*c+t*d)| ≤ E := by
  rw [show ((1-t)*a+t*b)-((1-t)*c+t*d)=(1-t)*(a-c)+t*(b-d) by ring]
  calc
    _ ≤ |(1-t)*(a-c)|+|t*(b-d)| := abs_add_le ..
    _ ≤ (1-t)*E+t*E := by
      rw [abs_mul,abs_mul,abs_of_nonneg (sub_nonneg.mpr hu),abs_of_nonneg ht]
      exact add_le_add (mul_le_mul_of_nonneg_left hl (sub_nonneg.mpr hu))
        (mul_le_mul_of_nonneg_left hr ht)
    _ = E := by ring

/-- A threshold cell stores extrema relative to the display floor. For a convex
    interpolation the deviation from interpolating already clipped vertices
    is bounded by the product of the two excursions divided by their sum. -/
theorem hinge {ι : Type*} [Fintype ι] (w x : ι → ℝ) {low high : ℝ}
    (hw : ∀ i, 0 ≤ w i) (hs : ∑ i,w i=1) (l0 : 0 ≤ low) (u0 : 0 ≤ high)
    (hb : ∀ i, -low ≤ x i ∧ x i ≤ high) :
    let z := (∑ i,w i*max 0 (x i)) - max 0 (∑ i,w i*x i)
    0 ≤ z ∧ z*(low+high) ≤ low*high := by
  let A := ∑ i,w i*max 0 (x i)
  let B := ∑ i,w i*max 0 (-x i)
  have a0 : 0 ≤ A := sum_nonneg (fun i _ => mul_nonneg (hw i) (le_max_left ..))
  have b0 : 0 ≤ B := sum_nonneg (fun i _ => mul_nonneg (hw i) (le_max_left ..))
  have diff : (∑ i,w i*x i) = A-B := by
    rw [show A-B=∑ i,w i*(max 0 (x i)-max 0 (-x i)) by simp [A,B,mul_sub,sum_sub_distrib]]
    apply sum_congr rfl;intro i _
    by_cases hi : 0 ≤ x i
    · simp [max_eq_right hi,max_eq_left (neg_nonpos.mpr hi)]
    · have hi' := le_of_not_ge hi
      simp [max_eq_left hi',max_eq_right (neg_nonneg.mpr hi')]
  have localBound (i) : low*max 0 (x i)+high*max 0 (-x i) ≤ low*high := by
    by_cases h : 0 ≤ x i
    · simp only [max_eq_right h,max_eq_left (neg_nonpos.mpr h),mul_zero,add_zero]
      exact mul_le_mul_of_nonneg_left (hb i).2 l0
    · have t := le_of_not_ge h
      simp only [max_eq_left t,max_eq_right (neg_nonneg.mpr t),mul_zero,zero_add]
      nlinarith [mul_le_mul_of_nonneg_left (show -x i ≤ low by linarith [(hb i).1]) u0]
  have tot : low*A+high*B ≤ low*high := by
    calc
      _ = ∑ i,w i*(low*max 0 (x i)+high*max 0 (-x i)) := by
        unfold A B; rw [mul_sum,mul_sum,←sum_add_distrib];apply sum_congr rfl;intros;ring
      _ ≤ ∑ i,w i*(low*high) := sum_le_sum (fun i _ => mul_le_mul_of_nonneg_left (localBound i) (hw i))
      _ = _ := by rw [←sum_mul,hs,one_mul]
  dsimp only
  change 0 ≤ A-max 0 (∑ i,w i*x i) ∧ (A-max 0 (∑ i,w i*x i))*(low+high) ≤ _
  rw [diff]
  by_cases h : 0 ≤ A-B
  · rw [max_eq_right h];constructor <;> nlinarith [mul_nonneg h l0]
  · have h' := le_of_not_ge h
    rw [max_eq_left h'];constructor <;> nlinarith [mul_nonneg (show 0 ≤ B-A by linarith) u0]

theorem gap_error {a b ma mb J : ℝ}
    (ha : 0 ≤ ma-a) (ha' : ma-a ≤ J) (hb : 0 ≤ mb-b) (hb' : mb-b ≤ J) :
    |a-b| ≤ |ma-mb|+J := by
  apply abs_le.mpr
  constructor <;> linarith [le_abs_self (ma-mb),neg_le_abs (ma-mb)]

theorem cell_mean_square {ι : Type*} [Fintype ι] (w v : ι → ℝ) (e : ℝ)
    (hw : ∀ i, 0 ≤ w i) (hs : ∑ i,w i=1)
    (hb : |e| ≤ ∑ i,w i*v i) : e^2 ≤ ∑ i,w i*(v i)^2 := by
  let A := ∑ i,w i*v i
  have h0 : 0 ≤ A := (abs_nonneg _).trans hb
  have pos := Finset.sum_nonneg (s:=Finset.univ) (fun i _ => mul_nonneg (hw i) (sq_nonneg (v i-A)))
  have eq : (∑ i,w i*(v i-A)^2) = (∑ i,w i*(v i)^2)-A^2 := by
    calc
      _ = ∑ i,(w i*(v i)^2 - (2*A)*(w i*v i) + A^2*w i) := by apply sum_congr rfl;intros;ring
      _ = _ := by rw [sum_add_distrib,sum_sub_distrib,←mul_sum,←mul_sum,hs];change _-2*A*A+A^2*1=_;ring
  rw [eq] at pos
  nlinarith [sq_abs e,mul_self_le_mul_self (abs_nonneg e) hb]

theorem checked_visible_totals :
    (2^239 < (1000:ℕ)^24) ∧
    (3628417522904:ℚ) / (7136501760*128^2) ≤ (19/100-1/128)^2 ∧
    (13785360:ℚ) / (478*128^2) ≤ (134/100-1/128)^2 := by norm_num

/-- A conservative one-sided 95% lower coverage endpoint for 92 observed successes in 96
    fresh trials. This is the exact numerator of the binomial tail at p = 907/1000. -/
theorem audit_binomial_tail :
    20 * (∑ j ∈ Finset.range 5, Nat.choose 96 j * 907^(96-j) * 93^j) < 1000^96 := by
  decide +kernel

end NoiseDisplay
