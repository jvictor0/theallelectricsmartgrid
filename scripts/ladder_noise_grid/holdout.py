"""Predict withheld midpoint spectra from the coarser nested grid; process completed tiles only."""
import argparse
import itertools
import json
from pathlib import Path
import numpy as np
from display_metric import display_delta
from survey import EDGES, atomic_json, band_power


class Errors:
    def __init__(self):
        self.bins=np.zeros(20001,np.int64)
        self.maximum=-1.
        self.worst=None
        self.within_one=0

    def add(self,errors,valid,knobs):
        e=errors[valid]
        if not len(e):
            return
        self.bins+=np.bincount(np.minimum((100*e).astype(np.int32),20000),minlength=len(self.bins))
        self.within_one+=np.sum(e<=1.)
        worst=np.unravel_index(np.argmax(np.where(valid,errors,-1)),valid.shape)
        if errors[worst]>self.maximum:
            self.maximum=float(errors[worst])
            self.worst=dict(grid_index=knobs[worst[0]].tolist(),band_hz=EDGES[worst[1]:worst[1]+2].tolist())

    def result(self):
        n=int(self.bins.sum())
        q=np.searchsorted(np.cumsum(self.bins),np.array([.5,.95,.99])*n)
        return dict(eligible=n,p50_p95_p99_db=(q*.01).tolist(),fraction_within_one_db=float(self.within_one/n),
                    max_db=self.maximum,worst=self.worst)


def run(directory,full):
    m=json.loads((directory/'manifest.json').read_text())
    grid=np.array(m['grid_shape'])
    shape=grid if full else np.array([19,19,19])
    if np.any((grid-1)%(shape-1)) or np.any(shape%2!=1):
        raise ValueError('Holdout axes must be aligned odd grids.')
    stride=(grid-1)//(shape-1)
    measured=np.empty((*shape,120),np.float32)
    seed_delta=np.empty_like(measured)
    seen=np.zeros(shape,bool)
    count=0
    for file in sorted(directory.glob('tile-*.npz')):
        with np.load(file) as z:
            ids=z['ids'].astype(np.int32)
            take=np.all(ids%stride==0,axis=1)
            if not np.any(take):
                continue
            idx=tuple((ids[take]//stride).T)
            if seen[idx].any():
                raise ValueError('Duplicate grid points')
            psd=band_power(z['halves'][take]).astype(np.float64)
            dry=band_power(z['dry_halves']).astype(np.float64)
            measured[idx]=10*np.log10(np.maximum(psd.sum(1),1e-35)/dry.sum(0))
            split=10*np.log10(np.maximum(psd,1e-35)/dry)
            seed_delta[idx]=display_delta(split[:,1],split[:,0])
            seen[idx]=True
            count+=take.sum()
        if count==measured.size//120:
            break
    if not seen.all():
        raise RuntimeError(f'Requested lattice incomplete: {count}/{seen.size}')
    errors=Errors()
    repeat=Errors()
    axes={name:Errors() for name in ('drive','resonance','cutoff')}
    for d in range(shape[0]):
        points=np.indices(shape[1:]).reshape(2,-1).T
        p=np.column_stack((np.full(len(points),d),points))
        odd=p%2
        hold=odd.any(1)
        p,odd=p[hold],odd[hold]
        target=measured[tuple(p.T)]
        estimate=np.zeros_like(target)
        for corner in itertools.product((-1,1),repeat=3):
            neighbors=p+odd*np.array(corner)
            estimate+=measured[tuple(neighbors.T)]*.125
        diff=display_delta(estimate,target)
        valid=np.ones(target.shape,dtype=bool)
        errors.add(diff,valid,p*stride)
        repeat.add(seed_delta[tuple(p.T)],valid,p*stride)
        for a,e in enumerate(axes.values()):
            e.add(diff,valid&((odd[:,a]==1)&(odd.sum(1)==1))[:,None],p*stride)
    stats=dict(coarse_shape=((shape+1)//2).tolist(),test_shape=shape.tolist(),
               withheld=int(seen.size-np.prod((shape+1)//2)),method='trilinear dB; log cutoff; all centers clamped at -60 dB after interpolation; percentiles quantized down to 0.01 dB',
               interpolation=errors.result(),independent_seed_halves=repeat.result(),by_single_axis={k:e.result() for k,e in axes.items()})
    atomic_json(directory/('holdout-full.json' if full else 'holdout-10-to-19.json'),stats)
    print(json.dumps(stats,indent=2),flush=True)


if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('directory',type=Path);ap.add_argument('--full',action='store_true')
    a=ap.parse_args(); run(a.directory,a.full)
