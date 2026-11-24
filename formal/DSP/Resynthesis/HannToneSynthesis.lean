import DSP.Resynthesis.HannTone
import Mathlib.Tactic.FieldSimp
import Mathlib.Tactic.Linarith
import Mathlib.Tactic.NormNum
import Mathlib.Tactic.Positivity
import Mathlib.Tactic.Ring

namespace DSP.Resynthesis

noncomputable section

private theorem binTable_ext {n : ℕ} {A : Type} {a b : BinTable A n}
    (h : ∀ k, a.get k = b.get k) : a = b := by
  cases a with
  | mk a =>
    cases b with
    | mk b =>
      congr 1
      ext i hi
      exact h ⟨i, hi⟩

private theorem addCoefficient_get {n : ℕ} (table : BinTable (ComplexValue ℝ) n)
    (bin key : ActiveBin n) (z : ComplexValue ℝ) :
    (addCoefficient table bin z).get key =
      if bin = key then ComplexValue.add (table.get key) z else table.get key := by
  by_cases h : bin = key
  · subst key
    simp [addCoefficient, BinTable.get, BinTable.set]
  · have hv : bin.val ≠ key.val := fun hv => h (Fin.ext hv)
    simp [addCoefficient, BinTable.get, BinTable.set, hv, h]

private theorem zeroPolar (phase : ℝ) :
    exactMath.polar 0 phase = ComplexValue.ofReal 0 := by
  simp [exactMath, ComplexValue.ofReal]

private theorem addCoefficient_zero {n : ℕ} (table : BinTable (ComplexValue ℝ) n)
    (bin : ActiveBin n) : addCoefficient table bin (ComplexValue.ofReal 0) = table := by
  apply binTable_ext
  intro key
  simp [addCoefficient_get, ComplexValue.add, ComplexValue.ofReal]

private theorem accumulateDry_zero {n : ℕ} (magnitude phase : BinTable ℝ n)
    (table : BinTable (ComplexValue ℝ) n) :
    accumulateDry exactMath 0 magnitude phase table = table := by
  unfold accumulateDry
  simp only [zero_mul, zeroPolar, addCoefficient_zero]
  simp

private theorem accumulateShifted_zero {n : ℕ} (shift : PitchRatio)
    (omega magnitude phase : BinTable ℝ n) (graph : PVDRGraph ℝ n)
    (table : BinTable (ComplexValue ℝ) n) :
    accumulateShifted exactMath 0 shift omega magnitude phase graph table = table := by
  unfold accumulateShifted
  generalize graph.order = order
  induction order generalizing table with
  | nil => rfl
  | cons bin rest ih =>
    rw [List.foldl_cons, ih]
    dsimp only
    split
    · rfl
    · split <;> simp [zeroPolar, addCoefficient_zero]

private theorem phaseRound_int (k : ℤ) :
    exactMath.phaseRound (k : ℝ) = k := by
  by_cases hk : (k : ℝ) < 0
  · norm_num [exactMath, hk, ← Int.cast_neg]
  · norm_num [exactMath, hk, add_comm (k : ℝ)]

theorem shiftedBin_exact_index {n target : ℕ} (hlo : 0 < target) (hhi : target < n) :
    shiftedBin (n := n) exactMath ((target : ℝ) / n) = (target : ℤ) := by
  have hn : (n : ℝ) ≠ 0 := by exact_mod_cast (by omega : n ≠ 0)
  have heq : (target : ℝ) / n * n = ((target : ℤ) : ℝ) := by
    push_cast
    field_simp
  change min (max 1 (exactMath.phaseRound ((target : ℝ) / n * n))) ((n : ℤ) - 1) = _
  rw [heq, phaseRound_int, max_eq_right (by omega), min_eq_left (by omega)]

