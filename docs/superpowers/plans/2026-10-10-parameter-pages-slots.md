# Parameter, Page, and Slot Refactor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [x]`) syntax for tracking.

**Goal:** Separate parameter definitions, page placement, and typed machine-to-page slot routing without changing existing data or behavior.

**Architecture:** Three X-macro catalogues emit descriptor rows. A constexpr helper derives page layouts, trio replication, instance offsets, and indexed routing. Existing encoder ownership, persistence, DSP processing, controller selectors, and visual display choices remain intact.

**Tech Stack:** C++17, constexpr std::array tables, existing encoder grids, doctest, CMake, JUCE desktop build.

## Global Constraints

- Preserve parameter order and names, descriptions, short labels, defaults, switch counts, bipolar flags, colors, coordinates, selector positions, visual display mappings, modulation behavior, and persistence.
- Exactly three catalogue row signatures: `PARAM(name, shortName, mode, defaultValue, description, color, switchValues, bipolar)`, `PAGE(page, param, x, y)`, and `SLOT(slot, enumType, enumValue, page, color)`.
- No new color roles or changed RGB values. The user permits offline evaluation of existing Interpolate calls and adding a named exact color in the existing Color.hpp to simplify the catalogue or constexpr construction.
- No joins in macro expansions and no runtime catalogue scans to classify or select pages.
- Page and slot replication is derived from Voice parameter presence. Author each logical layout and routing row once.
- Follow AGENTS.md C++ style; no new default arguments, C-style casts, private sections, or comments after code.
- Preserve the encoder owner index `param * 3 + trio`, trio suffixes, patch version, and recording identifiers.
- Work in the current isolated checkout. Do not modify main, publish, or deploy as part of this refactor.

**Design:** `docs/superpowers/specs/2026-10-10-parameter-pages-slots-design.md`.

## File boundaries

- Modify `private/src/ForEachSmartGridOneParam.hpp`: retain parameter facts and replace bank-derived scope with existing mode.
- Create `private/src/ForEachSmartGridOnePage.hpp`: explicit `(page, param, x, y)` placements.
- Create `private/src/ForEachSmartGridOneSlot.hpp`: typed `(slot, enumType, enumValue, page, color)` routes.
- Create `private/src/SmartGridOneLayout.hpp`: logical identifiers, SingletonMachine, plain descriptors, compile-time topology construction and direct accessors. Keep it independent of encoder allocation and DSP implementation.
- Modify `private/src/SmartGridOneEncoders.hpp`: parameter construction, mode-based reads, slot/page binding, compatibility with existing Bank callers.
- Modify `private/src/PatchFormat.hpp`: classify existing parameter names using the explicit mode.
- Modify `private/src/SquiggleBoy.hpp` only where machine/configuration updates need to provide trio context.
- Update `private/test/unit/encoder_trios.cpp` to stop relying on a unique parameter address; retain its behavioral assertions.
- Create `private/test/unit/encoder_layout.cpp` and a frozen test-only baseline fixture for unchanged parameter data and layouts.
- Update `docs/encoder-system.md` and the affected requirements in `openspec/specs/encoder-parameter-system/spec.md` after implementation.

## Task 1: Capture the existing behavioral baseline

**Consumes:** Existing `SmartGridOneEncoders`, `TrioEncoderRig`, selector tests, and original twelve-field parameter catalogue.

**Produces:** A test-only frozen expectation set for all 176 parameter definitions and the existing machine-dependent grids. The fixture must not include the new production catalogues to derive expected results.

- [x] Configure and build a standalone baseline:
  ```sh
  cmake -S private/test -B /tmp/smartgrid-parameter-pages-slots
  cmake --build /tmp/smartgrid-parameter-pages-slots -j6
  /tmp/smartgrid-parameter-pages-slots/smartgrid_tests
  ```
- [x] Before editing the original catalogue, capture each retained parameter field, original bank/position, machine masks, and bank selector colors into the test-only fixture. Keep the parameter order unchanged. Retain descriptions verbatim in the fixture; check evaluated encoder properties and RGB values. Do not assert source spelling: an equivalent named color is permitted.
- [x] Add characterization tests that resolve the fixture independently for all four source machines, both filter machines, and all three trios. For each bank coordinate compare the named cell or null and its existing color. Use the current placement behavior as the baseline:
  ```cpp
  rig.m_encoders.SetTrack(1);
  rig.m_encoders.UpdateEncodersForMachine(
      VoiceMachine::SourceMachine::PhysicalModeling,
      VoiceMachine::FilterMachine::Ladder4Pole);
  auto& bank = rig.m_encoders.m_encoderBankBank.m_banks[
      static_cast<size_t>(Encoders::Bank::SourceFire)];
  DOCTEST_REQUIRE(bank.GetBase(0, 0) != nullptr);
  DOCTEST_CHECK(std::string(bank.GetBase(0, 0)->m_name)
      == "PMMainSVFCutoffFire");
  DOCTEST_CHECK(bank.GetBase(1, 2) == nullptr);
  ```
