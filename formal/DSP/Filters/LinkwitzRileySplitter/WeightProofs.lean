import DSP.Filters.LinkwitzRileySplitter.Weights
import DSP.Filters.LinkwitzRileySplitter.Transfer

noncomputable section
namespace DSP.LinkwitzRileySplitter
open scoped BigOperators

theorem weight_denominator_pos (g a b : ℝ) (hg : 0 < g) (hab : a*a+b*b=1) :
    0 < (g*b)^4+a^4 := by
  have h : g*b ≠ 0 ∨ a ≠ 0 := by
    by_contra! h
    have hb : b = 0 := (mul_eq_zero.mp h.1).resolve_left (ne_of_gt hg)
    simp [hb,h.2] at hab
  rcases h with h | h
  · exact add_pos_of_pos_of_nonneg (by nlinarith [sq_pos_of_ne_zero h, sq_pos_of_ne_zero (pow_ne_zero 2 h)]) (by positivity)
  · exact add_pos_of_nonneg_of_pos (by positivity) (by nlinarith [sq_pos_of_ne_zero h, sq_pos_of_ne_zero (pow_ne_zero 2 h)])

theorem splitWeight_properties (g a b : ℝ) (hg : 0 < g) (hab : a*a+b*b=1) :
    0 ≤ (splitWeight g a b).1 ∧ 0 ≤ (splitWeight g a b).2 ∧
      (splitWeight g a b).1+(splitWeight g a b).2=1 := by
  have hd := weight_denominator_pos g a b hg hab
  have he : (g*b*(g*b))*(g*b*(g*b))+(a*a)*(a*a) = (g*b)^4+a^4 := by ring
  simp only [splitWeight,he]
  refine ⟨div_nonneg (mul_self_nonneg _) hd.le,div_nonneg (mul_self_nonneg _) hd.le,?_⟩
  rw [← add_div,he,div_self (ne_of_gt hd)]

theorem lowSplitTransfer_factor (s : TPTSVF.Snapshot ℝ) (a b : ℝ)
    (hg : 0 < s.g) (hk : s.k^2=2) (hab : a*a+b*b=1) :
    lowSplitTransfer (Complex.ofReal <$> s) ⟨0,a⟩ b =
      (Complex.ofReal <$> s).allPassTransfer ⟨0,a⟩ b * ((splitWeight s.g a b).1 : ℝ) := by
  let x := s.g*b
  let d : ℂ := ⟨x*x-a*a,s.k*x*a⟩
  let w := x^4+a^4
  have hw : 0 < w := weight_denominator_pos s.g a b hg hab
  have hn : Complex.normSq d = w := by
    simp only [Complex.normSq_apply,d,w]
    linear_combination (x*a)^2 * hk
  have hd : d ≠ 0 := by intro h; simp [h] at hn; linarith
  have hc : d*star d = (w:ℂ) := by change d * (starRingEnd ℂ) d = _; rw [Complex.mul_conj,hn]
  have hl : (Complex.ofReal <$> s).lowPassTransfer ⟨0,a⟩ b = (x*x:ℝ)/d := by
    unfold TPTSVF.Snapshot.lowPassTransfer TPTSVF.Snapshot.weightedTransfer Response.svf
    congr 1 <;> apply Complex.ext <;> simp [d,x,Functor.map]
    all_goals ring
  have ha : (Complex.ofReal <$> s).allPassTransfer ⟨0,a⟩ b = star d/d := by
    unfold TPTSVF.Snapshot.allPassTransfer TPTSVF.Snapshot.weightedTransfer Response.svf
    congr 1 <;> apply Complex.ext <;> simp [d,x,Functor.map]
    all_goals ring
  have hwc : (w:ℂ) ≠ 0 := by exact_mod_cast ne_of_gt hw
  rw [lowSplitTransfer,hl,ha]
  have he : (splitWeight s.g a b).1 = x^4/w := by dsimp [splitWeight,w,x]; ring
  rw [he]
  push_cast
  field_simp [hd,hwc]
  linear_combination -(x:ℂ)^4 * hc

theorem highSplitTransfer_factor (s : TPTSVF.Snapshot ℝ) (a b : ℝ)
    (hg : 0 < s.g) (hk : s.k^2=2) (hab : a*a+b*b=1) :
    highSplitTransfer (Complex.ofReal <$> s) ⟨0,a⟩ b =
      (Complex.ofReal <$> s).allPassTransfer ⟨0,a⟩ b * ((splitWeight s.g a b).2 : ℝ) := by
  rw [highSplitTransfer,lowSplitTransfer_factor s a b hg hk hab]
  have h := (splitWeight_properties s.g a b hg hab).2.2
  have hc : ((splitWeight s.g a b).1:ℂ)+((splitWeight s.g a b).2:ℂ)=1 := by exact_mod_cast h
  linear_combination -(Complex.ofReal <$> s).allPassTransfer ⟨0,a⟩ b * hc

theorem bandTransfer_factor (t : Topology) (s : TreeSnapshot ℝ t)
    (h : validSnapshot t s) (a b : ℝ) (hab : a*a+b*b=1) (i : Nat) :
    bandTransfer t (mapData (Functor.map Complex.ofReal) t s) ⟨0,a⟩ b i =
      phaseTransfer t (mapData (Functor.map Complex.ofReal) t s) ⟨0,a⟩ b *
        ((bandWeight t s a b i : ℝ) : ℂ) := by
  induction t generalizing i with
  | leaf => simp [bandTransfer,phaseTransfer,bandWeight]
  | branch low high hl hh =>
    simp only [mapData,bandTransfer,bandWeight,phaseTransfer]
    split
    · rw [hl s.low h.2.2.2.1 i,lowSplitTransfer_factor s.value a b h.1 h.2.2.1 hab]
      push_cast
      ring
    · rw [hh s.high h.2.2.2.2 (i-low.bands),highSplitTransfer_factor s.value a b h.1 h.2.2.1 hab]
      push_cast
      ring

theorem bandWeight_nonneg (t : Topology) (s : TreeSnapshot ℝ t)
    (h : validSnapshot t s) (a b : ℝ) (hab : a*a+b*b=1) (i : Nat) :
    0 ≤ bandWeight t s a b i := by
  induction t generalizing i with
  | leaf => exact zero_le_one
  | branch low high hl hh =>
    simp only [bandWeight]
    split
    · exact mul_nonneg (hl s.low h.2.2.2.1 i) (splitWeight_properties s.value.g a b h.1 hab).1
    · exact mul_nonneg (hh s.high h.2.2.2.2 _) (splitWeight_properties s.value.g a b h.1 hab).2.1

theorem sum_bandWeight (t : Topology) (s : TreeSnapshot ℝ t)
    (h : validSnapshot t s) (a b : ℝ) (hab : a*a+b*b=1) :
    (∑ i ∈ Finset.range t.bands, bandWeight t s a b i) = 1 := by
  induction t with
  | leaf => simp [Topology.bands,bandWeight]
  | branch low high hl hh =>
    simp only [Topology.bands,bandWeight]
    rw [sum_append low.bands high.bands
      (fun i => bandWeight low s.low a b i * (splitWeight s.value.g a b).1)
      (fun i => bandWeight high s.high a b i * (splitWeight s.value.g a b).2),← Finset.sum_mul,← Finset.sum_mul,hl s.low h.2.2.2.1,hh s.high h.2.2.2.2]
    simpa only [one_mul] using (splitWeight_properties s.value.g a b h.1 hab).2.2

end DSP.LinkwitzRileySplitter
