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
