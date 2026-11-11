## 1. Glyphs and assignments

- [x] 1.1 Add six rounded path glyphs and cache their transparent renders alongside existing badges.
- [x] 1.2 Apply role-specific catalogue glyphs and publish source-pair glyphs using the owning trio's source machine.

## 2. Verification and documentation

- [x] 2.1 Validate source-machine switching, source ownership, role mappings, and native rendering; inspect a rendered size comparison.
- [x] 2.2 Document the assignments and verified cosine convention; sync current requirements and complete core/native checks.

## Validation

- Native: `SMARTGRID_BADGE_PREVIEW="$PWD/build/results/modulator-badges.png" make visualizers-test JOBS=3` passed all 85 cases / 337,335 assertions.
- Inspected the actual cached glyphs with existing symbols and colored badge backgrounds at 16/24/32/64 px. The preserved comparison is `docs/images/modulator-badges.png`.
- Core source-owner publication and mixer role tests passed on the first full run. That run exposed an older downsampling test that inferred DSP rate from glyphs; it now checks source identities, preserving the production rate flags. The final `make core-test JOBS=6` passed both CTest targets: 916 main-suite cases and 39 passthrough-rate cases; 4 existing main-suite cases remain skipped. The default RelWithDebInfo configuration and enabled assertions were retained.
- The change and current grid-visualizers specification pass strict OpenSpec validation; `git diff --check` passes.
