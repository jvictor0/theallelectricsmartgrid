import DSP.Filters.LinkwitzRileySplitter.Prepared
import DSP.Filters.LinkwitzRileySplitter.Tuning
import DSP.Filters.Capture.Proofs

namespace DSP.LinkwitzRileySplitter

section Arithmetic
variable {R : Type} [OfNat R 0] [OfNat R 1] [OfNat R 8]
  [Add R] [Mul R] [Div R] [FMA R] [Trig R]

theorem install_prepare_section (cutoff : R) (s : TPTSVF R) :
    installSection (prepareSection cutoff) s = TPTSVF.computeCoefficients (sectionSpec cutoff) s := rfl

theorem install_prepare_phase (t : Topology) (spec : TreeData R t) (s : PhaseState R t) :
    installPhase t (prepareTree t spec) s = tunePhase t spec s := by
  induction t with
  | leaf => rfl
  | branch low high hl hh => simp only [installPhase, prepareTree, tunePhase,
      install_prepare_section, hl, hh]

theorem install_prepare_tree (t : Topology) (spec : TreeData R t) (s : TreeState R t) :
    installTree t (prepareTree t spec) s = tuneTree t spec s := by
  induction t with
  | leaf => rfl
  | branch low high hl hh => simp only [installTree, prepareTree, tuneTree,
      install_prepare_section, install_prepare_phase, hl, hh]

theorem install_prepare {n : Nat} (spec : LogicalSpec n R) (s : State n R) :
    installPrepared (prepare spec) s = computeCoefficients spec s := by
  simp only [installPrepared, prepare, computeCoefficients, install_prepare_tree]

theorem preparedSnapshotTree_prepare (t : Topology) (spec : TreeData R t) :
    preparedSnapshotTree t (prepareTree t spec) = snapshotTree t spec := by
  induction t with
  | leaf => rfl
  | branch low high hl hh => simp only [preparedSnapshotTree, prepareTree,
      snapshotTree, hl, hh]; rfl

theorem prepare_snapshot {n : Nat} (spec : LogicalSpec n R) :
    (prepare spec).snapshot = makeSnapshot spec := by
  simp only [prepare, PreparedSpec.snapshot, makeSnapshot, preparedSnapshotTree_prepare]
end Arithmetic

theorem resetPhase_atRest (t : Topology) (s : PhaseState ℝ t) : PhaseAtRest t (resetPhase t s) := by
  induction t with
  | leaf => trivial
  | branch low high hl hh => exact ⟨⟨rfl,rfl⟩,hl s.low,hh s.high⟩

theorem resetTree_atRest (t : Topology) (s : TreeState ℝ t) : TreeAtRest t (resetTree t s) := by
  induction t with
  | leaf => trivial
  | branch low high hl hh =>
    exact ⟨⟨rfl,rfl⟩,⟨rfl,rfl⟩,resetPhase_atRest high s.lowPhase,
      resetPhase_atRest low s.highPhase,hl s.low,hh s.high⟩

end DSP.LinkwitzRileySplitter