theorem acceptedBin_exact_neighbor {n target : ℕ} (bin : ActiveBin n)
    (near : |(target : ℤ) - (bin.coefficient.val : ℤ)| ≤ 1) (hop : 0 < n / 4) :
    acceptedBin exactMath ((target : ℝ) / n) (bin.coefficient.val : ℤ) = some bin := by
  have hk := bin.coefficient.isLt
  have hb : 0 < bin.coefficient.val := by simp [ActiveBin.coefficient]
  have hn : (0 : ℝ) < n := by exact_mod_cast (by omega : 0 < n)
  have hhop : (0 : ℝ) < (n / 4 : ℕ) := by exact_mod_cast hop
  have hlo : -(1 : ℝ) ≤ (target : ℝ) - bin.coefficient.val := by
    exact_mod_cast (abs_le.mp near).1
  have hhi : (target : ℝ) - bin.coefficient.val ≤ 1 := by
    exact_mod_cast (abs_le.mp near).2
  have hlim : (1 : ℝ) / n ≤ (0.5 : ℝ) / (n / 4 : ℕ) := by
    apply (div_le_div_iff₀ hn hhop).2
    have hs : (2 * (n / 4) : ℕ) ≤ n := by omega
    have hs' : (2 : ℝ) * (n / 4 : ℕ) ≤ n := by exact_mod_cast hs
    nlinarith
  unfold acceptedBin acceptedBinWith
  rw [dif_pos (by exact ⟨Int.ofNat_lt.mpr hb, Int.ofNat_lt.mpr hk⟩)]
  have heq : (⟨(bin.coefficient.val : ℤ).toNat - 1, by
      simp [ActiveBin.coefficient, bin.isLt]⟩ : ActiveBin n) = bin := by
    apply Fin.ext
    simp [ActiveBin.coefficient]
  dsimp only
  rw [heq]
  change (if (0.5 : ℝ) / (n / 4 : ℕ) < (target : ℝ) / n - bin.coefficient.val / n ∨
      (0.5 : ℝ) / (n / 4 : ℕ) < -((target : ℝ) / n - bin.coefficient.val / n)
    then none else some bin) = some bin
  rw [← sub_div, if_neg]
  push Not
  constructor
  · exact ((div_le_div_iff_of_pos_right hn).2 hhi).trans hlim
  · rw [← neg_div]
    exact ((div_le_div_iff_of_pos_right hn).2 (by linarith)).trans hlim

private theorem gain_exact {controls : Controls ℝ} (unison : controls.unisonGain = 0)
    (oscillator : Fin 3) :
    oscillatorGain exactMath controls oscillator = if oscillator = 0 then 1 else 0 := by
  by_cases h : oscillator = 0
  · subst oscillator
    simp [oscillatorGain, gainFor, unison, exactMath]
  · have hv : oscillator.val ≠ 0 := fun hv => h (Fin.ext hv)
    simp [oscillatorGain, gainFor, unison, h, hv]

theorem synthesizeSpectrum_exact_wet {n : ℕ} (controls : Controls ℝ)
    (omega magnitude : BinTable ℝ n) (phase : Fin 3 → BinTable ℝ n) (graph : PVDRGraph ℝ n)
    (fade : controls.fade = 1) (unison : controls.unisonGain = 0) :
    synthesizeSpectrum exactMath controls omega magnitude phase graph =
      let start : BinTable (ComplexValue ℝ) n := BinTable.ofFn fun _ => ComplexValue.ofReal 0
      let bins := if controls.shift = .unity then accumulateDry exactMath 1 magnitude (phase 0) start
        else accumulateShifted exactMath 1 controls.shift omega magnitude (phase 0) graph start
      ⟨fun k => if h : 0 < k.val then bins.get ⟨k.val - 1, by have := k.isLt; omega⟩
        else ComplexValue.ofReal 0⟩ := by
  unfold synthesizeSpectrum
  have hr : List.finRange 3 = ([0, 1, 2] : List (Fin 3)) := by decide
  simp only [fadeWeight, fade, mul_one, sub_self, max_self, sup_of_le_right zero_le_one,
    zero_mul, one_mul, hr, List.foldl_cons, List.foldl_nil, gain_exact unison, if_true,
    show (1 : Fin 3) ≠ 0 by decide, show (2 : Fin 3) ≠ 0 by decide, if_false,
    accumulateDry_zero, accumulateShifted_zero]
  split <;> simp_all

