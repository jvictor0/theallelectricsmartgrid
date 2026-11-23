"""One-second stock ladder sine probes, indexed by drive, resonance, cutoff and tone."""
import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

import numpy as np
from pilot import AMPLITUDE, LIB, RATE


def run(directory, workers):
    drives=np.linspace(0,1,10)
    resonances=np.linspace(0,1,10)
    cutoffs=np.geomspace(20,20000,10)
    tones=np.geomspace(20,20000,100)
    gains=.25*20**drives
    commands=np.expm1(np.log(49)*resonances)/48
    window=np.hanning(RATE//2)
    window/=window.sum()
    n=np.arange(RATE)
    measured=np.empty((len(drives),len(resonances),len(cutoffs),len(tones),2))
    chunks=directory/'tones'
    chunks.mkdir(parents=True,exist_ok=True)

    def evaluate(index,hz):
        file=chunks/f'tone-{index:02d}.npy'
        if file.exists():
            return index,np.load(file)
        phase=2*np.pi*hz*n/RATE
        cosine=np.cos(phase)
        u=np.ascontiguousarray(AMPLITUDE*cosine)
        quadrature=np.stack((2*window*cosine[RATE//2:],2*window*np.sin(phase[RATE//2:])))
        y=np.empty(RATE)
        state=np.zeros(14)
        result=np.empty((10,10,10,2))
        for d,g in enumerate(gains):
            for r,command in enumerate(commands):
                for c,cutoff in enumerate(cutoffs):
                    state.fill(0.)
                    LIB.ReturnMap(cutoff/RATE,command,g,u,RATE,1,state,y)
                    late=y[RATE//2:]
                    re,im=np.einsum('ij,j->i',quadrature,late)
                    total=np.einsum('i,i,i->',window,late,late)
                    result[d,r,c]=(10*np.log10((re*re+im*im)/AMPLITUDE**2),10*np.log10(2*total/AMPLITUDE**2))
        if not np.isfinite(result).all():
            raise ArithmeticError(f'nonfinite gains at {hz}')
        tmp=file.with_suffix('.partial')
        with tmp.open('wb') as f: np.save(f,result)
        tmp.replace(file)
        return index,result

    with ThreadPoolExecutor(workers) as pool:
        jobs=[pool.submit(evaluate,i,f) for i,f in enumerate(tones)]
        for count,job in enumerate(as_completed(jobs),1):
            i,values=job.result()
            measured[:,:,:,i,:]=values
            if count%10==0: print(f'{count}/{len(tones)} input frequencies finished',flush=True)
    np.savez_compressed(directory/'responses.npz',drives=drives,resonances=resonances,cutoffs=cutoffs,tones=tones,
                        gains=gains,commands=commands,db=measured,rate=RATE,amplitude=AMPLITUDE)
    print('points',measured.size//2,'dB range',measured.min(axis=(0,1,2,3)),measured.max(axis=(0,1,2,3)),flush=True)


if __name__=='__main__':
    ap=argparse.ArgumentParser()
    ap.add_argument('directory',type=Path)
    ap.add_argument('--workers',type=int,default=8)
    a=ap.parse_args()
    run(a.directory,a.workers)
