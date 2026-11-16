import DSP.IIR.Proofs
import Mathlib.RingTheory.PowerSeries.Basic
import Mathlib.FieldTheory.RatFunc.Basic

namespace DSP.IIR

open Polynomial

variable {m n : ℕ} {R : Type} [CommRing R]

noncomputable def numerator (c : Coefficients m n R) : R[X] :=
  C c.b0 + ∑ i, C (c.b i) * X ^ (i.val + 1)

noncomputable def denominator (c : Coefficients m n R) : R[X] :=
  1 + ∑ i, C (c.a i) * X ^ (i.val + 1)

@[simp] theorem denominator_constant (c : Coefficients m n R) :
    (denominator c).coeff 0 = 1 := by simp [denominator]

noncomputable def transfer {K : Type} [Field K] (c : Coefficients m n K) : RatFunc K :=
  algebraMap K[X] (RatFunc K) (numerator c) / algebraMap K[X] (RatFunc K) (denominator c)

def series (u : ℕ → R) : PowerSeries R := PowerSeries.mk u

def SeriesEquation (c : Coefficients m n R) (u y : ℕ → R) : Prop :=
  (denominator c : PowerSeries R) * series y = (numerator c : PowerSeries R) * series u

theorem coeff_delay (a : R) (k t : ℕ) (u : ℕ → R) :
    PowerSeries.coeff t ((PowerSeries.C a * PowerSeries.X ^ k) * series u) = a * delay k u t := by
  rw [mul_assoc, PowerSeries.coeff_C_mul, PowerSeries.coeff_X_pow_mul']
  simp only [series, PowerSeries.coeff_mk, delay]

theorem polynomial_series_sum {ι : Type} [Fintype ι] (p : ι → R[X]) :
    ((∑ i, p i : R[X]) : PowerSeries R) = ∑ i, (p i : PowerSeries R) := by
  exact map_sum (Polynomial.coeToPowerSeries.ringHom (R := R)) p Finset.univ

theorem coeff_numerator_mul (c : Coefficients m n R) (u : ℕ → R) (t : ℕ) :
    PowerSeries.coeff t ((numerator c : PowerSeries R) * series u) =
      c.b0 * u t + ∑ i, c.b i * delay (i.val + 1) u t := by
  simp [numerator, polynomial_series_sum, add_mul, Finset.sum_mul,
    series]
  exact Finset.sum_congr rfl (fun i _ => coeff_delay _ _ _ _)

theorem coeff_denominator_mul (c : Coefficients m n R) (y : ℕ → R) (t : ℕ) :
    PowerSeries.coeff t ((denominator c : PowerSeries R) * series y) =
      y t + ∑ i, c.a i * delay (i.val + 1) y t := by
  simp [denominator, polynomial_series_sum, add_mul, Finset.sum_mul,
    series]
  exact Finset.sum_congr rfl (fun i _ => coeff_delay _ _ _ _)

theorem recurrence_iff_seriesEquation (c : Coefficients m n R) (u y : ℕ → R) :
    SatisfiesDifferenceEquation c u y ↔ SeriesEquation c u y := by
  rw [SeriesEquation, PowerSeries.ext_iff]
  simp only [coeff_denominator_mul, coeff_numerator_mul, SatisfiesDifferenceEquation]
  apply forall_congr'
  intro t
  exact eq_sub_iff_add_eq

theorem iir_hasTransfer (c : Coefficients m n R) (u : ℕ → R) :
    SeriesEquation c u (Processor.run (processor c) u zeroState) :=
  (recurrence_iff_seriesEquation c u _).mp (iir_run_correct c u)

end DSP.IIR
