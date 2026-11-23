"""Independent nonnegative four-record wet readouts at the high-resonance audit setting."""
import argparse
import ctypes as ct
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

import numpy as np
from scipy.signal import upfirdn

from fresh_dry_power import sin_pi
from lagged_records import shifted_bands
from record_bank import TAIL, bind
from survey import BAND_WEIGHTS, EPOCH, make_noise


SEED = 2817609503
COUNT = 1024
BLOCK = 4


def checked_analyzer(analyzer, library):
    fir, bass_fir, window = analyzer
    n = 4096
    assert np.sum(abs(fir))<2.04 and np.sum(abs(bass_fir))<1.47 and max(abs(window))<=1
    reverse=np.array([int(f'{j:012b}'[::-1],2) for j in range(n)],'i4')
    theta=2*np.arange(n//2)
    cos=sin_pi(theta+n//2,n)
    sin=sin_pi(theta,n)
    assert max(np.max(b-a) for a,b in (sin,cos)) < 2e-14
    twiddle=np.column_stack([cos[0],-sin[0]]).ravel()
    run=ct.CDLL(str(library.resolve())).WetFramePower
    doubles=np.ctypeslib.ndpointer('f8',flags='C_CONTIGUOUS')
    run.argtypes=[doubles,ct.c_int,ct.c_int,doubles,doubles,np.ctypeslib.ndpointer('i4'),doubles]
    run.restype=None
    def measure(wet, history, offset):
        x=np.concatenate([history,wet[:len(wet)-offset]]).astype(float)
        assert np.max(abs(x))<5
        host=upfirdn(fir,x,down=4)[:len(x)//4]
        bass=upfirdn(bass_fir,host,down=8)[:len(x)//32][TAIL//32:]
        host=host[TAIL//4:].reshape(-1,EPOCH//4)
        high=np.zeros(n//2+1)
        power=np.empty_like(high)
        for epoch in host:
            run(epoch,n//2,15,window,twiddle,reverse,power)
            high+=power
        high *= 2/(len(host)*15*48000*1536)
        run(bass,n,len(host),window,twiddle,reverse,power)
        low=power[:513]*(2/(len(host)*6000*1536))
        high[[0,-1]] *= .5
        low[0] *= .5
        psd=np.r_[high,low]
        return np.array([np.dot(psd[part],widths) for part,widths in BAND_WEIGHTS])
    return measure


def main(results, output, library, workers, verified, seed):
    with np.load(results/'screen-v3/survivor-replay-250/fresh-parents.npz') as z:
        knobs = z['settings'][0]
    with np.load(results/'physics-v1/analysis-coefficients.npz') as z:
        analyzer = [z[k].astype(float) for k in ('first', 'second', 'window')]
    lib = bind(library)
    measure = (checked_analyzer(analyzer,verified) if verified else
               lambda wet, history, offset: shifted_bands(wet,history,offset,analyzer))
    def read(child):
        rng = np.random.default_rng(child)
        def draw():
            return make_noise(int(rng.integers(2**63)),1,14).ravel()
        f = lib.CreateCycleFilter(*knobs)
        try:
            def run(x):
                wet = np.empty_like(x)
                lib.RunCycleRecord(f, len(x), x, wet)
                return wet
            first = draw()
            start = int(rng.integers(len(first)))
            run(first[start:])
            offset = (len(first)-start)%EPOCH
            for _ in range(BLOCK):
                wet = run(draw())
            history = wet[-(TAIL+offset):].copy()
            powers = []
            for _ in range(BLOCK):
                wet = run(draw())
                powers.append(measure(wet, history, offset))
                history = wet[-(TAIL+offset):].copy()
            return np.stack(powers)
        finally:
            lib.DestroyCycleFilter(f)
    output.parent.mkdir(parents=True, exist_ok=True)
    with ThreadPoolExecutor(workers) as pool:
        rows = []
        for row in pool.map(read, np.random.SeedSequence(seed).spawn(COUNT)):
            rows.append(row)
            if len(rows)%128 == 0:
                print(f'{len(rows)}/{COUNT} independent histories', flush=True)
    p = np.stack(rows)
    assert np.all(np.isfinite(p)) and np.all(p >= 0)
    np.savez_compressed(output, powers=p, seed=seed, knobs=knobs, warm_records=BLOCK,
                        checked_fft=verified is not None)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('results', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--library', type=Path, default=Path('build/tools/ladder-noise-grid/cycle.so'))
    parser.add_argument('--workers', type=int, default=8)
    parser.add_argument('--checked-fft', type=Path)
    parser.add_argument('--seed', type=int, default=SEED)
    args = parser.parse_args()
    main(args.results, args.output, args.library, args.workers, args.checked_fft, args.seed)
