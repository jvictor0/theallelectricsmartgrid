import DSP.Analysis.IntervalArithmetic
import Mathlib.Analysis.Real.Pi.Bounds
import Mathlib.Analysis.SpecialFunctions.Trigonometric.Bounds

/-! Tight rational trigonometric enclosures for Fourier-cell certificates.
Range reduction uses the proved twenty-decimal pi bounds. Taylor is evaluated
at the interval center; the derivative bound one supplies the cell width.
The integer shift is just a proposal: correctness holds for every shift. -/

namespace DSP.IntervalTrig
open LeanCert.Core DSP.IntervalArithmetic

def pi : IntervalRat := ⟨3.14159265358979323846, 3.14159265358979323847, by norm_num⟩

theorem pi_mem : Real.pi ∈ pi := ⟨le_of_lt Real.pi_gt_d20, le_of_lt Real.pi_lt_d20⟩

def shift (angle : IntervalRat) (k : ℤ) : IntervalRat := angle.sub (pi.scale (2*k))

theorem shift_mem (angle : IntervalRat) (k : ℤ) (θ : ℝ) (hθ : θ ∈ angle) :
    θ-2*Real.pi*k ∈ shift angle k := by
  have h := IntervalRat.mem_sub hθ (IntervalRat.mem_scale (2*(k:ℚ)) pi_mem)
  have hid : θ-2*Real.pi*k = θ-((2*(k:ℚ):ℚ):ℝ)*Real.pi := by push_cast; ring
  rw [hid]
  exact h

def shiftCount (angle : IntervalRat) : ℤ :=
  (angle.midpoint/(2*pi.midpoint)+1/2).floor

def sinePoint (x : ℚ) (depth : ℕ) : IntervalRat :=
  (shift (IntervalRat.singleton x) (shiftCount (IntervalRat.singleton x))).sinComputable depth

def cosinePoint (x : ℚ) (depth : ℕ) : IntervalRat :=
  (shift (IntervalRat.singleton x) (shiftCount (IntervalRat.singleton x))).cosComputable depth

theorem sinePoint_mem (x : ℚ) (depth : ℕ) : Real.sin (x:ℝ) ∈ sinePoint x depth := by
  let k := shiftCount (IntervalRat.singleton x)
  have h := IntervalRat.mem_sinComputable
    (shift_mem (IntervalRat.singleton x) k x (IntervalRat.mem_singleton x)) depth
  have he := Real.sin_add_int_mul_two_pi (x:ℝ) (-k)
  have hid : (x:ℝ)+(-k:ℤ)*(2*Real.pi) = x-2*Real.pi*k := by push_cast; ring
  rw [hid] at he
  exact he ▸ h

theorem cosinePoint_mem (x : ℚ) (depth : ℕ) : Real.cos (x:ℝ) ∈ cosinePoint x depth := by
  let k := shiftCount (IntervalRat.singleton x)
  have h := IntervalRat.mem_cosComputable
    (shift_mem (IntervalRat.singleton x) k x (IntervalRat.mem_singleton x)) depth
  have he := Real.cos_add_int_mul_two_pi (x:ℝ) (-k)
  have hid : (x:ℝ)+(-k:ℤ)*(2*Real.pi) = x-2*Real.pi*k := by push_cast; ring
  rw [hid] at he
  exact he ▸ h

def widen (point angle : IntervalRat) : IntervalRat :=
  ⟨point.lo-angle.width/2, point.hi+angle.width/2, by
    have h : 0 ≤ angle.width := sub_nonneg.mpr angle.le
    linarith [point.le]⟩

theorem midpoint_distance (angle : IntervalRat) (θ : ℝ) (hθ : θ ∈ angle) :
    |θ-(angle.midpoint:ℝ)| ≤ (angle.width:ℝ)/2 := by
  rw [abs_le]
  simp only [IntervalRat.midpoint, IntervalRat.width, Rat.cast_div, Rat.cast_add,
    Rat.cast_sub, Rat.cast_ofNat]
  constructor <;> linarith [hθ.1,hθ.2]

theorem widen_mem (point angle : IntervalRat) (x y : ℝ) (hx : x ∈ point)
    (hxy : |y-x| ≤ (angle.width:ℝ)/2) : y ∈ widen point angle := by
  have h := abs_le.mp hxy
  simp only [widen, IntervalRat.mem_def, Rat.cast_sub, Rat.cast_add, Rat.cast_div, Rat.cast_ofNat]
  exact ⟨by linarith [hx.1], by linarith [hx.2]⟩

def sine (angle : IntervalRat) (depth : ℕ) : IntervalRat := widen (sinePoint angle.midpoint depth) angle
def cosine (angle : IntervalRat) (depth : ℕ) : IntervalRat := widen (cosinePoint angle.midpoint depth) angle

theorem sine_mem (angle : IntervalRat) (depth : ℕ) (θ : ℝ) (hθ : θ ∈ angle) :
    Real.sin θ ∈ sine angle depth := by
  apply widen_mem _ angle _ _ (sinePoint_mem angle.midpoint depth)
  exact (Real.abs_sin_sub_sin_le θ _).trans (midpoint_distance angle θ hθ)

theorem cosine_mem (angle : IntervalRat) (depth : ℕ) (θ : ℝ) (hθ : θ ∈ angle) :
    Real.cos θ ∈ cosine angle depth := by
  apply widen_mem _ angle _ _ (cosinePoint_mem angle.midpoint depth)
  exact (Real.abs_cos_sub_cos_le θ _).trans (midpoint_distance angle θ hθ)

end DSP.IntervalTrig
