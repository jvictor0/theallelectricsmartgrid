"""Enclose visible error widths throughout all cells of the measured response interpolant."""
import argparse
from hashlib import sha256
import json
from pathlib import Path
import sys

import numpy as np

PARTS = 4096
FLOOR = -60*128
LEVELS = (1, 3, 6)


def above(left, right, cut):
    """Outer interval in 1/PARTS segment coordinates where the affine line exceeds cut."""
    lo = np.zeros_like(left)
    hi = np.full_like(left, PARTS)
    empty = (left <= cut) & (right <= cut)
    hi[empty] = 0
    rising = (left <= cut) & (cut < right)
    falling = (right <= cut) & (cut < left)
    lo[rising] = PARTS*(cut-left[rising])//(right[rising]-left[rising])
    gap = left[falling]-right[falling]
    hi[falling] = (PARTS*(left[falling]-cut)+gap-1)//gap
    return lo, hi


def positive(upper, delta, level):
    # A visible positive error above t requires both raw level > floor+t and
    # signed raw difference > t. One integer unit covers both vertex roundings.
    a,b = above(upper[:,:,:-1], upper[:,:,1:], FLOOR+level*128-1)
    c,d = above(delta[:,:,:-1], delta[:,:,1:], level*128-1)
    lo = np.maximum(a,c)
    return lo, np.maximum(lo,np.minimum(b,d))


def corner_max(a,b):
    hi = np.maximum(a,b)
    return np.maximum.reduce([hi[d:d+72,r:r+72] for d in (0,1) for r in (0,1)])


def scan(root, fit_file, expected):
    sys.path.insert(0,str(root))
    from verify import get_slice
    grid = np.load(root/'truth.npy',mmap_mode='r')
    archive_hash = sha256(memoryview(grid).cast('B')).hexdigest()
    model_hash = sha256(fit_file.read_bytes()).hexdigest()
    if archive_hash != expected['archive_hash'] or model_hash != expected['model_hash']:
        raise ValueError('Source hashes differ from the frozen certificate')
    if grid.shape != (361,73,73,120) or grid.dtype != np.int16:
        raise ValueError('Unexpected reference geometry')
    with np.load(fit_file) as data:
        model = dict(data)
    weights = np.full(121,2,dtype=np.int64)
    weights[[0,-1]] = 1
    cache, totals, worst = {}, dict.fromkeys(LEVELS,0), dict.fromkeys(LEVELS,0)
    by_cutoff = []
    old = None
    for c in range(len(grid)):
        numerator, denominator = get_slice(model,c,cache)
        if denominator.min() <= 0 or denominator.max() >= 2**36 or np.max(np.abs(numerator)) >= 2**50:
            raise ArithmeticError('Unsafe integer model evaluation')
        fit = (2*numerator+denominator)//(2*denominator)
        def pad(x):
            return np.pad(x,((0,0),(0,0),(1,1)),mode='edge').astype(np.int64)
        new = pad(grid[c]), pad(fit)
        if old is not None:
            wet, pred = [corner_max(a,b) for a,b in zip(old,new)]
            delta_wet = corner_max(old[0]-old[1],new[0]-new[1])
            delta_pred = corner_max(old[1]-old[0],new[1]-new[0])
            row = {}
            for level in LEVELS:
                a,b = positive(wet,delta_wet,level)
                c0,d = positive(pred,delta_pred,level)
                overlap = np.maximum(0,np.minimum(b,d)-np.maximum(a,c0))
                width = b-a+d-c0-overlap
                if width.min()<0 or width.max()>PARTS:
                    raise ArithmeticError('Invalid interval union')
                counts = width@weights
                totals[level] += int(counts.sum())
                worst[level] = max(worst[level],int(counts.max()))
                row[level] = int(counts.max())
            by_cutoff.append(row)
        old = new
    # The actual axis is log2(1000) octaves, longer than 239/24.
    # All widths are bounded above by weights/24 octaves, including the shortened last band.
    den = 239*PARTS
    cells = 360*72*72
    return dict(model_hash=model_hash,archive_hash=archive_hash,cells=cells,
                ticks_per_segment=PARTS,denominator=den,
                levels={k:dict(worst_ticks=worst[k], summed_ticks=totals[k],
                    max_percent=100*worst[k]/den, mean_percent=100*totals[k]/(den*cells)) for k in LEVELS},
                max_ticks_by_cutoff=by_cutoff)


if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('base_fit',type=Path)
    ap.add_argument('model',type=Path)
    args=ap.parse_args()
    prior=json.loads((args.model.parent/'certificate.json').read_text())
    print(json.dumps(scan(args.base_fit,args.model,prior)))
