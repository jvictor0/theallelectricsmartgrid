## Context

The current external-input path is built around four mono `SourceMixer` sources. `MainComponent` requests four JUCE input channels, `NonagonWrapper` clamps the active input count to four, and `SourceMixer::Input::SetInputs` writes each active input directly into a mono source slot. The Squiggle Boy config grid lets each trio select from those sources, but `PropagateSourceSelection()` repeats selected sources across all three voices and prevents a trio from having no selected source.

The requested behavior changes the source mixer into four configured external inputs. Each input can be mono or stereo, selected stereo inputs consume two trio voices, and unfilled voices stay silent. Source colors also need to be available from DSP code so config-grid buttons, meters, and visualizers all agree on the same color.

The source metering UI currently draws one mono meter per source from the existing mixer input meters. With stereo-configured sources, the meter strip and source mixer reduction visualizer need to draw both source lanes from those existing mixer input meters without taking extra horizontal space from the rest of the metering strip.

## Goals / Non-Goals

**Goals:**

- Request enough JUCE input channels to feed four configured source slots with left/right lanes, for a maximum of eight physical input channels.
- Store per-source mono/stereo width in the source mixer state/UIState path, and expose source colors from source mixer DSP code.
- Represent trio Thru assignment as an ordered list of selected source channels, where mono expands to one channel and stereo expands to left/right.
- Make unassigned trio voices silent instead of filling them by repeating selected sources.
- Expose four source selectors and four mono/stereo toggles on the Squiggle Boy config grid.
- Persist source selection and source width so saved patches/config restore the same routing.
- Render source mixer meters as paired left/right lane meters within each source's existing footprint.

**Non-Goals:**

- Do not change the fixed nine-voice, three-trio architecture.
- Do not introduce true stereo voices; stereo inputs are still routed as two mono voice assignments.
- Do not change non-Thru source machines except for carrying harmless source assignment state.
- Do not change output channel layout or stereo/quad mixdown behavior.
- Do not shrink or rearrange quadraphonic return meters.

## Decisions

1. Source mixer owns source configuration.

   Add a `SourceMixer::SourceConfig` or equivalent source-owned configuration object with width (`Mono` or `Stereo`). `SourceMixer::Input` should carry one config per source so the audio population path can decide whether a source's right lane is live or silent. Source colors should be exposed by source mixer DSP code via a static source-color helper, and UIState may publish those colors as packed values for consumers that need state snapshots.

   Alternative considered: keep colors and width interpretation in the config UI. That would keep the first edit smaller, but it would split source truth across UI and DSP and would not satisfy the requirement that color assignment move into DSP.

2. Four configured sources, each with left/right lanes.

   Keep `SourceMixer::x_numSources` at four and give each source enough state for left and right source channels. Mono configuration populates the left lane from the configured input and populates the right lane with silence. Stereo configuration populates both lanes from the configured source's left/right physical lanes. Source 0 maps to physical inputs 0/1, source 1 maps to 2/3, source 2 maps to 4/5, and source 3 maps to 6/7. This preserves a uniform source shape at the cost of processing an always-present silent lane for mono sources, which is an accepted CPU trade-off.

   Alternative considered: dynamically allocate one or two source channels per input. That would save CPU but would make audio-thread indexing and trio assignment more branchy.

3. Voice Thru assignment becomes source-channel assignment.

   Replace the single voice `m_sourceIndex` interpretation with a source assignment that can represent Silent, Source Left, or Source Right. `SquiggleBoySource` should output zero for Silent assignments; otherwise it upsamples the assigned source channel into the voice filter path.

   Alternative considered: encode right channels as synthetic source indices. That would reuse more current code, but it obscures the relationship between a configured source and its stereo pair, and complicates color/UI mapping.

4. Trio assignment expands selected sources once, with no infill.

   For each trio, build an ordered channel list from selected sources. A mono selected source contributes one Left assignment. A stereo selected source contributes Left then Right. Assign list entries to the trio's three voices in order. Voices beyond the list length become Silent. There is no modulo repeat, so selecting only one mono source leaves two voices silent, and selecting only one stereo source leaves one voice silent.

   When a press or mono-to-stereo toggle would produce more than three selected channels, rerun assignment enforcement before propagation. Prefer preserving the user's latest action and unselecting older selected sources in deterministic source order until the selected channel count is at most three. If preserving the latest action alone would still exceed three channels, reject or revert that action.

   Alternative considered: keep the existing infill behavior for mono-only selections. That would preserve old defaults but conflicts with the desired explicit silence behavior.

5. Configuration grid uses source-colored controls.

   The source selector cluster, monitor cluster, deep-vocoder cluster, and new stereo-toggle cluster should each draw active and inactive states from the matching source color. The stereo toggle is on when the source is Stereo and off when Mono. Toggling width must rerun trio assignment enforcement for every trio because the selected channel count can change without a source selector press.

   Alternative considered: add only a patch/config page control and leave grid buttons white. That would make the setting reachable but would leave the hardware/grid surface inconsistent with the visual source identity.

6. Source metering keeps mono layout and splits stereo source footprints.

   The meter strip and source mixer reduction visualizer should treat each source as a fixed-width group. The visualizers should read the existing mixer input meters for source lanes, not a separate SourceMixer meter. A Mono source keeps the existing two-slot layout, with the meter and reduction horizontally offset from each other. A Stereo source splits the same source footprint into four half-width horizontal slots ordered left reduction, left meter, right meter, and right reduction. Quad return meters keep their current channel and reduction slot widths.

   Alternative considered: double the number of source meter slots. That would make the stereo lanes visually straightforward but would steal width from voice, quad return, or master meters and conflicts with keeping the total source meter footprint stable.

## Risks / Trade-offs

- Silent right-lane processing can raise CPU use -> keep the implementation simple first, then profile before optimizing.
- Existing patches saved with four mono sources have no width data -> default missing source widths to Mono and missing source selections to unselected.
- Stereo width changes can unexpectedly unselect sources when a trio is already full -> make enforcement deterministic and test the exact overflow cases.
- Moving color access can cause visual regressions if some visualizers keep using presentation-only helpers -> update all source color consumers to query the source mixer DSP helper or its UIState-published equivalent.
- A silent source assignment must not read invalid source indices on the audio thread -> model Silent explicitly and test Thru output for silent voices.
- Stereo source metering can accidentally shrink quad meters if the total slot math is changed globally -> keep source-specific split math localized to source meter drawing.

## Migration Plan

1. Introduce the new source config and assignment representation with compatibility defaults.
2. Keep four configured sources and expand the JUCE input request to eight physical lanes.
3. Update config-grid persistence so missing width data loads as Mono and missing selected-source entries load as false, except the existing default source selection policy.
4. Update UI color consumers to use DSP-owned source colors.
5. Update source metering to draw left/right lanes in the existing source meter footprint using the existing mixer input meters.
6. Add focused tests for assignment, input population, and stereo source metering before broad manual UI verification.

Rollback is straightforward before archive: revert the proposal branch. After archive/implementation, rollback would restore four mono sources, remove width persistence, and map any saved stereo widths back to Mono.

## Open Questions

- The request says "Juice"; this design treats that as JUCE audio-device channel configuration.
- The implementation maps the four configured sources to eight physical JUCE lanes: source `i` reads its left lane from physical input `2 * i`, and its right lane from physical input `2 * i + 1` when configured as Stereo. Mono sources keep that right lane silent.
