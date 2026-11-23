"""Branch the five age-eight histories in the frozen 249.70 Hz lag experiment."""
import argparse
from concurrent.futures import ThreadPoolExecutor
from contextlib import ExitStack
from functools import partial
from hashlib import sha256
from pathlib import Path

import numpy as np

from lagged_records import shifted_bands, trial
from record_bank import TAIL, bind
from survey import EPOCH, atomic_json, make_noise


AGE = 8
BRANCH_SEED = 2743296701
REPLAYS = 104
HORIZON = 32


def checkpoint(pool, lib, source_seed, knobs, archive, parent, analyzer):
    def capture(n, setting, offset, states, histories, bits):
        if n == AGE:
            for state in states:
                copies.append(lib.CopyCycleFilter(state))
                pool.callback(lib.DestroyCycleFilter, copies[-1])
            boundary.update(offset=offset, histories=np.stack(histories), words=np.stack(bits))

    copies = []
    boundary = {}
    base, delta, met, offset = trial(lib, source_seed, [knobs], AGE+1, analyzer, capture)
    # The original ninth record is reserved for a bit-exact positive control.
    np.testing.assert_array_equal(base[0].view('u8'), archive['base'][parent, 0].view('u8'))
    np.testing.assert_array_equal(delta[0].view('u8'), archive['delta'][parent, 0, :AGE+1].view('u8'))
    assert offset == archive['offsets'][parent]
    assert (met[0] or np.inf) == (archive['met'][parent, 0] if archive['met'][parent, 0] <= AGE+1 else np.inf)
    assert len(copies) == 2 and boundary['histories'].shape == (2, TAIL+offset)
    return copies, boundary


def replay(task, lib, checkpoints, analyzer):
    index, parent, repetition = task
    copies, boundary = checkpoints[index]
    rng = np.random.default_rng(np.random.SeedSequence([BRANCH_SEED, int(parent), repetition]))
    return continue_pair(lib, copies, boundary, analyzer, rng)


def continue_pair(lib, copies, boundary, analyzer, rng):
    offset = boundary['offset']
    histories = list(boundary['histories'])
    delta = np.zeros((HORIZON, 120))
    # Differences for the four ladder stages, dry detector, and feedback detector.
    distances = np.zeros((HORIZON, 3))
    words = np.empty((2, 14), dtype=np.uint32)
    with ExitStack() as scope:
        states = [lib.CopyCycleFilter(x) for x in copies]
        for state in states:
            scope.callback(lib.DestroyCycleFilter, state)
        for record in range(HORIZON):
            noise = make_noise(int(rng.integers(2**63)), 1, 14).reshape(-1)
            levels = []
            for k, state in enumerate(states):
                out = np.empty_like(noise)
                lib.RunCycleRecord(state, len(noise), noise, out)
                lib.ReadCycleState(state, words[k])
                levels.append(shifted_bands(out, histories[k], offset, analyzer))
                histories[k] = out[-(TAIL+offset):].copy()
            delta[record] = levels[1]-levels[0]
            diff = abs(np.diff(words.view('f4').astype(float), axis=0)[0])
            distances[record] = [max(diff[:4]), max(diff[4:9]), max(diff[9:])]
            if not np.isfinite(delta[record]).all() or not np.isfinite(diff).all():
                raise ArithmeticError('Nonfinite paired stock replay')
            if (np.array_equal(words[0], words[1]) and
                    np.array_equal(histories[0].view('u4'), histories[1].view('u4'))):
                return delta, distances, record+1
    return delta, distances, 0


def check_original_tail(lib, origin, analyzer, seed, archive, parent):
    rng = np.random.default_rng(seed)
    rng.integers(2**63)  # initial record seed
    rng.integers(14*EPOCH)  # initial phase
    for _ in range(1+AGE):  # one extra record and the common prefix
        rng.integers(2**63)
    delta, _, met = continue_pair(lib, *origin, analyzer, rng)
    expected = archive['delta'][parent,0,AGE:AGE+HORIZON,0]
    np.testing.assert_array_equal(delta.view('u8'), expected.view('u8'))
    assert met and AGE+met == archive['met'][parent,0]


def main(results, library, output, workers):
    archive_path = results/'screen-v3/lagged-fresh-extended/pairs.npz'
    with np.load(archive_path) as z:
        archive = dict(z)
    with np.load(results/'physics-v1/analysis-coefficients.npz') as z:
        analyzer = [z[k].astype(float) for k in ('first', 'second', 'window')]
    lib = bind(library)
    met = archive['met'][:, 0]
    parents = np.flatnonzero((met > AGE) | (met == 0))
    seeds = np.random.SeedSequence(int(archive['seed'])).spawn(len(met))
    output.mkdir(parents=True, exist_ok=True)
    with ExitStack() as scope:
        checkpoints = [checkpoint(scope, lib, seeds[i], archive['settings'][0], archive, i, analyzer)
                       for i in parents]
        for i, origin in zip(parents, checkpoints):
            check_original_tail(lib, origin, analyzer, seeds[i], archive, i)
            _, boundary = origin
            np.savez_compressed(output/f'parent-{i}.npz', **boundary)
        print(f'Reproduced parents {parents.tolist()} and archived their boundaries', flush=True)
        tasks = [(j, parent, rep) for rep in range(REPLAYS) for j, parent in enumerate(parents)]
        run = partial(replay, lib=lib, checkpoints=checkpoints, analyzer=analyzer)
        with ThreadPoolExecutor(workers) as executor:
            rows = []
            for row in executor.map(run, tasks):
                rows.append(row)
                if len(rows) % (16*len(parents)) == 0:
                    print(f'{len(rows)}/{len(tasks)} completed', flush=True)
    delta, distances, meetings = [np.stack(items).reshape(REPLAYS, len(parents), *np.shape(items[0]))
                                  for items in zip(*rows)]
    np.savez_compressed(output/'replays.npz', parents=parents, delta=delta, distances=distances,
                        meetings=meetings)
    atomic_json(output/'protocol.json', dict(source_hash=sha256(archive_path.read_bytes()).hexdigest(),
                knobs=archive['settings'][0].tolist(), age=AGE, seeds=[int(archive['seed']), BRANCH_SEED],
                repetitions=REPLAYS, horizon=HORIZON, parents=parents.tolist()))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('results', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--library', type=Path, default=Path('build/tools/ladder-noise-grid/cycle.so'))
    parser.add_argument('--workers', type=int, default=6)
    args = parser.parse_args()
    main(args.results, args.library, args.output, args.workers)
