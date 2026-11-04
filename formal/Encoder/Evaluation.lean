import Encoder.Core

namespace Encoder

-- @cpp mix: BankedEncoderCell::Modulators::Compute and EncoderMatrix::ProcessGraph
def absRat (x : Rat) : Rat := if x < 0 then -x else x
def maxRat (a b : Rat) : Rat := if a < b then b else a

structure RouteInput where
  depth : Rat
  amplitude : Rat
  source : Rat

structure MixSums where
  value : Rat
  weight : Rat
  offset : Rat
  deriving Inhabited, DecidableEq

def accumulate (s : MixSums) (r : RouteInput) : MixSums :=
  let w := r.depth * r.amplitude
  ⟨s.value + w * r.source, s.weight + absRat w, s.offset + maxRat 0 (-w)⟩

def mix (base : Rat) (routes : List RouteInput) : Rat :=
  let s := routes.foldl accumulate ⟨0, 0, 0⟩
  base * maxRat 0 (1 - s.weight) + (s.value + s.offset) / maxRat 1 s.weight

theorem no_modulators_uses_base (base : Rat) : mix base [] = base := by
  simp only [mix, List.foldl_nil, maxRat]
  grind

theorem zero_amplitude_neutral (s : MixSums) (d x : Rat) :
    accumulate s ⟨d, 0, x⟩ = s := by
  cases s
  simp only [accumulate, absRat, maxRat]
  congr <;> grind

theorem full_positive_modulation (base source : Rat) :
    mix base [⟨1, 1, source⟩] = source := by
  simp only [mix, List.foldl_cons, List.foldl_nil, accumulate, absRat, maxRat]
  grind

theorem full_negative_modulation (base source : Rat) :
    mix base [⟨-1, 1, source⟩] = 1-source := by
  simp only [mix, List.foldl_cons, List.foldl_nil, accumulate, absRat, maxRat]
  grind

-- @head mix: identical ordered sums and normalization in the recursive evaluator.
theorem partial_negative_modulation (base source weight : Rat) (hw : UnitInterval weight) :
    mix base [⟨-weight, 1, source⟩] = base*(1-weight) + weight*(1-source) := by
  simp only [mix, List.foldl_cons, List.foldl_nil, accumulate, absRat, maxRat]
  grind [UnitInterval]


def MixValid (s : MixSums) : Prop :=
  0 ≤ s.weight ∧ 0 ≤ s.value + s.offset ∧ s.value + s.offset ≤ s.weight

theorem route_contribution_bounded (w x : Rat) (hx : UnitInterval x) :
    0 ≤ w*x + maxRat 0 (-w) ∧ w*x + maxRat 0 (-w) ≤ absRat w := by
  rcases hx with ⟨h0, h1⟩
  by_cases hw : w < 0
  · have wa : 0 ≤ -w := by grind
    have xb : 0 ≤ 1-x := by grind
    have p := Rat.mul_nonneg wa h0
    have q := Rat.mul_nonneg wa xb
    simp only [maxRat, absRat, hw, ↓reduceIte]
    grind
  · have wa : 0 ≤ w := by grind
    have xb : 0 ≤ 1-x := by grind
    have p := Rat.mul_nonneg wa h0
    have q := Rat.mul_nonneg wa xb
    simp only [maxRat, absRat, hw, ↓reduceIte]
    grind

theorem accumulate_valid (s : MixSums) (r : RouteInput) (hs : MixValid s)
    (hr : UnitInterval r.source) : MixValid (accumulate s r) := by
  have h := route_contribution_bounded (r.depth*r.amplitude) r.source hr
  have wa : 0 ≤ absRat (r.depth*r.amplitude) := by unfold absRat; split <;> grind
  unfold MixValid accumulate at *
  grind

theorem fold_valid (routes : List RouteInput) (s : MixSums) (hs : MixValid s)
    (hr : ∀ r ∈ routes, UnitInterval r.source) : MixValid (routes.foldl accumulate s) := by
  induction routes generalizing s with
  | nil => exact hs
  | cons r rs ih =>
    apply ih (accumulate s r) (accumulate_valid s r hs (hr r (by simp)))
    intro r' h; exact hr r' (by simp [h])

