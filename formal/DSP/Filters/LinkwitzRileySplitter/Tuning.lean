import DSP.Filters.LinkwitzRileySplitter.Realizes
import DSP.Filters.Tuning.History

namespace DSP.LinkwitzRileySplitter

def ValidSpec : (t : Topology) → TreeData ℝ t → Prop
  | .leaf, _ => True
  | .branch low high, s => Tuning.ValidCutoff s.value ∧ ValidSpec low s.low ∧ ValidSpec high s.high

def PhaseAtRest : (t : Topology) → PhaseState ℝ t → Prop
  | .leaf, _ => True
  | .branch low high, f => f.svf.AtRest ∧ PhaseAtRest low f.low ∧ PhaseAtRest high f.high

def TreeAtRest : (t : Topology) → TreeState ℝ t → Prop
  | .leaf, _ => True
  | .branch low high, f => f.first.AtRest ∧ f.second.AtRest ∧
      PhaseAtRest high f.lowPhase ∧ PhaseAtRest low f.highPhase ∧
      TreeAtRest low f.low ∧ TreeAtRest high f.high

noncomputable section

theorem section_ready (cutoff : ℝ) (f : TPTSVF ℝ)
    (hc : Tuning.ValidCutoff cutoff) (hr : f.AtRest) :
    SectionReady (TPTSVF.computeCoefficients (sectionSpec cutoff) f)
      ⟨Trig.tanPi cutoff, 1 / (sectionSpec cutoff).q⟩ := by
  refine ⟨?_, rfl, ⟨hr.ic1, hr.ic2⟩⟩
  exact TPTSVF.computeCoefficients_tuned (sectionSpec cutoff) f ⟨hc, Tuning.butterworth_q_pos⟩

theorem tunePhase_ready (t : Topology) (spec : TreeData ℝ t) (f : PhaseState ℝ t)
    (hs : ValidSpec t spec) (hr : PhaseAtRest t f) :
    PhaseReady t (tunePhase t spec f) (snapshotTree t spec) := by
  induction t with
  | leaf => trivial
  | branch low high hl hh =>
    exact ⟨section_ready spec.value f.svf hs.1 hr.1,
      hl spec.low f.low hs.2.1 hr.2.1, hh spec.high f.high hs.2.2 hr.2.2⟩

theorem tuneTree_ready (t : Topology) (spec : TreeData ℝ t) (f : TreeState ℝ t)
    (hs : ValidSpec t spec) (hr : TreeAtRest t f) :
    TreeReady t (tuneTree t spec f) (snapshotTree t spec) := by
  induction t with
  | leaf => trivial
  | branch low high hl hh =>
    exact ⟨section_ready spec.value f.first hs.1 hr.1,
      section_ready spec.value f.second hs.1 hr.2.1,
      tunePhase_ready high spec.high f.lowPhase hs.2.2 hr.2.2.1,
      tunePhase_ready low spec.low f.highPhase hs.2.1 hr.2.2.2.1,
      hl spec.low f.low hs.2.1 hr.2.2.2.2.1,
      hh spec.high f.high hs.2.2 hr.2.2.2.2.2⟩

theorem snapshotTree_valid (t : Topology) (spec : TreeData ℝ t) (h : ValidSpec t spec) :
    validSnapshot t (snapshotTree t spec) := by
  induction t with
  | leaf => trivial
  | branch low high hl hh =>
    refine ⟨Tuning.warp_pos spec.value h.1, one_div_pos.mpr Tuning.butterworth_q_pos,
      ?_, hl spec.low h.2.1, hh spec.high h.2.2⟩
    change (1 / (Trig.cos2pi (1 / 8) : ℝ))^2 = 2
    rw [div_pow, one_pow, Tuning.butterworth_q_squared]
    norm_num

