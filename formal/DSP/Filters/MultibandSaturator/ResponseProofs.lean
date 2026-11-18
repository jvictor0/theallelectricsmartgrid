import DSP.Filters.MultibandSaturator.Response
import DSP.Filters.LinkwitzRileySplitter.WeightProofs
import DSP.Filters.LinkwitzRileySplitter.PreparedProofs

noncomputable section
namespace DSP.MultibandSaturator
open scoped BigOperators

theorem sum_eq_sum {n : Nat} {R : Type} [AddCommMonoid R] (f : Fin n → R) :
    sum f = ∑ i, f i := by
  induction n with
  | zero => simp [sum]
  | succ n ih => rw [sum,ih,Fin.sum_univ_castSucc]

/-- Candidate complex small-signal response, with the splitter's exact phase. -/
def Snapshot.transfer {n : Nat} (s : Snapshot n ℝ) (frequency : ℝ) : ℂ :=
  (s.masterGain:ℂ) * ∑ i, (s.bandGain i:ℂ) * s.splitter.bandResponse frequency i

theorem Snapshot.magnitudeSquared_eq_normSq {n : Nat} (s : Snapshot n ℝ)
    (h : LinkwitzRileySplitter.validSnapshot _ s.splitter.tree) (f : ResponseFrequency ℝ)
    (hs : f.sinHalf = Real.sin (Real.pi*f.cyclesPerSample))
    (hc : f.cosHalf = Real.cos (Real.pi*f.cyclesPerSample)) :
    s.magnitudeSquared f = Complex.normSq (s.transfer f.cyclesPerSample) := by
  let a := f.sinHalf
  let b := f.cosHalf
  have hab : a*a+b*b=1 := by
    dsimp [a,b]
    rw [hs,hc]
    nlinarith [Real.sin_sq_add_cos_sq (Real.pi*f.cyclesPerSample)]
  let phase := LinkwitzRileySplitter.phaseTransfer _
    (LinkwitzRileySplitter.mapData (Functor.map Complex.ofReal) _ s.splitter.tree) ⟨0,a⟩ b
  have hp : Complex.normSq phase = 1 := LinkwitzRileySplitter.phaseTransfer_unit _ _ h a b hab
  have hb (i : Fin n) : s.splitter.bandResponse f.cyclesPerSample i = phase *
      ((LinkwitzRileySplitter.bandWeight _ s.splitter.tree a b i.val : ℝ):ℂ) := by
    dsimp [LinkwitzRileySplitter.Snapshot.bandResponse]
    rw [← hs,← hc]
    exact LinkwitzRileySplitter.bandTransfer_factor _ _ h a b hab i.val
  have he : s.transfer f.cyclesPerSample = phase *
      ((s.masterGain * ∑ i : Fin n, s.bandGain i *
        LinkwitzRileySplitter.bandWeight _ s.splitter.tree a b i.val : ℝ):ℂ) := by
    simp only [Snapshot.transfer,hb]
    push_cast
    simp only [Finset.mul_sum]
    apply Finset.sum_congr rfl
    intro i _
    ring
  rw [he,Complex.normSq_mul,hp,one_mul,Complex.normSq_ofReal]
  simp only [Snapshot.magnitudeSquared,sum_eq_sum,a,b]

theorem Snapshot.equal_gain_flat {n : Nat} (s : Snapshot n ℝ) (hn : 0 < n)
    (h : LinkwitzRileySplitter.validSnapshot _ s.splitter.tree) (gain : ℝ)
    (hg : ∀ i, s.bandGain i= gain) (frequency : ℝ) :
    Complex.normSq (s.transfer frequency) = (s.masterGain*gain)^2 := by
  simp only [Snapshot.transfer,hg,← Finset.mul_sum,Complex.normSq_mul]
  rw [s.splitter.reconstruction_flat hn h frequency]
  simp [Complex.normSq_ofReal]
  ring

end DSP.MultibandSaturator
