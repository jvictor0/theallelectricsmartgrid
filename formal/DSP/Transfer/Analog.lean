import DSP.Transfer.Basic
import Mathlib.Analysis.SpecialFunctions.ExpDeriv
import Mathlib.Analysis.Complex.RealDeriv

namespace DSP

noncomputable def expMode (s : ℂ) (t : ℝ) : ℂ := Complex.exp (s * (t : ℂ))

@[simp] theorem expMode_zero (s : ℂ) : expMode s 0 = 1 := by simp [expMode]

theorem expMode_hasDerivAt (s : ℂ) (t : ℝ) :
    HasDerivAt (expMode s) (s * expMode s t) t := by
  unfold expMode
  simpa [mul_comm] using ((Complex.ofRealCLM.hasDerivAt (x := t)).const_mul s).cexp

namespace StateSpace

variable {n : ℕ}

/-- Componentwise derivatives are the continuous-time state equation. -/
def AnalogSolution (s : StateSpace n ℂ) (u : ℝ → ℂ)
    (q : ℝ → Fin n → ℂ) (y : ℝ → ℂ) : Prop :=
  (∀ t i, HasDerivAt (fun r => q r i) (s.advance (q t) (u t) i) t) ∧
    ∀ t, y t = s.observe (q t) (u t)

def HasAnalogMode (s : StateSpace n ℂ) (z gain : ℂ) : Prop :=
  ∃ v : Fin n → ℂ,
    s.AnalogSolution (expMode z) (fun t => expMode z t • v) (fun t => gain * expMode z t)

theorem analogMode_iff_equations (s : StateSpace n ℂ) (z gain : ℂ) :
    s.HasAnalogMode z gain ↔ s.ModeEquations z gain := by
  constructor
  · rintro ⟨v, hd, hy⟩
    refine ⟨v, ?_, ?_⟩
    · funext i
      have h := ((expMode_hasDerivAt z 0).mul_const (v i)).unique (hd 0 i)
      simpa [advance_eq] using h
    · simpa [observe_eq] using hy 0
  · rintro ⟨v, hv, hg⟩
    have hstep : s.advance v 1 = z • v := by simpa [advance_eq] using hv.symm
    have hobs : s.observe v 1 = gain := by simpa [observe_eq] using hg.symm
    refine ⟨v, (fun t i => ?_), fun t => ?_⟩
    · have h := (expMode_hasDerivAt z t).mul_const (v i)
      rw [show expMode z t = expMode z t * 1 by simp, advance_smul, hstep]
      simpa [mul_assoc, mul_left_comm] using h
    · rw [show expMode z t = expMode z t * 1 by simp, observe_smul, hobs]
      simp [mul_comm]

theorem analog_hasTransfer (s : StateSpace n ℂ) (z gain : ℂ) (hz : s.RegularAt z) :
    s.HasAnalogMode z gain ↔ gain = s.transfer z :=
  (s.analogMode_iff_equations z gain).trans (s.modeEquations_iff_transfer z gain hz)

end StateSpace
end DSP
