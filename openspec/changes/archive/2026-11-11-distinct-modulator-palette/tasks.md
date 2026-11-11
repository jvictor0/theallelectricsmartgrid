## 1. Palette and presentation

- [x] 1.1 Add shared named palette constants and apply the approved slot colors to every source catalogue row.
- [x] 1.2 Share Sheafy colors with corresponding Nonagon cells and bind associated parameter encoders to source colors.

## 2. Documentation and validation

- [x] 2.1 Document the exact palette, functional pairs, source/control associations, and perceptual constraints in encoder documentation and current specifications.
- [x] 2.2 Update independent presentation baselines and validate local-slot consistency, Nonagon agreement, and parameter encoder colors.
- [x] 2.3 Run the relevant core/native validation and record results.

## Validation

- Native visualizers: `make visualizers-test JOBS=6` passed all 84 cases / 328,809 assertions. The Sheafy spoke check now allows the alpha-dependent rounding of 8-bit premultiplied edge pixels (two RGB units for translucent pixels, one for opaque pixels); production drawing is unchanged.
- `openspec validate distinct-modulator-palette --strict`, `openspec validate grid-visualizers --type spec --strict`, and `git diff --check` passed.
- Core: `make core-test JOBS=6` passed both CTest targets with the default RelWithDebInfo configuration and enabled assertions (914 main-suite cases passed; 4 existing cases skipped). Coverage includes every populated local slot, matching Nonagon colors, all associated encoder RGB baselines, and inherited gesture colors after patch load.
- Build/test logs are retained under `build/results/modulator-palette-*.log` in this worktree.
