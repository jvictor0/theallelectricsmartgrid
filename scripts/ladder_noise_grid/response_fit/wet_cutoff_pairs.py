"""Check a pathwise cutoff-slope argument at adjacent stock binary32 inputs."""
import argparse
import json
from pathlib import Path

import numpy as np

from record_bank import EDGES, EPOCH, TAIL, bands, bind, curve_mse, make_noise
from stress import SETTINGS


def cutoff_word(hz):
    return int(np.float32(hz/192000).view(np.uint32))


def cutoff_hz(word):
    return float(np.uint32(word).view(np.float32))*192000


def measure(lib, knobs, x, analyzer):
    y = np.empty_like(x)
    lib.Reference(*knobs, len(x), x, y)
    start = 6*EPOCH
    return bands(y[start:], y[start-TAIL:start], analyzer)


def difference(a, b, dry):
    left, right = (10*np.log10(p/dry) for p in (a,b))
    return float(np.sqrt(curve_mse(left[None], right[None], EDGES)[0]))


def main(results, library):
    lib = bind(library)
    with np.load(results/'physics-v1/analysis-coefficients.npz') as z:
        analyzer = [z[k].astype(float) for k in ('first', 'second', 'window')]
    seed = 658421
    x = make_noise(seed, 1, 14).ravel()
    start = 6*EPOCH
    dry = bands(x[start:], x[start-TAIL:start], analyzer)
    report = []
    for drive, resonance, fc in SETTINGS[:3]:
        cell = int(np.floor(360*np.log(fc/20)/np.log(1000)))
        lo, hi = [cutoff_word(20*1000**(j/360)) for j in (cell, cell+1)]
        left, right = [measure(lib, (drive,resonance,cutoff_hz(j)), x, analyzer) for j in (lo, hi)]
        coarse_difference = difference(left, right, dry)
        while hi-lo > 1:
            mid = (lo+hi)//2
            middle = measure(lib, (drive,resonance,cutoff_hz(mid)), x, analyzer)
            if difference(left,middle,dry)/(mid-lo) >= difference(middle,right,dry)/(hi-mid):
                hi, right = mid, middle
            else:
                lo, left = mid, middle
        assert hi == lo+1
        report.append(dict(drive=float(drive), resonance=float(resonance), cutoff_cell=cell,
            words=[lo,hi], cutoff_hz=[cutoff_hz(lo),cutoff_hz(hi)], coarse_pair_rms_db=coarse_difference,
            selected_pair_rms_db=difference(left, right, dry)))
    # Use fresh records at the selected endpoints. Pool linear powers before computing dB.
    for row in report:
        left, right, dry_sum = np.zeros((3,120))
        errors = []
        for j in range(1,9):
            independent = make_noise(seed+j, 1, 14).ravel()
            w0, w1 = [measure(lib, (row['drive'],row['resonance'],fc), independent, analyzer)
                      for fc in row['cutoff_hz']]
            d = bands(independent[start:], independent[start-TAIL:start], analyzer)
            errors.append(difference(w0, w1, d))
            left += w0
            right += w1
            dry_sum += d
        row.update(fresh_individual_rms_db=errors, fresh_pooled_rms_db=difference(left,right,dry_sum))
    print(json.dumps(dict(seed=seed, warm_seconds=start/192000, rows=report), indent=2, allow_nan=False))


if __name__ == '__main__':
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('results', type=Path)
    ap.add_argument('library', type=Path)
    args = ap.parse_args()
    main(args.results, args.library)
