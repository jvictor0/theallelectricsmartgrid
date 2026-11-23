# Ladder survey: SQL analysis

The completed [Metal survey](ladder-gpu-survey.md) can be queried with DuckDB.
The database contains all 216 saved fields for each of 36,290,000 configurations:
100 randomized starts per configuration, amplitude 0.25, one second at 192 kHz,
detector off, production-style float/FMA rational saturation. Only configurations
with k*(g/R(g))³ > 3.5 were sampled. The original binary files remain immutable.

## Completed analysis, 2026-11-20

All 9,072,500 records passed checksum/identity/layout validation; the database
contains exactly 36,290,000 unique configurations. Import took 395 seconds on
Studio, and the aggregate/neighbor/interpolation analysis about 21 seconds.

| Diagnostic | Total power | Fundamental power |
| --- | ---: | ---: |
| Points above the −40 dB gain threshold | 35,888,950 | 27,111,954 |
| Across-start CV ≤1% | 97.14% | 66.46% |
| Across-start CV ≤5% | 98.10% | 80.05% |
| Mean within-run late-window CV ≤5% | 95.89% | 61.84% |
| Approximate 95th percentile across-start CV | 0.51% | 60.5% |

Each column's percentages use its own eligible population. CV means power
standard deviation divided by mean, not a bound covering every individual start.
Low total-power spread is widespread, including strongly driven configurations.
The heatmap concentrates its less-consistent region near the sampled resonance
threshold; fundamental power varies over a much wider region. Restricting both
knobs to ≤80% gives 97.25% of eligible total-power points and 85.24% of eligible
fundamental-power points with trial CV ≤5%. This subset is not uniformly better
for total power: many strongly driven cases have very stable total energy.

Approximate 95th percentile local dB interpolation errors:

| Axis | Total power | Fundamental power |
| --- | ---: | ---: |
| Input frequency | 0.10 dB | 3.60 dB |
| Cutoff | 0.15 dB | 3.85 dB |
| Drive | 0.32 dB | 0.41 dB |
| Resonance | 0.46 dB | 0.35 dB |

For frequency interpolation, 95.08% of eligible total-power triples have error
≤0.1 dB and 99.47% have error ≤1 dB. For fundamental power these fractions are
68.53% and 86.79%. Filtering out triples with trial CV >5% still leaves a roughly
1.38 dB 95th-percentile fundamental frequency error, versus 0.081 dB for total
power. Thus the difference is not explained solely by random-start uncertainty.

The tails matter: 99th-percentile total-power interpolation errors reach roughly
3.7 dB along drive and 4.8 dB along resonance. An explicit repeatable transition
at input 17,379.31 Hz, cutoff 1,144.47 Hz, resonance 10.101% is:

| Drive | Total power gain | Fundamental gain | Total trial CV |
| --- | ---: | ---: | ---: |
| 97.980% | −39.989 dB | −39.989 dB | 0.000576% |
| 98.990% | +13.273 dB | −47.706 dB | 0.004799% |
| 100% | +14.217 dB | −52.647 dB | 0.003639% |

Nearly all of the new power is residual power outside the driven fundamental
and DC. This is consistent with onset of self-oscillation, not Monte Carlo noise.
It establishes a sharp resolved transition, not a mathematical discontinuity:
a denser grid could reveal a continuous transition between these points.

Several high-drive response slices have nearly frequency-independent total
power while the driven fundamental has pronounced structure. Stable total
energy is therefore not automatically the most informative visualization of the
driven response. Both measurements remain useful and are retained separately.

These are finite-time grid statistics, not a proof of long-run convergence,
unique response, continuity between points, or behavior almost everywhere.
The exports include summary.json, consistency-map.json, representative frequency
slices, and the largest locally consistent interpolation residuals on every axis.

## Where higher variance occurs

The follow-up uses CV >5% and the same −40 dB eligibility threshold. It finds
682,452 high-total-variance points and 5,408,625 high-fundamental-variance points.
Each table entry below is the fraction of eligible configurations within that
cutoff band, not the fraction of all bad points:

| Cutoff | High total-power trial CV | High fundamental trial CV |
| --- | ---: | ---: |
| 20–80 Hz | 7.77% | 77.27% |
| 80–320 Hz | 1.54% | 47.39% |
| 320–1,280 Hz | 0.14% | 12.87% |
| 1,280–5,120 Hz | 0.01% | 1.29% |
| 5,120–20,000 Hz | 0.01% | 0.03% |

The additional diagnostic coordinate is η = k*s³/kcritical, with the actual
coefficient-setup formulas reconstructed in binary64. Here k includes kSafe,
s=g/R(g), and kcritical=(2+alpha/(1-alpha))² over this cutoff range. The nominal
oscillation frequency is fs/pi * asin(alpha/(2*(1-alpha))). This is the linear
phase crossing, not a claim about measured nonlinear oscillation frequency.
The analysis checks the crossing formula against the complex delayed-loop
expression at every cutoff. Coordinates are not bit-exact GPU coefficients;
the original float measurements and point selection remain unchanged.

