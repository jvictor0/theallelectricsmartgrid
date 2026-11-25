import DSP.Filters.ButterworthDownsampler.Proofs
import DSP.ResponseFrequency.Proofs
import DSP.Transfer.IIR

noncomputable section

namespace DSP.ButterworthDownsampler

open Polynomial

/-- Numerator and denominator polynomials in one sample of delay. -/
structure Transfer where
  numerator : ℝ[X]
  denominator : ℝ[X]

instance : Mul Transfer where
  mul a b := ⟨a.numerator * b.numerator, a.denominator * b.denominator⟩

def Coefficients.transfer (c : Coefficients ℝ) : Transfer :=
  ⟨IIR.numerator c.toIIR, IIR.denominator c.toIIR⟩

def Cascade.transfer {S : Type} (sectionTransfer : S → Transfer) (c : Cascade S) : Transfer :=
  sectionTransfer c.first * sectionTransfer c.second

def rest (c : Coefficients ℝ) : Section ℝ := ⟨c, 0, 0⟩

def Transfer.Realizes {S : Type} (h : Transfer) (step : Processor S (Sample ℝ) (Sample ℝ))
    (initial : S) : Prop :=
  ∀ input : Nat → Sample ℝ,
    (h.denominator : PowerSeries ℝ) * IIR.series (fun n => (Processor.run step input initial n).x) =
      (h.numerator : PowerSeries ℝ) * IIR.series (fun n => (input n).x)

def tuned2 (cutoff : ℝ) : Coefficients ℝ := prepare2 Real.tan Real.cos Real.pi cutoff

def tuned4 (cutoff : ℝ) : Cascade (Coefficients ℝ) := prepare4 Real.tan Real.cos Real.pi cutoff

def tuned8 (cutoff : ℝ) : FourSections (Coefficients ℝ) := prepare8 Real.tan Real.cos Real.pi cutoff

def transfer2 (cutoff : ℝ) : Transfer := (tuned2 cutoff).transfer

def transfer4 (cutoff : ℝ) : Transfer := (tuned4 cutoff).transfer Coefficients.transfer

def transfer8 (cutoff : ℝ) : Transfer :=
  (tuned8 cutoff).transfer (Cascade.transfer Coefficients.transfer)

def Transfer.response (h : Transfer) (frequency : ℝ) : ℂ :=
  let q := ResponseFrequency.unitDelay frequency
  h.numerator.eval₂ Complex.ofRealHom q / h.denominator.eval₂ Complex.ofRealHom q

def Transfer.power (h : Transfer) (frequency : ℝ) : ℝ := Complex.normSq (h.response frequency)

theorem Transfer.power_mul (a b : Transfer) (frequency : ℝ) :
    (a * b).power frequency = a.power frequency * b.power frequency := by
  change Complex.normSq ((a.numerator * b.numerator).eval₂ _ _ /
    (a.denominator * b.denominator).eval₂ _ _) = _
  simp only [Polynomial.eval₂_mul, ← div_mul_div_comm, Complex.normSq_mul, power, response]

def sectionPowerDenominator (angle ratio : ℝ) : ℝ :=
  1 + 2 * Real.cos (2 * angle) * ratio ^ 2 + ratio ^ 4

end DSP.ButterworthDownsampler
