# Parameter, page, and slot catalogue refactor

## Agreed scope

Separate the current parameter catalogue into three X-macro catalogues. This is
a structural refactor: preserve parameter order and names, descriptions, short
labels, defaults, switch counts, bipolar flags, colors, coordinates, selector
positions, visual display mappings, modulation behavior, and persistence.

```cpp
PARAM(name, shortName, mode, defaultValue, description,
      color, switchValues, bipolar)
PAGE(page, param, x, y)
SLOT(slot, enumType, enumValue, page, color)
```

The include files may retain the repository's conventional callback name `F`.
`mode` explicitly records the existing Voice/Quad/Global classification that is
currently inferred from the parameter's bank. It introduces no new behavior.

Fixed-page slots use the following machine type:

```cpp
enum class SingletonMachine
{
    Unit = 0,
    NumMachines = 1
};
```

Source slot rows use the existing `VoiceMachine::SourceMachine` enumerators.
The current filter variants share one layout and therefore require no new
filter-specific pages. Support typed slot rows without changing the existing
source or filter enumerator values or count sentinels.

## Derived topology

Author each page layout and each slot routing row once. A page containing any
Voice parameter has three bound instances; a page without Voice parameters has
one. A slot with any Voice-containing candidate page likewise has three
instances. SingletonMachine describes the number of machine choices, not the
number of trio instances. Shared parameter references keep their single owner
even when used in a trio-bound layout.

Introduce a `constexpr BuildPageAndSlotTables()` helper to resolve descriptor
relationships and derive page grids, instance counts and offsets, machine
lookup tables, and dependency lists. Macro expansions only emit rows. They do
not join catalogues or perform runtime filtering. Structural lookup tables must
be constant-initialized; evaluated colors remain unchanged. The user permits replacing Interpolate
expressions with an offline-computed named color in the existing Color.hpp.
Keep topology constexpr without requiring a general palette refactor.

Validate page references, parameter references, coordinates, coordinate
uniqueness, one machine enum type per slot, and complete unambiguous enum-value
coverage. Fail invalid catalogue definitions at compile time.

Use small explicit logical Page and Slot enums if necessary: repeated catalogue
keys must not require extra declaration macros, string-key lookup machinery, or
triplicated handwritten enum entries for each page.

## Runtime binding

Keep the existing independently owned encoder state and its index formula
`param * 3 + trio`. Voice parameters occupy all three instances; shared
parameters occupy only instance zero. DSP lookup uses parameter mode directly.

Bind a selected page instance to a slot instance's existing grid. Alternative
source pages are immutable layouts with bound parameter references, not
additional owners of encoder state or simultaneously populated UI grids. This
preserves the existing single owner-bank association and gesture aggregation.
The current data never places a parameter into two simultaneously bound slots;
check that invariant rather than introducing general multi-owner machinery.

Preserve the existing Bank-facing selector API as a compatibility boundary where
that avoids unrelated controller and visualizer edits. Resolve it through the
derived slot-instance tables, preserving the current twenty bank instances and
their ordering. A machine change uses the machine's precomputed affected-slot
list; a page bind visits only that page's fixed sixteen positions. No parameter
catalogue scans, machine-rule scans, or unrelated page rebinds are needed at
runtime. Rebinding to the same page is a no-op.

Initialization and patch/configuration updates must establish the correct page
for each trio before its grid is exposed. Parameter values, nested modulation,
gestures, hidden-parameter processing, and serialization remain independent of
page binding. Preserve existing selection/reset semantics during rebinding.

## Verification

Capture the old catalogue's complete parameter metadata and resolved layouts
before replacing it. Compare all source/filter combinations, trios, and slot
families after the refactor. Cover singleton voice slots as well as shared slots,
machine switching with edited state, selector colors, per-trio isolation, and
patch migration/round trips. Run the standalone suite and desktop compilation.

This document records the implemented design. Verification results are recorded
in the companion implementation plan.
