import DSP.Analysis.FinitePrimitive
import DSP.Analysis.PhasePower

/-! A constructed finite correction for the drifting Adler phase model.
The observable is quadratic in the unit phase, covering BOTH the first-order
driven coherent observer and its total-power observer. Bounds concern sampled
trajectories, allow rational rotation and bounded perturbations, and do not
assume convergence or identify continuous density with discrete time average.
The ladder-to-phase approximation defect is a separate, explicit input. -/

noncomputable section
namespace DSP.PhasePower.Discrete
open Finset DSP.ResponseMeasurement

structure Quadratic where
  constant : ℂ
  linear : ℂ
  quadratic : ℂ

def Quadratic.eval (q : Quadratic) (z : ℂ) : ℂ := q.constant+q.linear*z+q.quadratic*z^2
def center (c : ℝ) : ℂ := runningMoment c
def flow (c : ℝ) (z : ℂ) : ℂ := Complex.I*(1+(c:ℂ)^2)*z+(c:ℂ)*(z^2-1)

/-- q(z)-q(center) factors as (z-center)*(linear+quadratic*(z+center)). -/
def firstCoefficient (q : Quadratic) (c : ℝ) : ℂ := q.linear+q.quadratic*center c

def coefficient (q : Quadratic) (c : ℝ) (N : ℕ) (j : Fin N × Fin 2) : ℂ :=
  -Complex.I*(Complex.I*(c:ℂ))^j.1.val*
    (if j.2 = 0 then firstCoefficient q c else q.quadratic)

def degree {N : ℕ} (j : Fin N × Fin 2) : ℕ := j.1.val+j.2.val

def potential (q : Quadratic) (c : ℝ) (N : ℕ) : ℂ → ℂ :=
  FinitePrimitive.value (coefficient q c N) degree

def derivative (q : Quadratic) (c : ℝ) (N : ℕ) : ℂ → ℂ :=
  FinitePrimitive.slope (coefficient q c N) degree

def size0 (q : Quadratic) (c : ℝ) (N : ℕ) : ℝ :=
  FinitePrimitive.bound0 (coefficient q c N) degree
def size1 (q : Quadratic) (c : ℝ) (N : ℕ) : ℝ :=
  FinitePrimitive.bound1 (coefficient q c N)
def size2 (q : Quadratic) (c : ℝ) (N : ℕ) : ℝ :=
  FinitePrimitive.bound2 (coefficient q c N) degree

def tail (q : Quadratic) (c : ℝ) (N : ℕ) : ℝ :=
  (‖q.linear‖*(1+c)+‖q.quadratic‖*(1+c^2))*c^N

theorem center_norm (c : ℝ) (hc : 0 ≤ c) : ‖center c‖ = c := by
  simp [center,runningMoment,Complex.norm_real,Real.norm_eq_abs,abs_of_nonneg hc]

theorem flow_factor (c : ℝ) (z : ℂ) :
    flow c z = Complex.I*(z-center c)*(1-Complex.I*(c:ℂ)*z) := by
  unfold flow center runningMoment
  ring_nf
  simp only [Complex.I_sq,Complex.I_pow_three]
  ring

theorem derivative_eq (q : Quadratic) (c : ℝ) (N : ℕ) (z : ℂ) :
    derivative q c N z = -Complex.I*(firstCoefficient q c+q.quadratic*z)*
      ∑ l ∈ range N, (Complex.I*(c:ℂ)*z)^l := by
  simp only [derivative,FinitePrimitive.slope,Fintype.sum_prod_type,Fin.sum_univ_two,
    coefficient,degree,Fin.val_zero,Fin.val_one,ite_true,show (1:Fin 2) ≠ 0 by decide,ite_false,
    Nat.add_zero,pow_succ]
  rw [← Fin.sum_univ_eq_sum_range]
  rw [Finset.mul_sum]
  apply sum_congr rfl
  intro l _
  simp only [mul_pow]
  ring

/-- The finite correction cancels the entire quadratic observable except
for an explicit geometric tail. This is an identity, not a fitted residual. -/
theorem correction_identity (q : Quadratic) (c : ℝ) (N : ℕ) (z : ℂ) :
    derivative q c N z*flow c z =
      (q.eval z-q.eval (center c))*(1-(Complex.I*(c:ℂ)*z)^N) := by
  rw [derivative_eq,flow_factor]
  have hg := mul_neg_geom_sum (Complex.I*(c:ℂ)*z) N
  have he : q.eval z-q.eval (center c) = (z-center c)*(firstCoefficient q c+q.quadratic*z) := by
    unfold Quadratic.eval firstCoefficient
    ring
  rw [he]
  calc
    _ = ((-Complex.I)*Complex.I)*(z-center c)*(firstCoefficient q c+q.quadratic*z)*
        ((1-Complex.I*(c:ℂ)*z)*∑ l ∈ range N, (Complex.I*(c:ℂ)*z)^l) := by ring
    _ = _ := by rw [hg]; simp

theorem tail_bound (q : Quadratic) (c : ℝ) (hc : 0 ≤ c) (N : ℕ) (z : ℂ) (hz : ‖z‖ = 1) :
    ‖derivative q c N z*flow c z-(q.eval z-q.eval (center c))‖ ≤ tail q c N := by
  have he : q.eval z-q.eval (center c) =
      q.linear*(z-center c)+q.quadratic*(z^2-(center c)^2) := by unfold Quadratic.eval; ring
  have h1 : ‖z-center c‖ ≤ 1+c := by simpa only [hz,center_norm c hc] using norm_sub_le z (center c)
  have h2 : ‖z^2-(center c)^2‖ ≤ 1+c^2 := by
    have h := norm_sub_le (z^2) ((center c)^2)
    simpa only [norm_pow,hz,center_norm c hc,one_pow] using h
  rw [correction_identity,show (q.eval z-q.eval (center c))*(1-(Complex.I*(c:ℂ)*z)^N)-
      (q.eval z-q.eval (center c)) = -(q.eval z-q.eval (center c))*(Complex.I*(c:ℂ)*z)^N by ring]
  simp only [norm_mul,norm_neg,norm_pow,Complex.norm_I,Complex.norm_real,Real.norm_eq_abs,
    abs_of_nonneg hc,hz,one_mul,mul_one]
  apply mul_le_mul_of_nonneg_right _ (pow_nonneg hc N)
  rw [he]
  apply (norm_add_le _ _).trans
  simp only [norm_mul]
  exact add_le_add (mul_le_mul_of_nonneg_left h1 (norm_nonneg _))
    (mul_le_mul_of_nonneg_left h2 (norm_nonneg _))

end DSP.PhasePower.Discrete
