import DSP.Filters.LinkwitzRileySplitter.Runtime

namespace DSP.LinkwitzRileySplitter.Runtime

variable {R : Type} [OfNat R 0]

@[simp] theorem savePhase_restore (t : Topology) (s : PhaseState R t) :
    savePhase t (restorePhase t s) = s := by
  induction t with
  | leaf => cases s; rfl
  | branch low high hl hh =>
    simp only [savePhase, restorePhase, TPTSVF.Runtime.save_restore, hl, hh]

@[simp] theorem saveTree_restore (t : Topology) (s : TreeState R t) :
    saveTree t (restoreTree t s) = s := by
  induction t with
  | leaf => cases s; rfl
  | branch low high hl hh =>
    simp only [saveTree, restoreTree, TPTSVF.Runtime.save_restore, savePhase_restore, hl, hh]

@[simp] theorem save_restore {n : Nat} (s : State n R) : save (restore s) = s := by
  simp only [save, restore, saveTree_restore]

theorem installPhase_save (t : Topology) (c : TreeData (PreparedSection R) t)
    (s : LinkwitzRileySplitter.PhaseState R t) :
    savePhase t (installPhase t c (restorePhase t (savePhase t s))) = savePhase t (installPhase t c s) := by
  induction t with
  | leaf => rfl
  | branch low high hl hh =>
    simp only [installPhase, restorePhase, savePhase, hl, hh,
      installSection, TPTSVF.Runtime.save, TPTSVF.Runtime.restore]

theorem installTree_save (t : Topology) (c : TreeData (PreparedSection R) t)
    (s : LinkwitzRileySplitter.TreeState R t) :
    saveTree t (installTree t c (restoreTree t (saveTree t s))) = saveTree t (installTree t c s) := by
  induction t with
  | leaf => rfl
  | branch low high hl hh =>
    simp only [installTree, restoreTree, saveTree, installPhase_save, hl, hh,
      installSection, TPTSVF.Runtime.save, TPTSVF.Runtime.restore]

theorem install_save {n : Nat} (c : PreparedSpec n R) (s : LinkwitzRileySplitter.State n R) :
    installPrepared c (save s) = save (LinkwitzRileySplitter.installPrepared c s) := by
  simp only [installPrepared, LinkwitzRileySplitter.installPrepared, save, restore, installTree_save]

theorem resetPhase_save (t : Topology) (s : LinkwitzRileySplitter.PhaseState R t) :
    savePhase t (resetPhase t (restorePhase t (savePhase t s))) = savePhase t (resetPhase t s) := by
  induction t with
  | leaf => rfl
  | branch low high hl hh =>
    simp only [resetPhase, restorePhase, savePhase, hl, hh,
      resetSection, TPTSVF.Runtime.save, TPTSVF.Runtime.restore]

theorem resetTree_save (t : Topology) (s : LinkwitzRileySplitter.TreeState R t) :
    saveTree t (resetTree t (restoreTree t (saveTree t s))) = saveTree t (resetTree t s) := by
  induction t with
  | leaf => rfl
  | branch low high hl hh =>
    simp only [resetTree, restoreTree, saveTree, resetPhase_save, hl, hh,
      resetSection, TPTSVF.Runtime.save, TPTSVF.Runtime.restore]

theorem reset_save {n : Nat} (s : LinkwitzRileySplitter.State n R) :
    reset (save s) = save (LinkwitzRileySplitter.reset s) := by
  simp only [reset, LinkwitzRileySplitter.reset, save, restore, resetTree_save]

section Tuning
variable [OfNat R 1] [OfNat R 8] [Add R] [Mul R] [Div R] [FMA R] [Trig R]

theorem tunePhase_save (t : Topology) (c : TreeData R t) (s : LinkwitzRileySplitter.PhaseState R t) :
    savePhase t (tunePhase t c (restorePhase t (savePhase t s))) = savePhase t (tunePhase t c s) := by
  induction t with
  | leaf => rfl
  | branch low high hl hh =>
    simp only [tunePhase, restorePhase, savePhase, hl, hh,
      TPTSVF.computeCoefficients, TPTSVF.Runtime.save, TPTSVF.Runtime.restore]

theorem tuneTree_save (t : Topology) (c : TreeData R t) (s : LinkwitzRileySplitter.TreeState R t) :
    saveTree t (tuneTree t c (restoreTree t (saveTree t s))) = saveTree t (tuneTree t c s) := by
  induction t with
  | leaf => rfl
  | branch low high hl hh =>
    simp only [tuneTree, restoreTree, saveTree, tunePhase_save, hl, hh,
      TPTSVF.computeCoefficients, TPTSVF.Runtime.save, TPTSVF.Runtime.restore]

theorem computeCoefficients_save {n : Nat} (c : LogicalSpec n R) (s : LinkwitzRileySplitter.State n R) :
    computeCoefficients c (save s) = save (LinkwitzRileySplitter.computeCoefficients c s) := by
  simp only [computeCoefficients, LinkwitzRileySplitter.computeCoefficients, save, restore, tuneTree_save]

end Tuning
end DSP.LinkwitzRileySplitter.Runtime
