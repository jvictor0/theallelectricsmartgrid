# Parent validation and cross-run input-transfer comparison

The Mac capture diagnostics contain no overload, underrun, overrun, restart or transaction-error candidates during tone playback. The only matching error is an iPad control-endpoint transaction at 22:58:38.961 when the iPad has been returned to the Mac, after the analog test stopped. SoX capture stderr is empty and finalized normally. This supports, but does not independently prove, the integrity of the analog capture.

The app monotonic-to-wall-time mapping used 697 Audio system observations. Absolute residuals of the linear mapping are shown below; submillisecond residuals make the repeated approximately250ms driver-step/callback separation meaningful within the iPad logs. Separate Mac/K-Mix waveform wall times remain approximate.

- Clock mapping absolute residual median/p99/max: [0.2582073211669922, 0.49163818359375, 0.5218982696533203] ms.

The counts below exclude startup and final app-stop/disconnect transitions. Zero-length counts are reported transfer completions, not assertions about physical USB packet contents. Session start/end choices and exact source lines are in cross-run-input-transfer-summary.json.

| Stable interval | Reported zero-length transfers | MAYA transaction errors | Drift log messages |
| --- | ---: | ---: | ---: |
| Tone | 26 | 0 | 28 |
| DSP no WB then WB | 0 | 7 | 0 |
| DSP output-only | 6 | 10 | 6 |
| Drambo after output-only | 0 | 0 | 0 |
| DSP input 512 | 2 | 30 | 36 |
| Long Drambo | 0 | 0 | 0 |
| Short clean SmartGrid | 2 | 0 | 3 |

This distinguishes successful-looking zero-length input completions from explicit transaction failures. The tone run accumulates26 reported zero-length transfers across13 drift steps without a hard failure. Some normal-DSP runs have hard failures without logged zero-length completions. Short clean SmartGrid playback also has two zero-length completions and a small drift, so a zero-length event is not sufficient for audible failure. The stable Drambo comparisons have neither event type in the available extracts. Different logging/reporting thresholds and short, sequential experiments limit causal inference; the table is not a probability estimate.
