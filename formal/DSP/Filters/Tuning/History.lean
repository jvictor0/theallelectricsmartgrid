import DSP.Filters.Tuning.Proofs

namespace DSP
open Channels

namespace OnePoleZOHLowPass
instance logicalSpecFunctor : Functor LogicalSpec where
  map f s := ⟨f s.cutoff⟩

def history {R : Type} (s : OnePoleZOHLowPass R) : List R :=
  [s.y]

theorem computeCoefficients_preserves_history {R : Type} [OfNat R 1] [Sub R] [ExpDecay R]
    (spec : LogicalSpec R) (state : OnePoleZOHLowPass R) :
    history (computeCoefficients spec state) = history state := rfl

theorem computeCoefficients_idempotent {R : Type} [OfNat R 1] [Sub R] [ExpDecay R]
    (spec : LogicalSpec R) (state : OnePoleZOHLowPass R) :
    computeCoefficients spec (computeCoefficients spec state) = computeCoefficients spec state := rfl

theorem computeCoefficients_channel {R : Type} [OfNat R 1] [Sub R] [ExpDecay R] {n : ℕ}
    (spec : LogicalSpec (Fin n → R)) (state : OnePoleZOHLowPass (Fin n → R)) (i : Fin n) :
    channel i (computeCoefficients spec state) =
      computeCoefficients (channel i spec) (channel i state) := rfl
end OnePoleZOHLowPass

namespace OnePoleTPTLowPass
instance logicalSpecFunctor : Functor LogicalSpec where
  map f s := ⟨f s.cutoff⟩

def history {R : Type} (s : OnePoleTPTLowPass R) : List R :=
  [s.s, s.y]

theorem computeCoefficients_preserves_history {R : Type} [OfNat R 1] [Add R] [Div R] [Trig R]
    (spec : LogicalSpec R) (state : OnePoleTPTLowPass R) :
    history (computeCoefficients spec state) = history state := rfl

theorem computeCoefficients_idempotent {R : Type} [OfNat R 1] [Add R] [Div R] [Trig R]
    (spec : LogicalSpec R) (state : OnePoleTPTLowPass R) :
    computeCoefficients spec (computeCoefficients spec state) = computeCoefficients spec state := rfl

theorem computeCoefficients_channel {R : Type} [OfNat R 1] [Add R] [Div R] [Trig R] {n : ℕ}
    (spec : LogicalSpec (Fin n → R)) (state : OnePoleTPTLowPass (Fin n → R)) (i : Fin n) :
    channel i (computeCoefficients spec state) =
      computeCoefficients (channel i spec) (channel i state) := rfl
end OnePoleTPTLowPass

namespace OnePoleTPTHighPass
instance logicalSpecFunctor : Functor LogicalSpec where
  map f s := ⟨f s.cutoff⟩

def history {R : Type} (s : OnePoleTPTHighPass R) : List R :=
  OnePoleTPTLowPass.history s.lowPass ++ [s.y]

theorem computeCoefficients_preserves_history {R : Type} [OfNat R 1] [Add R] [Div R] [Trig R]
    (spec : LogicalSpec R) (state : OnePoleTPTHighPass R) :
    history (computeCoefficients spec state) = history state := rfl

theorem computeCoefficients_idempotent {R : Type} [OfNat R 1] [Add R] [Div R] [Trig R]
    (spec : LogicalSpec R) (state : OnePoleTPTHighPass R) :
    computeCoefficients spec (computeCoefficients spec state) = computeCoefficients spec state := rfl

theorem computeCoefficients_channel {R : Type} [OfNat R 1] [Add R] [Div R] [Trig R] {n : ℕ}
    (spec : LogicalSpec (Fin n → R)) (state : OnePoleTPTHighPass (Fin n → R)) (i : Fin n) :
    channel i (computeCoefficients spec state) =
      computeCoefficients (channel i spec) (channel i state) := rfl
end OnePoleTPTHighPass

namespace TPTBaseWidth
instance logicalSpecFunctor : Functor LogicalSpec where
  map f s := ⟨f <$> s.highPass, f <$> s.lowPass⟩

def history {R : Type} (s : TPTBaseWidth R) : List R :=
  OnePoleTPTHighPass.history s.highPass ++ OnePoleTPTLowPass.history s.lowPass

theorem computeCoefficients_preserves_history {R : Type} [OfNat R 1] [Add R] [Div R] [Trig R]
    (spec : LogicalSpec R) (state : TPTBaseWidth R) :
    history (computeCoefficients spec state) = history state := rfl

theorem computeCoefficients_idempotent {R : Type} [OfNat R 1] [Add R] [Div R] [Trig R]
    (spec : LogicalSpec R) (state : TPTBaseWidth R) :
    computeCoefficients spec (computeCoefficients spec state) = computeCoefficients spec state := rfl

theorem computeCoefficients_channel {R : Type} [OfNat R 1] [Add R] [Div R] [Trig R] {n : ℕ}
    (spec : LogicalSpec (Fin n → R)) (state : TPTBaseWidth (Fin n → R)) (i : Fin n) :
    channel i (computeCoefficients spec state) =
      computeCoefficients (channel i spec) (channel i state) := rfl
end TPTBaseWidth

namespace TPTSVF
instance logicalSpecFunctor : Functor LogicalSpec where
  map f s := ⟨f s.cutoff, f s.q⟩

