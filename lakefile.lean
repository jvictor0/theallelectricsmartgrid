import Lake
open Lake DSL

package smartgrid where
  srcDir := "formal"
  buildDir := "build/lean/artifacts"
  packagesDir := "build/lean/packages"

require veil from git "https://github.com/verse-lab/veil.git" @ "e737e7b8fb4b61bdee837800ab6b97ee894e28c3"
require mathlib from git "https://github.com/leanprover-community/mathlib4.git" @ "v4.32.0"

@[default_target]
lean_lib Encoder where
  globs := #[.submodules `Encoder]
  leanOptions := #[⟨`autoImplicit, false⟩, ⟨`warningAsError, true⟩]
