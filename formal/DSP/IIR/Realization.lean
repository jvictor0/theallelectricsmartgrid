import DSP.Transfer.IIR
import Mathlib.Algebra.Polynomial.Degree.Support
import Mathlib.RingTheory.PowerSeries.NoZeroDivisors

set_option maxHeartbeats 1000000

namespace DSP.IIR

open Polynomial

variable {K : Type} [Field K]

/-- A causal rational function is normalized by its nonzero constant denominator. -/
noncomputable def ofPolynomials (p q : K[X]) : Coefficients p.natDegree q.natDegree K where
  b0 := p.coeff 0 / q.coeff 0
  b i := p.coeff (i.val + 1) / q.coeff 0
  a i := q.coeff (i.val + 1) / q.coeff 0

theorem polynomial_split (p : K[X]) :
    p = C (p.coeff 0) + ∑ i : Fin p.natDegree, C (p.coeff (i.val + 1)) * X ^ (i.val + 1) := by
  calc
    p = ∑ i ∈ Finset.range (p.natDegree + 1), C (p.coeff i) * X ^ i := p.as_sum_range_C_mul_X_pow
    _ = _ := by
      rw [Finset.sum_range_succ']
      simp only [pow_zero, mul_one]
      rw [add_comm]
      congr 1
      exact (Fin.sum_univ_eq_sum_range _ _).symm

theorem ofPolynomials_numerator (p q : K[X]) :
    numerator (ofPolynomials p q) = C ((q.coeff 0)⁻¹) * p := by
  conv_rhs => arg 2; rw [polynomial_split p]
  simp only [numerator, ofPolynomials, div_eq_mul_inv, map_mul,
    mul_add, Finset.mul_sum]
  congr 1
  · ring
  · apply Finset.sum_congr rfl
    intro i _
    ring

theorem ofPolynomials_denominator (p q : K[X]) (hq : q.coeff 0 ≠ 0) :
    denominator (ofPolynomials p q) = C ((q.coeff 0)⁻¹) * q := by
  conv_rhs => arg 2; rw [polynomial_split q]
  simp only [denominator, ofPolynomials, div_eq_mul_inv, map_mul,
    mul_add, Finset.mul_sum]
  rw [← map_mul, inv_mul_cancel₀ hq, map_one]
  congr 1
  apply Finset.sum_congr rfl
  intro i _
  ring

theorem realize_transfer (p q : K[X]) (hq : q.coeff 0 ≠ 0) :
    transfer (ofPolynomials p q) = algebraMap K[X] (RatFunc K) p / algebraMap K[X] (RatFunc K) q := by
  rw [transfer, ofPolynomials_numerator, ofPolynomials_denominator p q hq]
  simp only [map_mul]
  have hc : algebraMap K[X] (RatFunc K) (C ((q.coeff 0)⁻¹)) ≠ 0 := by
    simpa using (inv_ne_zero hq)
  exact mul_div_mul_left _ _ hc

theorem realize_series (p q : K[X]) (hq : q.coeff 0 ≠ 0) (u y : ℕ → K) :
    y = Processor.run (processor (ofPolynomials p q)) u zeroState ↔
      (q : PowerSeries K) * series y = (p : PowerSeries K) * series u := by
  rw [iir_run_iff_recurrence, recurrence_iff_seriesEquation, SeriesEquation,
    ofPolynomials_numerator, ofPolynomials_denominator p q hq]
  simp only [Polynomial.coe_mul, mul_assoc]
  have hc : ((C ((q.coeff 0)⁻¹) : K[X]) : PowerSeries K) ≠ 0 := by
    intro h
    have h0 := congrArg (PowerSeries.coeff 0) h
    exact hq (by simpa using h0)
  exact mul_right_inj' hc

end DSP.IIR