private theorem foldl_only_coefficient {n : ℕ}
    (step : BinTable (ComplexValue ℝ) n → ActiveBin n → BinTable (ComplexValue ℝ) n)
    (key src : ActiveBin n) (z : ComplexValue ℝ)
    (rule : ∀ table bin, (step table bin).get key =
      if bin = src then ComplexValue.add (table.get key) z else table.get key)
    (order : List (ActiveBin n)) (nodup : order.Nodup) (table : BinTable (ComplexValue ℝ) n) :
    (order.foldl step table).get key =
      if src ∈ order then ComplexValue.add (table.get key) z else table.get key := by
  induction order generalizing table with
  | nil => simp
  | cons bin rest ih =>
    obtain ⟨absent, unique⟩ := List.nodup_cons.mp nodup
    rw [List.foldl_cons, ih unique, rule]
    by_cases eq : bin = src
    · subst bin
      simp [absent]
    · simp [eq, Ne.symm eq]

theorem accumulateDry_exact_get {n : ℕ} (gain : ℝ) (magnitude phase : BinTable ℝ n)
    (start : BinTable (ComplexValue ℝ) n) (key : ActiveBin n) :
    (accumulateDry exactMath gain magnitude phase start).get key =
      ComplexValue.add (start.get key) (exactMath.polar (gain * magnitude.get key) (phase.get key)) := by
  unfold accumulateDry
  rw [foldl_only_coefficient _ key key _ ?_ _ (List.nodup_finRange _) start,
    if_pos (List.mem_finRange key)]
  intro table bin
  rw [addCoefficient_get]
  split
  · rename_i h
    subst bin
    rfl
  · rfl

theorem accumulateShifted_exact_get {n : ℕ} (gain : ℝ) (shift : PitchRatio)
    (omega magnitude phase : BinTable ℝ n) (graph : PVDRGraph ℝ n)
    (start : BinTable (ComplexValue ℝ) n) (key src : ActiveBin n) (z : ComplexValue ℝ)
    (unique : graph.order.Nodup)
    (rule : ∀ bin,
      let link := graph.link.get bin
      let ratio := (shift.numerator : ℝ) / (shift.denominator : ℝ)
      let rootOmega := omega.get link.root * ratio
      let rootTarget := shiftedBin (n := n) exactMath rootOmega
      let rootPhase := phase.get link.root * ratio
      let position := if link.root = bin then (rootTarget, rootPhase)
        else (rootTarget - Int.ofNat link.root.coefficient.val + Int.ofNat bin.coefficient.val,
          followerPhase rootPhase link.delta)
      (if link.small then ComplexValue.ofReal 0 else
        if acceptedBin (n := n) exactMath rootOmega position.1 = some key then
          exactMath.polar (gain * magnitude.get bin) position.2 else ComplexValue.ofReal 0) =
        if bin = src then z else ComplexValue.ofReal 0) :
    (accumulateShifted exactMath gain shift omega magnitude phase graph start).get key =
      if src ∈ graph.order then ComplexValue.add (start.get key) z else start.get key := by
  unfold accumulateShifted
  apply foldl_only_coefficient _ key src _ _ _ unique
  intro table bin
  have h := rule bin
  dsimp only at h ⊢
  simp only [show exactMath.intPhase = (fun (k : ℤ) => (k : ℝ)) by rfl]
  refine Eq.trans ?_ (Eq.trans (congrArg (ComplexValue.add (table.get key)) h) ?_)
  swap
  · by_cases eq : bin = src <;> simp [eq, ComplexValue.add, ComplexValue.ofReal]
  split
  · rename_i small
    simp [ComplexValue.add, ComplexValue.ofReal]
  · rename_i small
    split
    · rename_i hnone
      rw [hnone]
      simp [ComplexValue.add, ComplexValue.ofReal]
    · rename_i out hout
      rw [addCoefficient_get, hout]
      simp only [Option.some.injEq]
      by_cases eq : out = key <;> simp [eq, ComplexValue.add, ComplexValue.ofReal]

