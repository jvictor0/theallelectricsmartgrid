"""Necessary all-history checks for a common fresh block after dry-detector erasure."""
import argparse
from concurrent.futures import ThreadPoolExecutor
from contextlib import ExitStack
from pathlib import Path
import json

import numpy as np

from record_bank import TAIL, EDGES, bands, bind, curve_mse
from survey import make_noise, atomic_json


def main(root):
    lib=bind(Path('build/tools/ladder-noise-grid/cycle.so'))
    with np.load(root.parent/'survivor-replay-250/fresh-parents.npz') as z:
        knobs=z['settings'][0]
    with np.load(root.parent.parent/'physics-v1/analysis-coefficients.npz') as z:
        analyzer=[z[k].astype(float) for k in ('first','second','window')]
    dry=json.loads((root.parent/'survivor-replay-250/dry-power.json').read_text())
    denom=np.sqrt(np.array(dry['dry_power_lower'])*dry['dry_power_upper'])
    certificate=json.loads((root/'detector-16.json').read_text())
    assert certificate['rows'][0]['code']==0
    common=make_noise(certificate['seed'],1,14).ravel()
    def words(f):
        state=np.empty(14,'u4')
        lib.ReadCycleState(f,state)
        return state
    with ExitStack() as outer:
        starts=[]
        for j in range(32):
            f=lib.CreateCycleFilter(*knobs)
            outer.callback(lib.DestroyCycleFilter,f)
            y=np.empty_like(common)
            x=make_noise(192745839+j,1,14).ravel()
            lib.RunCycleRecord(f,len(x),x,y)
            lib.RunCycleRecord(f,len(common),common,y)
            starts.append((f,y[-TAIL:].copy()))
        state=np.stack([words(f) for f,_ in starts])
        assert np.all(state[:,4:9]==state[0,4:9]) and state[0,8]==certificate['rows'][0]['terminal']
        def run(child):
            rng=np.random.default_rng(child)
            with ExitStack() as scope:
                live=[]
                for j,(f,h) in enumerate(starts):
                    cp=lib.CopyCycleFilter(f)
                    scope.callback(lib.DestroyCycleFilter,cp)
                    live.append((cp,h,[j]))
                counts=[]
                level=np.zeros((32,120))
                y=np.empty_like(common)
                for age in range(32):
                    x=make_noise(int(rng.integers(2**63)),1,14).ravel()
                    children={}
                    for f,h,parents in live:
                        lib.RunCycleRecord(f,len(x),x,y)
                        if 4 <= age < 8:
                            level[parents]+=bands(y,h,analyzer)/4
                        history=y[-TAIL:].copy()
                        key=(words(f).tobytes(),history.tobytes())
                        if key in children:
                            children[key][2].extend(parents)
                        else:
                            children[key]=(f,history,parents.copy())
                    live=list(children.values())
                    counts.append(len(live))
                    if len(live)==1 and age>=7:
                        break
            return counts+[1]*(32-len(counts)),level
        with ThreadPoolExecutor(6) as pool:
            rows=list(pool.map(run,np.random.SeedSequence(3297154601).spawn(24)))
    counts,levels=map(np.stack,zip(*rows))
    assert np.all(np.isfinite(levels)) and np.all(levels>0)
    low,high=levels.min(1),levels.max(1)
    def db(x):
        return 10*np.log10(x/denom)
    spread=np.sqrt(curve_mse(db(low),db(high),EDGES))
    np.savez_compressed(root/'wet-starts.npz', states=state, image_counts=counts, levels=levels)
    report=dict(start_histories=32, fresh_words=24, different_wet_starts=len(np.unique(state,axis=0)),
        blocks_still_distinct_after_four=int(sum(counts[:,3]>1)),
        blocks_still_distinct_after_eight=int(sum(counts[:,7]>1)),
        blocks_still_distinct_after_32=int(sum(counts[:,-1]>1)),
        ensemble_block_envelope_rms=float(np.sqrt(curve_mse(db(low.mean(0))[None],
            db(high.mean(0))[None],EDGES)[0])),
        max_block_envelope_rms=float(max(spread)))
    atomic_json(root/'wet-starts.json',report)
    print(report)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('root',type=Path)
    main(parser.parse_args().root)