**Total-power variance mostly occupies low-cutoff, near-threshold transients.**
82.37% of these points have cutoff below 80 Hz; 98.39% are below 320 Hz. Their
median cutoff is about 38 Hz. 83.69% have η between 0.9 and 1.05, and 78.71%
are below η=1. The input is usually well above the nominal oscillation point:
its median frequency ratio is about 26.5. 97.55% also have average temporal
CV >5%, and 57.62% have final-half power below one tenth of full-run mean power.
These facts suggest slowly decaying initial ringing contributes heavily; they
do not classify every trajectory or prove eventual convergence. In one extreme
example (input 264 Hz, cutoff 21.45 Hz), successive window means fall from
0.879 to 0.064, 0.0058 and 0.00056. Stronger drive is often more consistent in
total power than this near-threshold region.

**Fundamental variance occupies a broader region above threshold.** 85.36% of
high-variance points have cutoff below 320 Hz, but their median η is about 5.3.
98.07% have total trial CV ≤5%, and 96.55% also have fundamental temporal CV >5%.
This is consistent with drifting phase, beating and redistribution of spectral
energy while total energy stays nearly fixed. These summaries cannot uniquely
identify the mechanism or distinguish all quasiperiodic and chaotic trajectories.
High relative variance far above the nominal oscillation frequency is often in
a weak fundamental. It is not exclusively a quiet-tail effect: 1,372,968 of the
high-fundamental-variance points have fundamental power at least as large as input
power. Those louder cases mostly involve bass frequencies: approximate median
input 46 Hz, median cutoff 93 Hz, and median input/nominal ratio 0.61.

**A small class has steady windows but persistent differences across starts.**
1,938 points have total trial CV >5% and mean temporal CV ≤1%, about 0.0054% of
eligible points. Of these, 60.37% have input/nominal ratio 0.8–1.2 and 27.50%
have ratio 2.5–3.5. Their median fundamental coherence is 0.9999993. These are
candidates for coexisting responses rather than ordinary ongoing energy wobble.

Example point 89931773: input 9,955.56 Hz, cutoff 3,259.50 Hz, drive 93.939%,
resonance 17.172%, η=1.255, input/nominal ratio 2.896. Its final-half total powers
span 0.199506–0.811547 (6.094 dB), with mean 0.676890 and trial CV 37.46%.
Its reported temporal CV is zero at the measurement precision and coherence is
one. Window ensemble means are already nearly unchanged after the initial part.
The moments closely fit about 22 starts at the lower power and 78 at the upper.

That two-level inference uses SD²/((maximum−mean)*(mean−minimum)), which equals
one when values are confined to the two endpoints. The example's ratio is
0.999924, and the median ratio across the 1,938 cases is 0.99950. Saved float
rounding can make ratios slightly above one. Individual trials were not retained,
so these are moment-based inferences, not reconstructed trial histograms or a
proof of attractors persisting beyond the observed second.

Reproduce with scripts/characterize_ladder_variance.py DATABASE OUTPUT_DIRECTORY.
It opens DuckDB read-only and uses temporary diagnostic coordinates. JSON outputs
under build/results/ladder-survey-sql/variance include conditional rates, location
quantiles, joint maps, representative points and their chronological window
statistics. Every grouped population count was checked against the overall count.
Use scripts/plot_ladder_variance.py OUTPUT_DIRECTORY for the two figures.
No additional trajectories were simulated for this characterization.

The subsequent [long-run follow-up](ladder-long-runs.md) tests 30 representatives
for two and ten minutes, retaining individual starts and distinguishing longer
settling from longer coherent averaging.

## Location and queries

On studio.local, the experiment bundle is
/Users/joyo/benchmarks/ladder-energy-grid. Relative to that directory:

- Database: build/results/ladder-survey-sql/survey.duckdb
- Analysis: build/results/ladder-survey-sql/analysis/
- Import evidence: build/results/ladder-survey-sql/import-report.json
- Isolated Python environment: build/tools/ladder-sql-env/

Run a read-only query, with CSV output:

    build/tools/ladder-sql-env/bin/python scripts/query_ladder_survey_sql.py \
      build/results/ladder-survey-sql/survey.duckdb \
      'SELECT count(*) FROM responses'

Pass @path/to/query.sql instead of inline SQL to read a query file. DuckDB's
Python API can also open the file directly with read_only=True.

Tables and views:

| Name | Contents |
| --- | --- |
| dataset | Manifest, source identity, import status |
| axes | Actual rounded tone frequency, target frequency, cutoff, drive/resonance coordinates, measurement window sizes |
| points | Point ID, four grid indices, all saved statistics |
| responses | points plus physical coordinates, dB power gains, trial CV and Monte Carlo standard errors |
| measures | Compact power/variation view used by neighbor queries |
| neighbors_frequency / drive / resonance / cutoff | Pairs differing by exactly one grid index along that axis |
| interpolation_frequency / drive / resonance / cutoff | Interior point and immediate neighbors on that axis |
| consistency_map | Per-drive/resonance summary over frequency and cutoff |
| ingest_chunks | Transactional import progress for safe resume |

