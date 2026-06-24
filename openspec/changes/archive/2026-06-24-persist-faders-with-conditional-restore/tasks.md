## 1. Fader JSON Persistence

- [x] 1.1 Add arena-backed serialization of all sixteen `m_squiggleBoyState.m_faders` values to the root patch JSON.
- [x] 1.2 Add guarded fader deserialization that applies saved values only when `restoreFaders` is true and leaves faders unchanged when the field is missing or incomplete.
- [x] 1.3 Keep patch save/load real-time safe by allocating fader JSON nodes only from the provided `JsonArena`.

## 2. Load Policy Plumbing

- [x] 2.1 Change the higher-level `FromJSON` entry points to accept a `bool restoreFaders`.
- [x] 2.2 Store `restoreFaders` in `StateInterchange` alongside the parsed load JSON and pass it to audio-thread deserialization.
- [x] 2.3 Update direct save/load pad or test helper paths to pass an explicit restore policy.
- [x] 2.4 Add a top-level `NonagonWrapper` query that returns false for fader restoration when `NonagonWrapperWrldBldr::IsOpen()` is true and true otherwise.
- [x] 2.5 Update `MainComponent::RequestLoad()` so manual and startup patch loads set `restoreFaders` from the WRLD.BLRD attachment query.
- [x] 2.6 Update reload-style patch functionality to call the load path with `restoreFaders` false unconditionally.

## 3. Verification

- [x] 3.1 Add or update system tests proving patch JSON contains all sixteen faders after save.
- [x] 3.2 Add tests proving faders restore when `restoreFaders` is true and remain unchanged when it is false.
- [x] 3.3 Add compatibility coverage for loading a patch that lacks the `faders` member.
- [x] 3.4 Add coverage proving reload-style patch functionality leaves live faders unchanged.
- [x] 3.5 Add startup-order coverage or a narrowly scoped test seam proving saved WRLD.BLRD configuration is applied before the current patch load policy is chosen.
- [x] 3.6 Run the relevant private test target and OpenSpec validation for this change.
