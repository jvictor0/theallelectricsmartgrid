"""Validate connected tubes of discrete periodic solutions using all sample states.

The linear inverse is that of h[i+1] - A[i] h[i] on cyclic sequences. This
allows a branch to pass through an unstable physical multiplier away from +1.
"""
import argparse
import ctypes as ct
import gzip
import json
from pathlib import Path

import numpy as np

from phase_tube import (AMP, Ball, EPS, Jet, LIB, RATE, TINY, Tone, advance, constant,
                        hull, positive_mv, product, stack, trig)
from pilot import solve
from check_fit import check_anchor

LIB.CriticalFeedback.argtypes=[ct.c_double]*3
LIB.CriticalFeedback.restype=ct.c_double


def combine(early, late, length):
    a,v = early
    b,w = late
    shift = lambda x: np.roll(x,-length,axis=0)
    second=Ball(shift(b.m),shift(b.r))
    # Absolute forcing bounds for two separate unit disturbances and residuals.
    tail=(shift(w)+(second.magnitude()@v+TINY)*(1+32*EPS)+TINY)*(1+8*EPS)
    return product(second,a),tail


def inverse_bounds(nominal, forcing):
    q=len(nominal)
    a=Ball(nominal,np.zeros_like(nominal))
    power=a,forcing
    acc=None
    length=0
    for bit in range(q.bit_length()):
        if q & (1<<bit):
            acc=power if acc is None else combine(acc,power,length)
            length+=1<<bit
        if 1<<(bit+1)<=q:
            power=combine(power,power,1<<bit)
    monodromy,source=acc
    matrix=Ball(np.eye(nominal.shape[-1]))-monodromy
    approx=np.linalg.inv(matrix.m)
    error=(Ball(np.eye(nominal.shape[-1]))-product(Ball(approx,np.zeros_like(approx)),matrix)).magnitude()
    norm=float(error.sum(axis=2).max()*(1+32*EPS))
    if norm>=1:
        raise ArithmeticError("singular cyclic inverse enclosure")
    return ((abs(approx)@source+TINY)*(1+32*EPS)).max(axis=(0,1))*(1+8*EPS)/(1.-norm)


class Chain:
    inverse = staticmethod(inverse_bounds)

    def __init__(self,tone,scale):
        self.tone=tone
        self.coeff=[Ball(v,2**-48) for v in tone.coeffs]
        self.critical=Ball(LIB.CriticalFeedback(*tone.params),2**-48)
        self.scale=scale
        n=(np.arange(tone.q)*tone.cycles)%tone.q
        self.input=AMP*trig(Ball(np.where(2*n<=tone.q,n,n-tone.q))/tone.q)[0]
        self.eye=np.eye(14)
        check_anchor(tone.coeffs)

    def orbit(self,k,seed):
        t=self.tone
        t.set_resonance(np.log1p(48*k/3.9)/np.log(49))
        if abs(t.coeffs[1]-k)>1e-10:
            raise ValueError("clipped base feedback")
        root=solve(t,seed,None)
        if root is None:
            raise ArithmeticError("shooting proposal")
        z=root[0].copy()
        result=[]
        for n in range(t.q):
            result.append(z.copy())
            LIB.ReturnMap(*t.params,t.input[n:n+1],1,1,z,t.output[:1])
        return np.asarray(result)

    def check(self,lo,hi,left,right,radius):
        interval=hull(lo,hi)
        span=Ball(hi)-lo
        coeff=self.coeff.copy()
        coeff[1]=interval
        h=(self.critical-interval).inv()
        # The pad covers the coefficient rounding in the prepared reference.
        coeff[8]=h*h+Ball(0.,2**-48)
        between=hull(np.minimum(left,right),np.maximum(left,right))
        tube=between+Ball(0.,(Ball(self.scale)*radius).magnitude())
        def jets(ball):
            return [Jet(Ball(ball.m[:,j],ball.r[:,j]),Ball(self.eye[j])) for j in range(14)]
        u=constant(self.input)
        states,*_=advance(jets(tube),u,coeff)
        jac=stack([s.d for s in states])*(Ball(self.scale[None,:])/Ball(self.scale[:,None]))
        # Freeze the exact dyadic midpoints as preconditioner coefficients.
        err=jac.r.sum(axis=2)*(1+32*EPS)
        velocity=Ball(right)-Ball(left)
        directed=[Jet(Ball(between.m[:,j],between.r[:,j]),Ball(velocity.m[:,j,None],velocity.r[:,j,None])) for j in range(14)]
        states,*_=advance(directed,u,coeff,span.col(),(2.*h*h*h*span).col())
        deriv=stack([s.d for s in states])
        slope=(Ball(deriv.m[:,:,0],deriv.r[:,:,0])-Ball(np.roll(velocity.m,-1,axis=0),np.roll(velocity.r,-1,axis=0))).magnitude()
        gaps=[]
        for k,x in [(lo,left),(hi,right)]:
            c=coeff.copy()
            c[1]=Ball(k)
            b=(self.critical-k).inv()
            c[8]=b*b+Ball(0.,2**-48)
            states,*_=advance(jets(Ball(x,np.zeros_like(x))),u,c)
            gaps.append((stack([s.v for s in states])-Ball(np.roll(x,-1,axis=0))).magnitude())
        defect=(np.maximum(*gaps)+.5*slope+TINY)*(1+8*EPS)/self.scale
        eta,lip=self.inverse(jac.m,np.stack((defect*(1+8*EPS),err),axis=-1))
        accepted=np.isfinite([eta,lip]).all() and (Ball(lip)*radius+eta).hi()<radius
        return bool(accepted),float(lip),float(eta)


