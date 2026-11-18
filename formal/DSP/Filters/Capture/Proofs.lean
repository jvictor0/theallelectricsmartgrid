import DSP.Filters.Capture
import DSP.Filters.Tuning.Digital

namespace DSP
open Channels

namespace OnePoleZOHLowPass
theorem prepareCoefficients_state {R : Type} [OfNat R 1] [Sub R] [ExpDecay R]
    (spec : LogicalSpec R) (state : OnePoleZOHLowPass R) :
    (prepareCoefficients spec state).1 = computeCoefficients spec state := rfl

theorem prepareCoefficients_channel {R : Type} [OfNat R 1] [Sub R] [ExpDecay R] {n : ℕ}
    (spec : LogicalSpec (Fin n → R)) (state : OnePoleZOHLowPass (Fin n → R)) (i : Fin n) :
    (channel i (prepareCoefficients spec state).1, channel i (prepareCoefficients spec state).2) =
      prepareCoefficients (channel i spec) (channel i state) := rfl

theorem prepareCoefficients_snapshot (spec : LogicalSpec ℝ) (state : OnePoleZOHLowPass ℝ) :
    (prepareCoefficients spec state).2 = spec.snapshot := rfl
end OnePoleZOHLowPass

namespace OnePoleTPTLowPass
theorem prepareCoefficients_state {R : Type} [OfNat R 1] [Add R] [Div R] [Trig R]
    (spec : LogicalSpec R) (state : OnePoleTPTLowPass R) :
    (prepareCoefficients spec state).1 = computeCoefficients spec state := rfl

theorem prepareCoefficients_channel {R : Type} [OfNat R 1] [Add R] [Div R] [Trig R] {n : ℕ}
    (spec : LogicalSpec (Fin n → R)) (state : OnePoleTPTLowPass (Fin n → R)) (i : Fin n) :
    (channel i (prepareCoefficients spec state).1, channel i (prepareCoefficients spec state).2) =
      prepareCoefficients (channel i spec) (channel i state) := rfl

theorem prepareCoefficients_snapshot (spec : LogicalSpec ℝ) (state : OnePoleTPTLowPass ℝ) :
    (prepareCoefficients spec state).2 = spec.snapshot := rfl
end OnePoleTPTLowPass

namespace OnePoleTPTHighPass
theorem prepareCoefficients_state {R : Type} [OfNat R 1] [Add R] [Div R] [Trig R]
    (spec : LogicalSpec R) (state : OnePoleTPTHighPass R) :
    (prepareCoefficients spec state).1 = computeCoefficients spec state := rfl

theorem prepareCoefficients_channel {R : Type} [OfNat R 1] [Add R] [Div R] [Trig R] {n : ℕ}
    (spec : LogicalSpec (Fin n → R)) (state : OnePoleTPTHighPass (Fin n → R)) (i : Fin n) :
    (channel i (prepareCoefficients spec state).1, channel i (prepareCoefficients spec state).2) =
      prepareCoefficients (channel i spec) (channel i state) := rfl

theorem prepareCoefficients_snapshot (spec : LogicalSpec ℝ) (state : OnePoleTPTHighPass ℝ) :
    (prepareCoefficients spec state).2 = spec.snapshot := rfl
end OnePoleTPTHighPass

namespace TPTBaseWidth
theorem prepareCoefficients_state {R : Type} [OfNat R 1] [Add R] [Div R] [Trig R]
    (spec : LogicalSpec R) (state : TPTBaseWidth R) :
    (prepareCoefficients spec state).1 = computeCoefficients spec state := rfl

theorem prepareCoefficients_channel {R : Type} [OfNat R 1] [Add R] [Div R] [Trig R] {n : ℕ}
    (spec : LogicalSpec (Fin n → R)) (state : TPTBaseWidth (Fin n → R)) (i : Fin n) :
    (channel i (prepareCoefficients spec state).1, channel i (prepareCoefficients spec state).2) =
      prepareCoefficients (channel i spec) (channel i state) := rfl

