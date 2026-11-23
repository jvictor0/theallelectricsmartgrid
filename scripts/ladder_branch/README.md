# Input-periodic orbits of the Naive Reupping Ladder

For each input frequency, cutoff and drive, start at zero commanded resonance. Solve for a state which returns to itself after the repeated sine buffer, then continue that state as resonance increases. An unstable periodic state can satisfy this equation. Each point gives a phase-referenced fundamental and a mean-square output including all harmonics.

## Numerical pilot

Build the app reference and the standalone Metal worker from the worktree root.

```sh
make -f scripts/ladder_branch/Makefile ladder-branch-reference ladder-branch-metal
python3 scripts/ladder_branch/pilot.py build/results/ladder-branch-pilot-v1/repeat-grid.jsonl --suite grid --grid-size 4 --workers 12 --metal-command 'build/tools/ladder-branch/metal-return scripts/ladder_branch/return.metal'
```

The driver needs NumPy. The pilot used the M3 Ultra on Studio, the stock detector, the production rational saturator, −18 dBFS peak and 192 kHz. Frequencies and cutoffs in the four-point grid are 20, 200, 2,000 and 20,000 Hz. These tones all have exact digital periods, with five cycles in 48 samples at 20 kHz. The driver records both requested and realized frequencies for other grids.

The double-precision C++ processor supplies the residual and the observable. The input detector is independent of feedback. Its five steady-periodic coordinates are initialized at zero resonance and held fixed at the shooting boundary during correction. Metal integrates all 14 physical coordinates and the nine wet/audio tangent directions. The host uses those derivatives for Newton corrections, halves the parameter step if the corrector wanders, and repeats the path independently at half the maximum step. Each stored intermediate includes its resonance, full starting state, prediction error, iteration count and residual. Target observations include the Fourier coefficient, both powers, the double return residual, the scaled conditioning of the shooting equation and an independent double finite-difference Jacobian check. A multiplier exceeding one indicates a growing perturbation of this particular periodic orbit.

```sh
python3 scripts/ladder_branch/summarize.py build/results/ladder-branch-pilot-v1/studio-grid.jsonl build/results/ladder-branch-pilot-v1/repeat-table.csv build/results/ladder-branch-pilot-v1/repeat-summary.json
```

The numerical acceptance thresholds are a scaled state discrepancy below or equal to 1e−5 across meshes, an absolute full-state residual at most 1e−8 on both meshes and at most 1% relative matrix infinity-norm disagreement between Metal and the independent double derivative, with a denominator floor of one. Stalled paths leave the remaining grid points open. Crossing a fold in resonance or selecting a new branch would require a separate continuation rule.

## Interval replay and fitted response

The witness in this pilot varies base feedback from 0 to 5/64, at input 192000/18 Hz, cutoff approximately 995.405 Hz and drive knob 90/99. The reference processor supplies the coefficient centers. The interval calculation uses real arithmetic and the clamped rational saturator. It encloses each tone sample, quadrature, and each of the fixed stage and detector coefficients used in the recurrence within 2⁻⁴⁸ of its stored value. Throughout this example the wet detector power stays below 0.04 times the dry detector power. The controller therefore passes the commanded feedback throughout every checked state box for any reference power gain at least 0.04. The active-detector continuation and frequency enclosures for the second stress example are described in [the frequency study](FREQUENCY.md).

```sh
python3 scripts/ladder_branch/enclose.py build/results/ladder-branch-pilot-v1/repeat-witness.json --lo 0 --hi .078125 --cells 4096 --radius .00390625
python3 scripts/ladder_branch/check_fit.py build/results/ladder-branch-pilot-v1/repeat-witness.json build/results/ladder-branch-pilot-v1/repeat-fit.json
```

Each cell records a candidate center and a floating-point preconditioner. The checker treats these proposals as numbers on a dyadic lattice and rounds interval operations outward using integers. For the scaled return residual it checks that the preconditioned correction maps the ball into itself with Lipschitz constant below one, uniformly in feedback. This also gives injectivity of the square preconditioner, since its product with a residual Jacobian is within unit distance of the identity. At a shared endpoint the previous cell's root enclosure lies inside the next uniqueness ball. At zero feedback the four stages are a feedforward cascade of stable scalar poles. The checker also verifies decay of the detector transition matrix and the envelope smoothing pole. Each stage and detector then has a unique periodic response to its upstream input.

The fit has nine knots and is linear in base feedback and power. The checker bounds the full-period RMS-squared and fundamental power for every parameter in each cell and compares those bounds with the two ends of the fitted segment restricted to that cell. It verifies the exact sinusoid and −18 dBFS amplitude enclosures by Taylor bounds, Machin's formula and integer comparisons. It saves the worst multiplicative errors as exact fractions. The generic cell and fit theorems live in `formal/DSP/Analysis/ValidatedBranch.lean`. The concrete witness replay runs in Python. Connecting this checker to Lean and bridging the rational audio saturation to the existing real-tanh model remain separate tasks.

The quiet-detector argument fixes the period. The subsequent [phase-circle study](FREQUENCY.md) treats continuously varying input frequency and follows the active controller through turns in resonance.
