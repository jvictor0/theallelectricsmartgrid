"""Score the forked and independent parents on the floored 20 Hz–20 kHz display."""
import argparse
from fractions import Fraction as Q
from hashlib import sha256
import json
from math import comb
from pathlib import Path

import numpy as np
from scipy.stats import beta

from record_bank import EDGES, curve_mse
from survey import atomic_json


def curve(wet, dry):
    assert np.all(wet > 0) and np.all(dry > 0)
    return 10*np.log10(wet/dry)


def rms(a, b):
    a, b = np.broadcast_arrays(a,b)
    return np.sqrt(curve_mse(a.reshape(-1,120), b.reshape(-1,120), EDGES))


def survival_upper(count, misses, alpha):
    """Round the one-sided binomial endpoint upward and check it over the rationals."""
    if misses == count:
        return 1.
    estimate = beta.ppf(1-float(alpha), misses+1, count-misses)
    q = Q(int(np.ceil(1000*estimate)), 1000)
    assert sum(comb(count,j)*q**j*(1-q)**(count-j) for j in range(misses+1)) <= alpha
    return float(q)


def draw(output, reference, replayed, independent):
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    x = np.geomspace(20,20000,2400)
    centers = np.sqrt(EDGES[1:]*EDGES[:-1])
    def visible(raw):
        return np.maximum(-60, np.interp(np.log(x), np.log(centers), raw))
    view = visible(reference)
    fig, axes = plt.subplots(2, 1, figsize=(9,5.7), sharex=True, height_ratios=(2,1),
                             layout='constrained')
    axes[0].plot(x, view, color='#27638a', lw=2, label='Original 24 pairs')
    axes[0].plot(x, visible(independent), '--', color='#a64423', lw=1.4,
                 label='96 independent pairs')
    axes[0].set_ylabel('Displayed response (dB)')
    axes[0].legend(loc='upper right', frameon=False)
    for j, raw in enumerate(replayed):
        axes[1].plot(x, visible(raw)-view, color='#5492ae', alpha=.25, lw=.6,
                     label='104 replays of the five tails' if j == 0 else None)
    axes[1].plot(x, visible(independent)-view, color='#a64423', lw=1.4,
                 label='Independent pairs')
    axes[1].axhline(0, color='black', lw=.5)
    axes[1].legend(loc='lower right', frameon=False, fontsize=8)
    axes[1].set_ylabel('Change from original (dB)')
    axes[1].set_xlabel('Frequency (Hz)')
    for ax in axes:
        ax.set_xscale('log')
        ax.set_xlim(x[0],x[-1])
        ax.grid(alpha=.17)
    fig.suptitle('Noise response at 249.70 Hz cutoff, 60.33% drive, 97.73% resonance')
    fig.savefig(output/'survivor-response.png', dpi=160)
    plt.close(fig)


def main(output):
    protocol = json.loads((output/'protocol.json').read_text())
    source = output.parent/'lagged-fresh-extended/pairs.npz'
    assert sha256(source.read_bytes()).hexdigest() == protocol['source_hash']
    with np.load(source) as archive:
        count = len(archive['base'])
        base = archive['base'][:,0]
        old = archive['delta'][:,0]
        age = protocol['age']
        prefix = (base+old[:,:age].sum(1)).mean(0)
        original = (base+old.sum(1)).mean(0)
        live = np.flatnonzero((archive['met'][:,0] == 0) | (archive['met'][:,0] > age))
        assert np.all(old[:,age:,1] == 0)
    fixed = curve(*original)
    with np.load(output/'replays.npz') as z:
        delta, meetings = z['delta'], z['meetings']
        np.testing.assert_array_equal(z['parents'], live)
        assert np.all(meetings > 0)
        assert np.all(delta[np.arange(delta.shape[2]) >= meetings[:,:,None]] == 0)
        assert np.all(np.isfinite(delta))
    tail = delta.sum(2)/count
    replayed = curve(prefix[0]+tail.sum(1), prefix[1])
    score = rms(replayed,fixed)
    avg = curve(prefix[0]+tail.mean(0).sum(0),prefix[1])
    with np.load(output/'fresh-parents.npz') as audit:
        fresh = (audit['base']+audit['delta'].sum(1)).mean(0)
        fresh8 = (audit['base']+audit['delta'][:,:age].sum(1)).mean(0)
        counts = audit['meetings']
        assert np.all(counts>0)
    dry = json.loads((output/'dry-power.json').read_text())
    lo, hi = np.array(dry['dry_power_lower']), np.array(dry['dry_power_upper'])
    midpoint = np.sqrt(lo*hi)
    anchor = curve(fresh[0],midpoint)
    half = float(np.ceil(max(5*np.log10(hi/lo))*1e6)/1e6)
    with np.load(output/'holdout/powers.npz') as z:
        np.testing.assert_array_equal(z['reference'], anchor)
        errors = rms(curve(z['means'][:,0],z['means'][:,1]), anchor)
        np.testing.assert_array_equal(errors,z['errors'])
    calibration = json.loads((output/'holdout/summary.json').read_text())
    assert sha256((output/'fresh-parents.npz').read_bytes()).hexdigest() == calibration['reference_hash']
    assert sha256((output/'dry-power.json').read_bytes()).hexdigest() == calibration['dry_hash']
    # The conditional score treats a censored fork as infinite. Every fork finished.
    n = len(score)
    assert Q(95,100)**n < Q(1,40)
    parents = []
    simultaneous_alpha = Q(1, 40 * meetings.shape[1] * protocol['horizon'])
    for j, i in enumerate(live):
        with np.load(output/f'parent-{i}.npz') as boundary:
            words = boundary['words'].view('f4').astype(float)
        parent_score = rms(curve(prefix[0]+tail[:,j], prefix[1]), curve(*prefix))
        parents.append(dict(id=int(i), start_stage_gap=float(abs(words[0,:4]-words[1,:4]).max()),
                       extra_records_median=float(np.median(meetings[:,j])),
                       extra_records_max=int(max(meetings[:,j])),
                       max_rms_change_in_original_pool=float(max(parent_score)),
                       survival_upper_by_extra_record=[survival_upper(n,int(sum(meetings[:,j]>h)),simultaneous_alpha)
                           for h in range(1,protocol['horizon']+1)]))
    budget = Q(1,2)-Q(str(half))-Q(str(calibration['outward_tolerance_db']))
    report = dict(parents=parents, replay_groups=n, comparisons=int(meetings.size),
        original_vs_branches_median=float(np.median(score)), original_vs_branches_max=float(max(score)),
        original_vs_rebranched_mean=float(rms(fixed,avg)[0]),
        conditional_probability_at_least_95_failure_upper=float(Q(95,100)**n),
        fresh_parents=len(counts), fresh_live_after_8=int(sum(counts>age)),
        fresh_latest_meeting=int(max(counts)), fresh_vs_original=float(rms(curve(*fresh),fixed)[0]),
        fresh_8_vs_full=float(rms(curve(*fresh),curve(*fresh8))[0]),
        fresh_survival_40_upper_975=survival_upper(len(counts), 0, Q(1,40)),
        dry_midpoint_half_width_db_upper=half,
        frozen_vs_frozen_wet_analytic_dry=float(rms(fixed,curve(original[0],midpoint))[0]),
        frozen_vs_new_anchor=float(rms(fixed,anchor)[0]),
        new_anchor_probe_radius=calibration['outward_tolerance_db'],
        remaining_stationary_wet_allowance_db=float(budget))
    atomic_json(output/'summary.json', report)
    draw(output, fixed, replayed, curve(*fresh))
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    main(parser.parse_args().output)
