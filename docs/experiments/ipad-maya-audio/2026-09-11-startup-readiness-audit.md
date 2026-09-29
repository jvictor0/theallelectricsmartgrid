# Startup readiness and initial overruns

Read-only audit prompted by the user asking whether asynchronous iOS initialization races our workers. No source change, deployment, recording, or timer started.

## Ordering

MainComponent constructs NonagonWrapper before its constructor body. MidiSender starts its real-time worker and IoTaskThread starts its worker in their constructors, before LoadConfig and OpenAudioDevice. The IO worker fields it initially consumes are constructed before its thread; early thread creation alone is not evidence of an uninitialized-member race. Workers consume queues; they do not independently run the DSP.

Main loads configuration and queues the saved patch, enables autoplay, opens the device, adds AudioSourcePlayer as callback, then sets its source to MainComponent. JUCE AudioSourcePlayer::setSource calls prepareToPlay before publishing the source under its callback lock. Main initializes SampleTimer and sample-rate/diagnostics there; NonagonWrapper::PrepareToPlay is empty. Thus DSP is gated by preparation despite native setup being asynchronous. The message timer starts after OpenAudioDevice returns. Async logger interleaves per-thread output: printed line ordering is not global execution ordering.

## Concrete first-render work

SquiggleBoy::ProcessSample checks m_firstFrame and synchronously calls InitRandomWaveTables. Its two oscillators times three tracks times left/right invoke GenerateCompletely twelve times; each generator prepares three voice tables. GenerateCompletely loops through incremental synthesis until ready on the audio thread. This is a concrete expensive initialization candidate, not yet separately timed.

The saved patch is applied by HandleStateInterchange at the first 512-sample ProcessFrame. In the cooled explicit48 run this occurs inside callback1. In the earlier 470-frame startup, callback1 is already expensive and patch application occurs in callback2, so patch loading cannot explain all of the first-callback cost. The introductory progress message identifying patch application in callback1 referred to the latest run only.

## Startup evidence

Five inspected iPad normal-DSP launches have first app callback durations 19.420ms (clock baseline), 21.910ms (explicit48 hot), 21.044ms (explicit48 cool), 18.535ms (Sep10 cooled repeat), and 17.296ms (Sep10 hot hour), each greater than its supplied sub-block duration. JUCE xr rises by callback3 in three of these five; the other two remain zero through callback10. This is a bounded convenience sample, not proof of always or a population incidence estimate.

Native records give: baseline first470 frames 19.436ms vs9.792ms frame duration, with continuous next sample timestamp; explicit48 hot first1014 frames29.138ms vs21.125ms; explicit48 cool first1014 frames27.048ms vs21.125ms. In both explicit48 launches the next sample timestamp advances2088 rather than1014, a discontinuity of1074samples (22.375ms), which increments JUCE xr. The1014 native block reaches the app as512+502 sub-blocks. Comparing the whole native duration avoids treating those app sub-blocks as independent hardware deadlines. Frame duration is a useful budget, not a direct physical USB deadline measurement.

Desktop revalidation app.log also records an initial Audio xrun19.232250ms /10.666667ms. Its subsequent long clean playback is therefore compatible with an expensive first callback; this startup issue is not uniquely iOS. Keep startup distinct from the steady-state clean result.

JUCE iOS recordXruns counts discontinuities of sample timestamps between native callbacks. It does not directly count application processing overruns or USB transaction failures. An initial Audio xrun duration log, a JUCE xr increment, and a driver error are three different observations. Do not label these startup xr increments as proven USB failures without matching driver evidence.

## Interpretation and next steps

There is genuine avoidable first-render initialization work. Precompute initial wavetables and apply the initial patch before attaching the DSP to live callbacks; preserve sample clock and normal workload afterward. This is a proposed change only. Profile the one-time sections if attributing exact cost. Worker start gating is a separate ordering dimension and should not be mixed into the same first-render intervention without reason.

No evidence yet connects this startup overrun to transaction errors minutes later. Desktop has an initial overrun and then clean prolonged playback. Pure-tone experiments reproduced artifacts while bypassing normal DSP and its first-sample wavetable generation; that generation therefore is not necessary for all observed failures. Retain JUCE session/temporary-unit lifecycle as a separate hypothesis. The JUCE iOS18 temporary-unit wait checks success only with jassert; timeout handling is worth auditing but no timeout is established here.

Evidence: /private/tmp/smartgrid-clock-lifecycle-trial-20260911/final.app.log and final-decoded/callbacks.jsonl; /private/tmp/smartgrid-explicit48-trial-20260911/final.app.log and preflight-decoded/callbacks.jsonl; /private/tmp/smartgrid-explicit48-cooled-trial-20260911-r1/final.app.log and final-decoded/callbacks.jsonl; /private/tmp/smartgrid-ipad-cooled-r1-20260910.app.log; /private/tmp/smartgrid-ipad-hot-hour-20260910.app.log; /private/tmp/smartgrid-desktop-revalidation-20260911/app.log.
