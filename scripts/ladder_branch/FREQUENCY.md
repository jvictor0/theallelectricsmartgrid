# Active detector and input frequency

The experiment drives the stock Naive Reupping Ladder at −18 dBFS peak and 192 kHz. The cutoff is 995.405 Hz, the drive knob is 90/99 and the final resonance knob is 81/99. The unit-gain delayed loop crosses at 1,011.750 Hz. The second search band is centered at one third of this frequency. The observable is output mean-square or twice the squared complex coefficient at the input frequency. Both gains use input mean-square as the reference.

## Sweeping the branch

Each exact tone has one input cycle in an integer number of engine samples. The 66-tone sweep uses the same stock detector, Metal analytic derivative, independent double C++ residual, and two resonance step sizes as the original grid. Where both meshes stall, the second solver continues in arclength. It predicts along the tangent of the periodic-equation solution set, then corrects both the state and resonance in a perpendicular plane. The positive-resonance direction at zero sets the initial orientation. A turn in resonance can then be traversed. A first transverse crossing of the requested knob position in this orientation selects the reported target. Agreement requires the same scaled state on two arclength meshes and a full-state residual at most 1e−8. A stall leaves a gap in the plot. The separate neighboring-frequency experiment produces additional candidates without a resonance anchor of their own.

The sweep resolves 15 tones by stepping resonance and another 12 by following arclength with a whole-period return. For the remaining 39, restart from the oriented branch before that return becomes ill conditioned. Split the period into several pieces and solve for the initial state of each piece and resonance together. The two runs use different resonance step sizes and different segment boundaries. The check compares all 14 sample states on every phase, the two output waveforms, and the wrap residuals at every segment boundary. All 39 pass. The table retains the direct whole-period residual as a conditioning diagnostic. This resolved all 66 original tones. Each adjacent pair differing by more than 3 dB in total power then received every intervening one-cycle digital tone. All 19 additional tones passed. The plot contains the resulting 85 numerical observations without assuming a frequency interpolant.

## Certifying the resonance path

Use all the sample histories in a period as unknowns. The equation at each sample says that its successor is the result of one physical step. Each parameter cell interpolates between two proposed cycles. To bound the correction, freeze the linearized transitions to matrices Aᵢ, then bound the inverse of hᵢ₊₁ − Aᵢ hᵢ on cyclic sequences. The period matrix product determines the wraparound condition. A floating-point inverse proposes a preconditioner. The checker bounds its defect against the enclosed product before using it. Keeping all intermediate samples in the equation gives local derivative and residual bounds at the actual signal level.

The preconditioned equation is contractive on a tube around the interpolated cycles when the checked Lipschitz bound L and the residual η satisfy Lr + η < r. The fixed point is unique there and lies within η/(1−L) of the center. Consecutive cells share a numerical endpoint and the same coordinate scales. Their endpoint balls are nested, identifying the same exact solution. At zero feedback the stable feedforward poles have a unique periodic solution.

At a turn, resonance also becomes an unknown. Its augmented transition keeps K unchanged except at sample zero, where the redundant scalar wrap equation imposes a hyperplane through the proposed branch point. At an exact periodic solution K is constant throughout the cycle and the gauge equation holds. The hyperplane normals interpolate between nodes. Neighboring cells share the same hyperplane at each endpoint. The first and last hyperplanes fix K. This certifies an oriented connected path and its reversals in K. A classification of a turn as a nondegenerate saddle-node would require further derivative conditions.

## Certifying frequencies between digital tones

Let θ be input phase in turns. A graph X assigns a 14-coordinate filter state to each θ. The one-sample equation is

    X(θ + f/Fs) = F(X(θ), A cos(2πθ)).

A rational f/Fs yields an exact digital cycle on this graph. The same equation applies when the phase never repeats. The candidate uses a piecewise-linear table in θ and a first-order correction in frequency. The correction solves the differentiated graph equation at the seed tone. Both tables are proposals and their residuals are checked on each full phase bin.

The checker encloses all sample Jacobians in a tube around the graph, including the dependence of effective feedback on both detector powers. The saturation has its clamped rational production formula. The detector knee uses a slope interval when necessary. For the two reported rectangles the minimum excess is strictly positive. The previous-sample feedback and the detector measurement of the unsaturated fourth stage are included.

Binary composition bounds the product of the sample Jacobians over a block and the propagated sum of the graph defects. For shorter initial prefixes, zero disturbances can pad the beginning of a block. For subsequent blocks, a product bound a and accumulated defect D satisfying ar + D < r keep the candidate's forward iterates inside the tube. For two trajectories that stay inside, the distance after each full block is at most a times the initial distance. On continuous sections over the phase circle, the closed set of forward-trapped sections contains the candidate and is complete. The block contraction gives a unique fixed section there. Its single-step image is also a fixed section of the block, proving the single-step graph equation. The base can include the parameter rectangle as fixed coordinates, giving a continuous family in the checked region. The forcing phase is external and independent of the state.

