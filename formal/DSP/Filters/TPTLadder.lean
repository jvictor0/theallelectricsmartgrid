import DSP.Filters.OnePoleTPTLowPass

namespace DSP

/-- Idealized linear ladder: four TPT lowpasses in series with global
feedback k from the last output to the input, solved without a unit delay.
Each stage's output is affine in its input, y = S + G x, so the loop
e = x - k y₄ has the closed form e = (x - k S) / (1 + k G₁ G₂ G₃ G₄), where S
is the cascade's response to a zero input. `drive` stores the solved loop
input e, and the output is `stage4.y`. -/
structure TPTLadder (R : Type) where
  k : R
  drive : R
  stage1 : OnePoleTPTLowPass R
  stage2 : OnePoleTPTLowPass R
  stage3 : OnePoleTPTLowPass R
  stage4 : OnePoleTPTLowPass R

namespace TPTLadder

variable {R : Type} [OfNat R 0] [OfNat R 1] [Add R] [Sub R] [Mul R] [Div R]

def step (f : TPTLadder R) (input : Sample R) : TPTLadder R :=
  let response := f.stage4.respond (f.stage3.respond (f.stage2.respond (f.stage1.respond 0)))
  let gain := f.stage1.G * f.stage2.G * f.stage3.G * f.stage4.G
  let drive := (input.x - f.k * response) / (1 + f.k * gain)
  let stage1 := f.stage1.step ⟨drive⟩
  let stage2 := f.stage2.step ⟨stage1.y⟩
  let stage3 := f.stage3.step ⟨stage2.y⟩
  { f with drive, stage1, stage2, stage3, stage4 := f.stage4.step ⟨stage3.y⟩ }

def stepFloat : Step (TPTLadder Float) (Sample Float) := step

end TPTLadder
end DSP
