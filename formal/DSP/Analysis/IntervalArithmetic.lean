import DSP.Analysis.IntervalTanh
import DSP.Analysis.NormalizedTanhDerivatives
import Mathlib.LinearAlgebra.Matrix.ToLin

/-! Small witness checks shared by the orbit enclosure replay. The producer
chooses the rational boxes. Containment and tanh checks, not the producer,
establish their meaning. -/

namespace DSP.IntervalArithmetic
open LeanCert.Core
open Matrix

def contains (outer inner : IntervalRat) : Bool := decide (outer.lo ≤ inner.lo ∧ inner.hi ≤ outer.hi)

theorem contains_sound {outer inner : IntervalRat} (h : contains outer inner = true)
    {x : ℝ} (hx : x ∈ inner) : x ∈ outer := by
  simp only [contains, decide_eq_true_eq] at h
  have hl : (outer.lo:ℝ) ≤ inner.lo := by exact_mod_cast h.1
  have hu : (inner.hi:ℝ) ≤ outer.hi := by exact_mod_cast h.2
  exact ⟨hl.trans hx.1, hx.2.trans hu⟩

def sqrtCheck (input output : IntervalRat) : Bool :=
  decide (0 ≤ output.lo ∧ output.lo^2 ≤ input.lo ∧ input.hi ≤ output.hi^2)

theorem sqrtCheck_sound (input output : IntervalRat) (hc : sqrtCheck input output = true)
    (x : ℝ) (hx : x ∈ input) : Real.sqrt x ∈ output := by
  simp only [sqrtCheck, decide_eq_true_eq] at hc
  have hl : (0:ℝ) ≤ output.lo := by exact_mod_cast hc.1
  have hl2 : (output.lo:ℝ)^2 ≤ input.lo := by exact_mod_cast hc.2.1
  have hu2 : (input.hi:ℝ) ≤ (output.hi:ℝ)^2 := by exact_mod_cast hc.2.2
  have hou : (output.lo:ℝ) ≤ output.hi := by exact_mod_cast output.le
  have hx0 : 0 ≤ x := by nlinarith [hx.1]
  have hs := Real.sq_sqrt hx0
  have hp := Real.sqrt_nonneg x
  exact ⟨by nlinarith [hx.1], by nlinarith [hx.2]⟩

def inverseCheck (input output : IntervalRat) : Bool :=
  decide (0 < input.lo ∧ 0 ≤ output.lo ∧ output.lo*input.hi ≤ 1 ∧ 1 ≤ output.hi*input.lo)

theorem inverseCheck_sound (input output : IntervalRat) (hc : inverseCheck input output = true)
    (x : ℝ) (hx : x ∈ input) : x⁻¹ ∈ output := by
  simp only [inverseCheck, decide_eq_true_eq] at hc
  have hi : (0:ℝ) < input.lo := by exact_mod_cast hc.1
  have hlo : (0:ℝ) ≤ output.lo := by exact_mod_cast hc.2.1
  have hmullo : (output.lo:ℝ)*input.hi ≤ 1 := by exact_mod_cast hc.2.2.1
  have hmulhi : (1:ℝ) ≤ (output.hi:ℝ)*input.lo := by exact_mod_cast hc.2.2.2
  have hou : (output.lo:ℝ) ≤ output.hi := by exact_mod_cast output.le
  have hx0 : 0 < x := hi.trans_le hx.1
  rw [IntervalRat.mem_def, ← one_div]
  constructor
  · apply (le_div_iff₀ hx0).mpr
    nlinarith [mul_le_mul_of_nonneg_left hx.2 hlo]
  · apply (div_le_iff₀ hx0).mpr
    nlinarith [mul_le_mul_of_nonneg_left hx.1 (hlo.trans hou)]

def absoluteBound (I : IntervalRat) : ℚ := max |I.lo| |I.hi|

