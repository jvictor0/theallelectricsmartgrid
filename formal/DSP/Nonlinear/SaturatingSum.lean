import DSP.Nonlinear.Analysis
import Mathlib.Analysis.Calculus.FDeriv.Prod
import Mathlib.Analysis.Calculus.FDeriv.Add

/-! Analysis of gain, componentwise saturation, summation and final saturation.
This memoryless map can follow any stable filter bank or linear routing. -/
noncomputable section
namespace DSP.SaturatingSum
open scoped BigOperators
variable {n : Nat}

def gains (g : Fin n → ℝ) : (Fin n → ℝ) →L[ℝ] (Fin n → ℝ) :=
  ContinuousLinearMap.pi (fun i => g i • ContinuousLinearMap.proj i)

def mix (master : ℝ) : (Fin n → ℝ) →L[ℝ] ℝ :=
  ∑ i, master • ContinuousLinearMap.proj i

def saturate (x : Fin n → ℝ) : Fin n → ℝ := fun i => Real.tanh (x i)

def apply (g : Fin n → ℝ) (master : ℝ) (x : Fin n → ℝ) : ℝ :=
  Real.tanh (mix master (saturate (gains g x)))

def tangent (g : Fin n → ℝ) (master : ℝ) : (Fin n → ℝ) →L[ℝ] ℝ :=
  (mix master).comp (gains g)

theorem apply_eq (g : Fin n → ℝ) (master : ℝ) (x : Fin n → ℝ) :
    apply g master x = Real.tanh (master * ∑ i, Real.tanh (x i*g i)) := by
  simp [apply,mix,saturate,gains,← Finset.mul_sum,mul_comm]

theorem tangent_eq (g : Fin n → ℝ) (master : ℝ) (x : Fin n → ℝ) :
    tangent g master x = master * ∑ i, x i*g i := by
  simp [tangent,mix,gains,← Finset.mul_sum,mul_comm]

/-- The real tangent acts on a complex sinusoid by the same weighted sum. -/
theorem tangent_real (g : Fin n → ℝ) (master : ℝ) (v : Fin n → ℂ) (q : ℂ) :
    tangent g master (fun i => (v i*q).re) =
      (((master:ℂ) * ∑ i, (g i:ℂ)*v i)*q).re := by
  rw [tangent_eq]
  simp only [Finset.sum_mul,Finset.mul_sum,Complex.re_sum]
  apply Finset.sum_congr rfl
  intro i _
  simp only [Complex.mul_re,Complex.mul_im,Complex.ofReal_re,Complex.ofReal_im]
  ring

theorem apply_zero (g : Fin n → ℝ) (master : ℝ) : apply g master 0 = 0 := by
  simp [apply_eq]

theorem saturate_zero : saturate (0 : Fin n → ℝ) = 0 := by ext i; simp [saturate]

theorem saturate_hasFDerivAt : HasFDerivAt (saturate (n := n)) (ContinuousLinearMap.id ℝ _) 0 := by
  apply hasFDerivAt_pi'.mpr
  intro i
  have h := Tanh.hasDerivAt_zero.comp_hasFDerivAt 0 (ContinuousLinearMap.proj i : (Fin n → ℝ) →L[ℝ] ℝ).hasFDerivAt
  convert! h using 1
  simp

theorem hasFDerivAt_zero (g : Fin n → ℝ) (master : ℝ) :
    HasFDerivAt (apply g master) (tangent g master) 0 := by
  have hs : HasFDerivAt saturate (ContinuousLinearMap.id ℝ _) ((gains g) 0) := by
    simpa only [map_zero] using (saturate_hasFDerivAt (n := n))
  have hg := hs.comp 0 (gains g).hasFDerivAt
  have hm := (mix (n := n) master).hasFDerivAt.comp 0 hg
  have ht := Tanh.hasDerivAt_zero
  have hzero : mix master (saturate (gains g 0))=0 := by simp [saturate_zero]
  rw [← hzero] at ht
  convert! ht.comp_hasFDerivAt 0 hm using 1
  simp [tangent]

theorem saturate_lipschitz : LipschitzWith 1 (saturate (n := n)) := by
  apply LipschitzWith.of_dist_le_mul
  intro x y
  simp only [NNReal.coe_one,one_mul]
  apply (dist_pi_le_iff (dist_nonneg)).mpr
  intro i
  exact (Tanh.lipschitz_real.dist_le_mul (x i) (y i)).trans (by simpa using dist_le_pi_dist x y i)

theorem lipschitz (g : Fin n → ℝ) (master : ℝ) :
    LipschitzWith (‖mix (n := n) master‖₊ * ‖gains g‖₊) (apply g master) := by
  have h := Tanh.lipschitz_real.comp ((mix master).lipschitz.comp (saturate_lipschitz.comp (gains g).lipschitz))
  convert! h using 1
  simp

end DSP.SaturatingSum
