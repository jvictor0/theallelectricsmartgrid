"""Continue failed one-period solves using several independently closed sample blocks."""
import argparse
import ctypes as ct
import json
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path
import numpy as np
from pilot import ACTIVE, LIB, RATE, Tone


DOUBLE=np.ctypeslib.ndpointer(dtype=np.float64,flags='C_CONTIGUOUS')
EDGES=np.ctypeslib.ndpointer(dtype=np.int32,ndim=1,flags='C_CONTIGUOUS')
LIB.ShootingBlocks.argtypes=[ct.c_double,DOUBLE,ct.c_double,DOUBLE,EDGES,ct.c_int]+[DOUBLE]*6
LIB.ShootingBlocks.restype=None


class Blocks:
    def __init__(self,t,block_length):
        self.t=t
        self.n=int(np.ceil(t.q/block_length))
        self.edges=np.linspace(0,t.q,self.n+1).astype(np.int32)
        self.scale=np.array([1.]*8+[.5])*np.sqrt(self.n)
        self.ends=np.empty((self.n,14))
        self.mat=np.empty((self.n,9,9))
        self.ctrl=np.empty((self.n,9))
        self.jac=np.zeros((self.n*9,self.n*9+1))
        self.template=np.zeros((self.n,14))

    def proposal(self,row):
        z=np.array(row['state'])
        self.t.set_resonance(row['r'])
        for i,(lo,hi) in enumerate(zip(self.edges,self.edges[1:])):
            self.template[i]=z
            LIB.ReturnMap(*self.t.params,self.t.input[lo:hi],int(hi-lo),1,z,self.t.output[lo:hi])
        return np.r_[(self.template[:,ACTIVE]/self.scale).reshape(-1),row['r']]

    def system(self,p):
        r=p[-1]
        res=np.expm1(np.log(49)*np.array([r,r+1e-6,r-1e-6]))/48
        self.template[:,ACTIVE]=p[:-1].reshape(self.n,9)*self.scale
        t=self.t
        LIB.ShootingBlocks(t.cutoff/RATE,res,t.params[2],t.input,self.edges,self.n,self.template,self.scale,
                           self.ends,self.mat,self.ctrl,t.output)
        for i in range(self.n):
            a=9*i;b=9*((i+1)%self.n)
            self.jac[a:a+9,a:a+9]=self.mat[i]
            self.jac[a:a+9,b:b+9]=-np.eye(9)
        self.jac[:,-1]=self.ctrl.ravel()
        mismatch=self.ends-np.roll(self.template,-1,axis=0)
        self.error=float(abs(mismatch).max())
        return (mismatch[:,ACTIVE]/self.scale).ravel(),self.jac

    def correct(self,guess,normal):
        x=guess.copy()
        for _ in range(10):
            if not np.all(np.isfinite(x)) or not -.01<=x[-1]<=1.05: return None
            gap,j=self.system(x)
            residual=np.r_[gap,(x-guess)@normal]
            if max(abs(residual))<5e-12 and self.error<3e-10: return x
            try: dx=np.linalg.solve(np.vstack((j,normal)),residual)
            except np.linalg.LinAlgError: return None
            x-=dx
        return None


def track(row):
    t=Tone(row['q'],20*1000**(56/99),90/99)
    return continue_blocks(t,row,81/99,56 if row['step']==.04 else 44)


def continue_blocks(t,row,target,block_length):
    step_max=row['step']
    b=Blocks(t,block_length)
    # Start comfortably before single shooting becomes ill conditioned. This
    # retains its orientation and avoids treating an unsuccessful last step as a root.
    start=max(1,len(row['path'])-12)
    a=b.proposal(row['path'][start-1])
    z=b.proposal(row['path'][start])
    direction=z-a
    direction/=np.linalg.norm(direction)
    corrected=b.correct(z,direction)
    if corrected is None:
        return dict(reason='restart',q=t.q,step=step_max)
    z=corrected
    step=step_max
    rs=[z[-1]]
    for _ in range(4000):
        _,j=b.system(z)
        try:
            tangent=np.linalg.solve(np.vstack((j,direction)),np.r_[np.zeros(len(z)-1),1.])
        except np.linalg.LinAlgError: break
        tangent/=np.linalg.norm(tangent)
        if z[-1]>=target-1e-10 and tangent[-1]>0:
            guess=z.copy();guess[-1]=target
            normal=np.zeros_like(z);normal[-1]=1.
            fixed=b.correct(guess,normal)
            if fixed is not None:
                b.system(fixed)
                ph=2*np.mean(t.output*t.phase)
                prod=np.eye(9)
                for jac in b.mat: prod=jac@prod
                return dict(reason='target',q=t.q,step=step_max,edges=b.edges.tolist(),
                            block_states=b.template.tolist(),state=b.template[0].tolist(),output=t.output.tolist(),
                            fundamental_power=float(abs(ph)**2/2),total_power=float(np.mean(t.output**2)),
                            phase_rad=float(np.angle(ph)),wet_multiplier=float(max(abs(np.linalg.eigvals(prod)))),
                            block_residual=b.error,base_feedback=float(3.9*np.expm1(np.log(49)*target)/48),resonance_path=rs)
        if tangent[-1]>0 and target>z[-1]: step=min(step,max(3e-6,1.005*(target-z[-1])/tangent[-1]))
        while step>1e-6:
            guess=z+step*tangent
            new=b.correct(guess,tangent)
            if new is not None and np.linalg.norm(new-guess)<.2*step:
                direction=tangent;z=new;step=min(step*1.3,step_max)
                rs.append(z[-1]);break
            step*=.5
        else: break
    return dict(reason='stalled',q=t.q,step=step_max,resonance_path=rs)


def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('arcs',type=Path)
    ap.add_argument('out',type=Path)
    ap.add_argument('--workers',type=int,default=4)
    ap.add_argument('--q',type=int,nargs='+')
    args=ap.parse_args()
    items=[x for x in map(json.loads,args.arcs.open()) if x['reason']!='target' and (not args.q or x['q'] in args.q)]
    with args.out.open('x',buffering=1) as f, ProcessPoolExecutor(args.workers) as pool:
        for r in pool.map(track,items):
            print(json.dumps(r,allow_nan=False),file=f)
            print(r['q'],r['step'],r['reason'],flush=True)


if __name__=='__main__':
    main()