theorem norm_le_absoluteBound {x : ℝ} {I : IntervalRat} (hx : x ∈ I) :
    ‖x‖ ≤ (absoluteBound I:ℝ) := by
  rw [Real.norm_eq_abs, abs_le]
  have hl : -absoluteBound I ≤ I.lo := by
    dsimp [absoluteBound]
    have h := le_max_left |I.lo| |I.hi|
    linarith [neg_abs_le I.lo]
  have hu : I.hi ≤ absoluteBound I := (le_abs_self I.hi).trans (le_max_right _ _)
  have hl' : -(absoluteBound I:ℝ) ≤ I.lo := by exact_mod_cast hl
  have hu' : (I.hi:ℝ) ≤ absoluteBound I := by exact_mod_cast hu
  exact ⟨hl'.trans hx.1, hx.2.trans hu'⟩

def matrixBound {m n : ℕ} (box : Fin m → Fin n → IntervalRat) (bound : ℚ) : Bool :=
  decide (0 ≤ bound ∧ ∀ i, ∑ j, absoluteBound (box i j) ≤ bound)

theorem matrixBound_sound {m n : ℕ} (box : Fin m → Fin n → IntervalRat) (bound : ℚ)
    (hc : matrixBound box bound = true) (matrix : Matrix (Fin m) (Fin n) ℝ)
    (hm : ∀ i j, matrix i j ∈ box i j) (v : Fin n → ℝ) :
    ‖matrix *ᵥ v‖ ≤ (bound:ℝ)*‖v‖ := by
  simp only [matrixBound, decide_eq_true_eq] at hc
  have hb : (0:ℝ) ≤ bound := by exact_mod_cast hc.1
  apply (pi_norm_le_iff_of_nonneg (mul_nonneg hb (norm_nonneg v))).mpr
  intro i
  have hr : ∑ j, (absoluteBound (box i j):ℝ) ≤ bound := by exact_mod_cast hc.2 i
  calc
    ‖(matrix *ᵥ v) i‖ ≤ ∑ j, ‖matrix i j * v j‖ := by
      change ‖∑ j, matrix i j * v j‖ ≤ _
      exact norm_sum_le _ _
    _ ≤ ∑ j, (absoluteBound (box i j):ℝ)*‖v‖ := by
      apply Finset.sum_le_sum
      intro j _
      rw [norm_mul]
      exact mul_le_mul (norm_le_absoluteBound (hm i j)) (norm_le_pi_norm v j) (norm_nonneg _)
        ((norm_nonneg _).trans (norm_le_absoluteBound (hm i j)))
    _ = (∑ j, (absoluteBound (box i j):ℝ))*‖v‖ := (Finset.sum_mul ..).symm
    _ ≤ _ := mul_le_mul_of_nonneg_right hr (norm_nonneg v)

def sum {n : ℕ} (values : Fin n → IntervalRat) : IntervalRat :=
  (List.ofFn values).foldl IntervalRat.add (IntervalRat.singleton 0)

theorem mem_sum {n : ℕ} {values : Fin n → IntervalRat} {x : Fin n → ℝ}
    (hx : ∀ i, x i ∈ values i) : ∑ i, x i ∈ sum values := by
  have hfold (xs : List (ℝ × IntervalRat)) (hxs : ∀ p ∈ xs, p.1 ∈ p.2)
      (a : ℝ) (I : IntervalRat) (ha : a ∈ I) :
      a+(xs.map Prod.fst).sum ∈ (xs.map Prod.snd).foldl IntervalRat.add I := by
    induction xs generalizing a I with
    | nil => simpa using ha
    | cons p xs ih =>
      have hp := hxs p (by simp)
      have ht := ih (fun q hq => hxs q (by simp [hq])) (a+p.1) (I.add p.2) (IntervalRat.mem_add ha hp)
      simpa [add_assoc] using ht
  have h := hfold (List.ofFn (fun i => (x i, values i))) (by simpa using hx)
    0 (IntervalRat.singleton 0) (by simpa using IntervalRat.mem_singleton 0)
  simpa [sum, List.map_ofFn, Function.comp_def, List.sum_ofFn] using h