def history {R : Type} (s : TPTSVF R) : List R :=
  [s.ic1, s.ic2, s.lowPass, s.bandPass, s.highPass, s.notch, s.allPass]

theorem computeCoefficients_preserves_history {R : Type} [OfNat R 1] [Add R] [Mul R] [Div R] [FMA R] [Trig R]
    (spec : LogicalSpec R) (state : TPTSVF R) :
    history (computeCoefficients spec state) = history state := rfl

theorem computeCoefficients_idempotent {R : Type} [OfNat R 1] [Add R] [Mul R] [Div R] [FMA R] [Trig R]
    (spec : LogicalSpec R) (state : TPTSVF R) :
    computeCoefficients spec (computeCoefficients spec state) = computeCoefficients spec state := rfl

theorem computeCoefficients_channel {R : Type} [OfNat R 1] [Add R] [Mul R] [Div R] [FMA R] [Trig R] {n : ℕ}
    (spec : LogicalSpec (Fin n → R)) (state : TPTSVF (Fin n → R)) (i : Fin n) :
    channel i (computeCoefficients spec state) =
      computeCoefficients (channel i spec) (channel i state) := rfl
end TPTSVF

namespace MorphingTPTSVF
instance logicalSpecFunctor : Functor LogicalSpec where
  map f s := ⟨f <$> s.svf, f s.blend⟩

def history {R : Type} (s : MorphingTPTSVF R) : List R :=
  TPTSVF.history s.svf ++ [s.output]

theorem computeCoefficients_preserves_history {R : Type} [OfNat R 1] [OfNat R 2] [Add R] [Sub R]
    [Mul R] [Div R] [FMA R] [IfLess R] [Trig R]
    (spec : LogicalSpec R) (state : MorphingTPTSVF R) :
    history (computeCoefficients spec state) = history state := rfl

theorem computeCoefficients_idempotent {R : Type} [OfNat R 1] [OfNat R 2] [Add R] [Sub R]
    [Mul R] [Div R] [FMA R] [IfLess R] [Trig R]
    (spec : LogicalSpec R) (state : MorphingTPTSVF R) :
    computeCoefficients spec (computeCoefficients spec state) = computeCoefficients spec state := rfl

theorem computeCoefficients_channel {R : Type} [OfNat R 1] [OfNat R 2] [Add R] [Sub R]
    [Mul R] [Div R] [FMA R] [IfLess R] [Trig R] {n : ℕ}
    (spec : LogicalSpec (Fin n → R)) (state : MorphingTPTSVF (Fin n → R)) (i : Fin n) :
    channel i (computeCoefficients spec state) =
      computeCoefficients (channel i spec) (channel i state) := rfl
end MorphingTPTSVF

namespace TPTSVF4Pole
instance logicalSpecFunctor : Functor LogicalSpec where
  map f s := ⟨f <$> s.stage1, f <$> s.stage2⟩

def history {R : Type} (s : TPTSVF4Pole R) : List R :=
  TPTSVF.history s.stage1 ++ TPTSVF.history s.stage2

theorem computeCoefficients_preserves_history {R : Type} [OfNat R 1] [Add R] [Mul R] [Div R] [FMA R] [Trig R]
    (spec : LogicalSpec R) (state : TPTSVF4Pole R) :
    history (computeCoefficients spec state) = history state := rfl

theorem computeCoefficients_idempotent {R : Type} [OfNat R 1] [Add R] [Mul R] [Div R] [FMA R] [Trig R]
    (spec : LogicalSpec R) (state : TPTSVF4Pole R) :
    computeCoefficients spec (computeCoefficients spec state) = computeCoefficients spec state := rfl

theorem computeCoefficients_channel {R : Type} [OfNat R 1] [Add R] [Mul R] [Div R] [FMA R] [Trig R] {n : ℕ}
    (spec : LogicalSpec (Fin n → R)) (state : TPTSVF4Pole (Fin n → R)) (i : Fin n) :
    channel i (computeCoefficients spec state) =
      computeCoefficients (channel i spec) (channel i state) := rfl
end TPTSVF4Pole

namespace TPTLadder
instance logicalSpecFunctor : Functor LogicalSpec where
  map f s := ⟨f s.cutoff, f s.feedback⟩

def history {R : Type} (s : TPTLadder R) : List R :=
  [s.drive] ++ OnePoleTPTLowPass.history s.stage1 ++ OnePoleTPTLowPass.history s.stage2 ++ OnePoleTPTLowPass.history s.stage3 ++ OnePoleTPTLowPass.history s.stage4

theorem computeCoefficients_preserves_history {R : Type} [OfNat R 1] [Add R] [Div R] [Trig R]
    (spec : LogicalSpec R) (state : TPTLadder R) :
    history (computeCoefficients spec state) = history state := rfl

theorem computeCoefficients_idempotent {R : Type} [OfNat R 1] [Add R] [Div R] [Trig R]
    (spec : LogicalSpec R) (state : TPTLadder R) :
    computeCoefficients spec (computeCoefficients spec state) = computeCoefficients spec state := rfl

theorem computeCoefficients_channel {R : Type} [OfNat R 1] [Add R] [Div R] [Trig R] {n : ℕ}
    (spec : LogicalSpec (Fin n → R)) (state : TPTLadder (Fin n → R)) (i : Fin n) :
    channel i (computeCoefficients spec state) =
      computeCoefficients (channel i spec) (channel i state) := rfl
end TPTLadder

end DSP
