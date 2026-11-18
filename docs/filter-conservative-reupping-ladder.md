# Conservative Reupping Ladder

Select **Conservative Reupping Ladder** in the voice filter-machine selector to
compare it with **Naive Reupping Ladder**. Both selections share the same cutoff,
resonance, drive, high-pass and sample-rate-reduction controls. The original
naive ladder remains the default; its saved selector value is unchanged.

Both shapes route audio through the low-pass ladder, the sample-rate reducer,
then the generated four-pole SVF high-pass. They use the same stage-corner cutoff
convention, 12-octave LP factor range, shared HP factor and 0.707 pitch tracking.
Cutoff caps at engine Nyquist. Drive still ranges from 0.25 to 5; the resonance
knob still maps through `(49^x - 1)/48`.

The conservative low-pass uses the generated Lean core. It keeps the four Euler
updates, per-stage normalized saturation, previous-sample fourth-stage feedback,
and `1+k` output compensation. It removes the adaptive bandpass/envelope
controller. Instead it fixes feedback at

```
s = gain / Tanh(gain)
k = resonance * 0.975 * kCritical(alpha) / s^3
```

Three saturators are inside the feedback loop. Cancelling their small-signal
slope reserves a 2.5% margin below the exact delayed loop's critical gain.
Consequently the same resonance knob position can sound different, especially
at high drive. The old adaptive ladder can allow more resonance. There is no
extra loudness matching between selections.

The new plot comes from the generated `MagnitudeSquared` function, proved equal
to the power of the nonlinear ladder's small-signal transfer, including drive
and the one-sample delay. It uses the same prepared-frequency cache, sampled
snapshot history and log-power display as the generated linear filters. The
shared UI frequency cache uses Math4096 with double-precision interpolation
for all filters, matching the generator's trig family. The outer shape
multiplies the LP and HP powers. It does not predict the nonlinear
sample-rate reducer or large-signal distortion. The old naive plot is its
existing approximation and omits saturation slopes.

The proof covers exact-real tanh with fixed valid coefficients, starting from
reset. Generated processing uses the existing clamped cubic rational tanh and
Math4096 tuning; those numerical approximations retain the same boundary as the
other generated filters. See the [Lean theorem inventory and implementation
notes](../formal/DSP/Filters/ConservativeReuppingLadder/README.md).

The conservative core reuses the Lean saturation and one-pole definitions and
the common sampled-filter generator. The handwritten naive app filter remains
available for comparison.
