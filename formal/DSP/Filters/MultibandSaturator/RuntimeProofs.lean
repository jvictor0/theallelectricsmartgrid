import DSP.Filters.MultibandSaturator.Runtime
import DSP.Filters.LinkwitzRileySplitter.RuntimeProofs

namespace DSP.MultibandSaturator.Runtime
variable {R : Type} [OfNat R 0]

theorem install_save (c : PreparedSpec 4 R) (s : MultibandSaturator.State 4 R) :
    install4Band c (save s) = (save (MultibandSaturator.installPrepared c s), c.snapshot) := by
  simp only [install4Band, MultibandSaturator.installPrepared, save, restore,
    LinkwitzRileySplitter.Runtime.save, LinkwitzRileySplitter.Runtime.restore,
    LinkwitzRileySplitter.installPrepared, LinkwitzRileySplitter.Runtime.installTree_save]

theorem reset_save (s : MultibandSaturator.State 4 R) :
    reset4Band (save s) = save (MultibandSaturator.reset s) := by
  simp only [reset4Band, MultibandSaturator.reset, save, restore,
    LinkwitzRileySplitter.Runtime.save, LinkwitzRileySplitter.Runtime.restore,
    LinkwitzRileySplitter.reset, LinkwitzRileySplitter.Runtime.resetTree_save]

theorem prepare_save [OfNat R 1] [OfNat R 8] [Add R] [Mul R] [Div R] [FMA R] [Trig R]
    (spec : LogicalSpec 4 R) (s : MultibandSaturator.State 4 R) :
    prepare4Band spec (save s) =
      (save (MultibandSaturator.prepare4Band spec s).1, (MultibandSaturator.prepare4Band spec s).2) := by
  exact install_save (prepare spec) s

end DSP.MultibandSaturator.Runtime