theorem prepareCoefficients_snapshot (spec : LogicalSpec ℝ) (state : TPTBaseWidth ℝ) :
    (prepareCoefficients spec state).2 = spec.snapshot := rfl
end TPTBaseWidth

namespace TPTSVF
theorem prepareCoefficients_fromSnapshot {R : Type} [OfNat R 1] [Add R] [Mul R] [Div R] [FMA R] [Trig R]
    (spec : LogicalSpec R) (state : TPTSVF R) :
    prepareCoefficients spec state = prepareFromSnapshot ⟨Trig.tanPi spec.cutoff, 1 / spec.q⟩ state := rfl

theorem prepareFromSnapshot_snapshot {R : Type} [OfNat R 1] [Add R] [Mul R] [Div R] [FMA R]
    (snapshot : Snapshot R) (state : TPTSVF R) :
    (prepareFromSnapshot snapshot state).2 = snapshot := rfl

theorem prepareFromSnapshot_preserves_history {R : Type} [OfNat R 1] [Add R] [Mul R] [Div R] [FMA R]
    (snapshot : Snapshot R) (state : TPTSVF R) :
    let next := (prepareFromSnapshot snapshot state).1
    next.ic1 = state.ic1 ∧ next.ic2 = state.ic2 ∧ next.lowPass = state.lowPass ∧
      next.bandPass = state.bandPass ∧ next.highPass = state.highPass ∧
      next.notch = state.notch ∧ next.allPass = state.allPass := ⟨rfl,rfl,rfl,rfl,rfl,rfl,rfl⟩

theorem prepareFromSnapshot_idempotent {R : Type} [OfNat R 1] [Add R] [Mul R] [Div R] [FMA R]
    (snapshot : Snapshot R) (state : TPTSVF R) :
    prepareFromSnapshot snapshot (prepareFromSnapshot snapshot state).1 =
      prepareFromSnapshot snapshot state := rfl

theorem prepareFromSnapshot_channel {R : Type} [OfNat R 1] [Add R] [Mul R] [Div R] [FMA R] {n : ℕ}
    (snapshot : Snapshot (Fin n → R)) (state : TPTSVF (Fin n → R)) (i : Fin n) :
    (channel i (prepareFromSnapshot snapshot state).1, channel i (prepareFromSnapshot snapshot state).2) =
      prepareFromSnapshot (channel i snapshot) (channel i state) := rfl

theorem prepareFromSnapshot_tuned (snapshot : Snapshot ℝ) (state : TPTSVF ℝ)
    (hg : 0 ≤ snapshot.g) (hk : 0 ≤ snapshot.k) :
    (prepareFromSnapshot snapshot state).1.Tuned snapshot.g := by
  have hd : snapshot.g * (snapshot.g + snapshot.k) + 1 ≠ 0 := by positivity
  constructor
  · simp only [prepareFromSnapshot, fma_eq]
    rw [add_comm 1, one_div_mul_cancel hd]
  · rfl
  · rfl

theorem prepareCoefficients_state {R : Type} [OfNat R 1] [Add R] [Mul R] [Div R] [FMA R] [Trig R]
    (spec : LogicalSpec R) (state : TPTSVF R) :
    (prepareCoefficients spec state).1 = computeCoefficients spec state := rfl

theorem prepareCoefficients_channel {R : Type} [OfNat R 1] [Add R] [Mul R] [Div R] [FMA R] [Trig R] {n : ℕ}
    (spec : LogicalSpec (Fin n → R)) (state : TPTSVF (Fin n → R)) (i : Fin n) :
    (channel i (prepareCoefficients spec state).1, channel i (prepareCoefficients spec state).2) =
      prepareCoefficients (channel i spec) (channel i state) := rfl

theorem prepareCoefficients_snapshot (spec : LogicalSpec ℝ) (state : TPTSVF ℝ) :
    (prepareCoefficients spec state).2 = spec.snapshot := rfl
