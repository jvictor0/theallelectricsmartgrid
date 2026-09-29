# Final integration review: MIDI worker off experiment

## Assessment

**Experiment readiness: Ready.** The exact two-file delta implements the approved launch-off/discard intervention, and the recorded build, signature, installation, enabled-mode and disabled-mode gates support running the exposure. No Critical or Important issue was found in this scope. One already-deferred Minor diagnostic limitation remains acceptable for this experiment.

**Audio outcome: Pending.** The measured capture manifest reports recording, starting September 10, 2026 at 14:36:12 PDT with a 960-second maximum. This review does not score that recording or claim improvement, a fix, or a clean exposure. **Ready to merge: Not assessed/requested**; this is a reversible diagnostic prototype atop earlier uncommitted diagnostics, with no commits or landing authorized.

## Scope and method

Read the final integration review package, approved implementation plan, SDD progress ledger and code-reviewer guidance. Reviewed the exact snapshot delta and relevant current source: MidiSender, MainComponent audio/timer integration, NonagonWrapper producers/direct sends/lifetime, MidiHandlers, CircularQueue and AsyncLogger. Consulted local JUCE 8.0.15 thread-start implementation to interpret the startup result.

Read existing build/package/install and both runtime-mode verification artifacts, disabled launch metadata, preflight audio activity and measured-capture metadata. Independently compared the four recorded source hashes, built executable hash and IPA hash against the package manifest; all matched. Also verified the preserved preceding JUCE 8.0.15 IPA against its recorded hash. No build, test, iPad query, recording manipulation, source edit or Git mutation was performed for this review. Only this report was written.

## Strengths

- `JUCE/SmartGridOne/Source/MidiSender.hpp:17` reads an immutable environment option with default enabled. Exact value `0` disables; unset and `1` preserve the worker path. The disabled constructor initializes routes and clock route before returning, and does so before realtime thread creation (`:25`, `:32`).
- `MidiSender.hpp:85` discards at entry before route assignment or queue push. The path performs one relaxed atomic increment, has no explicit allocation, lock, wait or per-message logging, and adds no queue producer. Its queue counters remain initialized to zero when disabled.
- The original enabled scheduling options, queue handling, routing and shutdown behavior remain unchanged (`MidiSender.hpp:40`, `:106`, `:111`). Startup logs now include the thread-start return value, while relaxed atomic worker state records actual entry to and normal exit from `run()` (`:54`). The startup `real_time=1` label describes the requested mode; readiness relies on `started=1` plus `running=1`, not on that label as independent OS scheduling evidence.
- `MainComponent.cpp:524` contains the sole new aggregate call in its existing approximately once-second message-thread diagnostic branch (`:532`). There is no new timer or callback/per-message log. The atomic status reads need no cross-thread publication guarantee because they are observational counters, not synchronization for queue payload access.
- `MidiSender.hpp:160` still clears `MessageOutBuffer` after routing its events, so discarded messages do not accumulate upstream. WB indicator/transport messages pass through the new gate; direct WB SysEx, Launchpad/encoder and K-Mix sends remain on their existing paths in `NonagonWrapper.hpp:70`, `:103`, `:294`, `:306`, `:350`, `:572` and `:601`. The I/O worker and normal DSP/UI execution remain present.

## Issues

### Critical (must fix)

None found in the reviewed experiment delta or its preparation evidence.

### Important (should fix)

None found for experiment readiness.

### Minor (nice to have)

1. **Enabled queue depth is an inconsistent snapshot.** `JUCE/SmartGridOne/Source/MidiSender.hpp:102` calls `private/src/CircularQueue.hpp:94`, whose independent head and tail loads can straddle producer/consumer progress. The reported depth can be inaccurate and, with an older head and newer tail, wrap to a very large unsigned value. This can mislead later enabled-mode backlog analysis; it does not corrupt the queue, and disabled-mode zero is reliable because neither counter advances. This is the exact limitation already deferred in `progress.md`: the queue implementation predates this task, while the new aggregate diagnostic exposes its size. Keep enabled queue depth explicitly approximate; if quantitative backlog evidence is later needed, add suitable bounded diagnostic snapshot semantics in a separate change. No queue implementation change is needed before completing this exposure.

## Preparation evidence

- `/private/tmp/smartgrid-midi-worker-ios-build.log` ends with `BUILD SUCCEEDED`. The package manifest records strict signature verification as valid on disk and satisfying its designated requirement. Existing stale-product-path warnings do not negate the successful build or matching current package hashes.
- `/private/tmp/smartgrid-midi-worker-20260910.package.json` identifies JUCE 8.0.15, binary `c1ab35fd9cd627798583aaa08ba40e12cabde0915acbcb0275b0d1ad32961fd6` and IPA `20b2fd707a832c72eca805c453ea79aef120c12bd71c3b92d1efbeb400a835c1`. Current source and artifact hashes match those records.
- `/private/tmp/smartgrid-midi-worker-20260910.install.json` records the in-place upgrade of that IPA and equal before/after config and patch hashes. `/private/tmp/smartgrid-juce815-20260910.ipa` remains preserved and matches its prior package manifest.
- Enabled preflight records `enabled=1 started=1`, then four aggregate observations with `running=1`, queue depths 2–4 and discarded=0. Thus the enabled comparison actually had a running worker; this is not merely an assumption from requesting realtime startup.
- Disabled launch metadata records normal DSP, UI enabled, autoplay enabled and `SMARTGRID_MIDI_SEND_THREAD=0`. Its preflight records `enabled=0 started=0`, five observations with `running=0 queued=0`, and increasing discards 31→290→567→821→1077. These observations corroborate the source gate while messages are actively generated.
- Both preflights identify MAYA44 USB+, 48 kHz, active app inputs 0/outputs 4 and normal DSP/UI/autoplay. Both include startup frame sizes 470 and 512, then 200 settled callbacks exclusively at 512 frames/48 kHz with nominal thermal state. Startup transitions are correctly separate from settled buffer evidence.
- The distinct five-second K-Mix 3/4 audio preflight shows active stereo output and no candidate both-channel silence of at least 20 ms under its stated threshold. This establishes usable capture signal, not sustained symptom success.

## Pre-existing observations and interpretation limits

The earlier tone default, output-only audio initialization, callback tracing, platform diagnostics, native audio setup and direct MIDI sends are integration context, not newly introduced changes in this experiment. Their effect on timing remains part of the experimental baseline. No general approval of those prototypes for production is implied.

The intervention removes both worker wakeups and the messages that worker would send. Even a changed audio outcome cannot separate scheduling effects from worker-originated MIDI traffic by itself. Remaining direct sends mean this is not a MIDI-free experiment. The previous SonoBus control is a separate completed hour at observed 48 kHz/256, not a matched 512-frame SmartGrid control.

## Recommendations and remaining gate

Complete the already-running exposure and preserve its exact duration, archive and configuration identity. Score sporadic interruptions and periodic holes independently; mark startup transitions separately and correlate candidates with device logs. Retain the enabled/disabled preflight artifacts with the final experiment record. Do not interpret a quiet five-second preflight or successful source/build review as the requested sustained audio result, and do not promote a 16-minute or shortened trial to a clean hour.