theorem mix_bounded (base : Rat) (routes : List RouteInput) (hb : UnitInterval base)
    (hr : ∀ r ∈ routes, UnitInterval r.source) : UnitInterval (mix base routes) := by
  have hs := fold_valid routes ⟨0,0,0⟩ (by unfold MixValid; grind) hr
  unfold mix
  generalize routes.foldl accumulate ⟨0,0,0⟩ = s at *
  dsimp only
  rcases hs with ⟨hw, hlo, hhi⟩
  rcases hb with ⟨hb0, hb1⟩
  by_cases small : s.weight ≤ 1
  · have a : 0 ≤ 1-s.weight := by grind
    have b : 0 ≤ 1-base := by grind
    have p := Rat.mul_nonneg hb0 a
    have q := Rat.mul_nonneg b a
    have ma : maxRat 0 (1-s.weight) = 1-s.weight := by unfold maxRat; split <;> grind
    have mb : maxRat 1 s.weight = 1 := by unfold maxRat; split <;> grind
    have divOne : (s.value+s.offset)/(1 : Rat) = s.value+s.offset := by grind
    rw [ma, mb, divOne]
    unfold UnitInterval
    grind
  · have pos : 0 < s.weight := by grind
    have lo : ¬ (s.value+s.offset)/s.weight < 0 := by
      rw [Rat.div_lt_iff pos]; grind
    have hi : ¬ 1 < (s.value+s.offset)/s.weight := by
      rw [Rat.lt_div_iff pos]; grind
    have ma : maxRat 0 (1-s.weight) = 0 := by unfold maxRat; split <;> grind
    have mb : maxRat 1 s.weight = s.weight := by unfold maxRat; split <;> grind
    rw [ma, mb, Rat.mul_zero, Rat.zero_add]
    unfold UnitInterval
    grind

-- @cpp gestures: ComputePostGestureValues and SetEffectiveModulatorWeight.
-- @head gestures: shared arithmetic/control operation at the pinned revision.
-- `banked` is the scene-interpolated child value, deliberately not its output.
structure GestureInput where
  weight : Rat
  banked : Rat

def gestureBase (base : Rat) (gestures : List GestureInput) : Rat :=
  let totals := gestures.foldl (fun (s : Rat × Rat) g =>
    (s.1 + g.weight, s.2 + g.weight * blend base g.banked g.weight)) (0, 0)
  if totals.1 > 0 then totals.2 / totals.1 else base

theorem no_gestures_uses_base (base : Rat) : gestureBase base [] = base := by
  simp [gestureBase]

theorem held_gesture_uses_banked (base target : Rat) :
    gestureBase base [⟨1, target⟩] = target := by
  simp only [gestureBase, List.foldl_cons, List.foldl_nil, blend]
  grind

-- @cpp route_depth: EncoderMatrix::Route::UpdateDepth and Process.
-- @head route_depth: shared arithmetic/control operation at the pinned revision.
-- Curve is a parameter: this does not axiomatize or prove the C++ exp approximation.
def routeDepth (curve : Rat → Rat) (left right t : Rat) : Rat :=
  curve (2 * blend left right t - 1)

theorem settled_depth_cache_valid (curve : Rat → Rat) (l r oldBlend newBlend : Rat)
    (h : newBlend = oldBlend) :
    routeDepth curve l r newBlend = routeDepth curve l r oldBlend := by rw [h]

-- @cpp scene_edit: StateEncoderCell::IncrementInternal.
-- @head scene_edit: shared arithmetic/control operation at the pinned revision.
-- These are the compensated branches after one scene hits its bound.
def compensateRight (wanted left t : Rat) : Rat := (wanted - left * (1-t)) / t
def compensateLeft (wanted right t : Rat) : Rat := (wanted - right*t) / (1-t)

theorem right_compensation_preserves_blend (wanted left t : Rat) (ht : t ≠ 0) :
    blend left (compensateRight wanted left t) t = wanted := by
  unfold blend compensateRight
  grind

theorem left_compensation_preserves_blend (wanted right t : Rat) (ht : 1-t ≠ 0) :
    blend (compensateLeft wanted right t) right t = wanted := by
  unfold blend compensateLeft
  grind

end Encoder