The proof bounds phase drift for all frequencies in the rectangle. For short drift it widens each one-step chart. Larger drift uses all reachable successor bins when stacking two half blocks. The continuous frequency enclosures use blocks of 4,096 samples. The power checker separately encloses every consecutive q-sample output average and every complex projection against the actual forcing phase. Averaging complete blocks keeps the result in the same interval. The bounded trailing block vanishes in the long-run average. This gives a bound even at rational frequencies where a discrete phase average may differ from the full-circle integral.

The checker uses binary64 intervals with explicit roundoff padding, finite-dimensional matrix products of at most 15 terms, and the usual IEEE round-to-nearest model. Trig enclosures use Taylor remainders and a rational Machin bound for π. The target recurrence uses real arithmetic around the app's prepared coefficients and its rational saturator. Floating-point proposals, GPU derivatives and observed fits do not establish an interval result by themselves. The Lean file `formal/DSP/Analysis/TrappedGraph.lean` proves the abstract trapping and graph-transform implications. The numerical enclosure and its connection to the ladder recurrence are implemented in the external checker.

## Reproduction

From the worktree root, build the reference with `make -f scripts/ladder_branch/Makefile ladder-branch-reference`. The Python checker needs NumPy and SciPy. Results below use `build/results/ladder-branch-frequency-v1` as D. Use fresh filenames when regenerating the input data.

```sh
D=build/results/ladder-branch-frequency-v1
python3 scripts/ladder_branch/replay_frequency.py "$D/chain180.jsonl" --graph "$D/verified-180.json" --recalculate
python3 scripts/ladder_branch/replay_frequency.py "$D/chain569.jsonl" --graph "$D/third-harmonic.json" --recalculate
python3 scripts/ladder_branch/replay_frequency.py "$D/fold190.jsonl" --recalculate
```

The first two replays also check that the zero-resonance branch endpoint lies inside the frequency tube at the seed frequency. The arclength replay checks every augmented chart and the ordering of the two parameter reversals. `orbit_power.py` independently encloses each crossing of 60% and the final crossing of 81/99 using fixed-resonance charts. `report_frequency.py` produces the sweep and turn plots from the raw observations and C++ sample processor.

The three 60% crossings are checked against the oriented arc by `match_crossings.py`. It locates every chart that can contain the requested K, uses the interval for K to restrict the chart parameter, and encloses the corresponding state. A local cyclic uniqueness check puts the independently measured solution and the crossing in the same ball. This also links the final power enclosure to the endpoint.

## First-crossing switches

The follow-up experiment tracks an entire waveform as a function of input phase. At each frequency it solves the one-sample graph equation above on two phase grids. A coordinate of the filter state at phase zero parametrizes the part of the solution curve near the first resonance maximum. Its resonance derivative vanishes at the maximum. Equating that maximum to the chosen knob setting locates the edge numerically. Solving at fixed resonance tracks the quiet waveform across the edge, and solving on either side of the maximum in the state coordinate recovers the two loud waveforms where they coexist.

For exact inputs with p cosine cycles in q samples, the numerical proposal is reordered into time order and corrected against q individual production steps. The interval checker evaluates the exact phase advance p/q and all 14 state coordinates. The local maximum check uses a fixed state-coordinate interval. Its center has higher resonance than either endpoint, and the chart tubes bound the maximum throughout that interval. The power check at fixed resonance independently isolates each reported periodic waveform. These local checks apply at the specified rational tones. Uniform control of the turn in continuously varying frequency remains open.

For an unstable orbit, forming a full-period transition matrix can lose the decaying directions. The segmented inverse instead bounds products over 32 samples, solves for all block boundaries, and bounds the defect of that approximate inverse. Signed transition products and positive disturbance bounds recover enclosures at the intermediate samples. A power enclosure can therefore apply even when a forward simulation would depart from that orbit.

The four original tones have an additional check. Each parameter chart connects to the next at an identical center and hyperplane, up to the sign of its normal. The fixed-resonance endpoint ball contains every chart portion that could realize the target. The check measures fundamental, total and third-harmonic power on that orbit.

The observed switch frequencies, proof boundaries, plots and witness files are in `build/results/ladder-branch-switch-v1`. To repeat a near-edge check from the worktree root, using the reference build and input files described above, run

```sh
D=build/results/ladder-branch-frequency-v1
O=build/results/ladder-branch-switch-v1
python3 scripts/ladder_branch/switch_frequency.py "$D" "$O" --q 183 --nodes 366
python3 scripts/ladder_branch/switch_probe.py "$O/phase-183-366.npz" "$O/recheck-16-2917.json" 16 2917 --reference 183 --maximum
python3 scripts/ladder_branch/switch_replay.py "$O" --q 183 --recalculate
```

The lower band uses reference 561 and grids of 561 and 1,122 nodes. The upper band uses reference 183 and grids of 183 and 366 nodes. To prepare fresh arclength and quiet-branch seeds, give `switch_path.py` the earlier frequency directory and an empty output directory. The `sections` field in each anchored power report lists the ordered chart files used in its replay. `plot_switches.py` regenerates the figure from the fine-grid sweeps and four near-edge reports.
