import Encoder.ResetTree

namespace Encoder.Reset

-- @cpp mix: zero-depth routes are algebraically neutral, even with changing inputs.
-- @head mix: the recursive evaluator uses the same ordered accumulation.
theorem zero_depth_accumulate (s : MixSums) (r : RouteInput) (h : r.depth = 0) :
    accumulate s r = s := by
  cases s
  simp only [accumulate, h, absRat, maxRat]
  congr <;> grind

theorem zero_routes_fold (routes : List RouteInput) (s : MixSums)
    (h : ∀ r ∈ routes, r.depth = 0) : routes.foldl accumulate s = s := by
  induction routes generalizing s with
  | nil => rfl
  | cons r rs ih =>
    simp only [List.foldl_cons, zero_depth_accumulate s r (h r (by simp))]
    exact ih s (fun x hx => h x (by simp [hx]))

theorem zero_routes_use_base (base : Rat) (routes : List RouteInput)
    (h : ∀ r ∈ routes, r.depth = 0) : mix base routes = base := by
  unfold mix
  rw [zero_routes_fold routes _ h]
  simp only [maxRat]
  grind

theorem zero_gestures_use_base (base : Rat) (gestures : List GestureInput)
    (h : ∀ g ∈ gestures, g.weight = 0) : gestureBase base gestures = base := by
  have fold (gs : List GestureInput) (s : Rat × Rat) (hg : ∀ g ∈ gs, g.weight = 0) :
      gs.foldl (fun s g => (s.1+g.weight, s.2+g.weight*Encoder.blend base g.banked g.weight)) s = s := by
    induction gs generalizing s with
    | nil => rfl
    | cons g rest ih =>
      simp only [List.foldl_cons, hg g (by simp), Rat.add_zero, Rat.zero_mul]
      exact ih s (fun x hx => hg x (by simp [hx]))
  unfold gestureBase
  rw [fold gestures (0,0) h]
  simp

def postGesture (scope : Scope) (n : Node) : Rat :=
  gestureBase (banked scope n.cell.values)
    (n.gestures.map (fun g => ⟨effectiveWeight scope g, banked scope g.cell.values⟩))

theorem cleared_gestures_use_base (scope : Scope) (n : Node)
    (h : ∀ g ∈ n.gestures, effectiveWeight scope g = 0) :
    postGesture scope n = banked scope n.cell.values := by
  apply zero_gestures_use_base
  intro g hg
  rcases List.mem_map.mp hg with ⟨old, ho, rfl⟩
  exact h old ho

-- Pure recursive evaluation visits logical children; a zero-depth child may be
-- skipped/collected without altering the result. Sources/amplitudes are arbitrary.
-- @cpp reset_evaluate: ComputePostGestureValues, Compute and matrix fallback.
-- @head reset_evaluate: recursive cell evaluation and host-frame publication.
def evaluate (curve : Rat → Rat) (inputs : Inputs) (scope : Scope) (lane : Lane) : Tree → Rat
  | .node n children =>
    mix (postGesture scope n) (children.map (fun child =>
      ⟨curve (2 * evaluate curve inputs scope lane child - 1),
       inputs.amplitude (Edits.root child).source lane,
       inputs.source (Edits.root child).source lane⟩))

theorem cleared_node_evaluates (curve : Rat → Rat) (hz : curve 0 = 0)
    (inputs : Inputs) (scope : Scope) (lane : Lane) (n : Node) (children : List Tree)
    (hchildren : ∀ child ∈ children, evaluate curve inputs scope lane child = 1/2)
    (hgestures : ∀ g ∈ n.gestures, effectiveWeight scope g = 0) :
    evaluate curve inputs scope lane (.node n children) = banked scope n.cell.values := by
  rw [evaluate]
  rw [zero_routes_use_base]
  · exact cleared_gestures_use_base scope n hgestures
  · intro r hr
    rcases List.mem_map.mp hr with ⟨child, hc, rfl⟩
    dsimp only
    rw [hchildren child hc]
    have he : (2 : Rat)*(1/2)-1 = 0 := by grind
    rw [he, hz]

theorem zeroed_tree_settles (t : Timing) (scope : Scope) (curve : Rat → Rat)
    (hz : curve 0 = 0) (inputs : Inputs) (lane : Lane) (tree : Tree) (h : TreeReady t tree) :
    evaluate curve inputs scope lane (advanceTree t t.maximum (zeroTree t scope tree)) = 1/2 := by
  revert h
  induction tree using treeInduction with
  | step n children ih =>
    intro h
    simp only [TreeReady] at h
    simp only [advanceTree, zeroTree, Edits.mapAll]
    change evaluate curve inputs scope lane
      (.node (advanceNode t t.maximum (resetNode t scope true (1/2) n))
        ((children.map (zeroTree t scope)).map (advanceTree t t.maximum))) = 1/2
    rw [cleared_node_evaluates curve hz]
    · exact reset_base_settles t scope true (1/2) n h.1 (by unfold UnitInterval; grind)
    · intro child hc
      rcases List.mem_map.mp hc with ⟨reset, hr, rfl⟩
      rcases List.mem_map.mp hr with ⟨old, ho, rfl⟩
      exact ih old ho (h.2 old ho)
    · exact advanced_reset_gestures_zero t scope true (1/2) n

