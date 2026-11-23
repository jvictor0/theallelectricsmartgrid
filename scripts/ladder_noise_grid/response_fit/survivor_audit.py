"""Estimate the independent-parent contribution to the 249.70 Hz reference error."""
import argparse
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

import numpy as np

from lagged_records import trial
from record_bank import bind


def main(results, library, output, workers):
    seed, count, depth = 681093254, 96, 40
    with np.load(results/'screen-v3/lagged-fresh-extended/pairs.npz') as z:
        settings = z['settings'][:1]
    with np.load(results/'physics-v1/analysis-coefficients.npz') as z:
        analyzer = [z[k].astype(float) for k in ('first', 'second', 'window')]
    lib = bind(library)
    seeds = np.random.SeedSequence(seed).spawn(count)
    def one(child):
        return trial(lib, child, settings, depth, analyzer)
    with ThreadPoolExecutor(workers) as pool:
        rows = list(pool.map(one, seeds))
    base, delta, meetings, offsets = map(np.stack, zip(*rows))
    np.savez_compressed(output, base=base[:,0], delta=delta[:,0], meetings=meetings[:,0],
                        seed=seed, offsets=offsets, settings=settings)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('results', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--library', type=Path, default=Path('build/tools/ladder-noise-grid/cycle.so'))
    parser.add_argument('--workers', type=int, default=4)
    args = parser.parse_args()
    main(args.results, args.library, args.output, args.workers)
