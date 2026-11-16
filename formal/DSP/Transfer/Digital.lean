import DSP.Transfer.Basic

namespace DSP.StateSpace

open Matrix

variable {n : ℕ} {K : Type} [Field K]

/-- A geometric state/output trajectory for a geometric input. -/
def HasDigitalMode (s : StateSpace n K) (z gain : K) : Prop :=
  ∃ v : Fin n → K, ∀ k : ℕ,
    z ^ (k + 1) • v = s.advance (z ^ k • v) (z ^ k) ∧
    gain * z ^ k = s.observe (z ^ k • v) (z ^ k)

theorem digitalMode_iff_equations (s : StateSpace n K) (z gain : K) :
    s.HasDigitalMode z gain ↔ s.ModeEquations z gain := by
  constructor
  · rintro ⟨v, hv⟩
    refine ⟨v, ?_, ?_⟩
    · simpa [advance_eq] using (hv 0).1
    · simpa [observe_eq] using (hv 0).2
  · rintro ⟨v, hv, hg⟩
    refine ⟨v, fun k => ⟨?_, ?_⟩⟩
    · rw [pow_succ, mul_smul, hv, smul_add, advance_eq, mulVec_smul]
      congr 1
      funext i
      simp [mul_comm]
    · rw [observe_eq, dotProduct_smul, hg]
      simp [smul_eq_mul]
      ring

theorem digital_hasTransfer (s : StateSpace n K) (z gain : K) (hz : s.RegularAt z) :
    s.HasDigitalMode z gain ↔ gain = s.transfer z :=
  (s.digitalMode_iff_equations z gain).trans (s.modeEquations_iff_transfer z gain hz)

end DSP.StateSpace
