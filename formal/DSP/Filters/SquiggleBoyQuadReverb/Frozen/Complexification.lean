import DSP.Filters.SquiggleBoyQuadReverb.Frozen.Harmonic

namespace DSP.SquiggleBoyQuadReverb.Frozen

noncomputable section

open scoped BigOperators

private abbrev lanesComplex (x : Lanes ℝ) : Lanes ℂ := fun i => x i

private theorem lanesRe_add (a b : Lanes ℂ) :
    lanesRe (a + b) = lanesRe a + lanesRe b := rfl

private theorem lanesRe_sub (a b : Lanes ℂ) :
    lanesRe (a - b) = lanesRe a - lanesRe b := rfl

private theorem lanesRe_scale (r : ℝ) (a : Lanes ℂ) :
    lanesRe ((r : ℂ) • a) = r • lanesRe a := by
  funext i
  change ((r : ℂ) * a i).re = r * (a i).re
  simp [Complex.mul_re]

private theorem lanesRe_laneGain (g : Lanes ℝ) (a : Lanes ℂ) :
    lanesRe (laneGain (lanesComplex g) a) = laneGain g (lanesRe a) := by
  funext i
  change ((g i : ℂ) * a i).re = g i * (a i).re
  simp [Complex.mul_re]

private theorem tapeRe_push {n : ℕ} (h : Tape n ℂ) (x : Lanes ℂ) :
    tapeRe (push (h, x)) = push (tapeRe h, lanesRe x) := by
  rw [push_eq_shift, push_eq_shift]
  cases n with
  | zero => exact funext fun j => j.elim0
  | succ n =>
    funext j
    exact Fin.cases rfl (fun _ => rfl) j

theorem cubicWeights_ofReal (r : ℝ) (j : Fin 4) :
    ((cubicWeights r j : ℝ) : ℂ) = cubicWeights (r : ℂ) j := by
  fin_cases j <;> simp [cubicWeights, Matrix.cons_val_two, Matrix.cons_val_three]

private theorem lanesRe_tapRead {n : ℕ} (p : Tap n ℝ) (h : Tape n ℂ) :
    lanesRe (tapRead p.complex h) = tapRead p (tapeRe h) := by
  funext i
  change (tapRead p.complex h i).re = tapRead p (tapeRe h) i
  simp only [tapRead_apply, Complex.re_sum, Tap.complex]
  apply Finset.sum_congr rfl
  intro j _
  change (cubicWeights (↑(p.fraction i) : ℂ) j * h (p.age i j) i).re =
    cubicWeights (p.fraction i) j * (h (p.age i j) i).re
  calc
    _ = (((cubicWeights (p.fraction i) j : ℝ) : ℂ) * h (p.age i j) i).re :=
      congrArg (fun c : ℂ => (c * h (p.age i j) i).re)
        (cubicWeights_ofReal (p.fraction i) j).symm
    _ = _ := by
      simp only [Complex.mul_re, Complex.ofReal_re, Complex.ofReal_im, zero_mul, sub_zero]

private theorem allPassRe {n : ℕ} (p : Tap n ℝ) (g : ℝ) (h : Tape n ℂ) (x : Lanes ℂ) :
    let y := allPass p.complex (g : ℂ) (h, x)
    (tapeRe y.1, lanesRe y.2) = allPass p g (tapeRe h, lanesRe x) := by
  dsimp only
  rw [allPass_apply, allPass_apply]
  dsimp only
  simp only [tapeRe_push, lanesRe_add, lanesRe_sub, lanesRe_scale, lanesRe_tapRead]

