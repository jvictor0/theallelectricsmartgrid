"""Check the same frozen fit against each complete stage of the measured cutoff refinement."""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import time

import numpy as np


def reference_grid(root, old, factor):
    """Combine existing vertices with compact measurements. Preserve gains below the drawing floor."""
    ncut, nd, nr, nb = old.shape
    truth = np.empty(((ncut - 1) * factor + 1, nd, nr, nb), np.int16)
    seen = np.zeros(truth.shape[:3], bool)
    truth[::factor] = old
    seen[::factor] = True
    step = 8 // factor
    for offset in range(step, 8, step):
        for path in sorted(root.glob(f'offset-{offset}-tile-*.npz')):
            with np.load(path) as z:
                d,r,c = z['ids'].astype(np.intp).T
                if np.any(c % 8 != offset):
                    raise ValueError(f'Wrong cutoff phase in {path}')
                ix = np.ravel_multi_index((c // step, d, r), seen.shape)
                if len(np.unique(ix)) != len(ix) or seen.flat[ix].any():
                    raise ValueError(f'Duplicate measurement in {path}')
                wet = z['band_halves'].sum(axis=1, dtype=np.float64)
                dry = z['dry_band_halves'].sum(axis=0, dtype=np.float64)
                gains = np.rint((10 * np.log10(wet / dry)) * 128)
                if not np.isfinite(gains).all() or np.max(abs(gains)) >= 32768:
                    raise ValueError(f'Unrepresentable reference dB in {path}')
                truth[c // step,d,r] = gains.astype(np.int16)
                seen.flat[ix] = True
    if not seen.all():
        raise ValueError(f'Grid still missing {seen.size - seen.sum()} vertices at factor {factor}')
    return truth


def wait_for_stage(root, factor):
    # The acquisition writes offsets in order 4,2,6,1,3,5,7, after an atomic tile rename.
    stages = {1: (), 2: (2,6,1,3,5,7), 4: (1,3,5,7), 8: ()}
    if factor == 1:
        return
    while True:
        p = json.loads((root / 'progress.json').read_text())
        if p['complete'] or p['offset'] in stages[factor]:
            return
        if time.time() - p['updated_unix'] > 3600:
            raise RuntimeError('Acquisition progress is more than one hour old')
        time.sleep(60)


def run(args):
    sys.path.insert(0, str(args.base_fit))
    from certify import check_grid
    archive = np.load(args.base_fit / 'truth.npy', mmap_mode='r')
    prior = json.loads((args.model.parent / 'certificate.json').read_text())
    if (hashlib.sha256(memoryview(archive).cast('B')).hexdigest() != prior['archive_hash']
            or hashlib.sha256(args.model.read_bytes()).hexdigest() != prior['model_hash']):
        raise ValueError('Earlier reference or frozen model differs from its certificate')
    for factor in args.factor:
        if args.wait:
            wait_for_stage(args.grid, factor)
        truth = archive if factor == 1 else reference_grid(args.grid, archive, factor)
        result, intervals = check_grid(truth, args.model, factor)
        if factor == 1 and result != prior:
            raise ValueError('Original certificate did not reproduce')
        del truth
        dst = args.grid / f'fit-certificate-{factor}x.json'
        with tempfile.NamedTemporaryFile(dir=args.grid, mode='w', delete=False) as f:
            json.dump(dict(certificate=result, by_cutoff=intervals), f)
            f.write('\n')
        Path(f.name).replace(dst)
        print(f'Completed {factor}x grid: {result}', flush=True)


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--grid', type=Path, required=True)
    p.add_argument('--model', type=Path, required=True)
    p.add_argument('--base-fit', type=Path, required=True)
    p.add_argument('--factor', type=int, nargs='+', choices=[1,2,4,8], required=True)
    p.add_argument('--wait', action='store_true', help='Start each check after its entire grid has arrived')
    run(p.parse_args())
