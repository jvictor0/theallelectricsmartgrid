import DSP.Filters.SquiggleBoyQuadReverb.Frozen.Blocks
import DSP.Filters.SquiggleBoyQuadReverb.SmallSignal

namespace DSP.SquiggleBoyQuadReverb.Frozen

noncomputable section

abbrev History (K : Type) :=
  IntroHistory squiggleBoyReverbIntroOrder K × Tape squiggleBoyReverbDelayCapacity K ×
    Tape squiggleBoyReverbFirstCapacity K × (Lanes K × Lanes K) ×
    Tape squiggleBoyReverbSecondCapacity K × Lanes K

structure Spec (K : Type) where
  intro : IntroTaps squiggleBoyReverbIntroOrder K
  main : Tap squiggleBoyReverbDelayCapacity K
  first : Tap squiggleBoyReverbFirstCapacity K
  second : Tap squiggleBoyReverbSecondCapacity K
  feedback : Lanes K
  satGain : Lanes K
  highAlpha : Lanes K
  lowAlpha : Lanes K

namespace History

def wet {K : Type} (h : History K) : Lanes K := h.2.2.2.2.2

def wetMap {K : Type} [Field K] : History K →ₗ[K] Lanes K :=
  (LinearMap.snd _ _ _).comp <| (LinearMap.snd _ _ _).comp <| (LinearMap.snd _ _ _).comp <|
    (LinearMap.snd _ _ _).comp (LinearMap.snd _ _ _)

@[simp] theorem wetMap_apply {K : Type} [Field K] (h : History K) : wetMap h = h.wet := rfl
end History

def beforeSAT {K : Type} [Field K] [OfScientific K] (p : Spec K) :
    History K × Lanes K →ₗ[K] History K × Lanes K :=
  let state := LinearMap.fst K (History K) (Lanes K)
  let input := LinearMap.snd K (History K) (Lanes K)
  let rest := (LinearMap.snd _ _ _).comp state
  let diffusion := (intro allPassGain _ p.intro).comp (((LinearMap.fst _ _ _).comp state).prod input)
  let write := (LinearMap.snd _ _ _).comp diffusion + (laneGain p.feedback).comp (History.wetMap.comp state)
  let main := push.comp (((LinearMap.fst _ _ _).comp rest).prod write)
  let mixed := mix.comp ((tapRead p.main).comp main)
  let tank := (LinearMap.snd _ _ _).comp rest
  let first := (allPass p.first allPassGain).comp (((LinearMap.fst _ _ _).comp tank).prod mixed)
  let saved := ((LinearMap.fst _ _ _).comp diffusion).prod
    (main.prod (((LinearMap.fst _ _ _).comp first).prod ((LinearMap.snd _ _ _).comp tank)))
  saved.prod ((LinearMap.snd _ _ _).comp first)

def afterSAT {K : Type} [Field K] [OfScientific K] (p : Spec K) :
    History K × Lanes K →ₗ[K] History K :=
  let state := LinearMap.fst K (History K) (Lanes K)
  let input := LinearMap.snd K (History K) (Lanes K)
  let rest := (LinearMap.snd _ _ _).comp state
  let tank := (LinearMap.snd _ _ _).comp rest
  let post := (LinearMap.snd _ _ _).comp tank
  let damp := (damping p.highAlpha p.lowAlpha).comp (((LinearMap.fst _ _ _).comp post).prod input)
  let second := (allPass p.second allPassGain).comp
    (((LinearMap.fst _ _ _).comp ((LinearMap.snd _ _ _).comp post)).prod ((LinearMap.snd _ _ _).comp damp))
  ((LinearMap.fst _ _ _).comp state).prod
    (((LinearMap.fst _ _ _).comp rest).prod
      (((LinearMap.fst _ _ _).comp tank).prod (((LinearMap.fst _ _ _).comp damp).prod second)))

set_option maxRecDepth 2500 in
set_option maxHeartbeats 1200000 in
theorem beforeSAT_apply {K : Type} [Field K] [OfScientific K] (p : Spec K)
    (h : History K) (x : Lanes K) :
    beforeSAT p (h, x) =
      let (diffusion, diffused) := intro allPassGain _ p.intro (h.1, x)
      let main := push (h.2.1, diffused + laneGain p.feedback h.wet)
      let (first, signal) := allPass p.first allPassGain (h.2.2.1, mix (tapRead p.main main))
      ((diffusion, main, first, h.2.2.2), signal) := by
  simp only [beforeSAT, LinearMap.prod_apply]
  rfl