def matrixProduct {m n p : ℕ} (a : Fin m → Fin n → IntervalRat)
    (b : Fin n → Fin p → IntervalRat) : Fin m → Fin p → IntervalRat :=
  fun i j => sum (fun k => (a i k).mul (b k j))

theorem mem_matrixProduct {m n p : ℕ} (a : Matrix (Fin m) (Fin n) ℝ)
    (b : Matrix (Fin n) (Fin p) ℝ) (abox : Fin m → Fin n → IntervalRat)
    (bbox : Fin n → Fin p → IntervalRat) (ha : ∀ i j, a i j ∈ abox i j)
    (hb : ∀ i j, b i j ∈ bbox i j) : ∀ i j, (a*b) i j ∈ matrixProduct abox bbox i j := by
  intro i j
  exact mem_sum (fun k => IntervalRat.mem_mul (ha i k) (hb k j))

def matrixVector {m n : ℕ} (a : Fin m → Fin n → IntervalRat)
    (b : Fin n → IntervalRat) : Fin m → IntervalRat :=
  fun i => sum (fun j => (a i j).mul (b j))

theorem mem_matrixVector {m n : ℕ} (a : Matrix (Fin m) (Fin n) ℝ)
    (b : Fin n → ℝ) (abox : Fin m → Fin n → IntervalRat) (bbox : Fin n → IntervalRat)
    (ha : ∀ i j, a i j ∈ abox i j) (hb : ∀ i, b i ∈ bbox i) :
    ∀ i, (a *ᵥ b) i ∈ matrixVector abox bbox i := by
  intro i
  exact mem_sum (fun j => IntervalRat.mem_mul (ha i j) (hb j))

def slope (gain inverseNormalization saturation : IntervalRat) : IntervalRat :=
  (gain.mul ((IntervalRat.singleton 1).sub (saturation.mul saturation))).mul inverseNormalization

theorem mem_slope (c : NormalizedTanh.Coefficients ℝ)
    (gain inverseNormalization input saturation : IntervalRat) (depth : ℕ)
    (hg : c.gain ∈ gain) (hi : c.tanhGain⁻¹ ∈ inverseNormalization)
    (hc : IntervalTanh.check (input.mul gain) saturation depth = true)
    (x : ℝ) (hx : x ∈ input) : NormalizedTanh.slope c x ∈ slope gain inverseNormalization saturation := by
  have hs := IntervalTanh.check_sound _ _ depth hc (x*c.gain) (IntervalRat.mem_mul hx hg)
  have h := IntervalRat.mem_mul
    (IntervalRat.mem_mul hg (IntervalRat.mem_sub (IntervalRat.mem_singleton 1) (IntervalRat.mem_mul hs hs))) hi
  simpa only [NormalizedTanh.slope, slope, div_eq_mul_inv, pow_two, Rat.cast_one] using h

/-- Uniform slope boxes also enclose exact secant coefficients. This is the
finite-difference fact required for a uniform block contraction certificate. -/
theorem exists_secant (c : NormalizedTanh.Coefficients ℝ)
    (gain inverseNormalization input saturation : IntervalRat) (depth : ℕ)
    (hg : c.gain ∈ gain) (hi : c.tanhGain⁻¹ ∈ inverseNormalization)
    (hc : IntervalTanh.check (input.mul gain) saturation depth = true)
    (x y : ℝ) (hx : x ∈ input) (hy : y ∈ input) :
    ∃ d ∈ slope gain inverseNormalization saturation,
      NormalizedTanh.apply c x-NormalizedTanh.apply c y = d*(x-y) := by
  obtain ⟨z, hz, he⟩ := NormalizedTanh.exists_secant c input.lo input.hi x y hx hy
  exact ⟨NormalizedTanh.slope c z, mem_slope c gain inverseNormalization input saturation depth hg hi hc z hz, he⟩

end DSP.IntervalArithmetic
