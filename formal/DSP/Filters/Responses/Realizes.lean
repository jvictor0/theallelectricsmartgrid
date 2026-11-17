import DSP.Filters.Responses.Proofs

/-! The response functions are the transfers of the existing processors, not
independently asserted formulas. These theorems reuse the stream realizations. -/
noncomputable section
namespace DSP
open Polynomial Bilinear

namespace OnePoleTPTLowPass
theorem realizes_snapshot {K : Type} [Field K] (f : OnePoleTPTLowPass K) (g : K)
    (ht : f.G * (1+g) = g) (hr : f.s = 0) :
    Realizes step f (·.y) ((const <$> (⟨g⟩ : Snapshot K)).transfer sigma 1) := by
  apply (realizes f g ht hr).transfer_eq
  simp [Snapshot.transfer, Response.onePole, analog, bil_onePole, Functor.map]
end OnePoleTPTLowPass

namespace OnePoleTPTHighPass
theorem realizes_snapshot {K : Type} [Field K] (f : OnePoleTPTHighPass K) (g : K)
    (ht : f.lowPass.G * (1+g) = g) (hr : f.lowPass.s = 0) :
    Realizes step f (·.y) ((const <$> (⟨g⟩ : Snapshot K)).transfer sigma 1) := by
  apply (realizes f g ht hr).transfer_eq
  simp [Snapshot.transfer, Response.highPole, analog, bil_onePole, Functor.map]
end OnePoleTPTHighPass

namespace TPTSVF

theorem realizes_lowPass_snapshot {K : Type} [Field K] (f : TPTSVF K) (g : K)
    (ht : f.Tuned g) (hr : f.AtRest) :
    Realizes step f (·.lowPass) ((const <$> (⟨g,f.k⟩ : Snapshot K)).lowPassTransfer sigma 1) := by
  apply (realizes_lowPass f g ht hr).transfer_eq
  simp [Snapshot.lowPassTransfer, Snapshot.weightedTransfer, Response.svf,
    analog, lowPassNumerator, bil_denominator, Functor.map, pow_two]

theorem realizes_bandPass_snapshot {K : Type} [Field K] (f : TPTSVF K) (g : K)
    (ht : f.Tuned g) (hr : f.AtRest) :
    Realizes step f (·.bandPass) ((const <$> (⟨g,f.k⟩ : Snapshot K)).bandPassTransfer sigma 1) := by
  apply (realizes_bandPass f g ht hr).transfer_eq
  simp [Snapshot.bandPassTransfer, Snapshot.weightedTransfer, Response.svf,
    analog, bandPassNumerator, bil_denominator, Functor.map, pow_two]

theorem realizes_highPass_snapshot {K : Type} [Field K] (f : TPTSVF K) (g : K)
    (ht : f.Tuned g) (hr : f.AtRest) :
    Realizes step f (·.highPass) ((const <$> (⟨g,f.k⟩ : Snapshot K)).highPassTransfer sigma 1) := by
  apply (realizes_highPass f g ht hr).transfer_eq
  simp [Snapshot.highPassTransfer, Snapshot.weightedTransfer, Response.svf,
    analog, highPassNumerator, bil_denominator, Functor.map, pow_two]

theorem realizes_notch_snapshot {K : Type} [Field K] (f : TPTSVF K) (g : K)
    (ht : f.Tuned g) (hr : f.AtRest) :
    Realizes step f (·.notch) ((const <$> (⟨g,f.k⟩ : Snapshot K)).notchTransfer sigma 1) := by
  apply (realizes_notch f g ht hr).transfer_eq
  simp [Snapshot.notchTransfer, Snapshot.weightedTransfer, Response.svf,
    analog, notchNumerator, bil_denominator, Functor.map, pow_two, add_comm]
theorem realizes_allPass_snapshot {K : Type} [Field K] (f : TPTSVF K) (g : K)
    (ht : f.Tuned g) (hr : f.AtRest) :
    Realizes step f (·.allPass) ((const <$> (⟨g,f.k⟩ : Snapshot K)).allPassTransfer sigma 1) := by
  apply (realizes_allPass f g ht hr).transfer_eq
  simp only [Snapshot.allPassTransfer, Snapshot.weightedTransfer, Response.svf,
    analog, denominator, map_add, map_mul, map_neg, Bilinear.bil_C, Bilinear.bil_X,
    Functor.map, mul_one, one_mul, pow_two]
  congr 1
  ring
end TPTSVF

namespace MorphingTPTSVF

theorem realizes_snapshot {K : Type} [Field K] [IfLess K] [LawfulIfLess K]
    (f : MorphingTPTSVF K) (g : K) (ht : f.svf.Tuned g) (hr : f.svf.AtRest) :
    Realizes step f (·.output) ((const <$> (⟨⟨g,f.svf.k⟩,
      (stateWeights f).low, (stateWeights f).band, (stateWeights f).high⟩ : Snapshot K)).transfer sigma 1) := by
  apply (realizes f g ht hr).transfer_eq
  simp [Snapshot.transfer, TPTSVF.Snapshot.weightedTransfer, Response.svf,
    analog, numerator, TPTSVF.lowPassNumerator, TPTSVF.bandPassNumerator,
    TPTSVF.highPassNumerator, TPTSVF.bil_denominator, Functor.map, pow_two, mul_assoc]
