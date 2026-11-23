"""Lag-one coupling on fresh iid survey records, with a uniformly random reset phase."""
import argparse
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

import numpy as np

from record_bank import TAIL, bands, bind
from display_metric import curve_mse
from survey import EDGES, EPOCH, atomic_json, make_noise
from stress import SETTINGS


def tail_after(samples, length):
    if len(samples) >= length:
        return samples[-length:].copy()
    return np.pad(samples, (length-len(samples), 0))


def shifted_bands(samples, history, offset, analyzer):
    # Each edge owns 14 complete analyzer epochs. The boundary cuts the last
    # epoch offset samples after its start. Keep that prefix for the next edge.
    if offset == 0:
        return bands(samples, history, analyzer)
    data = np.r_[history[TAIL:], samples[:-offset]]
    return bands(data, history[:TAIL], analyzer)


def trial(lib, rng_seed, settings, count, analyzer, observe_boundary=None):
    indices = range(len(settings))
    rng = np.random.default_rng(rng_seed)
    def draw():
        return make_noise(int(rng.integers(2**63)), 1, 14).reshape(-1)
    def advance(f, x):
        y = np.empty_like(x)
        lib.RunCycleRecord(f, len(x), x, y)
        return y
    initial = draw()
    initial = initial[int(rng.integers(len(initial))):].copy()
    offset = len(initial) % EPOCH
    keep = TAIL+offset
    extra = draw()
    states = {}
    history = {}
    dry_history = [tail_after(initial, keep), extra[-keep:].copy()]
    try:
        for j in indices:
            x = lib.CreateCycleFilter(*settings[j])
            states[j] = [x]
            y = advance(x, initial)
            y0 = tail_after(y, keep)
            x1 = lib.CopyCycleFilter(x)
            states[j].append(x1)
            y1 = advance(x1, extra)
            history[j] = [y0, y1[-keep:].copy()]
        met = np.zeros(len(indices), int)
        deltas = np.zeros((len(indices), count, 2, 120))
        base = np.empty((len(indices), 2, 120))
        for n in range(count):
            w = draw()
            dry_zero = shifted_bands(w, dry_history[0], offset, analyzer) if n == 0 and analyzer else None
            dry_one = shifted_bands(w, dry_history[1], offset, analyzer) if n == 0 and analyzer else None
            for i, j in enumerate(indices):
                if met[i]:
                    continue
                levels, bits = [], []
                for k, f in enumerate(states[j]):
                    out = advance(f, w)
                    if analyzer:
                        levels.append(shifted_bands(out, history[j][k], offset, analyzer))
                    history[j][k] = out[-keep:].copy()
                    b = np.empty(14, np.uint32)
                    lib.ReadCycleState(f, b)
                    if not np.isfinite(b.view('f4')).all() or not np.isfinite(out).all():
                        raise ArithmeticError('Nonfinite trajectory')
                    bits.append(b)
                if n == 0 and analyzer:
                    base[i] = [levels[0], dry_zero]
                    deltas[i, n, 1] = dry_one-dry_zero
                if analyzer:
                    deltas[i, n, 0] = levels[1]-levels[0]
                if (np.array_equal(bits[0], bits[1]) and
                    np.array_equal(history[j][0].view('u4'), history[j][1].view('u4'))):
                    met[i] = n+1
                if observe_boundary is not None:
                    observe_boundary(n+1, j, offset, states[j], history[j], bits)
            if np.all(met):
                break
        return base, deltas, met, offset
    finally:
        for pair in states.values():
            for f in pair:
                lib.DestroyCycleFilter(f)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('results', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--library', type=Path, default=Path('build/tools/ladder-noise-grid/cycle.so'))
    parser.add_argument('--seed', type=int, default=509147342)
    parser.add_argument('--trials', type=int, default=24)
    parser.add_argument('--depth', type=int, default=12)
    args = parser.parse_args()
    if args.depth < 1 or args.trials < 2:
        parser.error('Positive depth and at least two independent trials required')
    lib = bind(args.library)
    with np.load(args.results/'physics-v1/analysis-coefficients.npz') as z:
        analyzer = [z[k].astype(float) for k in ('first', 'second', 'window')]
    args.output.mkdir(parents=True, exist_ok=True)
    def run(seed):
        return trial(lib, seed, SETTINGS[:3], args.depth, analyzer)
    with ThreadPoolExecutor(4) as pool:
        rows = list(pool.map(run, np.random.SeedSequence(args.seed).spawn(args.trials)))
    base, delta, met, offsets = [np.stack(block) for block in zip(*rows)]
    np.savez_compressed(args.output/'pairs.npz', base=base, delta=delta, met=met, offsets=offsets,
                        seed=args.seed, settings=SETTINGS[:3])
    with np.load(args.results/'screen-v3/physical-stress/curves.npz') as z:
        fits, stress = z['fitted'][:3], z['measured'][:3]
    def curve(p):
        wet, dry = p.mean(axis=0).transpose(1, 0, 2)
        if np.min(wet) <= 0 or np.min(dry) <= 0:
            raise ArithmeticError('Signed estimator has a nonpositive pooled power')
        return 10*np.log10(wet/dry)
    def rms(a, b):
        return np.sqrt(curve_mse(a, b, EDGES))
    unc = curve(base)
    corrected = base+delta.sum(axis=2)
    final = curve(corrected)
    half = args.trials//2
    report = dict(trials=args.trials, seed=args.seed, depth=args.depth,
                  record_seconds=14*EPOCH/192000, settings=SETTINGS[:3].tolist(),
                  met_by_record=[[int(np.count_nonzero((v>0) & (v<=t))) for t in range(1, args.depth+1)]
                                 for v in met.T],
                  pooled_correction_db=rms(final, unc).tolist(),
                  fit_error_db=rms(final, fits).tolist(), stress_disagreement_db=rms(final, stress).tolist(),
                  split_half_disagreement_db=rms(curve(corrected[:half]), curve(corrected[half:])).tolist())
    atomic_json(args.output/'summary.json', report)
    print(report, flush=True)


if __name__ == '__main__':
    main()