- [x] Check parameter counts, names including existing trio suffixes, defaults, short names, switch counts, bipolar flags, mode/channel counts, and evaluated colors against the frozen fixture. These characterization tests must pass on the original implementation.

## Task 2: Introduce the three catalogues and compile-time helper

Execution note: Tasks 2 and 3 form one coupled production change. Implement and review them together so each macro signature and all consumers stay consistent.

**Consumes:** Frozen baseline and existing machine enum values.

**Produces:** `SmartGridOneLayout` with `Param`, `ParamMode`, `Page`, and `Slot` identifiers, `SingletonMachine`, compile-time `x_tables`, and direct accessors `GetParamMode(Param)`, `GetPageInstance(Page, size_t)`, `GetSlotInstance(Slot, size_t)`, and typed `GetPage(Slot, Machine)`.

- [x] Write tests for the new topology before adding production code. Exercise real independent contracts, including the following:
  ```cpp
  using Layout = SmartGridOneLayout;
  DOCTEST_CHECK(Layout::GetPageInstance(Layout::Page::PanAndMelody, 0)
      != Layout::GetPageInstance(Layout::Page::PanAndMelody, 1));
  DOCTEST_CHECK(Layout::GetPageInstance(Layout::Page::Delay, 0)
      == Layout::GetPageInstance(Layout::Page::Delay, 2));
  DOCTEST_CHECK(Layout::GetSlotInstance(Layout::Slot::PanningAndSequencing, 0)
      != Layout::GetSlotInstance(Layout::Slot::PanningAndSequencing, 2));
  DOCTEST_CHECK(Layout::GetPage(Layout::Slot::Source,
      VoiceMachine::SourceMachine::Sample) == Layout::Page::SampleSource);
  DOCTEST_CHECK(Layout::GetPage(Layout::Slot::PanningAndSequencing,
      SingletonMachine::Unit) == Layout::Page::PanAndMelody);
  ```
  Confirm the missing helper prevents these contracts from passing before implementing it. Add synthetic descriptor tests for a page with one Voice parameter among shared parameters and a slot with one Voice page among shared alternatives.
- [x] Move data mechanically into the agreed signatures. Define source pages `DualVCOSource`, `ThruSource`, `PhysicalModelingSource`, and `SampleSource`; use `PanAndMelody` for the existing panning/sequencing layout. Other layouts retain existing family names. The Source slot uses source-machine enumerators; all other current slots use SingletonMachine. The fixture must prove unchanged data and placement.
- [x] Emit mode, page-member, and slot-route descriptor arrays through separate straightforward includes. Example expansions:
  ```cpp
  #define F(name, shortName, mode, defaultValue, description, color, switchValues, bipolar) ParamMode::mode,
  #include "ForEachSmartGridOneParam.hpp"
  #undef F
  ```
  ```cpp
  #define F(page, param, x, y) {Page::page, Param::param, x, y},
  #include "ForEachSmartGridOnePage.hpp"
  #undef F
  ```
  Use a typed descriptor constructor for slot rows; it converts enum types and values to structural machine keys without consulting other catalogues. Color construction remains separate from constexpr topology; store row indices to associate existing colors with routes.
- [x] Implement `constexpr BuildPageAndSlotTables()` in the helper. Traverse descriptor arrays at compile time to fill each sixteen-position grid, derive Voice presence, calculate instance offsets, map enum values directly to route rows, and collect affected-slot lists by machine type. `x_tables` must itself be constexpr. Retain a validity result and assert it at compile time.
- [x] Validate duplicate/out-of-range coordinates, unknown references, mixed machine enum types within a slot, missing or duplicate machine values, and simultaneous-slot ownership conflicts in the current data. Use the existing count sentinels for source/filter enums and `SingletonMachine::NumMachines`; do not rename existing values.
- [x] Keep typed accessors to direct indexing and small fixed arithmetic. Run the helper tests and metadata-preservation tests. Integrate the changed parameter signature into all consumers together so the repository builds before advancing.

## Task 3: Bind existing encoder state through page and slot tables

**Consumes:** `SmartGridOneLayout::x_tables` and the direct accessors from Task 2.

**Produces:** The existing public encoder/selector behavior using the normalized catalogues, with no parameter catalogue scans on machine changes.

