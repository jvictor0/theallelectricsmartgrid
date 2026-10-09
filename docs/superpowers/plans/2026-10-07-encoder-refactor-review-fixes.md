# Encoder refactor review fixes

**Goal:** Finish the five approved fixes found during the encoder refactor review.

**Architecture:** Keep the twenty internal banks and three independent Voice modes. Physical controllers select four Voice families, and the selected trio resolves each family to its internal bank. Visualizers classify by family while retaining the active trio for data. Recording headers and block tags agree on format v5; replay applies the same scene-loading semantics as the synth.

**Global Constraints**

- Preserve all existing uncommitted refactor work. Leave this task's changes uncommitted so they remain reviewable alongside that work.
- Old-patch upgrading is outside this task's scope, as agreed with the user.
- Apply repository C++ conventions, including new-line braces, public structs, HammerCase methods, and no newly introduced default arguments.
- Restrict production edits to the five approved issues. No unrelated DSP or encoder redesign.
- Use behavioral regressions with independent expected results; do not mask regressions by loosening assertions.

### Task 1: Fix controller selection, trio displays, and v5 recording compatibility

Read this task together with the Global Constraints supplied by the coordinator. There are five related defects in the current uncommitted refactor:

1. Both controller headers still call removed `BankFromOrdinal`, so the standalone build fails.
2. Their physical Voice selector loops use `x_numVoiceBanks`, now twelve, overrunning the four-selector layout and the Launchpad grid storage.
3. Recording headers declare v5 but `EncodeBlock` still writes `BLK4`, so the Python reader rejects nonempty recordings.
4. Visualizer bank and mode dispatch only matches Water aliases; Fire and Earth lose family panels, modulation panels, and the correct synth UI display mode.
5. Python replay preserves omitted trailing scenes in a supplied short flat `values` array; live loading clears them to zero.

**Files**

- Modify `private/src/SmartGridOneEncoders.hpp` and the controller loops in `private/src/TheNonagonSquiggleBoyQuadLaunchpadTwister.hpp` and `private/src/TheNonagonSquiggleBoyWrldBldr.hpp`.
- Modify bank/mode dispatch in `JUCE/SmartGridOne/Source/SmartGridOneVisualizerMain.hpp` and the display-mode selection in `private/src/SquiggleBoy.hpp`.
- Modify `private/src/RecordingFormat.hpp` and `scripts/sgrec.py`.
- Add focused controller/display regressions under `private/test/`; extend the existing encoder/recording tests only where coverage is missing.
- Check the encoder and recording documentation/specifications already updated in this worktree and adjust only if these fixes change their contract.

**Implementation and validation**

- [x] Add controller regressions that exercise the four physical Voice selectors for all three trios, and the Quad/Global selector positions, in both controller layouts. Assert concrete expected banks. Cover the area after the fourth Voice selector so extra internal banks cannot become physical selectors.
- [x] Add display-mode regressions for all four Voice families across all three trios, keeping nonvoice modes covered. Exercise actual `PopulateUIState` behavior. Cover JUCE family/mode dispatch behavior if the existing visualizer test target can support it without a broad harness redesign.
- [x] Confirm red evidence before implementing: the current full build has six missing `BankFromOrdinal` errors; current C++ recording tests fail on `BLK5`; the Python short-scene test fails. Run new display regressions before fixing display selection if practical.
- [x] Define explicit selector-bank lists containing four Voice family aliases (`Source`, `FilterAndAmp`, `PanningAndSequencing`, `VoiceLFOs`), four Quad banks (`Delay`, `Reverb`, `PartialMachine`, `QuadLFOs`), and four Global banks (`TheoryOfTime`, `Mastering`, `Inputs`, `DeepVocoder`). Iterate those lists in both controllers. Retain twelve internal Voice banks and their existing ordinal layout.
- [x] Normalize bank families using `BankForTrio(bank, 0)` for the visualizer paint/click family comparisons and the synth display-mode switch. Match all Voice trio modes for the Voice modulation panel via `IsVoiceMode`. Keep actual selected trio/bank when reading data or dispatching modulation behavior.
- [x] Add one format-version constant in `RecordingFormat` and use it for the JSON header and emitted block tag. The resulting header must contain version 5 and blocks must begin with `BLK5`. Keep the reader's legacy format support.
- [x] In Python replay, when a flat `values` array is present, write all eight scenes and use zero for omitted entries. When the field is absent, preserve existing values. Preserve existing bipolar conversion and nested legacy replay behavior.
- [x] Run targeted controller, encoder, display, recording, and Python tests. The coordinator will also run a full standalone build/test pass, controller AddressSanitizer checks, a desktop compile, and C++-generated recording fixtures through Python.
- [x] Inspect the final diff for unrelated changes and report the exact test commands/results and any remaining failures.

**Existing evidence and commands**

The standalone CMake build is `/tmp/encoder-refactor-review/build`. Its prior failed build log is `/tmp/encoder-refactor-review/build-updated-tests.log`. Use a separate focused build/output path if needed; the coordinator owns the full build directory during integration checks. The latest focused encoder/recording log is `/tmp/encoder-refactor-review/focused-tests.log` (37 of 39 cases passed; both failures concern the v5 tag). Python's baseline is 50 tests, one short-scene failure, five optional fixture tests skipped.

Python command: `PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s scripts/tests -p test_sgrec.py`.

Standalone command after changes: `cmake --build /tmp/encoder-refactor-review/build -j6`, followed by the built `smartgrid_tests` with an appropriate doctest filter.

Spec checks: `openspec validate encoder-parameter-system --type spec --strict`, `openspec validate recording-extraction --type spec --strict`, and `openspec validate streaming-recording --type spec --strict`.

**Completion:** All five issues are fixed, their targeted regressions pass, reviews are resolved, and any unrelated full-suite failures are reported accurately without changing old-patch migration.

## Verification results

- Full standalone build: passed.
- Full standalone suite: 541 test cases and 90,496,759 assertions passed, no skips.
- Python recording suite with all real C++ fixtures: 50 tests passed, no skips.
- Python patch extraction followed by C++ reload: passed, 20 assertions.
- Controller/display regressions under AddressSanitizer: four cases and 102 assertions passed.
- macOS universal Release build: passed.
- All three OpenSpec strict validations and `git diff --check`: passed.
- Task review: approved with no blocking findings. Direct JUCE paint/click dispatch remains a coverage limit; the existing visualizer test harness does not exercise the main panel wrapper.
- Final whole-worktree review: approved, no new actionable regressions. Changes remain uncommitted.