/-- In an interior shifted lobe, the root moves to its exact pitched bin and
each neighboring follower keeps its signed bin offset from that root. -/
theorem acceptedBin_exact_translated_neighbor {n : ℕ}
    (root bin rootOut binOut : ActiveBin n)
    (offset : (rootOut.coefficient.val : ℤ) - (root.coefficient.val : ℤ) +
      (bin.coefficient.val : ℤ) = (binOut.coefficient.val : ℤ))
    (near : |(rootOut.coefficient.val : ℤ) - (binOut.coefficient.val : ℤ)| ≤ 1)
    (hop : 0 < n / 4) :
    let omega := (rootOut.coefficient.val : ℝ) / (n : ℝ)
    let shifted := shiftedBin (n := n) exactMath omega
    let target := if root = bin then shifted
      else shifted - (root.coefficient.val : ℤ) + (bin.coefficient.val : ℤ)
    acceptedBin exactMath omega target = some binOut := by
  have low : 0 < rootOut.coefficient.val := by simp [ActiveBin.coefficient]
  have high : rootOut.coefficient.val < n := by
    have := rootOut.coefficient.isLt
    omega
  have hshift := shiftedBin_exact_index low high
  dsimp only
  rw [hshift]
  by_cases eq : root = bin
  · simp only [eq, if_true] at offset ⊢
    rw [sub_add_cancel] at offset
    rw [offset]
    exact acceptedBin_exact_neighbor binOut near hop
  · rw [if_neg eq, offset]
    exact acceptedBin_exact_neighbor binOut near hop

/-- The left sideband, center, and right sideband of an interior Hann lobe. -/
def interiorBand {n : ℕ} (center : ℕ) (lo : 1 < center) (hi : center + 1 < n / 2)
    (i : Fin 3) : ActiveBin n :=
  ⟨center - 2 + i.val, by have := i.isLt; omega⟩

theorem interiorBand_coefficient {n center : ℕ} (lo : 1 < center)
    (hi : center + 1 < n / 2) (i : Fin 3) :
    (interiorBand center lo hi i).coefficient.val = center - 1 + i.val := by
  unfold interiorBand ActiveBin.coefficient
  dsimp only
  omega

private theorem interiorBand_injective {n center : ℕ} (lo : 1 < center)
    (hi : center + 1 < n / 2) : Function.Injective (interiorBand center lo hi) := by
  intro i j h
  apply Fin.ext
  have hv := congrArg (fun k : ActiveBin n => k.coefficient.val) h
  simp only [interiorBand_coefficient] at hv
  omega

private theorem active_coefficient_injective {n : ℕ} :
    Function.Injective (fun k : ActiveBin n => k.coefficient.val) := by
  intro a b h
  apply Fin.ext
  simpa [ActiveBin.coefficient] using h

private theorem outside_interiorBand {n center : ℕ} (lo : 1 < center)
    (hi : center + 1 < n / 2) (key : ActiveBin n)
    (outside : ¬∃ i : Fin 3, key = interiorBand center lo hi i) :
    key.coefficient.val ≠ center ∧
      ¬(key.coefficient.val + 1 = center ∨ key.coefficient.val = center + 1) := by
  constructor
  · intro eq
    apply outside
    refine ⟨1, active_coefficient_injective ?_⟩
    simp only [interiorBand_coefficient, Fin.val_one]
    omega
  · rintro (eq | eq)
    · apply outside
      refine ⟨0, active_coefficient_injective ?_⟩
      simp only [interiorBand_coefficient, Fin.val_zero]
      omega
    · apply outside
      refine ⟨2, active_coefficient_injective ?_⟩
      simp only [interiorBand_coefficient]
      change key.coefficient.val = center - 1 + 2
      omega

