"""Independent one-record test of squared-state drift at the selected checkpoint."""
import argparse
from concurrent.futures import ThreadPoolExecutor
from contextlib import ExitStack
from fractions import Fraction as Q
from math import prod
from pathlib import Path

import numpy as np

from record_bank import bind
from survivor_replay import checkpoint
from survey import make_noise, atomic_json


def squared_gap(words):
    younger, older = words.view('f4')
    return max(abs(Q(float(a))-Q(float(b))) for a,b in zip(older, younger))**2


def main(results, library, output):
    # Select the parent on the pilot. Freeze the wager and sample size before the holdout.
    parent, seed, count = 6, 3957102386, 256
    with np.load(results/'screen-v3/lagged-fresh-extended/pairs.npz') as z:
        archive = dict(z)
    with np.load(results/'physics-v1/analysis-coefficients.npz') as z:
        analyzer = [z[k].astype(float) for k in ('first', 'second', 'window')]
    lib = bind(library)
    source = np.random.SeedSequence(int(archive['seed'])).spawn(len(archive['met']))[parent]
    with ExitStack() as scope:
        states, boundary = checkpoint(scope, lib, source, archive['settings'][0], archive, parent, analyzer)
        with np.load(output/f'parent-{parent}.npz') as saved:
            np.testing.assert_array_equal(boundary['words'], saved['words'])
        def step(child):
            rng = np.random.default_rng(child)
            noise = make_noise(int(rng.integers(2**63)), 1, 14).ravel()
            y = np.empty_like(noise)
            words = np.empty((2, 14), np.uint32)
            for i, origin in enumerate(states):
                copy = lib.CopyCycleFilter(origin)
                try:
                    lib.RunCycleRecord(copy, len(y), noise, y)
                    lib.ReadCycleState(copy, words[i])
                finally:
                    lib.DestroyCycleFilter(copy)
            return words
        with ThreadPoolExecutor(6) as pool:
            end = np.stack(list(pool.map(step, np.random.SeedSequence(seed).spawn(count))))
    initial = squared_gap(boundary['words'])
    ratios = [squared_gap(w)/initial for w in end]
    # If E X <= m, each independent nonnegative factor has expectation <= 1.
    # Markov's inequality bounds the probability that their product reaches 40.
    def evidence(m):
        return prod(Q(15,16)+x/(16*m) for x in ratios)
    unit = evidence(Q(1))
    lo, hi = 0, 1
    while evidence(Q(hi,100)) >= 40:
        lo, hi = hi, hi*2
    while hi-lo > 1:
        mid = (lo+hi)//2
        if evidence(Q(mid,100)) >= 40:
            lo = mid
        else:
            hi = mid
    np.savez_compressed(output/'drift-holdout.npz', end=end, seed=seed, parent=parent)
    atomic_json(output/'drift-holdout.json', dict(parent=parent, count=count, seed=seed,
                wager='1/16', confidence=0.975, mean_squared_ratio=float(sum(ratios)/count),
                median_squared_ratio=float(np.median([float(x) for x in ratios])),
                lower_mean_ratio_975=lo/100, null_mean_one_p_upper=float(min(Q(1),1/unit))))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('results', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--library', type=Path, default=Path('build/tools/ladder-noise-grid/cycle.so'))
    args = parser.parse_args()
    main(args.results, args.library, args.output)
