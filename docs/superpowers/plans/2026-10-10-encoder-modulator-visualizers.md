# Encoder Modulator Visualizers Implementation Plan

> **For agentic workers:** Execute inline using superpowers:executing-plans. Track the approved implementation below.

**Goal:** Show existing modulator visualizations behind their modulation-depth encoder dials.

**Architecture:** Publish encoder mode/type/index using shared enums. Extend the existing visualizer catalogue with optional typed modulator associations; instantiate the same constructors a second time into a fixed mode/index table. Encoder painting uses that table directly, clips to the dial square, and composites at 25% opacity.

**Tech Stack:** C++17, JUCE, doctest, iOS/Xcode.

## Global Constraints

- Preserve existing panel layout, colors, gestures, parameters, DSP data, and mouse handling.
- Three voice modes share one background table and follow the active trio.
- Sheaf, Noise, Spread, and absent sources have no background.
- No per-frame construction, searches, or duplicate constructor definitions.
- Keep panel voice selection separate from background scope selection.
- Leave changes uncommitted for the user's next review; deploy after validation.

### Task 1: Publish encoder identity

Files: `private/src/EncoderEnums.hpp`, `EncoderUIState.hpp`, `EncoderBank.hpp`, `SmartGridOneEncoders.hpp`; `private/test/unit/encoder_modulator_catalogue.cpp`.

- [x] Add a failing navigation test covering all five modes, nested selected depths at (3,3), reset, base/gesture cells and disconnected slots. Check `ui.GetMode()`, `ui.GetEncoderType(x,y)`, `ui.GetIndex(x,y)` against the actual selected source.
- [x] Move existing enums to the shared header with aliases preserving current callers. Add atomic identity fields/getters/setters; publish actual visible cell identity and clear disconnected identity.
- [x] Build `/tmp/smartgrid-parameter-pages-slots` and run the focused encoder/catalogue/reset suites.

### Task 2: Associate and draw backgrounds

Files: `JUCE/SmartGridOne/Source/ModulatorVisualizers.hpp`, `ForEachModulationVisualizer.hpp`, `SmartGridOneVisualizerMain.hpp`, `EncoderComponent.hpp`, `EncoderGrid.hpp`, `WrldBuildrComponent.hpp`; `private/test/juce-visualizers/encoder.cpp`, `CMakeLists.txt`.

- [x] Add failing JUCE tests for typed mode/index lookup, missing associations, and actual encoder painting (opacity, foreground preservation, both badge positions, connected/type filtering).
- [x] Add the optional sixth macro argument: typed modulator enum or `std::nullopt`. A local factory creates the existing panel and registers the second view once. The table owns 18 background views across three mode rows.
- [x] Pass the table by reference through the encoder grid. Construct the visualizer owner before the grids, retain existing add-to-parent order, and ensure owner outlives encoder references.
- [x] Draw before foreground with `ScopedSaveState`, dial clip/origin, and `beginTransparencyLayer(0.25f)` / `endTransparencyLayer()`.
- [x] Run native JUCE tests and inspect a rendered real-source preview.

### Task 3: Validate and deploy

Files: `docs/smart-grid-visualizers.md`.

- [x] Document associations and encoder draw path.
- [x] Run the full core suite and signed iOS build. Review the diff for redundant state, duplicate logic, and unrelated changes.
- [x] Deploy to iPad with `make ios-deploy`; report test results and deployment status.

## Validation

- Core suite: 569 cases, 90,656,250 assertions passed.
- Native JUCE suite: 38 cases, 64,774 assertions passed.
- Inspected real visualizer renders for Voice, Quad, and Global modes.
- Signed iOS Release build succeeded.
- Diff review: one fixed owning table; shared constructors; direct lookup; no per-frame view construction or searches.
- iPad upgrade installed over Wi-Fi and app launched successfully.

### Follow-up: stronger backgrounds and single-source parameters

- [x] Raise encoder background compositing to 50%; retain full foreground opacity and clipping.
- [x] Draw ganged-random and both quad LFO families' indicator dots in their supplied modulator color. Preserve path colors and Sheafy rendering.
- [x] Let base parameters show their single eligible source using the existing affecting-modulator mask. Ignore gesture badges; keep depth encoders bound to their own source, including nested depths.
- [x] Verify opacity, marker pixels, source eligibility, and transitions with native rendering tests; build and deploy the updated iPad app.

Follow-up clarification: count only modulators with associated visualizers. An LFO plus Noise shows the LFO; two eligible sources show neither. Native tests: 41 cases, 64,805 assertions passed. Updated iOS Release build succeeded. Inspected real Voice and Quad renders including the selected top-level parameter.
Follow-up deployed over Wi-Fi and launched successfully on iPad.

Final palette adjustment: swap RND2 and LFO2 between Mauve and Purple in Voice and Quad modes. Global has no LFO2 and retains its existing colors.