/-- The fully wet and shifted oscillator constructs exactly the three Hann
coefficients when the linked sidebands retain their opposite polarity. -/
theorem synthesizeSpectrum_shifted_hann {n : ℕ} (controls : Controls ℝ)
    (omega magnitude : BinTable ℝ n) (phase : Fin 3 → BinTable ℝ n) (graph : PVDRGraph ℝ n)
    (amplitude : ℝ) {source target : ℕ} (slo : 1 < source) (shi : source + 1 < n / 2)
    (tlo : 1 < target) (thi : target + 1 < n / 2)
    (wet : controls.fade = 1) (unison : controls.unisonGain = 0)
    (shifted : controls.shift ≠ .unity)
    (unique : graph.order.Nodup) (ordered : ∀ bin, bin ∈ graph.order)
    (small : ∀ bin, (graph.link.get bin).small = false ↔ ∃ i : Fin 3,
      bin = interiorBand source slo shi i)
    (root : ∀ i : Fin 3, (graph.link.get (interiorBand source slo shi i)).root =
      interiorBand source slo shi 1)
    (freq : omega.get (interiorBand source slo shi 1) *
      ((controls.shift.numerator : ℝ) / (controls.shift.denominator : ℝ)) = (target : ℝ) / n)
    (mag : ∀ i : Fin 3, magnitude.get (interiorBand source slo shi i) =
      if i = 1 then amplitude / 4 else amplitude / 8)
    (side : ∀ (i : Fin 3), i ≠ 1 → ∀ q : ℝ,
      exactMath.polar (amplitude / 8) (q + (graph.link.get (interiorBand source slo shi i)).delta) =
        exactMath.polar (-amplitude / 8) q) :
    synthesizeSpectrum exactMath controls omega magnitude phase graph =
      hannSinusoidSpectrum amplitude target
        ((phase 0).get (interiorBand source slo shi 1) *
          ((controls.shift.numerator : ℝ) / (controls.shift.denominator : ℝ))) := by
  rw [synthesizeSpectrum_exact_wet _ _ _ _ _ wet unison]
  dsimp only
  rw [if_neg shifted]
  apply congrArg DFT.mk
  funext k
  let src := interiorBand source slo shi
  let dst := interiorBand target tlo thi
  let q := (phase 0).get (src 1) * ((controls.shift.numerator : ℝ) / (controls.shift.denominator : ℝ))
  let radius (i : Fin 3) := if i = 1 then amplitude / 4 else -amplitude / 8
  let start : BinTable (ComplexValue ℝ) n := BinTable.ofFn fun _ => ComplexValue.ofReal 0
  change (if h : 0 < k.val then
    (accumulateShifted exactMath 1 controls.shift omega magnitude (phase 0) graph start).get
      ⟨k.val - 1, by have := k.isLt; omega⟩
    else ComplexValue.ofReal 0) =
    exactMath.polar (if k.val = target then amplitude / 4 else
      if k.val + 1 = target ∨ k.val = target + 1 then -amplitude / 8 else 0) q
  by_cases positive : 0 < k.val
  · rw [dif_pos positive]
    let key : ActiveBin n := ⟨k.val - 1, by have := k.isLt; omega⟩
    have keyVal : key.coefficient.val = k.val := by
      simp only [key, ActiveBin.coefficient, Fin.val_mk]
      omega
    have mapped (i : Fin 3) :
        let link := graph.link.get (src i)
        let ratio := (controls.shift.numerator : ℝ) / (controls.shift.denominator : ℝ)
        let rootOmega := omega.get link.root * ratio
        let rootTarget := shiftedBin (n := n) exactMath rootOmega
        let rootPhase := (phase 0).get link.root * ratio
        let position := if link.root = src i then (rootTarget, rootPhase)
          else (rootTarget - Int.ofNat link.root.coefficient.val + Int.ofNat (src i).coefficient.val,
            followerPhase rootPhase link.delta)
        acceptedBin (n := n) exactMath rootOmega position.1 = some (dst i) ∧
          exactMath.polar (1 * magnitude.get (src i)) position.2 = exactMath.polar (radius i) q := by
      dsimp only
      have off : (target : ℤ) - ((src 1).coefficient.val : ℤ) +
          ((src i).coefficient.val : ℤ) = ((dst i).coefficient.val : ℤ) := by
        simp only [src, dst, interiorBand_coefficient, Fin.val_one]
        omega
      have near : |(target : ℤ) - ((dst i).coefficient.val : ℤ)| ≤ 1 := by
        rw [abs_le]
        simp only [dst, interiorBand_coefficient]
        have := i.isLt
        omega
      have dstVal : (dst 1).coefficient.val = target := by
        simp only [dst, interiorBand_coefficient, Fin.val_one]
        omega
      have routed := acceptedBin_exact_translated_neighbor (src 1) (src i) (dst 1) (dst i)
        (by simpa only [dstVal] using off) (by simpa only [dstVal] using near) (by omega)
      dsimp only at routed
      rw [dstVal] at routed
      have rootEq : (graph.link.get (src i)).root = src 1 := root i
      have freqEq : omega.get (src 1) *
          ((controls.shift.numerator : ℝ) / (controls.shift.denominator : ℝ)) = (target : ℝ) / n := freq
      rw [rootEq, freqEq]
      constructor
      · by_cases same : src 1 = src i
        · rw [if_pos same] at routed ⊢
          exact routed
        · rw [if_neg same] at routed ⊢
          exact routed
      · have hm : magnitude.get (src i) = if i = 1 then amplitude / 4 else amplitude / 8 := mag i
        by_cases same : i = 1
        · subst i
          simp [radius, q, hm]
        · have diff : src 1 ≠ src i := fun h => same ((interiorBand_injective slo shi h).symm)
          simpa only [diff, if_false, one_mul, hm, same, radius, followerPhase] using side i same q
    have contrib (bin : ActiveBin n) :
        let link := graph.link.get bin
        let ratio := (controls.shift.numerator : ℝ) / (controls.shift.denominator : ℝ)
        let rootOmega := omega.get link.root * ratio
        let rootTarget := shiftedBin (n := n) exactMath rootOmega
        let rootPhase := (phase 0).get link.root * ratio
        let position := if link.root = bin then (rootTarget, rootPhase)
          else (rootTarget - Int.ofNat link.root.coefficient.val + Int.ofNat bin.coefficient.val,
            followerPhase rootPhase link.delta)
        (if link.small then ComplexValue.ofReal 0 else
          if acceptedBin (n := n) exactMath rootOmega position.1 = some key then
            exactMath.polar (1 * magnitude.get bin) position.2 else ComplexValue.ofReal 0) =
          if h : ∃ i : Fin 3, bin = src i then
            if dst (Classical.choose h) = key then exactMath.polar (radius (Classical.choose h)) q
              else ComplexValue.ofReal 0
          else ComplexValue.ofReal 0 := by
      classical
      by_cases h : ∃ i : Fin 3, bin = src i
      · rw [dif_pos h]
        have eq := Classical.choose_spec h
        dsimp only
        conv_lhs => rw [eq]
        have sm := (small (src (Classical.choose h))).2 ⟨_, rfl⟩
        simp only [sm, Bool.false_eq_true, if_false, (mapped (Classical.choose h)).1,
          (mapped (Classical.choose h)).2, Option.some.injEq]
      · rw [dif_neg h]
        have sm : (graph.link.get bin).small = true := by
          cases hb : (graph.link.get bin).small
          · exact False.elim (h ((small bin).1 hb))
          · rfl
        simp [sm]
    classical
    by_cases hit : ∃ i : Fin 3, key = dst i
    · obtain ⟨i, hi⟩ := hit
      have value : k.val = target - 1 + i.val := by
        simpa only [dst, interiorBand_coefficient] using (keyVal.symm.trans (congrArg (fun bin : ActiveBin n => bin.coefficient.val) hi))
      have coeff :
          (if k.val = target then amplitude / 4 else
            if k.val + 1 = target ∨ k.val = target + 1 then -amplitude / 8 else 0) = radius i := by
        have center : k.val = target ↔ i = 1 := by
          constructor
          · intro h
            apply Fin.ext
            change i.val = 1
            omega
          · intro h
            rw [h] at value
            change k.val = target - 1 + 1 at value
            omega
        by_cases mid : i = 1
        · simp [center.mpr mid, radius, mid]
        · have iv : i.val ≠ 1 := fun h => mid (Fin.ext h)
          have sideBin : k.val + 1 = target ∨ k.val = target + 1 := by
            have := i.isLt
            omega
          simp [center.not.mpr mid, radius, mid, sideBin]
      rw [coeff]
      have h := accumulateShifted_exact_get 1 controls.shift omega magnitude (phase 0) graph start
        key (src i) (exactMath.polar (radius i) q) unique (by
          intro bin
          dsimp only
          rw [contrib bin]
          split
          · rename_i same
            have he := Classical.choose_spec same
            have eq : dst (Classical.choose same) = key ↔ bin = src i := by
              constructor
              · intro h
                have sameIndex := interiorBand_injective tlo thi (h.trans hi)
                exact he.trans (congrArg src sameIndex)
              · intro h
                have sameIndex := interiorBand_injective slo shi (he.symm.trans h)
                exact (congrArg dst sameIndex).trans hi.symm
            by_cases sameKey : dst (Classical.choose same) = key
            · have sameIndex : Classical.choose same = i := interiorBand_injective tlo thi (sameKey.trans hi)
              rw [if_pos sameKey, if_pos (eq.mp sameKey), sameIndex]
            · rw [if_neg sameKey, if_neg (eq.not.mp sameKey)]
          · rename_i absent
            have ne : bin ≠ src i := fun eq => absent ⟨i, eq⟩
            rw [if_neg ne])
      rw [if_pos (ordered _)] at h
      simpa [start, BinTable.get, BinTable.ofFn, ComplexValue.add, ComplexValue.ofReal] using h
    · have zero := outside_interiorBand tlo thi key hit
      rw [keyVal] at zero
      rw [if_neg zero.1, if_neg zero.2, zeroPolar]
      have h := accumulateShifted_exact_get 1 controls.shift omega magnitude (phase 0) graph start
        key (src 1) (ComplexValue.ofReal 0) unique (by
          intro bin
          dsimp only
          rw [contrib bin]
          have none (i : Fin 3) : dst i ≠ key := fun hi => hit ⟨i, hi.symm⟩
          split <;> simp [none])
      rw [if_pos (ordered _)] at h
      simpa [start, BinTable.get, BinTable.ofFn, ComplexValue.add, ComplexValue.ofReal] using h
  · have hz : k.val = 0 := by omega
    have hside : ¬(k.val + 1 = target ∨ k.val = target + 1) := by omega
    rw [dif_neg positive, if_neg (by omega : k.val ≠ target), if_neg hside, zeroPolar]

