import DSP.Filters.MultibandSaturator.Exports
import DSP.Filters.MultibandSaturator.MonoBassSmallSignal

namespace DSP.MultibandSaturator

variable {c : Nat} [NeZero c]

theorem step4Band_independent (s : State 4 (Fin c → ℝ)) (x : Fin c → ℝ) :
    step4Band s ⟨x,false⟩ = step s ⟨x⟩ := by
  simp [step4Band,step,stepWith]

theorem step4Band_mono (s : State 4 (Fin c → ℝ)) (x : Fin c → ℝ) :
    step4Band s ⟨x,true⟩ = stepMonoBass s ⟨x⟩ := rfl

theorem install4Band_prepare (spec : LogicalSpec 4 ℝ) (s : State 4 ℝ) :
    install4Band (prepare spec) s = prepare4Band spec s := rfl

end DSP.MultibandSaturator