end MorphingTPTSVF

namespace TPTBaseWidth
theorem realizes_snapshot {K : Type} [Field K] (f : TPTBaseWidth K) (g1 g2 : K)
    (ht1 : f.highPass.lowPass.G * (1+g1) = g1) (hr1 : f.highPass.lowPass.s = 0)
    (ht2 : f.lowPass.G * (1+g2) = g2) (hr2 : f.lowPass.s = 0) :
    Realizes step f (·.lowPass.y) ((const <$> (⟨⟨g1⟩,⟨g2⟩⟩ : Snapshot K)).transfer sigma 1) := by
  have first := Realizes.project step OnePoleTPTHighPass.step (·.highPass) (fun _ _ => rfl)
    (OnePoleTPTHighPass.realizes_snapshot f.highPass g1 ht1 hr1)
  have both := Realizes.cascade step OnePoleTPTLowPass.step (·.lowPass) (·.highPass.y)
    (fun _ _ => rfl) first (OnePoleTPTLowPass.realizes_snapshot f.lowPass g2 ht2 hr2)
  simpa [Snapshot.transfer, Functor.map, mul_comm] using both
end TPTBaseWidth

namespace TPTSVF4Pole

theorem realizes_lowPass_snapshot {K : Type} [Field K] (f : TPTSVF4Pole K) (g1 g2 : K)
    (ht1 : f.stage1.Tuned g1) (hr1 : f.stage1.AtRest)
    (ht2 : f.stage2.Tuned g2) (hr2 : f.stage2.AtRest) :
    Realizes stepLowPass f (·.stage2.lowPass)
      ((const <$> (⟨⟨g1,f.stage1.k⟩,⟨g2,f.stage2.k⟩⟩ : Snapshot K)).lowPassTransfer sigma 1) := by
  have first := Realizes.project stepLowPass TPTSVF.step (·.stage1) (fun _ _ => rfl)
    (TPTSVF.realizes_lowPass_snapshot f.stage1 g1 ht1 hr1)
  have both := Realizes.cascade stepLowPass TPTSVF.step (·.stage2) (·.stage1.lowPass)
    (fun _ _ => rfl) first (TPTSVF.realizes_lowPass_snapshot f.stage2 g2 ht2 hr2)
  simpa [Snapshot.lowPassTransfer, Functor.map, mul_comm] using both

theorem realizes_bandPass_snapshot {K : Type} [Field K] (f : TPTSVF4Pole K) (g1 g2 : K)
    (ht1 : f.stage1.Tuned g1) (hr1 : f.stage1.AtRest)
    (ht2 : f.stage2.Tuned g2) (hr2 : f.stage2.AtRest) :
    Realizes stepBandPass f (·.stage2.bandPass)
      ((const <$> (⟨⟨g1,f.stage1.k⟩,⟨g2,f.stage2.k⟩⟩ : Snapshot K)).bandPassTransfer sigma 1) := by
  have first := Realizes.project stepBandPass TPTSVF.step (·.stage1) (fun _ _ => rfl)
    (TPTSVF.realizes_bandPass_snapshot f.stage1 g1 ht1 hr1)
  have both := Realizes.cascade stepBandPass TPTSVF.step (·.stage2) (·.stage1.bandPass)
    (fun _ _ => rfl) first (TPTSVF.realizes_bandPass_snapshot f.stage2 g2 ht2 hr2)
  simpa [Snapshot.bandPassTransfer, Functor.map, mul_comm] using both

theorem realizes_highPass_snapshot {K : Type} [Field K] (f : TPTSVF4Pole K) (g1 g2 : K)
    (ht1 : f.stage1.Tuned g1) (hr1 : f.stage1.AtRest)
    (ht2 : f.stage2.Tuned g2) (hr2 : f.stage2.AtRest) :
    Realizes stepHighPass f (·.stage2.highPass)
      ((const <$> (⟨⟨g1,f.stage1.k⟩,⟨g2,f.stage2.k⟩⟩ : Snapshot K)).highPassTransfer sigma 1) := by
  have first := Realizes.project stepHighPass TPTSVF.step (·.stage1) (fun _ _ => rfl)
    (TPTSVF.realizes_highPass_snapshot f.stage1 g1 ht1 hr1)
  have both := Realizes.cascade stepHighPass TPTSVF.step (·.stage2) (·.stage1.highPass)
    (fun _ _ => rfl) first (TPTSVF.realizes_highPass_snapshot f.stage2 g2 ht2 hr2)
  simpa [Snapshot.highPassTransfer, Functor.map, mul_comm] using both
end TPTSVF4Pole


namespace TPTLadder
theorem realizes_snapshot {K : Type} [Field K] (f : TPTLadder K) (g1 g2 g3 g4 : K)
    (h : f.Ready g1 g2 g3 g4) :
    Realizes step f (·.stage4.y)
      ((const <$> (⟨g1,g2,g3,g4,f.k⟩ : Snapshot K)).transfer sigma 1) := by
  apply (realizes f g1 g2 g3 g4 h).transfer_eq
  simp [Snapshot.transfer, Response.ladder, analog, numerator, denominator, Functor.map]
end TPTLadder
end DSP
