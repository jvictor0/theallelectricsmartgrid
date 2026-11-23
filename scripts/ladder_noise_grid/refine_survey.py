"""Fill the seven interior cutoff knots of each existing noise-survey interval."""
import argparse
import fcntl
import hashlib
import json
import os
from pathlib import Path
import shutil
import time

import numpy as np

import survey


OFFSETS = (4, 2, 6, 1, 3, 5, 7)
FACTOR = 8


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def compact(values):
    return {
        'ids': values['ids'],
        'band_halves': survey.band_power(values['halves']),
        'dry_band_halves': survey.band_power(values['dry_halves']),
        **{k: values[k] for k in ('band_sd', 'time_bands', 'dry_time_bands', 'dry_band_sd')},
    }


def checked_parent(parent, shape, tile_size):
    """Read only the coordinates. Check unique, exhaustive coverage before reusing them."""
    seen = np.zeros(tuple(shape), dtype=bool)
    tiles = []
    for index in range((seen.size + tile_size - 1) // tile_size):
        with np.load(parent / f'tile-{index:06d}.npz') as archive:
            ids = archive['ids']
        if ids.dtype != np.uint16 or ids.shape != (min(tile_size, seen.size - index * tile_size), 3):
            raise ValueError(f'Unexpected coordinate layout in parent tile {index}')
        flat = np.ravel_multi_index(ids.T, shape)
        if len(np.unique(flat)) != len(flat) or seen.flat[flat].any():
            raise ValueError(f'Repeated coordinates in parent tile {index}')
        seen.flat[flat] = True
        tiles.append(ids)
    if not seen.all():
        raise ValueError('Incomplete parent grid')
    return tiles


def assert_replay(gpu, parent, manifest, shape, ids):
    """Exercise the acquisition and serialization paths on the old tile's exact noise."""
    measured = survey.measure_tile(gpu, ids, shape, manifest['base_seed'], manifest['trials'],
                                   manifest['warm_epochs'], manifest['measure_epochs'])
    with np.load(parent / 'tile-000000.npz') as archive:
        expected = compact(archive)
    return measured, expected


def run(parent, out, tools):
    out.mkdir(parents=True, exist_ok=True)
    with (out / 'run.lock').open('a') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        acquire(parent, out, tools)


def acquire(parent, out, tools):
    source = Path(__file__).resolve().parent
    manifest = json.loads((parent / 'manifest.json').read_text())
    parent_progress = json.loads((parent / 'progress.json').read_text())
    base_shape = np.array(manifest['grid_shape'])
    if (not parent_progress['complete'] or parent_progress['done'] != int(base_shape.prod())
            or manifest['pilot'] or manifest['band_edges'] != survey.EDGES.tolist()):
        raise ValueError('Expected a complete full-spectrum parent with the same band definitions')
    for path in (source / 'spectrum.metal', source / 'metal.mm', tools / 'stock.so', tools / 'metal.so'):
        if digest(path) != manifest['code_hash'][path.name]:
            raise ValueError(f'{path} does not match the parent acquisition')
    shape = base_shape.copy()
    shape[2] = (shape[2] - 1) * FACTOR + 1
    if shape.max() - 1 > np.iinfo(np.uint16).max:
        raise ValueError('Grid coordinates exceed tile storage width')
    tile_size = manifest['tile']
    trials = manifest['trials']
    total = int(shape.prod() - base_shape.prod())
    tiles = checked_parent(parent, base_shape, tile_size)
    schedule_hash = hashlib.sha256()
    for ids in tiles:
        schedule_hash.update(ids.tobytes())
    identity = dict(schema='noise-cutoff-refinement-v1', parent=parent.name,
                    parent_manifest_sha256=digest(parent / 'manifest.json'),
                    parent_coordinates_sha256=schedule_hash.hexdigest(),
                    acquisition_sha256={p.name: digest(p) for p in (Path(__file__), source / 'survey.py')},
                    grid_shape=shape.tolist(), offsets=OFFSETS, total_new_points=total,
                    seed_rule='base_seed + parent tile index, identical records at the seven interior knots',
                    layout='ids in refined grid; wet and dry band_halves contain two 8-seed means of integrated linear power; remaining diagnostics are the parent band arrays')
    identity['offsets'] = list(OFFSETS)
    path = out / 'manifest.json'
    if path.exists() and json.loads(path.read_text()) != identity:
        raise ValueError('Refinement manifest differs from the existing run')
    survey.atomic_json(path, identity)
    gpu = survey.GPU(tools, (tile_size + 1) * trials, trials)
    measured, expected = assert_replay(gpu, parent, manifest, base_shape, tiles[0])
    replay = out / 'replay-parent-tile-000000.npz'
    survey.save_tile(replay, compact(measured))
    with np.load(replay) as saved:
        for key, value in expected.items():
            np.testing.assert_array_equal(saved[key].view(np.uint8), value.copy(order='C').view(np.uint8),
                                          err_msg=f'Parent replay mismatch for {key}')
    del measured, expected
    print('Verified compact writer against all seven arrays of parent tile 0.', flush=True)

    started = time.monotonic()
    computed = done = written = 0
    for offset in OFFSETS:
        for index, base_ids in enumerate(tiles):
            ids = base_ids[base_ids[:, 2] < base_shape[2] - 1].copy()
            if not len(ids):
                continue
            ids[:, 2] = ids[:, 2] * FACTOR + offset
            path = out / f'offset-{offset}-tile-{index:06d}.npz'
            done += len(ids)
            if path.exists():
                continue
            if shutil.disk_usage(out).free < 5 * 2**30:
                raise RuntimeError('Storage reserve reached; completed tiles retained')
            values = survey.measure_tile(gpu, ids, shape, manifest['base_seed'] + index, trials,
                                         manifest['warm_epochs'], manifest['measure_epochs'])
            survey.save_tile(path, compact(values))
            computed += len(ids)
            written += 1
            rate = computed / (time.monotonic() - started)
            status = dict(pid=os.getpid(), offset=offset, parent_tile=index, done=done, total=total,
                          rate_per_s=rate, eta_hours=(total - done) / rate / 3600,
                          free_gib=shutil.disk_usage(out).free / 2**30, complete=done == total,
                          updated_unix=time.time())
            survey.atomic_json(out / 'progress.json', status)
            if written <= 3 or written % 100 == 0 or done == total:
                print(json.dumps(status), flush=True)
    if done != total:
        raise RuntimeError(f'Coverage mismatch: {done} of {total}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--parent', required=True, type=Path)
    parser.add_argument('--out', required=True, type=Path)
    parser.add_argument('--tools', required=True, type=Path)
    args = parser.parse_args()
    run(args.parent.resolve(), args.out.resolve(), args.tools.resolve())