theorem afterSAT_apply {K : Type} [Field K] [OfScientific K] (p : Spec K)
    (h : History K) (x : Lanes K) :
    afterSAT p (h, x) =
      let (mem, signal) := damping p.highAlpha p.lowAlpha (h.2.2.2.1, x)
      let (second, wet) := allPass p.second allPassGain (h.2.2.2.2.1, signal)
      (h.1, h.2.1, h.2.2.1, mem, second, wet) := rfl

/-- Frozen nonlinear audio update. Only the prepared tape addresses are
constant. This still executes the gain and tanh at the production SAT node. -/
def update (p : Spec ℝ) (v : History ℝ × Lanes ℝ) : History ℝ :=
  let signal := beforeSAT p v
  afterSAT p (signal.1, saturate p.satGain signal.2)

def tangent {K : Type} [Field K] [OfScientific K] (p : Spec K) :
    History K × Lanes K →ₗ[K] History K :=
  (afterSAT p).comp (((LinearMap.fst _ _ _).prod
    ((laneGain p.satGain).comp (LinearMap.snd _ _ _))).comp (beforeSAT p))

theorem tangent_apply {K : Type} [Field K] [OfScientific K] (p : Spec K)
    (v : History K × Lanes K) :
    tangent p v =
      let signal := beforeSAT p v
      afterSAT p (signal.1, laneGain p.satGain signal.2) := rfl

theorem update_zero (p : Spec ℝ) : update p 0 = 0 := by
  unfold update
  rw [map_zero]
  have h : saturate p.satGain (0 : Lanes ℝ) = 0 := by
    funext i
    simp [saturate, Tanh.tanh]
  dsimp only
  rw [Prod.snd_zero, h, Prod.fst_zero]
  exact map_zero _

private theorem saturate_lane_derivative (gain : Lanes ℝ) :
    HasFDerivAt (saturate gain) (laneGain gain).toContinuousLinearMap 0 := by
  apply hasFDerivAt_pi'.mpr
  intro i
  let P : Lanes ℝ →L[ℝ] ℝ := ContinuousLinearMap.proj i
  have h := (saturate_hasDerivAt_zero (gain i)).comp_hasFDerivAt 0 P.hasFDerivAt
  convert h using 1 <;> rfl

theorem update_hasFDerivAt (p : Spec ℝ) :
    HasFDerivAt (update p) (tangent p).toContinuousLinearMap 0 := by
  let before := (beforeSAT p).toContinuousLinearMap
  let after := (afterSAT p).toContinuousLinearMap
  let first := (ContinuousLinearMap.fst ℝ (History ℝ) (Lanes ℝ)).comp before
  let second := (ContinuousLinearMap.snd ℝ (History ℝ) (Lanes ℝ)).comp before
  have hSat := saturate_lane_derivative p.satGain
  rw [← map_zero second] at hSat
  have h := first.hasFDerivAt.prodMk (hSat.comp 0 second.hasFDerivAt)
  convert after.hasFDerivAt.comp 0 h using 1 <;> rfl

theorem update_contDiff (p : Spec ℝ) : ContDiff ℝ 1 (update p) := by
  have hs : ContDiff ℝ 1 (saturate p.satGain) := by
    apply contDiff_pi.mpr
    intro i
    change ContDiff ℝ 1 (fun x : Lanes ℝ => Real.tanh (x i * p.satGain i))
    fun_prop
  unfold update
  change ContDiff ℝ 1 (fun v => (afterSAT p).toContinuousLinearMap
    (((beforeSAT p).toContinuousLinearMap v).1, saturate p.satGain (((beforeSAT p).toContinuousLinearMap v).2)))
  fun_prop

theorem update_hasStrictFDerivAt (p : Spec ℝ) :
    HasStrictFDerivAt (update p) (tangent p).toContinuousLinearMap 0 :=
  (update_contDiff p).contDiffAt.hasStrictFDerivAt' (update_hasFDerivAt p) (by norm_num)

end

end DSP.SquiggleBoyQuadReverb.Frozen
