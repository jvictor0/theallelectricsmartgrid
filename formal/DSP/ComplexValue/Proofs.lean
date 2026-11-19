import DSP.ComplexValue
import DSP.Response.Proofs

noncomputable section
namespace DSP.ComplexValue

def value (a : ComplexValue ℝ) : ℂ := ⟨a.real, a.imag⟩

@[simp] theorem value_ofReal (x : ℝ) : (ofReal x).value = (x : ℂ) := rfl
@[simp] theorem value_add (a b : ComplexValue ℝ) : (add a b).value = a.value + b.value := rfl
@[simp] theorem value_sub (a b : ComplexValue ℝ) : (sub a b).value = a.value - b.value := rfl
@[simp] theorem value_scale (x : ℝ) (a : ComplexValue ℝ) :
    (scale x a).value = (x : ℂ) * a.value := by apply Complex.ext <;> simp [scale, value]
@[simp] theorem value_mul (a b : ComplexValue ℝ) : (mul a b).value = a.value * b.value := rfl
@[simp] theorem value_div (a b : ComplexValue ℝ) : (div a b).value = a.value / b.value := by
  apply Complex.ext <;> simp [div, value, Complex.normSq_apply, div_eq_mul_inv]
  <;> ring
@[simp] theorem value_pow (a : ComplexValue ℝ) (n : ℕ) : (pow a n).value = a.value ^ n := by
  induction n with
  | zero => simp [pow]
  | succ n ih => simp only [pow, value_mul, ih, pow_succ]
@[simp] theorem normSquared_value (a : ComplexValue ℝ) :
    a.normSquared = Complex.normSq a.value := by
  exact Response.normSquared_eq _ _

end DSP.ComplexValue