private theorem introRe (g : ℝ) (order : List (Fin 8)) (p : IntroTaps order ℝ)
    (h : IntroHistory order ℂ) (x : Lanes ℂ) :
    let y := intro (g : ℂ) order (fun j => (p j).complex) (h, x)
    ((fun j => tapeRe (y.1 j)), lanesRe y.2) =
      intro g order p ((fun j => tapeRe (h j)), lanesRe x) := by
  induction order generalizing x with
  | nil =>
    apply Prod.ext
    · funext j
      exact j.elim0
    · rfl
  | cons i order ih =>
    dsimp only
    rw [intro_cons, intro_cons]
    dsimp only
    have head := allPassRe (p 0) g (h 0) x
    dsimp only at head
    have tail := ih (fun j => p j.succ) (fun j => h j.succ)
      (allPass (p 0).complex (g : ℂ) (h 0, x)).2
    dsimp only at tail
    have headOut := congrArg Prod.snd head
    dsimp only at headOut
    rw [headOut] at tail
    apply Prod.ext
    · funext j
      refine Fin.cases ?_ (fun k => ?_) j
      · exact congrArg Prod.fst head
      · exact congrFun (congrArg Prod.fst tail) k
    · have tailOut := congrArg Prod.snd tail
      exact tailOut

private theorem givensPair_fst_re (c s : ℝ) (x y : ℂ) :
    (givensPair (c : ℂ) (s : ℂ) x y).1.re = (givensPair c s x.re y.re).1 := by
  simp [givensPair, Complex.mul_re]

private theorem givensPair_snd_re (c s : ℝ) (x y : ℂ) :
    (givensPair (c : ℂ) (s : ℂ) x y).2.re = (givensPair c s x.re y.re).2 := by
  simp [givensPair, Complex.mul_re]

private theorem lanesRe_mix (x : Lanes ℂ) : lanesRe (mix x) = mix (lanesRe x) := by
  funext i
  change (squiggleBoyReverbMix x i).re = squiggleBoyReverbMix (lanesRe x) i
  unfold squiggleBoyReverbMix
  fin_cases i <;>
    simp [Matrix.cons_val_two, Matrix.cons_val_three, Matrix.vecHead, Matrix.vecTail,
      Function.comp_apply, ← Complex.ofReal_ofScientific, ← Complex.ofReal_div,
      ← Complex.ofReal_neg, givensPair_fst_re, givensPair_snd_re, lanesRe]

private theorem lowPass_apply_pair {K : Type} [Field K] (a h x : Lanes K) :
    lowPass a (h, x) =
      (h + laneGain a (x - h) + laneGain a (x - h), h + laneGain a (x - h)) := rfl

private theorem lowPassRe (a : Lanes ℝ) (h x : Lanes ℂ) :
    let y := lowPass (lanesComplex a) (h, x)
    (lanesRe y.1, lanesRe y.2) = lowPass a (lanesRe h, lanesRe x) := by
  dsimp only
  rw [lowPass_apply_pair, lowPass_apply_pair]
  dsimp only
  simp only [lanesRe_add, lanesRe_sub, lanesRe_laneGain]

private theorem damping_apply_pair {K : Type} [Field K] (a b h k x : Lanes K) :
    damping a b ((h, k), x) =
      let hi := lowPass a (h, x)
      let lo := lowPass b (k, x - hi.2)
      ((hi.1, lo.1), lo.2) := rfl

private theorem dampingRe (a b : Lanes ℝ) (h : Lanes ℂ × Lanes ℂ) (x : Lanes ℂ) :
    let y := damping (lanesComplex a) (lanesComplex b) (h, x)
    ((lanesRe y.1.1, lanesRe y.1.2), lanesRe y.2) =
      damping a b ((lanesRe h.1, lanesRe h.2), lanesRe x) := by
  rcases h with ⟨h, k⟩
  dsimp only
  rw [damping_apply_pair, damping_apply_pair]
  dsimp only
  have highState := congrArg Prod.fst (lowPassRe a h x)
  have highOut := congrArg Prod.snd (lowPassRe a h x)
  dsimp only at highState highOut
  have low := lowPassRe b k (x - (lowPass (lanesComplex a) (h, x)).2)
  dsimp only at low
  rw [lanesRe_sub, highOut] at low
  have lowState := congrArg Prod.fst low
  have lowOut := congrArg Prod.snd low
  dsimp only at lowState lowOut
  rw [highState, lowState, lowOut]

private theorem allPassGain_complex : (allPassGain : ℂ) = ((allPassGain : ℝ) : ℂ) := by
  exact (Complex.ofReal_ofScientific 6 true 1).symm

