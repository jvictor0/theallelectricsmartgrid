"""Independent blind audit of a fixed model. Every missing cycle is a failure for coverage."""
from replay import *
from fit_display import score,curve_mse
from math import comb
from hashlib import sha256

def main():
    records=12;n=96;limit=16;threshold=.5
    output=here/'stationary-data';output.mkdir(exist_ok=True)
    modelpath=here/'display-model.npz'
    assert sha256(modelpath.read_bytes()).hexdigest()==(here/'frozen.sha256').read_text().split()[0]
    model=dict(np.load(modelpath))
    for j in range(n):
        path=output/f'{j:03d}.npz'
        if path.exists():continue
        d,r,c=np.random.default_rng(np.random.SeedSequence([98407711,j])).random(3)
        p=np.array([d,r,20*1000**c]);x=make_noise(97503350+j,records,14).transpose(1,0,2).copy().reshape(records,-1)
        with ThreadPoolExecutor(8) as pool:
            runs=list(pool.map(lambda t:run(p,t,limit),x))
            dry=np.array(list(pool.map(lambda t:all_phases(t,t[-1024:]),x)))
            drylate=np.array(list(pool.map(host_bands,x)))
        a,b,state,wave,reset,power,late=map(np.array,zip(*runs))
        np.savez_compressed(path,p=p,first=a,period=b,state=state,sha=wave,dry=dry,power=power,late=late,drylate=drylate,cold=reset)
        if j%16==0:print(j,flush=True)
    loss=[];success=0;incomplete=0;cycles=0;maxperiod=0;startup=[];cold_error=[]
    for path in sorted(output.glob('*.npz')):
        with np.load(path) as z:
            cycles+=np.count_nonzero(z['period']);maxperiod=max(maxperiod,int(z['period'].max()))
            cold=10*np.log10(z['cold'].mean(0)/z['drylate'].mean(0))
            cold_error.extend(score(model,(z['p'][None],cold[None]),EDGES))
            if not np.all(z['period']):incomplete+=1;continue
            ref=10*np.log10(z['power'].mean(0)/z['dry'].mean(0))
            e=score(model,(z['p'][None],ref[None]),EDGES)[0]
            success+=int(e<=threshold**2);loss.append(e)
            lat=10*np.log10(z['late'].mean(0)/z['drylate'].mean(0))
            startup.extend(curve_mse(lat[None],cold[None],EDGES))
    # Invert the exact one-sided binomial tail at alpha .05.
    lo,hi=0.,1.
    for _ in range(55):
        mid=(lo+hi)/2
        tail=sum(comb(n,k)*mid**k*(1-mid)**(n-k) for k in range(success,n+1))
        if tail<.05:lo=mid
        else:hi=mid
    def stats(v):
        return dict(rms=float(np.sqrt(np.mean(v))),p95=float(np.sqrt(np.quantile(v,.95))),worst=float(np.sqrt(max(v))))
    out=dict(settings=n,records=records,cycles=int(cycles),maxperiod=maxperiod,complete=n-incomplete,
             threshold=threshold,success=success,lower95=lo,completed_errors=stats(loss),startup=stats(startup),
             reset_all=stats(cold_error))
    (here/'stationary.json').write_text(json.dumps(out,indent=2));print(out,flush=True)
if __name__=='__main__':main()
