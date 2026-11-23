"""Sample lagged meeting times jointly over a high-feedback knob box and fresh noise."""
import argparse
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

import numpy as np
from scipy.stats import beta

from lagged_records import trial
from record_bank import bind
from survey import atomic_json


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    parser.add_argument('--library', type=Path, default=Path('build/tools/ladder-noise-grid/cycle.so'))
    parser.add_argument('--seed', type=int, default=168230037)
    parser.add_argument('--trials', type=int, default=96)
    parser.add_argument('--depth', type=int, default=24)
    args = parser.parse_args()
    lib = bind(args.library)
    bounds = [[.49, .72], [.92, .98], [240., 530.]]
    def run(seed):
        knob_seed, noise_seed = seed.spawn(2)
        u, v, w = np.random.default_rng(knob_seed).random(3)
        d, r = .49+.23*u, .92+.06*v
        c = 240*(530/240)**w
        p = [d, r, c]
        coeff = np.empty(11, np.float32)
        lib.Coefficients(*p, coeff)
        ks3 = float(coeff[1])*(float(coeff[2])/float(coeff[3]))**3
        _, _, met, _ = trial(lib, noise_seed, [p], args.depth, None)
        return [*p, ks3, int(met[0])]
    with ThreadPoolExecutor(4) as pool:
        rows = list(pool.map(run, np.random.SeedSequence(args.seed).spawn(args.trials)))
    data = np.array(rows)
    missed = int(np.count_nonzero(data[:, -1] == 0))
    # Predefined endpoint. Each draw gets a new independent control and noise history.
    upper = float(beta.ppf(.95, missed+1, args.trials-missed)) if missed < args.trials else 1.
    args.output.mkdir(parents=True, exist_ok=True)
    np.savez_compressed(args.output/'range.npz', rows=data, seed=args.seed)
    report = dict(trials=args.trials, seed=args.seed, depth=args.depth, bounds=bounds,
                  ks3_range=[float(np.min(data[:, 3])), float(np.max(data[:, 3]))],
                  missed=missed, missed_upper_one_sided_95=upper,
                  met_by_record=[int(np.sum((data[:, 4]>0) & (data[:, 4]<=n)))
                                 for n in range(1, args.depth+1)])
    atomic_json(args.output/'summary.json', report)
    print(report, flush=True)


if __name__ == '__main__':
    main()