/-- The public coefficient-preparation path establishes the hypotheses of the
per-band stream theorem; the caller supplies valid cutoffs and zero histories. -/
theorem computeCoefficients_realizes_band {n : Nat} (spec : LogicalSpec n ℝ) (f : State n ℝ)
    (hs : ValidSpec _ spec.tree) (hr : TreeAtRest _ f.tree) (i : Fin n) :
    Bilinear.Realizes step (computeCoefficients spec f) (fun v => v.bands i)
      (bandTransfer _ (mapData (Functor.map Bilinear.const) _ (makeSnapshot spec).tree)
        Bilinear.sigma 1 i.val) :=
  realizes_band _ (makeSnapshot spec) (tuneTree_ready _ spec.tree f.tree hs hr) i

theorem makeSnapshot_reconstruction_flat {n : Nat} (spec : LogicalSpec n ℝ) (hn : 0 < n)
    (h : ValidSpec _ spec.tree) (frequency : ℝ) :
    Complex.normSq (∑ i : Fin n, (makeSnapshot spec).bandResponse frequency i) = 1 :=
  (makeSnapshot spec).reconstruction_flat hn (snapshotTree_valid _ spec.tree h) frequency

end

section History
variable {R : Type} [OfNat R 1] [OfNat R 8] [Add R] [Mul R] [Div R] [FMA R] [Trig R]

def phaseHistory : (t : Topology) → PhaseState R t → List R
  | .leaf, f => [f.x]
  | .branch low high, f => TPTSVF.history f.svf ++ phaseHistory low f.low ++ phaseHistory high f.high

def treeHistory : (t : Topology) → TreeState R t → List R
  | .leaf, f => [f.x]
  | .branch low high, f => TPTSVF.history f.first ++ TPTSVF.history f.second ++
      phaseHistory high f.lowPhase ++ phaseHistory low f.highPhase ++
      treeHistory low f.low ++ treeHistory high f.high

theorem tunePhase_preserves_history (t : Topology) (spec : TreeData R t) (f : PhaseState R t) :
    phaseHistory t (tunePhase t spec f) = phaseHistory t f := by
  induction t with
  | leaf => rfl
  | branch low high hl hh =>
    simp only [tunePhase, phaseHistory, TPTSVF.computeCoefficients_preserves_history, hl, hh]

theorem tuneTree_preserves_history (t : Topology) (spec : TreeData R t) (f : TreeState R t) :
    treeHistory t (tuneTree t spec f) = treeHistory t f := by
  induction t with
  | leaf => rfl
  | branch low high hl hh =>
    simp only [tuneTree, treeHistory, TPTSVF.computeCoefficients_preserves_history,
      tunePhase_preserves_history, hl, hh]

theorem tunePhase_idempotent (t : Topology) (spec : TreeData R t) (f : PhaseState R t) :
    tunePhase t spec (tunePhase t spec f) = tunePhase t spec f := by
  induction t with
  | leaf => rfl
  | branch low high hl hh =>
    simp only [tunePhase, TPTSVF.computeCoefficients_idempotent, hl, hh]

theorem tuneTree_idempotent (t : Topology) (spec : TreeData R t) (f : TreeState R t) :
    tuneTree t spec (tuneTree t spec f) = tuneTree t spec f := by
  induction t with
  | leaf => rfl
  | branch low high hl hh =>
    simp only [tuneTree, TPTSVF.computeCoefficients_idempotent, tunePhase_idempotent, hl, hh]

def history {n : Nat} (f : State n R) : List R := treeHistory _ f.tree ++ List.ofFn f.bands

theorem computeCoefficients_preserves_history {n : Nat} (spec : LogicalSpec n R) (f : State n R) :
    history (computeCoefficients spec f) = history f := by
  simp only [history, computeCoefficients, tuneTree_preserves_history]

theorem computeCoefficients_idempotent {n : Nat} (spec : LogicalSpec n R) (f : State n R) :
    computeCoefficients spec (computeCoefficients spec f) = computeCoefficients spec f := by
  simp only [computeCoefficients, tuneTree_idempotent]

end History
end DSP.LinkwitzRileySplitter
