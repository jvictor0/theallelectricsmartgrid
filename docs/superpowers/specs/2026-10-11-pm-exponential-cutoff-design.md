# PM exponential cutoff control

The user requests an exponential cutoff control increasing from minimum at 0 to maximum at 1, with full-open allowing at least seven octaves above the fundamental. Retain the current darkest cutoff, 8× the fundamental, and map `fc/Fs = min(0.49, (f0/Fs) * exp2(3 + 4*d))`. Thus 0, 0.25, 0.5, 0.75, and 1 select 8×, 16×, 32×, 64×, and 128× before the processing Nyquist clamp. The default 0.5 retains its current 32× cutoff. The old control was inverse-square and reversed; its zero bypass is removed. Exact DECAY center remains dry.

Keep the two-pole Butterworth, filtered excitation/output, radial transform, phase compensation, parameter identity/default/label, and all other synth behavior. Fundamental lifetimes remain approximate; the previously measured darkest-setting numbers now apply at control 0. Modes above the base-rate output Nyquist remain inaudible regardless of the cutoff ratio.

Validate the control through actual branch audio at five literal cutoff fixtures, not source-text/coefficient equality. Update endpoint-dependent existing tests and current documentation. Run focused comb/PM tests, review, and deploy for the ongoing iPad audition. No further design approval is needed for the user's explicit control-direction/range correction.

Removing endpoint bypass also makes the full-open pitch-boundary fixture (`f0/Fs=0.25`) retain the clamped low-pass loss. Independent prototype pole solutions followed by radial scaling predict negative/positive five-second losses of 60.497498756/60.486678806 dB at 48 kHz and 61.989995024/61.946715222 dB at 192 kHz. Tests use those literals with the existing 0.15 dB tolerance and retain the 0.002 Hz pitch tolerance; ordinary full-open fixtures retain the 60 dB expectation.
