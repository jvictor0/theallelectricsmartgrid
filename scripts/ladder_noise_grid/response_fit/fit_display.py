"""Fit and select a packed noise-response model using visible dB only."""
from pathlib import Path
import json
import sys
import numpy as np

here = Path(__file__).resolve().parent
old = here.parent / 'whole-curve-v1'
if not old.is_dir():
    old = here.parent / 'whole-curve-fit'
audits = here.parent / 'physics-v1'
sys.path.insert(0, str(old))
from fit import reference, mass, design, project_cutoff
from refine import costs, optimal_mesh
sys.path.append(str(audits))
from response import response_db

survey_source = here.parent.parent/'src'
if not survey_source.is_dir():
    survey_source = next(p/'scripts/ladder_noise_grid' for p in here.parents if (p/'scripts/ladder_noise_grid').is_dir())
sys.path.insert(0,str(survey_source))
from display_metric import DISPLAY_FLOOR_DB as FLOOR, curve_mse
BUDGET = 10_000_000

def bank(paths):
    coords=[];curves=[]
    for path in paths:
        with np.load(path) as z:
            p=z['power'].mean(axis=0)
            curves.append(10*np.log10(p[:-1]/p[-1]))
            coords.append(z['coords'])
    return np.concatenate(coords),np.concatenate(curves)

def score(model,data,edges):
    coords,truth=data
    f=np.sqrt(edges[:-1]*edges[1:])
    fit=np.array([response_db(model,*p,f) for p in coords])
    return curve_mse(fit,truth,edges)

def build(truth,edges,mesh):
    da,ra,ca=mesh
    D,R=[design(a,np.arange(73)) for a in (da,ra)]
    di,ri=np.linalg.pinv(D),np.linalg.pinv(R)
    v=np.empty((361,len(da),len(ra),120),np.float32)
    for c in range(361):
        target=np.maximum(truth[c],int(128*FLOOR)).astype(float)/128
        v[c]=ri@((di@target.reshape(73,-1)).reshape(len(da),73,120))
    values=project_cutoff(v,ca)
    L=np.linalg.cholesky(mass(edges));iL=np.linalg.inv(L)
    records=[];means=[];entries=[]
    for c,p in enumerate(values):
        p=p.astype(float)
        dense=R@((D@p.reshape(len(da),-1)).reshape(73,len(ra),120))
        mean=dense.mean(axis=(0,1))
        means.append(np.rint(128*mean).astype('<i2'))
        Z=(dense.reshape(-1,120)-mean)@L
        eig,V=np.linalg.eigh(Z.T@Z/len(Z));V=V[:,::-1][:,:36]
        coef=((p-mean)@L)@V
        scale=np.maximum(np.max(abs(coef),axis=(0,1)),1e-10)
        base=(V.T@iL)*scale[:,None]
        assert np.max(abs(base))<255
        basis=np.rint(128*base).astype('<i2')
        q=np.rint(127*coef/scale).astype('i1')
        # A second score plane for a mode can recover its coarse score rounding.
        # Every plane has the same storage cost in the existing decoder.
        fine=np.rint(127*128*(coef/scale-q.astype(float)/127)).astype('i1')
        fine_basis=np.rint(base).astype('<i2')
        diff=coef-q*scale/127
        err=R@((D@diff.reshape(len(da),-1)).reshape(73,len(ra),-1))
        err= (err**2).mean(axis=(0,1))
        spans=np.diff(ca);cw=(spans[max(0,c-1)]+spans[min(c,len(spans)-1)])/2
        for k in range(V.shape[1]):
            index=len(entries)
            entries.extend([(c,basis[k],q[:,:,k]),(c,fine_basis[k],fine[:,:,k])])
            # Projection energies and scalar quantization costs are in display dB.
            benefit=max(0.,eig[-1-k]-err[k])
            records.extend([(cw*benefit,index),(cw*min(err[k],benefit*.999),index+1)])
    count=(BUDGET-5000-240*len(ca))//(240+len(da)*len(ra))
    chosen=sorted(idx for val,idx in sorted(records,reverse=True)[:count])
    rows=[entries[i] for i in chosen]
    ranks=np.bincount([c for c,_,_ in rows],minlength=len(ca))
    out=dict(version=np.array([1],'u1'),da=da,ra=ra,ca=ca,center=np.stack(means),offset=np.r_[0,np.cumsum(ranks)].astype('<u2'),
             basis=np.stack([b for _,b,_ in rows]),score=np.stack([s for _,_,s in rows]))
    return out

def main():
    truth,edges=reference()
    L=np.linalg.cholesky(mass(edges))
    cl=lambda t:np.maximum(t,int(FLOOR*128))
    dc=costs(cl(truth[::4,:,::5]).transpose(1,0,2,3),L)
    rc=costs(cl(truth[::4,::5,:]).transpose(2,0,1,3),L)
    cc=costs(cl(truth[:,::5,::5]),L)
    paths=sorted((audits/'offgrid').glob('batch-*.npz'));assert len(paths)==96
    val=bank(paths[:16]);records=[];best=float('inf')
    for nd,nr,nc in [(39,47,181),(31,39,271),(35,43,241),(43,51,181),(31,39,361),(35,51,241)]:
        axes=[optimal_mesh(c,n,1) for c,n in [(dc,nd),(rc,nr),(cc,nc)]]
        m=build(truth,edges,axes)
        losses=score(m,val,edges)
        rms=float(np.sqrt(losses.mean()))
        stats=dict(grid=[nd,nr,nc],rms=rms,worst=float(np.sqrt(max(losses))))
        print(stats,flush=True);records.append(stats)
        if rms<best:
            best=rms;np.savez(here/'display-model.npz',**m)
        (here/'selection.json').write_text(json.dumps(records,indent=2))
    model=dict(np.load(here/'display-model.npz'))
    assert (here/'display-model.npz').stat().st_size<=BUDGET
    held=score(model,bank(paths[16:]),edges)
    stock=np.load(audits/'stock-curves.npz')
    # Exclude both settings from each of the 16 model-selection noise banks.
    measured=score(model,(stock['coords'][32:],stock['stock'][32:]),edges)
    chall=np.load(audits/'challenge.npz')
    challenge=score(model,(chall['coords'],chall['curves']),edges)
    def stats(x):return dict(n=len(x),rms=float(np.sqrt(np.mean(x))),p95=float(np.sqrt(np.quantile(x,.95))),worst=float(np.sqrt(max(x))))
    info=dict(bytes=(here/'display-model.npz').stat().st_size,heldout_gpu=stats(held),heldout_stock=stats(measured),
              challenge=np.sqrt(challenge).tolist(),grid=[len(model[k]) for k in ('da','ra','ca')])
    (here/'score.json').write_text(json.dumps(info,indent=2));print(info,flush=True)

if __name__=='__main__':main()
