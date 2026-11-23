"""Test interpolation at selected off-grid controls with common fresh stock-filter records."""
from concurrent.futures import ThreadPoolExecutor
from itertools import product
import argparse, json, sys
from pathlib import Path
import numpy as np
from stress import SETTINGS

def main(experiment,out):
    sys.path.insert(0,str(experiment/'fits/screen-v3'))
    from replay import make_noise,stock,host_bands,EDGES
    from fit_display import curve_mse
    out.mkdir(parents=True,exist_ok=True)
    controls=[]; ids={}; plans=[]
    def register(p):
        key=tuple(p)
        if key not in ids:
            ids[key]=len(controls)
            controls.append((p[0]/72,p[1]/72,20*1000**(p[2]/360)))
        return ids[key]
    for d,r,c in SETTINGS[:3]:
        p=np.array([72*d,72*r,360*np.log(c/20)/np.log(1000)])
        row={'direct':[(register(p),1.)]}
        for factor in [1,2,4,8]:
            for label,axes in [('all',(0,1,2)),('drive',(0,)),('resonance',(1,)),('cutoff',(2,)),('refine_cutoff_only',(0,1,2))]:
                if factor>1 and label in ('drive','resonance'): continue
                steps=np.array([1,1,factor]) if label=='refine_cutoff_only' else np.full(3,factor)
                lo=np.floor(p*steps)/steps
                t=(p-lo)*steps
                terms=[]
                for corner in product([0,1],repeat=len(axes)):
                    q=p.copy()
                    q[list(axes)]=lo[list(axes)]+np.array(corner)/steps[list(axes)]
                    w=np.prod([t[a] if j else 1-t[a] for a,j in zip(axes,corner)])
                    terms.append((register(q),w))
                row[f'{label}_{factor}']=terms
        plans.append(row)
    with ThreadPoolExecutor(16) as pool:
        for group in range(4):
            filename=out/f'group-{group}.npz'
            saved={}
            if filename.exists():
                with np.load(filename) as z:
                    saved=dict(zip(map(tuple,z['controls']),z['wet']))
                if all(tuple(p) in saved for p in controls):continue
            noise=make_noise(81294041+group,8,14).transpose(1,0,2).copy().reshape(8,-1)
            dry=np.mean(list(pool.map(host_bands,noise)),axis=0)
            def measure(p):
                y=np.empty_like(noise[0]); bands=[]
                for x in noise:
                    stock.Reference(*p,len(x),x,y)
                    bands.append(host_bands(y))
                return np.mean(bands,axis=0)
            missing=[p for p in controls if tuple(p) not in saved]
            saved.update(zip(missing,pool.map(measure,missing)))
            wet=np.stack([saved[tuple(p)] for p in controls])
            np.savez(filename,controls=controls,wet=wet,dry=dry)
    z=[np.load(out/f'group-{i}.npz') for i in range(4)]
    wet=np.mean([v['wet'] for v in z],axis=0);dry=np.mean([v['dry'] for v in z],axis=0)
    db=10*np.log10(wet/dry)
    results=[]
    for row in plans:
        target=db[row['direct'][0][0]]; errors={}
        for key,terms in row.items():
            pred=sum(w*db[i] for i,w in terms)
            errors[key]=float(np.sqrt(curve_mse(pred[None],target[None],EDGES)[0]))
        results.append(errors)
    data=dict(settings=SETTINGS[:3].tolist(),rms=results,points=len(controls),trials=32)
    (out/'refinement.json').write_text(json.dumps(data,indent=2)+'\n'); print(data)
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('experiment',type=Path);p.add_argument('out',type=Path)
    a=p.parse_args();main(a.experiment.resolve(),a.out.resolve())
