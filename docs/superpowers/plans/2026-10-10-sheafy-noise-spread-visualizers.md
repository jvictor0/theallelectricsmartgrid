# Sheafy, Noise, and Spread Encoder Visualizers

**Goal:** Add encoder-only views for the remaining modulators, preserving the existing bottom panels and the 50% background composition.

**Design:** A single-Sheafy dial shares the encoder's 270-degree angle mapping and fits inside its innermost voice ring. It draws attainable current-lens/time-slice values as spokes and the actual held modulation value of each active-trio voice as a filled concentric arc. Publish the 27 held timbre values; do not substitute pending pitch choices. Adapt `~/Sheaf`'s noise polyline and narrow constant bars to existing JUCE drawing, without importing its framework or adding DSP processing. Noise uses a local UI random generator; Spread shows the existing fixed three/four voice values.

**Catalogue:** Add an encoder-only entry form to the existing modulation visualizer catalogue so these views get neither bottom-panel objects nor bottom-panel dispatch. Existing depth lookup and single-eligible-source base-parameter lookup apply unchanged.

**Constraints:** Keep current colors, bottom panels, DSP semantics, and mouse handling. No visualization history, per-frame view construction, or additional source registry. New Noise/Spread associations now count toward the exactly-one-source rule. Leave this iteration uncommitted.

- [x] Add failing tests for held timbre publication (including pending choices/unison), Sheafy angle/attainability/trio rendering, Noise redraws, Spread voice bars, and encoder-only catalogue associations.
- [x] Implement the minimal publication, shared encoder angle helper, and three visualizer structs.
- [x] Register encoder-only sources through the existing catalogue and update documentation.
- [x] Run the focused core and native visualizer suites, inspect real encoder renders, build iOS, and review for duplicate code or unnecessary state.

Validation: all 570 core tests (90,656,304 assertions) and 45 native visualizer tests (96,185 assertions) passed. Inspected actual encoder renders for Voice, Quad, and Global modes. iOS Release build succeeded. Only the existing `PatchArena.hpp` unused-variable warning appeared. No deployment or commit in this iteration.

Cleanup: shared the encoder angle formula, reused the existing mode/index table and source-selection rules, and added only the 27 held timbre values needed by the UI. Encoder-only catalogue entries create one object each with no panel objects or dispatch. Noise randomness stays on the UI side; Spread computes its existing fixed values directly.

Follow-ups: deployed the initial views, then removed the three legacy Sheafy catalogue brightness multipliers as requested and redeployed. That color change passed 51 focused core tests and all 45 native visualizer tests.

Spread correction: centered its bars inside the innermost encoder ring and changed them to active-trio voice colors (Voice) or fixed voice colors (Quad). Replaced the fixed color member with the existing active-trio pointer; added no registry or color array. All 45 native tests passed (311,867 assertions), including bar bounds, heights, colors, and trio changes. Inspected fresh Voice and Quad encoder renders. This correction is not yet deployed.