theorem reset_tree_returns_default (t : Timing) (scope : Scope) (b : Bool) (d : DefaultValue)
    (curve : Rat → Rat) (hz : curve 0 = 0) (inputs : Inputs) (lane : Lane)
    (tree : Tree) (h : TreeReady t tree) :
    evaluate curve inputs scope lane (advanceTree t t.maximum (resetTree t scope b d tree)) = d.value := by
  cases tree with
  | node n children =>
    simp only [TreeReady] at h
    simp only [advanceTree, zeroTree, resetTree, Edits.mapAll]
    change evaluate curve inputs scope lane
      (.node (advanceNode t t.maximum (resetNode t scope b d.value n))
        ((children.map (zeroTree t scope)).map (advanceTree t t.maximum))) = d.value
    rw [cleared_node_evaluates curve hz]
    · exact reset_base_settles t scope b d.value n h.1 d.normalized
    · intro child hc
      rcases List.mem_map.mp hc with ⟨reset, hr, rfl⟩
      rcases List.mem_map.mp hr with ⟨old, ho, rfl⟩
      exact zeroed_tree_settles t scope curve hz inputs lane old (h.2 old ho)
    · exact advanced_reset_gestures_zero t scope b d.value n

-- @cpp reset_collect: CanBeGarbageCollected/AllNeutral; logical silence, not pointer safety.
-- @head reset_collect: the same removal predicates.
theorem collectable_modulator_silent (curve : Rat → Rat) (inputs : Inputs) (scope : Scope)
    (lane : Lane) (n : Node) (hn : ∀ i, n.cell.values i = 1/2) (hg : n.gestures = []) :
    evaluate curve inputs scope lane (.node n []) = 1/2 := by
  simp only [evaluate, List.map_nil, no_modulators_uses_base, postGesture, hg, List.map_nil,
    no_gestures_uses_base, banked, hn, Encoder.blend]
  grind

theorem inactive_gesture_weight_zero (scope : Scope) (g : GestureState)
    (h : ∀ i, g.active i = false) : effectiveWeight scope g = 0 := by
  simp [effectiveWeight, banked, h, Encoder.blend]

-- Erasing a silent route preserves ordered accumulation at any position.
theorem erase_zero_route (base : Rat) (before after : List RouteInput) (r : RouteInput)
    (h : r.depth = 0) : mix base (before ++ r::after) = mix base (before ++ after) := by
  simp only [mix, List.foldl_append, List.foldl_cons, zero_depth_accumulate _ r h]

theorem erase_zero_gesture (base : Rat) (before after : List GestureInput) (g : GestureInput)
    (h : g.weight = 0) : gestureBase base (before ++ g::after) = gestureBase base (before ++ after) := by
  simp only [gestureBase, List.foldl_append, List.foldl_cons, h, Rat.add_zero, Rat.zero_mul]

theorem collected_neutral_has_no_pending_target (t : Timing) (n : Node) (h : Ready t n.cell)
    (done : n.cell.remaining = 0) (neutral : ∀ i, n.cell.values i = 1/2) :
    ∀ i, n.cell.targets i = 1/2 := by
  intro i
  rw [← h.2.2 done]
  exact neutral i

-- @cpp publish: the final oversample reaches the newly evaluated default.
-- @head publish: identical observable publication, including bipolar conversion.
theorem both_publish_reset_default (t : Timing) (scope : Scope) (b : Bool) (d : DefaultValue)
    (inputs : Inputs) (tree : Tree) (h : TreeReady t tree) (old : Output) (host ratio : Nat)
    (lane : Lane) :
    let values := fun lane => evaluate Curve.approximate inputs scope lane
      (advanceTree t t.maximum (resetTree t scope b d tree))
    Encoder.publish (setOutput old host values) host ratio b (ratio-1) lane = toValue b d.value ∧
    Encoder.Head.publish (setOutput old host values) host ratio b (ratio-1) lane = toValue b d.value := by
  dsimp only
  have hv : (fun lane => evaluate Curve.approximate inputs scope lane
      (advanceTree t t.maximum (resetTree t scope b d tree))) = (fun _ => d.value) := by
    funext i
    exact reset_tree_returns_default t scope b d Curve.approximate Curve.approximate_zero inputs i tree h
  rw [hv, Encoder.Head.publication_equals_working_tree]
  have hp := last_slot_current (setOutput old host (fun _ => d.value)) host ratio b lane
  exact ⟨hp,hp⟩

end Encoder.Reset