- [x] Add state-preservation coverage before replacing the current routing. Set different trio values, create nested modulation and gesture state, switch every source machine and back, and verify cell identity and values remain unchanged. Extend tests to cover machine changes while a modulation view is open and shared-slot selection while the trio changes.
- [x] Construct parameters from their explicit mode. Alias the existing public Param type if moving its definition into the layout helper. Preserve enum ordinals, the owner index formula, and persisted root names. Replace DSP reads' `GetParamAddress`/bank classification with `GetParamMode`:
  ```cpp
  const bool voiceParam = Layout::GetParamMode(param) == Layout::ParamMode::Voice;
  const size_t trio = voiceParam ? static_cast<size_t>(voice) / x_voicesPerTrio : 0;
  const size_t channel = voiceParam ? static_cast<size_t>(voice) % x_voicesPerTrio
                                   : static_cast<size_t>(voice);
  return m_encoderBankBank.GetValueByEncoderIndex(EncoderIndex(param, trio), channel);
  ```
  Apply the same routing to unslewed and switch-value reads.
- [x] Preserve the current Bank-facing API for selector and visualizer callers. Back bank-to-slot/trio and slot-to-instance resolution with derived tables, and assert that existing bank enum ordering agrees with the generated twenty slot instances.
- [x] Bind only the active candidate page into each slot grid. Alternative page instances store layout/binding information; they do not acquire independent encoder state or duplicate live grids. A bind clears/replaces the sixteen positions, resets incompatible visible selection state, and refreshes gesture/topology state. If the page identity is unchanged, return immediately.
- [x] Route machine updates through precomputed machine-dependent slot lists. Read the selected trio's existing machine configuration and index the relevant slot route; do not scan all parameters, pages, or unrelated singleton slots. Establish correct bindings for each trio during configuration/initialization before exposing that trio's grid.
- [x] Preserve `m_ownerBank`'s single live-slot invariant and all hidden-parameter processing. Scope page resets to the currently placed cells as before. Do not introduce general multi-owner or arbitrary mixed-channel UI machinery.
- [x] Replace PatchFormat's bank-name scope inference with the existing explicit parameter mode. Keep JSON field names, version, migration semantics, and recording format unchanged.
- [x] Remove the obsolete unique-address lookup if no production callers remain. Update its one trio-test use to check the known `(0, 0)` layout coordinate while retaining all value assertions. Run encoder, selector, patch-format, patch-roundtrip, and recording tests.

## Task 4: Verify preservation and document the final implementation

**Consumes:** All implementation changes and frozen baseline.

**Produces:** Verified refactor with current documentation and no unrelated data edits.

- [x] Run the full standalone suite:
  ```sh
  cmake --build /tmp/smartgrid-parameter-pages-slots -j6
  /tmp/smartgrid-parameter-pages-slots/smartgrid_tests
  ```
- [x] Run recording integration tests and desktop compilation:
  ```sh
  PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s scripts/tests -p test_sgrec.py
  make build
  ```
  Honor any fixture-export prerequisites documented by the recording tests. Do not deploy or install the application.
- [x] Update encoder documentation and the affected specification requirements to describe the three catalogues, derived trio instances, compile-time topology, typed routing, and unchanged parameter ownership. Preserve behavioral requirements outside this refactor.
- [x] Validate the updated encoder specification and inspect the final diff:
  ```sh
  openspec validate encoder-parameter-system --type spec --strict
  git diff --check
  git diff --stat
  ```
- [x] Review specifically for reordered parameters, altered expressions, added runtime full-catalogue loops, duplicate parameter ownership, stale modulation views, changed selector colors, and altered patch/recording keys. Record actual verification results before claiming completion.

## Implementation and verification

All four tasks are complete. Scoped implementation and broad whole-change
reviews found no production correctness or minimality issues. The one stale
source-machine specification sentence found during final review was corrected.

- Original implementation baseline: 551 test cases and 90,496,876 assertions passed.
- Frozen characterization fixture: all 176 parameters, every source/filter/trio layout, and selector colors. It passed on the original implementation before refactoring.
- Final standalone suite: 558 test cases and 90,507,553 assertions passed, with no failures or skipped cases. Existing warning-only cases remain outside this refactor.
- Recording interoperability: all 50 Python tests passed with all four C++ fixture exports enabled; Python-reconstructed patch reloaded by C++ passed one case and 20 assertions.
- Separate source-data audit preserved all parameter ordinals, labels, defaults, descriptions, switches, bipolar flags, and existing modes. Only five color expression spellings changed, each to the exact same RGB value via `Color::Mauve`.
- Scoped implementation review passed spec compliance and quality/minimality with no actionable findings. Obsolete machine masks, unique-address lookup, bank-clearing code, unused counts, and macro-expanded runtime scans were removed.
- Encoder specification passed strict OpenSpec validation; reference docs match the three catalogue signatures.

- Desktop Release build: `make build` succeeded for arm64 and x86_64 after initializing the worktree's existing pinned JUCE submodule. The initial attempts exposed sandbox cache access and the uninitialized dependency; neither required a tracked build change.
- Both encoder-parameter-system and source-machine-oscillators specifications passed strict OpenSpec validation. Full-range `git diff --check` passed.
- Work remains committed in the existing isolated worktree. No deployment or publication was part of this refactor.