/-- The unity shortcut preserves all three coefficients of the phase-aligned lobe. -/
theorem synthesizeSpectrum_unity_hann {n : ℕ} (controls : Controls ℝ)
    (omega magnitude : BinTable ℝ n) (phase : Fin 3 → BinTable ℝ n) (graph : PVDRGraph ℝ n)
    (amplitude : ℝ) {center : ℕ} (lo : 1 < center) (hi : center + 1 < n / 2)
    (wet : controls.fade = 1) (unison : controls.unisonGain = 0)
    (unity : controls.shift = .unity)
    (mag : ∀ i : Fin 3, magnitude.get (interiorBand center lo hi i) =
      if i = 1 then amplitude / 4 else amplitude / 8)
    (quiet : ∀ bin, (¬∃ i : Fin 3, bin = interiorBand center lo hi i) → magnitude.get bin = 0)
    (side : ∀ (i : Fin 3), i ≠ 1 →
      exactMath.polar (amplitude / 8) ((phase 0).get (interiorBand center lo hi i)) =
        exactMath.polar (-amplitude / 8) ((phase 0).get (interiorBand center lo hi 1))) :
    synthesizeSpectrum exactMath controls omega magnitude phase graph =
      hannSinusoidSpectrum amplitude center ((phase 0).get (interiorBand center lo hi 1)) := by
  rw [synthesizeSpectrum_exact_wet _ _ _ _ _ wet unison]
  dsimp only
  rw [if_pos unity]
  apply congrArg DFT.mk
  funext k
  let band := interiorBand center lo hi
  let q := (phase 0).get (band 1)
  let start : BinTable (ComplexValue ℝ) n := BinTable.ofFn fun _ => ComplexValue.ofReal 0
  change (if h : 0 < k.val then
    (accumulateDry exactMath 1 magnitude (phase 0) start).get
      ⟨k.val - 1, by have := k.isLt; omega⟩
    else ComplexValue.ofReal 0) =
    exactMath.polar (if k.val = center then amplitude / 4 else
      if k.val + 1 = center ∨ k.val = center + 1 then -amplitude / 8 else 0) q
  by_cases positive : 0 < k.val
  · rw [dif_pos positive]
    let key : ActiveBin n := ⟨k.val - 1, by have := k.isLt; omega⟩
    have keyVal : key.coefficient.val = k.val := by
      simp only [key, ActiveBin.coefficient, Fin.val_mk]
      omega
    change (accumulateDry exactMath 1 magnitude (phase 0) start).get key = _
    rw [accumulateDry_exact_get]
    have empty : start.get key = ComplexValue.ofReal 0 := by
      simp [start, BinTable.get, BinTable.ofFn]
    rw [empty, one_mul]
    have addEmpty (z : ComplexValue ℝ) : ComplexValue.add (ComplexValue.ofReal 0) z = z := by
      cases z
      simp [ComplexValue.add, ComplexValue.ofReal]
    rw [addEmpty]
    by_cases hit : ∃ i : Fin 3, key = band i
    · obtain ⟨i, eq⟩ := hit
      have value : k.val = center - 1 + i.val := by
        simpa only [band, interiorBand_coefficient] using
          (keyVal.symm.trans (congrArg (fun bin : ActiveBin n => bin.coefficient.val) eq))
      rw [eq]
      have hm : magnitude.get (band i) = if i = 1 then amplitude / 4 else amplitude / 8 := mag i
      rw [hm]
      by_cases mid : i = 1
      · subst i
        change k.val = center - 1 + 1 at value
        simp [show k.val = center by omega, q]
      · have iv : i.val ≠ 1 := fun h => mid (Fin.ext h)
        have hc : k.val ≠ center := by omega
        have hs : k.val + 1 = center ∨ k.val = center + 1 := by
          have := i.isLt
          omega
        rw [if_neg mid, if_neg hc, if_pos hs]
        exact side i mid
    · have zero := outside_interiorBand lo hi key hit
      rw [keyVal] at zero
      have hm : magnitude.get key = 0 := quiet key hit
      rw [hm, if_neg zero.1, if_neg zero.2, zeroPolar, zeroPolar]
  · have hside : ¬(k.val + 1 = center ∨ k.val = center + 1) := by omega
    rw [dif_neg positive, if_neg (by omega : k.val ≠ center), if_neg hside, zeroPolar]

end
end DSP.Resynthesis