def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('sweep',type=Path)
    ap.add_argument('out',type=Path)
    ap.add_argument('--q',type=int,default=180)
    ap.add_argument('--minimum-step',type=float,default=1e-8)
    ap.add_argument('--start',type=float,default=0.)
    ap.add_argument('--stop',type=float)
    args=ap.parse_args()
    with gzip.open(args.sweep) as file:
        row=next(r for r in map(json.loads,file) if r['kind']=='target' and r['q']==args.q and r['r']>.8 and r['spacing']==.01)
    t=Tone(row['q'],row['cutoff_hz'],row['drive'])
    t.set_resonance(row['r'])
    end=t.coeffs[1] if args.stop is None else args.stop
    lo=args.start
    if not 0<=lo<end<=row['base_feedback']:
        raise ValueError("invalid feedback interval")
    dry=2.**np.floor(np.log2(t.anchor[8]))
    scale=np.array([1.]*4+[np.sqrt(dry)/4]*4+[dry/8]+[1.]*4+[.5])
    chain=Chain(t,scale)
    seed=t.anchor if lo==0 else np.array(min(row['steps'],key=lambda r:abs(3.9*np.expm1(np.log(49)*r['r'])/48-lo))['state'])
    left=chain.orbit(lo,seed)
    step=min(.005,end-lo)
    rows=0
    checkpoints=[left]
    with args.out.open('x',buffering=1) as f:
        def emit(**r): print(json.dumps(r,allow_nan=False),file=f)
        emit(kind="setup",q=t.q,cutoff=t.cutoff,drive=t.drive,coefficients=t.coeffs.tolist(),scale=scale.tolist(),critical=float(chain.critical.m))
        while lo<end:
            hi=min(end,lo+step)
            reason="no enclosing radius"
            try:
                right=chain.orbit(hi,left[0])
                # Set the first tested radius using an estimate of the interpolation error.
                radius=max(1e-8, min(.001,(hi-lo)**2))
                for _ in range(5):
                    ok,lip,eta=chain.check(lo,hi,left,right,radius)
                    if ok: break
                    if lip>=.98: break
                    radius=max(radius*1.5,eta/(1.-lip)*1.1)
                reason=f"lip={lip:g}, defect={eta:g}"
            except (ArithmeticError,np.linalg.LinAlgError) as e:
                ok=False
                reason=str(e)
            if not ok:
                step=(hi-lo)*.5
                if step<args.minimum_step:
                    emit(kind="unresolved",feedback=lo,next_feedback=hi,reason=reason)
                    break
                continue
            emit(kind="cell",lo=lo,hi=hi,radius=radius,contraction=lip,defect=eta)
            rows+=1
            checkpoints.append(right)
            lo,left=hi,right
            step=min(.04,step*1.25)
            if rows%500==0: print(rows,lo,flush=True)
    np.savez_compressed(args.out.with_suffix('.npz'),states=np.asarray(checkpoints))
    print(f"{rows} connected cells; reached {lo:g} of {end:g}",flush=True)


if __name__=='__main__':
    main()
