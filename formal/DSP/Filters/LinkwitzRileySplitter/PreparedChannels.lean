import DSP.Filters.LinkwitzRileySplitter.Prepared
import DSP.Filters.LinkwitzRileySplitter.Channels

/-! Reset commutes with selection of a channel, just like tuning and processing. -/

namespace DSP.LinkwitzRileySplitter
variable {R : Type} [OfNat R 0] {c : Nat}

theorem resetPhase_channel (j : Fin c) (t : Topology) (s : PhaseState (Fin c → R) t) :
    mapPhase (· j) t (resetPhase t s) = resetPhase t (mapPhase (· j) t s) := by
  induction t with
  | leaf => rfl
  | branch low high hl hh => simp only [mapPhase,resetPhase,hl,hh]; rfl

theorem resetTree_channel (j : Fin c) (t : Topology) (s : TreeState (Fin c → R) t) :
    mapTree (· j) t (resetTree t s) = resetTree t (mapTree (· j) t s) := by
  induction t with
  | leaf => rfl
  | branch low high hl hh => simp only [mapTree,resetTree,resetPhase_channel,hl,hh]; rfl

theorem reset_channel {n : Nat} (j : Fin c) (s : State n (Fin c → R)) :
    Channels.channel j (reset s) = reset (Channels.channel j s) := by
  change State.mk (mapTree (· j) _ (resetTree _ s.tree)) _ = _
  rw [resetTree_channel]
  rfl

end DSP.LinkwitzRileySplitter