Grid indices are f_i, d_i, r_i, c_i in the range 0–99. Missing points are omitted,
not represented as zero. Point ID retains its original position in the complete
100⁴ grid. Queries never wrap around an axis or bridge an omitted neighbor.

Example frequency sweep:

    SELECT frequency_hz, total_db, fundamental_db,
           total_trial_cv, fundamental_trial_cv,
           temporal_total_cv_mean, temporal_fundamental_cv_mean
    FROM responses
    WHERE d_i=60 AND r_i=80 AND c_i=50
    ORDER BY f_i;

Example: largest frequency interpolation errors among locally consistent points:

    SELECT f_i, d_i, r_i, c_i,
           total_interpolation_error_db,
           total_interpolation_error_input_power,
           total_interpolation_mc_se
    FROM interpolation_frequency
    WHERE total_max_trial_cv <= 0.05
    ORDER BY total_interpolation_error_db DESC NULLS LAST
    LIMIT 20;

## What is being measured

The principal response estimates use the period-aligned interval in the final
half-second. Input mean-square is 0.25²/2 = 0.03125. A power gain in dB is
10 log10(output power / input power). Fundamental power is computed separately
for every start before averaging; averaging complex coefficients first would
incorrectly cancel solutions with different phases.

**Consistency across starts:** population SD / mean power, plus minimum and
maximum over the 100 independent initial histories. This is variation among
initial conditions, not repeatability of the same deterministic trajectory.
The ensemble mean's estimated Monte Carlo standard error is SD / sqrt(99),
using the unbiased sample variance correction. It measures uncertainty from
randomly sampling starts; it does not account for finite observation duration.
Individual trial values were not retained, so clusters and trial pairings
cannot be recovered from these summaries.

**Variation over time:** each start's late-window power CV, averaged over starts;
fundamental coherence; and change between the period-aligned half- and full-run
prefix estimates. Windows contain whole tone periods. Prefix differences include
startup and are recorded as absolute changes divided by input power, not output
power. They can flag slow settling but are not convergence certificates.

**Neighbor change:** absolute power difference, normalized by input power, and
absolute dB difference along each grid axis. Frequency and cutoff coordinates
are log2 Hz; drive/resonance use knob positions. Actual tone frequencies include
the survey's small rational-period rounding. A large neighbor change alone does
not distinguish steep resonance from a discontinuity.

**Local interpolation error:** predict a measured center point from its two
immediate neighbors, using the actual coordinate distances. Report both linear
power error / input power and linear dB interpolation error. This tests how well
the existing grid resolves curvature. There are no new midpoint simulations:
the endpoints are two grid steps apart, with one known point between them.
It cannot bound unsampled features or establish mathematical continuity.

Power differences also carry estimated Monte Carlo standard errors propagated
from independent ensembles. Small interpolation residuals support fitting a
smooth curve on the observed scale; large residuals identify where refinement,
longer runs, or individual-trial retention would be useful. Conditioning on
trial CV ≤5% helps separate interpolation difficulty from ensemble variability.

Relative and dB summaries exclude powers below −40 dB gain (output power below
0.0001 times input power), where relative errors exaggerate negligible output.
All points remain available in the database, and absolute power summaries do
not apply this threshold. Neighbor/triple dB measures require every participating
power to pass it. Summary quantiles use DuckDB's approximate quantile aggregate;
counts and fractions are exact for the stored values.

## Reproduce

Pinned importer dependencies: DuckDB 1.4.4, NumPy 2.2.6, PyArrow 19.0.1,
Python 3.11 on Studio. The native helper checks every record's FNV checksum.
The importer also checks the manifest identity, unique in-range IDs, counts,
finite values, statistic ordering and the expected missing-window mask.

    make ladder-survey-sql-build
    build/tools/ladder-sql-env/bin/python scripts/validate_ladder_survey_sql.py \
      build/results/ladder-gpu-grid-validation-final/control \
      build/results/ladder-sql-validation
    build/tools/ladder-sql-env/bin/python scripts/import_ladder_survey_sql.py \
      build/results/grid100-starts100-kappa-gt3p5-rational-metal-v3-seed20261119 \
      build/results/ladder-survey-sql/survey.duckdb
    build/tools/ladder-sql-env/bin/python scripts/analyze_ladder_survey_sql.py \
      build/results/ladder-survey-sql/survey.duckdb \
      build/results/ladder-survey-sql/analysis

Import transactions cover a chunk and its progress record together. The same
command resumes safely; source and chunk size must match. Validation covers
exact round-trip of 55,296 fields, idempotent resume, corrupt-checksum rejection,
grid boundaries/holes, and exact interpolation of a known linear response.
Analysis checks the database's completed status before querying it.

The small JSON analysis exports can be copied elsewhere. Plot them with
scripts/plot_ladder_survey_sql.py DIRECTORY, using NumPy and Matplotlib.
The raw binary and database remain on Studio; no application audio code changes
are needed for any of this analysis.