end TPTSVF

namespace MorphingTPTSVF
theorem prepareCoefficients_state {R : Type} [OfNat R 0] [OfNat R 1] [OfNat R 2]
    [Add R] [Sub R] [Mul R] [Div R] [FMA R] [IfLess R] [Trig R]
    (spec : LogicalSpec R) (state : MorphingTPTSVF R) :
    (prepareCoefficients spec state).1 = computeCoefficients spec state := rfl

theorem prepareCoefficients_channel {R : Type} [OfNat R 0] [OfNat R 1] [OfNat R 2]
    [Add R] [Sub R] [Mul R] [Div R] [FMA R] [IfLess R] [Trig R] {n : ℕ}
    (spec : LogicalSpec (Fin n → R)) (state : MorphingTPTSVF (Fin n → R)) (i : Fin n) :
    (channel i (prepareCoefficients spec state).1, channel i (prepareCoefficients spec state).2) =
      prepareCoefficients (channel i spec) (channel i state) := rfl

theorem prepareCoefficients_snapshot (spec : LogicalSpec ℝ) (state : MorphingTPTSVF ℝ)
    (h : spec.Valid) : (prepareCoefficients spec state).2 = spec.snapshot := by
  have hw := computeCoefficients_weights spec state h
  have hs : (prepareCoefficients spec state).2 =
      ⟨spec.svf.snapshot, (stateWeights (computeCoefficients spec state)).low,
        (stateWeights (computeCoefficients spec state)).band,
        (stateWeights (computeCoefficients spec state)).high⟩ := by
    simp only [prepareCoefficients, TPTSVF.prepareCoefficients, stateWeights, upperHalf,
      computeCoefficients, weights, IfLess.ifLess, LawfulIfLess.less]
    simp only [show (0.5 : ℝ) = 1/2 by norm_num, decide_eq_true_eq]
    split_ifs <;> rfl
  rw [hs, hw]
  rfl
end MorphingTPTSVF

namespace TPTSVF4Pole
theorem prepareCoefficients_state {R : Type} [OfNat R 1] [Add R] [Mul R] [Div R] [FMA R] [Trig R]
    (spec : LogicalSpec R) (state : TPTSVF4Pole R) :
    (prepareCoefficients spec state).1 = computeCoefficients spec state := rfl

theorem prepareCoefficients_channel {R : Type} [OfNat R 1] [Add R] [Mul R] [Div R] [FMA R] [Trig R] {n : ℕ}
    (spec : LogicalSpec (Fin n → R)) (state : TPTSVF4Pole (Fin n → R)) (i : Fin n) :
    (channel i (prepareCoefficients spec state).1, channel i (prepareCoefficients spec state).2) =
      prepareCoefficients (channel i spec) (channel i state) := rfl

theorem prepareCoefficients_snapshot (spec : LogicalSpec ℝ) (state : TPTSVF4Pole ℝ) :
    (prepareCoefficients spec state).2 = spec.snapshot := rfl
end TPTSVF4Pole

namespace TPTLadder
theorem prepareCoefficients_state {R : Type} [OfNat R 1] [Add R] [Div R] [Trig R]
    (spec : LogicalSpec R) (state : TPTLadder R) :
    (prepareCoefficients spec state).1 = computeCoefficients spec state := rfl

theorem prepareCoefficients_channel {R : Type} [OfNat R 1] [Add R] [Div R] [Trig R] {n : ℕ}
    (spec : LogicalSpec (Fin n → R)) (state : TPTLadder (Fin n → R)) (i : Fin n) :
    (channel i (prepareCoefficients spec state).1, channel i (prepareCoefficients spec state).2) =
      prepareCoefficients (channel i spec) (channel i state) := rfl

theorem prepareCoefficients_snapshot (spec : LogicalSpec ℝ) (state : TPTLadder ℝ) :
    (prepareCoefficients spec state).2 = spec.snapshot := rfl
end TPTLadder

end DSP
