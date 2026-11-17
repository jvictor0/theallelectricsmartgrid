import DSP.Filters.LinkwitzRileySplitter
import DSP.Filters.Responses.Realizes
import Mathlib.Algebra.BigOperators.Fin

/-! Analog pieces and their compositional tree transfer. Homogeneous coordinates
(a,b) mean s=a/b: analog evaluation uses (s,1), digital evaluation uses
(1-q,1+q), and prepared frequency evaluation uses (i sin(pi f),cos(pi f)).
No splitter response kernel is exported; downstream consumers can combine the
proved band transfers with their own gains before taking a magnitude. -/
noncomputable section
namespace DSP.LinkwitzRileySplitter

open scoped BigOperators

variable {K : Type} [Field K]

abbrev TreeSnapshot (K : Type) (t : Topology) := TreeData (TPTSVF.Snapshot K) t

def mapData {V W : Type} (f : V → W) : (t : Topology) → TreeData V t → TreeData W t
  | .leaf, _ => ⟨⟩
  | .branch low high, s => ⟨f s.value, mapData f low s.low, mapData f high s.high⟩

def lowSplitTransfer (s : TPTSVF.Snapshot K) (a b : K) : K :=
  s.lowPassTransfer a b ^ 2

def highSplitTransfer (s : TPTSVF.Snapshot K) (a b : K) : K :=
  s.allPassTransfer a b - lowSplitTransfer s a b

/-- LR4's pole-zero identity reduces AP-LP4 to two highpass sections. -/
theorem highSplitTransfer_eq_highPassSquared (s : TPTSVF.Snapshot K) (a b : K)
    (hk : s.k ^ 2 = 2) (hd : a*a + s.k*(s.g*b)*a + s.g*b*(s.g*b) ≠ 0) :
    highSplitTransfer s a b = s.highPassTransfer a b ^ 2 := by
  unfold highSplitTransfer lowSplitTransfer TPTSVF.Snapshot.allPassTransfer
    TPTSVF.Snapshot.lowPassTransfer TPTSVF.Snapshot.highPassTransfer
    TPTSVF.Snapshot.weightedTransfer Response.svf
  simp only [one_mul, zero_mul, add_zero, zero_add]
  rw [sub_eq_iff_eq_add, div_pow, div_pow, ← add_div]
  apply (div_eq_div_iff hd (pow_ne_zero 2 hd)).mpr
  linear_combination -(s.g*b)^2*a^2 * (a*a + s.k*(s.g*b)*a + s.g*b*(s.g*b)) * hk

def phaseTransfer : (t : Topology) → TreeSnapshot K t → K → K → K
  | .leaf, _, _, _ => 1
  | .branch low high, s, a, b =>
    phaseTransfer high s.high a b * phaseTransfer low s.low a b * s.value.allPassTransfer a b

/-- Each output is the product of the split responses along its path and the
all-pass responses of the sibling subtrees that path bypasses. -/
def bandTransfer : (t : Topology) → TreeSnapshot K t → K → K → Nat → K
  | .leaf, _, _, _, _ => 1
  | .branch low high, s, a, b, i =>
    if i < low.bands then
      bandTransfer low s.low a b i * (phaseTransfer high s.high a b * lowSplitTransfer s.value a b)
    else
      bandTransfer high s.high a b (i-low.bands) *
        (phaseTransfer low s.low a b * highSplitTransfer s.value a b)

theorem sum_append (nl nh : Nat) (lo hi : Nat → K) :
    (∑ i ∈ Finset.range (nl+nh), if i < nl then lo i else hi (i-nl)) =
      (∑ i ∈ Finset.range nl, lo i) + ∑ i ∈ Finset.range nh, hi i := by
  rw [Finset.sum_range_add]
  congr 1
  · apply Finset.sum_congr rfl
    intro i h
    simp [Finset.mem_range.mp h]
  · apply Finset.sum_congr rfl
    intro i _
    simp

/-- Reconstruction is independent of the tree shape, including odd counts.
The common phase is one all-pass factor for every internal crossover. -/
theorem sum_bandTransfer (t : Topology) (s : TreeSnapshot K t) (a b : K) :
    (∑ i ∈ Finset.range t.bands, bandTransfer t s a b i) = phaseTransfer t s a b := by
  induction t with
  | leaf => simp [Topology.bands, bandTransfer, phaseTransfer]
  | branch low high hl hh =>
    simp only [Topology.bands, bandTransfer]
    rw [sum_append low.bands high.bands
      (fun i => bandTransfer low s.low a b i *
        (phaseTransfer high s.high a b * lowSplitTransfer s.value a b))
      (fun i => bandTransfer high s.high a b i *
        (phaseTransfer low s.low a b * highSplitTransfer s.value a b)),
      ← Finset.sum_mul, ← Finset.sum_mul, hl, hh]
    simp only [phaseTransfer, highSplitTransfer]
    ring

theorem phaseTransfer_scale (t : Topology) (s : TreeSnapshot ℂ t) (a b c : ℂ) (hc : c ≠ 0) :
    phaseTransfer t s (c*a) (c*b) = phaseTransfer t s a b := by
  induction t with
  | leaf => rfl
  | branch low high hl hh =>
    simp only [phaseTransfer, hl, hh, TPTSVF.Snapshot.allPassTransfer,
      TPTSVF.Snapshot.weightedTransfer_scale _ _ _ _ _ _ _ hc]

