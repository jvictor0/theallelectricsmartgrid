import DSP.Filters.MultibandSaturator.SmallSignal
import DSP.Filters.LinkwitzRileySplitter.PreparedChannels

/-! Mono bass is linear channel routing before the memoryless saturations.
Its response acts on a vector of complex input amplitudes, including phase. -/
noncomputable section
namespace DSP.MultibandSaturator
open scoped BigOperators
variable {n c : Nat}

/-- Average band zero across input channels; retain the selected output lane
for every higher band. The same map is used on real signals and complex modes. -/
def bassRoute (K : Type) [Field K] (lane : Fin c) :
    (Fin n → Fin c → K) →ₗ[K] (Fin n → K) where
  toFun x i := if i.val=0 then (∑ j, x i j)/(c:K) else x i lane
  map_add' x y := by ext i; by_cases h : i.val=0 <;> simp [h,Finset.sum_add_distrib,add_div]
  map_smul' a x := by
    ext i
    by_cases h : i.val=0
    · simp only [h,ite_true,Pi.smul_apply,smul_eq_mul,RingHom.id_apply]
      rw [← Finset.mul_sum,mul_div_assoc]
    · simp [h]

def bassRouteContinuous (lane : Fin c) :
    (Fin n → Fin c → ℝ) →L[ℝ] (Fin n → ℝ) :=
  (bassRoute ℝ lane).toContinuousLinearMap

theorem bassRoute_real (lane : Fin c) (x : Fin n → Fin c → ℂ) (q : ℂ) :
    bassRoute ℝ lane (fun i j => (x i j*q).re) =
      fun i => (bassRoute ℂ lane x i*q).re := by
  ext i
  simp only [bassRoute,LinearMap.coe_mk,AddHom.coe_mk]
  split
  · rw [div_mul_eq_mul_div,Finset.sum_mul]
    have hc : (c:ℂ) = ((c:ℝ):ℂ) := by simp
    rw [hc,Complex.div_ofReal_re,Complex.re_sum]
  · rfl

theorem sum_channels {K : Type} [AddCommMonoid K] (x : Fin n → Fin c → K) (j : Fin c) :
    sum x j = ∑ i, x i j := by simp [sum_eq_sum,Finset.sum_apply]

theorem sum_except_zero [NeZero n] {K : Type} [AddCommMonoid K] (x : Fin n → K) :
    (∑ i, if i.val=0 then 0 else x i) + x 0 = ∑ i, x i := by
  have he (i : Fin n) : (if i.val=0 then 0 else x i) + (if i=0 then x 0 else 0) = x i := by
    by_cases h : i=0
    · simp [h]
    · have hi : i.val ≠ 0 := by simpa using h
      simp [h,hi]
  calc
    _ = (∑ i, if i.val=0 then 0 else x i) + ∑ i : Fin n, if i=0 then x 0 else 0 := by simp
    _ = ∑ i, x i := by rw [← Finset.sum_add_distrib]; exact Finset.sum_congr rfl (fun i _ => he i)

theorem stepMonoBass_output_eq [NeZero n] [NeZero c]
    (s : State n (Fin c → ℝ)) (x : Fin c → ℝ) (lane : Fin c) :
    (stepMonoBass s ⟨x⟩).2.master.after lane =
      SaturatingSum.apply (fun i => s.bandGain i lane) (s.masterGain lane)
        (bassRoute ℝ lane (LinkwitzRileySplitter.step s.splitter ⟨x⟩).bands) := by
  simp only [stepMonoBass,stepWith,StateSpace.snapshot_values,Bool.true_and,
    Tanh.tanh,Pi.add_apply,Pi.mul_apply,ite_true]
  rw [sum_channels]
  simp only [meanChannels,sum_eq_sum,SaturatingSum.apply_eq,
    bassRoute,LinearMap.coe_mk,AddHom.coe_mk,beq_iff_eq,ite_apply,Pi.zero_apply,
    Fin.val_zero,ite_true]
  have hc : (OfNat.ofNat c : ℝ) = (c:ℝ) := by
    cases c with
    | zero => simp
    | succ c => cases c with
      | zero => simp
      | succ c => rfl
  simp only [hc]
  have h := sum_except_zero (fun i : Fin n => Real.tanh
    ((if i.val=0 then (∑ j, (LinkwitzRileySplitter.step s.splitter ⟨x⟩).bands i j)/(c:ℝ)
      else (LinkwitzRileySplitter.step s.splitter ⟨x⟩).bands i lane) * s.bandGain i lane))
  simp only [Fin.val_zero,ite_true] at h
  rw [h]
  congr 1
  ring

theorem monoBass_output_eq [NeZero n] [NeZero c] (s : State n (Fin c → ℝ))
    (u : ℕ → Fin c → ℝ) (k : ℕ) (lane : Fin c) :
    (Processor.run stepMonoBass (fun i => ⟨u i⟩) s k).master.after lane =
      SaturatingSum.apply (fun i => s.bandGain i lane) (s.masterGain lane)
        (bassRoute ℝ lane (Step.states LinkwitzRileySplitter.step (fun i => ⟨u i⟩) s.splitter (k+1)).bands) := by
  have hg := runState_gains true meanChannels (Tanh.tanh : (Fin c → ℝ) → _) s (fun i => ⟨u i⟩) k
  have hs := runState_splitter true meanChannels (Tanh.tanh : (Fin c → ℝ) → _) s (fun i => ⟨u i⟩) k
  change (stepMonoBass (Processor.runState stepMonoBass (fun i => ⟨u i⟩) s k) ⟨u k⟩).2.master.after lane = _
  rw [stepMonoBass_output_eq]
  change SaturatingSum.apply (fun i => (Processor.runState (stepWith true meanChannels Tanh.tanh) _ s k).bandGain i lane)
    ((Processor.runState (stepWith true meanChannels Tanh.tanh) _ s k).masterGain lane)
    (bassRoute ℝ lane (LinkwitzRileySplitter.step (Processor.runState (stepWith true meanChannels Tanh.tanh) _ s k).splitter ⟨u k⟩).bands) = _
  rw [hg.1,hg.2,hs]
  rfl

end DSP.MultibandSaturator
