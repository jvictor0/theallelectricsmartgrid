"""Extend the unresolved noise-record runs. Keep the timeout explicit."""
from concurrent.futures import ThreadPoolExecutor
import numpy as np
from replay import here,make_noise,run
z=np.load(here/'cycles.npz')
rows=list(range(len(z['k'])))
names=z.files
copy={k:z[k].copy() for k in names}
for j in range(4):
    xx=make_noise(85130319+j,8,14).transpose(1,0,2).copy().reshape(8,-1)
    pending=[i for i in rows if z['group'][i]==j and z['period'][i]==0]
    def worker(i):
        p=z['controls'][z['k'][i]]
        return run(p,xx[z['trial'][i]],512)
    with ThreadPoolExecutor(10) as pool:
        for i,result in zip(pending,pool.map(worker,pending)):
            for key,v in zip(['start','period','bits','wave_sha','cold','phase','late'],result):
                copy[key][i]=v
    print(j,'resolved',np.count_nonzero(copy['period']),'of',len(rows),flush=True)
    np.savez_compressed(here/'extended.npz',**copy)
