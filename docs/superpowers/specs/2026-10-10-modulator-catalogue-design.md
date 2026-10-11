# Typed modulator catalogue

Replace the imperative modulator skin switch with one catalogue:

```cpp
F(enumType, enumValue, mode, color, glyph)
```

Use explicit `VoiceModulator`, `QuadModulator`, and `GlobalModulator` enum classes.
Enumerator values are the existing modulation array indices. Keep sparse values
and the existing fifteen-position storage/persistence layout. Voice metadata is
authored once and shared by Water, Fire, and Earth. Modes mean Voice, Quad, Global.

Keep the existing two-field skin (glyph and color), with direct mode/index lookup.
A small builder initializes missing entries to Off/None and checks duplicate and
out-of-range entries and enum/mode consistency. Prefer constexpr construction;
existing half-bright colors may be named with exactly equivalent RGB values in
Color.hpp. No new registry, live state container, callback routing, or parameter
association table is needed.

Remove GetModulatorSkin's switch and the LFOColor, ADSRColor, SheafColor,
QuadratureColor and QuadLFOColor helper family. Parameter rows use a typed
GetModulatorColor(enumValue) lookup. Replace corresponding DSP index literals
with the named enum values. Retain the existing numeric index/BankMode skin API
as the small visualizer/UI compatibility boundary. Existing visualizer helper
calls may change mechanically to typed lookups; registration and routing stay as
currently implemented.

Preserve existing real modulator indices, colors, glyphs, signal assignment,
parameter metadata, parameter colors, patch/recording formats and recursive
encoder ownership. The user explicitly authorizes removal of phantom Global
skins at indices 6, 7 and 11: Global only defines random sources at 2 and 3 and
noise at 14. Other absent entries also default to Off/None. Color metadata is not
source availability or storage: keep the generic encoder connection/allocation
contract, including fifteen persisted positions, separate from skin existence.
No migration or removal of stored modulation depth is required.

Tests must cover all existing mode/index skins with only the three authorized
Global changes, the parameter-color baseline, empty-slot UI selection/processing
and legacy stored depths, and real source signal assignments at preserved
indices. Final verification includes the standalone suite and desktop build,
then an independent correctness and minimality scan.
