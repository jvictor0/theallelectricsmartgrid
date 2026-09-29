# Native callback and USB detail preparation — September 11

Prepared for the user's next iPad session; not deployed. Existing desktop/main build and topology left untouched. [Build provenance](2026-09-11-native-probe-build.json).

## Question and evidence

For sporadic dropouts, the original internal DSP recording was pristine. Existing system tracing shows USB transaction failure, restart, and missing callbacks after the previous processing completed. There is no positive evidence of damaged sample values in the application's output explaining these outages. Missing callback time does not require zeroed or corrupt samples in buffers that do arrive. A sample-time jump records stream timing; it is not proof of sample corruption.

Therefore default tracing is metadata only. Its purpose is to distinguish no native callback from a native callback that skips SGO, and to measure complete native processing versus the already-measured DSP region. Raw output PCM is opt-in only, potentially useful for the separate periodic/SRR symptom. Both symptoms remain unresolved; no new mechanism has been established by building these tools.

## Implementation

Retain JUCE8.0.15 backend and original processing branches. Source-pinned preparation hooks processStatic entry/return, branch results and app callback identity. New native hooks do not change samples, return status, session configuration or audio locks. Setup reads actual RemoteIO EnableIO, stream formats, maximum frames and session category/mode/options. Permanent48k/512 and zero app inputs remain, with normal DSP/UI/MIDI and autoplay.

Default flags: SMARTGRID_NATIVE_CAPTURE=1, SMARTGRID_NATIVE_CAPTURE_PCM=0, SMARTGRID_AUDIO_TEST_MODE=normal, SMARTGRID_AUDIO_TEST_PLAY=1, SMARTGRID_UI_RENDER=1, SMARTGRID_MIDI_SEND_THREAD=1. SMARTGRID_NATIVE_CAPTURE=0 permits an overhead control. Optional PCM requires an explicit SMARTGRID_NATIVE_CAPTURE_PCM=1.

Preallocated bounded queue, no new callback allocation/file I/O/logging/waiting. Dedicated writer polls at10ms idle, flushes metadata once per second, and caps output at2GiB without stopping audio. At48k/512 metadata records are approximately84MB/hour. Queue/overlap loss, writer tail loss and incomplete files are explicit. Decoder streams JSONL and caps summary event lists; counts and extrema remain exact, quantiles become labeled reservoir estimates beyond8192 records. Loss breaks timing comparisons and is not called a callback stall. Probe overhead excludes queue commit and the entry hook; hardware preflight/control is still needed.

## USB result

usb_trace_details.py extracts bounded windows and retains raw IDs/arguments and timestamp provenance. Eight tests pass including old macOS Python timezone parsing. Existing4traces (~17.2M records) expose raw events near a known failure but no documented argument layout for the observed USB debug IDs. Controller completion meaning, requested/actual transfer lengths and endpoint state remain not_exposed where absent. No private argument is guessed; this is not USB wire capture. The script also prepares a focused trace configuration for a later authorized capture.

## Validation and next session

Signed iOS Release build succeeded; host codesign verification passed. Original shared JUCE native source hash is unchanged; compiled generated module hash matches its manifest. Source-patch tests3/3, decoder8/8, USB8/8, portable capture6cases/714assertions including concurrent saturation and sanitizer checks passed. Independent reviews passed. Offline C++ to Python record interoperability is recorded in the follow-up validation entry.

Next: after user returns, install and launch these normal-DSP defaults; verify audible transport, MAYA48k/512, zero app inputs, physical MIDI output, session/EnableIO readbacks and capture mode. Run a short preflight, retrieve afterward, check zero capture loss and overhead against10.667ms callback budget. Do not count setup/relaunch/collection transitions as steady playback failures. Then collect normal workload with analog K-Mix and focused system trace; compare USB errors/restarts to native and SGO callbacks. Avoid device collection traffic during the measured window. If the probe alters behavior or costs materially, compare capture off before interpreting exposure. PCM capture is not a prerequisite for the sporadic-failure experiment.

## Offline interoperability result

Actual C++ RemoteIOCaptureRecord emission decoded successfully via the Python CLI: metadata-only244-byte header and276-byte known-PCM record; every scalar/array sentinel matched, planar samples matched exactly, and sequence1→4 with drop count2 produced coverage loss without false timing gaps. Artifacts: `/private/tmp/smartgrid-remoteio-compat*`; commands in `/private/tmp/smartgrid-remoteio-core-report.md`. Signed artifact is ready for hardware preflight, not a validated fix.