private theorem beforeSATRe (p : Spec ℝ) (h : History ℂ) (x : Lanes ℂ) :
    let y := beforeSAT p.complex (h, x)
    (realHistory y.1, lanesRe y.2) = beforeSAT p (realHistory h, lanesRe x) := by
  dsimp only
  rw [beforeSAT_apply, beforeSAT_apply]
  dsimp only [realHistory, LinearMap.coe_mk, AddHom.coe_mk, historyRe, History.wet,
    Spec.complex]
  rw [allPassGain_complex]
  simp only [tapeRe_push, lanesRe_add]
  rw [lanesRe_laneGain]
  have diffusion := introRe (allPassGain : ℝ) squiggleBoyReverbIntroOrder p.intro h.1 x
  dsimp only at diffusion
  have diffState := congrArg Prod.fst diffusion
  have diffOut := congrArg Prod.snd diffusion
  dsimp only at diffState diffOut
  rw [diffState, diffOut]
  let main := push
    (h.2.1, (intro ((allPassGain : ℝ) : ℂ) _ (fun j => (p.intro j).complex) (h.1, x)).2 +
      laneGain (lanesComplex p.feedback) h.wet)
  have mainRe : tapeRe main =
      push (tapeRe h.2.1,
        (intro allPassGain _ p.intro ((fun j => tapeRe (h.1 j)), lanesRe x)).2 +
          laneGain p.feedback (lanesRe h.wet)) := by
    dsimp only [main]
    rw [tapeRe_push, lanesRe_add, lanesRe_laneGain,
      diffOut]
  have first := allPassRe p.first allPassGain h.2.2.1 (mix (tapRead p.main.complex main))
  dsimp only at first
  rw [lanesRe_mix, lanesRe_tapRead, mainRe] at first
  have firstState := congrArg Prod.fst first
  have firstOut := congrArg Prod.snd first
  dsimp only at firstState firstOut
  dsimp only [main, History.wet, lanesComplex] at firstState firstOut
  rw [firstState, firstOut]

private theorem afterSATRe (p : Spec ℝ) (h : History ℂ) (x : Lanes ℂ) :
    realHistory (afterSAT p.complex (h, x)) = afterSAT p (realHistory h, lanesRe x) := by
  rw [afterSAT_apply, afterSAT_apply]
  dsimp only [realHistory, LinearMap.coe_mk, AddHom.coe_mk, historyRe, History.wet,
    Spec.complex]
  rw [allPassGain_complex]
  have damp := dampingRe p.highAlpha p.lowAlpha h.2.2.2.1 x
  dsimp only at damp
  have dampState := congrArg Prod.fst damp
  have dampOut := congrArg Prod.snd damp
  dsimp only at dampState dampOut
  rw [dampState]
  have second := allPassRe p.second (allPassGain : ℝ) h.2.2.2.2.1
    (damping (lanesComplex p.highAlpha) (lanesComplex p.lowAlpha) (h.2.2.2.1, x)).2
  dsimp only at second
  rw [dampOut] at second
  have secondState := congrArg Prod.fst second
  have secondOut := congrArg Prod.snd second
  dsimp only at secondState secondOut
  rw [secondState, secondOut]

/-- Taking the real part of the complexified tangent gives the real tangent. -/
theorem realHistory_tangent (p : Spec ℝ) (h : History ℂ) (u : Lanes ℂ) :
    realHistory (tangent p.complex (h, u)) = tangent p (realHistory h, lanesRe u) := by
  rw [tangent_apply, tangent_apply]
  dsimp only
  rw [afterSATRe]
  change afterSAT p (realHistory (beforeSAT p.complex (h, u)).1,
    lanesRe (laneGain (lanesComplex p.satGain) (beforeSAT p.complex (h, u)).2)) = _
  have before := beforeSATRe p h u
  dsimp only at before
  have beforeState := congrArg Prod.fst before
  have beforeOut := congrArg Prod.snd before
  dsimp only at beforeState beforeOut
  rw [lanesRe_laneGain, beforeState, beforeOut]

end

end DSP.SquiggleBoyQuadReverb.Frozen
