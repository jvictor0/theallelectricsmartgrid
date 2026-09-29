# User hypothesis: drift rate links periodic holes and sporadic restarts

The user proposes that a small nonzero derivative of a timing error produces slowly evolving periodic damage, while a large derivative produces rapid failure and reset. This is a testable shared-cause hypothesis, not yet an identified cause. Distinguish an underlying error e(t), its derivative, and the private reported ioDriftNS value.

## Newly inspected supportive episode

In the September10 old-JUCE normal/UI-on recording (`smartgrid-ipad-autoplay-ui-on-r3-20260910`), reported drift rises from0.520792ms at WAV176.664518s to198.914292ms at208.112100s, an average6.3087ms of reported drift per elapsed second. A MAYA endpoint0x82 transaction error appears at208.287983s, about176ms after the last report, followed by driver stop and restart. The near200ms terminal value deserves comparison against other failures, but one event cannot establish a200ms threshold. The ramp begins after a preceding transaction/restart at175.716s: the derivative may itself reflect a bad restart/timestamp state rather than its initial cause.

The periodic waveform burst spans approximately177.617–180.362s, so it clears roughly28seconds before the eventual transaction error while the logged drift continues rising rapidly. This fits a phase/alignment-dependent window better than a monotonic damage-versus-error rule. It does not fit a literal high-derivative-immediately-resets rule at the observed6.3ms/s slope; a threshold-crossing variant remains possible.

## Limits and contrary observations

- New-JUCE UI-on has five matched USB/restart/analog failures during168.192seconds with no reported drift or zero-length-transfer event in the measured window. Missing reports cannot exclude a short unlogged timing disturbance.
- Old-JUCE UI-off has one failure at WAV273.295s; the last preceding drift report is12ms at133.656s, nearly140seconds earlier. No preceding large logged derivative is established.
- New-JUCE UI-off periodic damage clears around26ms drift, which later reaches34ms without another dense episode or restart. Accumulated drift alone is insufficient.
- The cooled normal run has29alternate processing durations over budget aligned with29periodic waveform holes, with no nearby drift/zero-length/transaction reports. A shared scheduling cause could affect both, but the current traces do not prove that link.
- The pure-tone trace has discrete approximately2.020833ms drift steps associated with paired zero-length transfers. A finite difference of event-driven/quantized reports is not necessarily an instantaneous physical clock drift rate.

The common cause must explain the observed kernel USB transaction failure preceding recovery. A callback underrun or large scalar drift does not, by itself, establish that transport-level mechanism. Do not assume Apple intentionally resets at a known drift threshold.

## Analysis requested for the running thermal crossover

Without changing its protocol or live device traffic, compare reported drift step frequency and magnitude, within-epoch slopes, zero-length-transfer events and USB errors across verified nominal/Serious/off phases after archive retrieval. Check whether a slope/step change precedes the audio improvement. Never difference across IO restarts or sample-time resets. Keep both actual thermal exposure and unmatched/missing-report limitations. The private field remains undefined; absent drift records must not be converted to a numeric derivative of zero.

Exact extracted event timelines use `.drift-theory-events.json` under each old recording base in `/private/tmp`. The compact audit is copied to `../summaries/drift-derivative-hypothesis-20260910.json`. No iPad connection, condition, app, recorder or experiment sequence was changed by this audit.
