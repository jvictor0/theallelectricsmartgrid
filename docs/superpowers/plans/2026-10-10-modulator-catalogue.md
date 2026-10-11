# Typed Modulator Catalogue Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [x]`) syntax for tracking.

**Goal:** Replace the modulator skin switch and index-based color helpers with one typed catalogue, removing phantom Global skins.

**Architecture:** Explicit per-mode enums preserve numeric source indices. One macro emits metadata for direct mode/index skin lookup. Existing encoder state, source computation and visualizer registration remain in place.

**Tech Stack:** C++17, doctest, existing CMake and JUCE desktop builds.

## Global Constraints

- Exactly `F(enumType, enumValue, mode, color, glyph)` in `ForEachSmartGridOneModulator.hpp`; explicit enums VoiceModulator, QuadModulator, GlobalModulator preserve existing indices.
- Voice metadata is shared by all three trios. Modes are existing Voice/Quad/Global classifications.
- Global only defines sources at indices 2, 3, 14. Remove phantom colors/glyphs at 6, 7, 11; absent entries are Off/None.
- Preserve the fifteen-position encoder allocation/connection and persisted array contract. Skin existence must not gate storage or signal behavior. No new source availability mechanism.
- Preserve every real source color/glyph/index, every parameter field/color, and patch/recording names and formats. Named equivalent half-bright RGB colors are allowed.
- No visualizer routing expansion, runtime catalogue scan on lookup, duplicated state, parameter association table, or generic registry.
- Keep C++ changes in repository style. No new default arguments or C casts.
- Existing isolated worktree remains open; no deployment, publication or landing.

## Task 1: Implement and verify the typed catalogue

**Files:**
- Create `private/src/ForEachSmartGridOneModulator.hpp` and a small metadata/enums helper header if needed, preferably `SmartGridOneModulators.hpp`.
- Modify `private/src/SmartGridOneEncoders.hpp`, `private/src/ForEachSmartGridOneParam.hpp`, `private/src/SquiggleBoy.hpp`.
- Modify `private/src/Color.hpp` only for exact equivalent named colors or constexpr definitions needed by lookup construction.
- Mechanically adapt color helpers in `JUCE/SmartGridOne/Source/ForEachModulationVisualizer.hpp` and `ForEachSmartGridOneVisualizer.hpp`.
- Add focused tests `private/test/unit/encoder_modulator_catalogue.cpp`; use existing parameter baseline and signal tests.
- Update only relevant paragraphs in `docs/encoder-system.md` and `openspec/specs/encoder-parameter-system/spec.md` if necessary.

**Interfaces:**
- Consumes existing GetModulatorSkin(index, BankMode), ModulatorValues arrays and all existing source assignments.
- Produces explicit typed enums, GetModulatorColor(enumValue), and direct numeric GetModulatorSkin compatibility lookup.

- [x] Add independent expectation tests for all fifteen positions in each mode. Use literal RGB/glyph expectations, not the new catalogue as the expected side. Existing half-bright values are Orange(127,54,14), Yellow(127,124,31), Cyan(0,127,127), Indigo(28,30,124), SeaGreen(4,123,29); Pink(255,50,120), Purple(134,63,249). All three voice modes match. Global6/7/11 must be Off/None and must fail against the old switch before implementation.
- [x] Test real UI selection/processing and patch-restored nonneutral depth in a removed Global skin position. Assert finite output, stable indices/owned cells, no crash, and Off/None presentation. Preserve existing signal/connection semantics; removing color must not gate processing.
- [x] Define enums using existing numeric indices: Voice random0..3, ModulationEnvelope4, AmpEnvelope5, LFO1/2 at6/7, Sheaf1/2/3 at8/9/10, Spread11, Noise14; Quad random0..3, DelayLFO4, ReverbLFO5, LFO1/2 at6/7, Spread11, Noise14; Global Random1=2, Random2=3, Noise14. Naming can follow existing GangedRandom terminology without changing indices.
- [x] Emit catalogue metadata and build one small direct table defaulting to Off/None. Validate row indices, enum/mode consistency and duplicates. Keep the skin struct's two fields. Derive mode for typed lookups using straightforward overloads; avoid generic registration machinery.
- [x] Replace the skin switch and family color helpers. For example the LFO1 parameter color becomes `GetModulatorColor(VoiceModulator::LFO1)`. Numeric GetModulatorSkin maps trio bank modes to one Voice metadata row. Change visualizer color calls only as required to remove old helpers.
- [x] Replace source index literals with enum indices in SetVoiceModulators/SetQuadModulators/SetGlobalModulators. Preserve signal sources, formulas and index order. Keep existing loops for contiguous random sources; do not replace direct DSP assignments with function-pointer tables.
- [x] Run focused tests, including catalogue/binding/color baseline, quad source behavior, patch and trio coverage. Inspect code for redundant descriptors, unused helpers and accidental new runtime work. Commit implementation and write evidence/report.

## Task 2: Final verification and cleanup review

**Files:** Implementation plan verification record; scoped corrections identified by review.

- [x] Run full standalone suite once implementation review is clean: `cmake --build /tmp/smartgrid-parameter-pages-slots -j6`, then `/tmp/smartgrid-parameter-pages-slots/smartgrid_tests`. Export existing four recording fixture variables to fresh `/tmp/modulator-catalogue-*` paths.
- [x] Run `PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s scripts/tests -p test_sgrec.py` using those same fixture environment variables.
- [x] Run `make build` with normal Xcode cache permissions; do not deploy or run the app.
- [x] Validate affected OpenSpec requirements with `openspec validate encoder-parameter-system --type spec --strict` and run `git diff --check` over the whole change.
- [x] Request independent final review focused on data preservation, phantom-skin safety, duplicated code, scope creep, unnecessary structures and dead helpers. Fix concrete findings and record actual verification evidence below.

## Verification record

Completed at implementation commit `7d6eb26`.

- Red before production: two cases, 186 assertions, 18 expected failures covering the removed Global skins in lookup and actual UI state. Saved-depth restoration, selection ownership and numeric processing already passed.
- Focused green: 36 cases and 11,415 assertions passed, including the independent parameter baseline and source-index assignments.
- Full standalone suite: 561 cases, 90,507,916 assertions passed, zero failures or skipped cases. Existing warning-only cases remain outside this refactor.
- Recording interoperability: all 50 Python tests passed with the four C++ fixture exports enabled.
- Desktop Release build: `make build` succeeded for arm64 and x86_64.
- Valid catalogue compiled; isolated duplicate-position, enum/mode mismatch and out-of-range catalogue mutations were rejected by the C++17 compiler.
- Separate source-data audit preserved all 176 parameter ordinals, labels, descriptions, defaults, modes, switches and polarity. The frozen encoder baseline verified unchanged parameter colors.
- Strict encoder-parameter-system OpenSpec validation and full-range whitespace checks passed.
- Independent task review and final whole-change review found no actionable correctness, safety, duplication or scope issues. The final reviewer traced unchanged nesting, connection, selection, JSON and recording consumers as well as the direct table.
- Cleanup removed the old skin switch and five family color helpers, consolidated repeated lookup bodies into one typed accessor, and retained one constexpr table with no additional runtime descriptor/state registry.

Work remains committed in the existing worktree for continued development. No publication or deployment was performed.