theorem bandTransfer_scale (t : Topology) (s : TreeSnapshot ℂ t) (a b c : ℂ) (hc : c ≠ 0) (i : Nat) :
    bandTransfer t s (c*a) (c*b) i = bandTransfer t s a b i := by
  induction t generalizing i with
  | leaf => rfl
  | branch low high hl hh =>
    simp only [bandTransfer, lowSplitTransfer, highSplitTransfer,
      TPTSVF.Snapshot.lowPassTransfer, TPTSVF.Snapshot.allPassTransfer]
    split
    · rw [hl, phaseTransfer_scale _ _ _ _ _ hc]
      simp only [TPTSVF.Snapshot.weightedTransfer_scale _ _ _ _ _ _ _ hc]
    · rw [hh, phaseTransfer_scale _ _ _ _ _ hc]
      simp only [TPTSVF.Snapshot.weightedTransfer_scale _ _ _ _ _ _ _ hc]

/-- The physically meaningful snapshot region. LR4 fixes k²=2; positive g,k
exclude unstable or singular sections. No spacing between cutoffs is required. -/
def validSnapshot : (t : Topology) → TreeSnapshot ℝ t → Prop
  | .leaf, _ => True
  | .branch low high, s => 0 < s.value.g ∧ 0 < s.value.k ∧ s.value.k^2 = 2 ∧
      validSnapshot low s.low ∧ validSnapshot high s.high

/-- The all-pass numerator is the complex conjugate of its denominator.
This includes DC and Nyquist through the homogeneous half-angle coordinates. -/
theorem allPass_unit (s : TPTSVF.Snapshot ℝ) (hg : 0 < s.g) (hk : 0 < s.k)
    (a b : ℝ) (hab : a*a+b*b=1) :
    Complex.normSq ((Complex.ofReal <$> s).allPassTransfer ⟨0,a⟩ b) = 1 := by
  let d : ℂ := ⟨s.g*b*(s.g*b)-a*a, s.k*(s.g*b)*a⟩
  have hd : d ≠ 0 := by
    intro h
    have hr := congrArg Complex.re h
    have hi := congrArg Complex.im h
    change s.g*b*(s.g*b)-a*a = 0 at hr
    change s.k*(s.g*b)*a = 0 at hi
    have hz : b = 0 ∨ a = 0 := by
      simpa only [mul_eq_zero, ne_of_gt hg, ne_of_gt hk, false_or] using hi
    rcases hz with hb | ha
    · subst b
      nlinarith
    · subst a
      have : s.g*b ≠ 0 := mul_ne_zero (ne_of_gt hg) (by intro hb; subst b; norm_num at hab)
      exact this (mul_self_eq_zero.mp (by simpa using hr))
  have heq : (Complex.ofReal <$> s).allPassTransfer ⟨0,a⟩ b = starRingEnd ℂ d / d := by
    unfold TPTSVF.Snapshot.allPassTransfer TPTSVF.Snapshot.weightedTransfer Response.svf
    congr 1 <;> apply Complex.ext <;> simp [d, Functor.map] <;> ring
  rw [heq, Complex.normSq_div, Complex.normSq_conj, div_self (mt Complex.normSq_eq_zero.mp hd)]

theorem phaseTransfer_unit (t : Topology) (s : TreeSnapshot ℝ t) (h : validSnapshot t s)
    (a b : ℝ) (hab : a*a+b*b=1) :
    Complex.normSq (phaseTransfer t (mapData (Functor.map Complex.ofReal) t s) ⟨0,a⟩ b) = 1 := by
  induction t with
  | leaf => simp [phaseTransfer]
  | branch low high hl hh =>
    simp only [mapData, phaseTransfer, Complex.normSq_mul,
      hl s.low h.2.2.2.1, hh s.high h.2.2.2.2, allPass_unit s.value h.1 h.2.1 a b hab,
      one_mul]

def Snapshot.bandResponse {n : Nat} (s : Snapshot n ℝ) (frequency : ℝ) (i : Fin n) : ℂ :=
  bandTransfer _ (mapData (Functor.map Complex.ofReal) _ s.tree)
    ⟨0, Real.sin (Real.pi*frequency)⟩ (Real.cos (Real.pi*frequency)) i.val

/-- Relate the composed analog expression to the bilinear digital frequency
variable. This is finite rational evaluation, not evaluation of an arbitrary
infinite Laurent series on the unit circle. -/
theorem Snapshot.bandResponse_delay {n : Nat} (s : Snapshot n ℝ) (frequency : ℝ) (i : Fin n) :
    s.bandResponse frequency i = bandTransfer _ (mapData (Functor.map Complex.ofReal) _ s.tree)
      (1-ResponseFrequency.unitDelay frequency) (1+ResponseFrequency.unitDelay frequency) i.val :=
  by
    unfold Snapshot.bandResponse
    exact Response.frequency_eq_delay (fun a b => bandTransfer _
      (mapData (Functor.map Complex.ofReal) _ s.tree) a b i.val)
      (fun a b c hc => bandTransfer_scale _ _ a b c hc i.val) frequency

/-- Flat reconstruction is unity magnitude, retaining the product all-pass
phase. This exact theorem concerns fixed coefficients and a linear sum. -/
theorem Snapshot.reconstruction_flat {n : Nat} (s : Snapshot n ℝ) (hn : 0 < n)
    (h : validSnapshot _ s.tree) (frequency : ℝ) :
    Complex.normSq (∑ i : Fin n, s.bandResponse frequency i) = 1 := by
  simp only [Snapshot.bandResponse, Fin.sum_univ_eq_sum_range]
  have hs := sum_bandTransfer (Topology.balanced n)
    (mapData (Functor.map Complex.ofReal) _ s.tree)
    (⟨0, Real.sin (Real.pi*frequency)⟩ : ℂ) (Real.cos (Real.pi*frequency))
  rw [Topology.balanced_bands n hn] at hs
  rw [hs]
  exact phaseTransfer_unit _ s.tree h _ _ (by simpa [pow_two] using Real.sin_sq_add_cos_sq (Real.pi*frequency))

end DSP.LinkwitzRileySplitter
end
