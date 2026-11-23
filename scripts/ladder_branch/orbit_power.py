"""Validate all arc crossings of a requested resonance and enclose the output powers."""
import argparse
import json
from pathlib import Path

import numpy as np

from phase_tube import AMP, Ball, EPS, TINY, Tone, advance, constant, hull, stack, trig
from resonance_chain import Chain


def power_bounds(chain,initial,k,halfwidth):
    z=chain.orbit(k,initial)
    lo,hi=k-halfwidth,k+halfwidth
    left,right=chain.orbit(lo,z[0]),chain.orbit(hi,z[0])
    radius=1e-6
    for _ in range(8):
        ok,lip,eta=chain.check(lo,hi,left,right,radius)
        if ok: break
        if lip>=.98: raise ArithmeticError("failed to isolate crossing")
        radius=max(1.5*radius,1.15*eta/(1-lip))
    else: raise ArithmeticError("failed to enclose crossing")
    width=((Ball(eta)/(1.-Ball(lip)))*Ball(chain.scale)).magnitude()
    tube=hull(np.minimum(left,right),np.maximum(left,right))+Ball(0.,width)
    return dict(feedback=[lo,hi],radius=radius,contraction=lip,defect=eta,centers=[left.tolist(),right.tolist()],
                **measure_tube(chain,tube,lo,hi))


def measure_tube(chain,tube,lo,hi,harmonics=()):
    c=chain.coeff.copy()
    c[1]=hull(lo,hi)
    ref=(chain.critical-c[1]).inv()
    c[8]=ref*ref+Ball(0.,2**-48)
    _,out,eff,_=advance([constant(Ball(tube.m[:,i],tube.r[:,i])) for i in range(14)],constant(chain.input),c)
    q=chain.tone.q
    n=(np.arange(q)*chain.tone.cycles)%q
    cos,sin=trig(Ball(np.where(2*n<=q,n,n-q))/q)
    def avg(v):
        guard=(np.max(v.magnitude())+TINY)*(q+2)*EPS
        return hull(v.lo().mean()-guard,v.hi().mean()+guard)
    real,imag=avg(2.*out*cos),avg(2.*out*sin)
    p=(real*real+imag*imag)*.5
    total=avg(out*out)
    extra={}
    for harmonic in harmonics:
        index=(harmonic*n)%q
        if 2*int(index[1])%q==0:
            raise ValueError('harmonic folds onto DC or Nyquist')
        c,s=trig(Ball(np.where(2*index<=q,index,index-q))/q)
        re,im=avg(2.*out*c),avg(2.*out*s)
        power=(re*re+im*im)*.5
        extra[str(harmonic)]=[float(power.lo()),float(power.hi())]
    return dict(phase_zero=[tube.lo()[0].tolist(),tube.hi()[0].tolist()],
                fundamental_power=[float(p.lo()),float(p.hi())],total_power=[float(total.lo()),float(total.hi())],
                projection_real=[float(real.lo()),float(real.hi())],projection_imag=[float(-imag.hi()),float(-imag.lo())],
                effective_feedback=[float(eff.lo().min()),float(eff.hi().max())],
                **({'harmonic_power':extra} if extra else {}))


def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('arcs',type=Path)
    ap.add_argument('out',type=Path)
    args=ap.parse_args()
    data=next(r for r in map(json.loads,args.arcs.open()) if r['q']==190 and r['step']==.02)
    t=Tone(190,20*1000**(56/99),90/99)
    dry=2.**np.floor(np.log2(t.anchor[8]))
    c=Chain(t,np.array([1.]*4+[np.sqrt(dry)/4]*4+[dry/8]+[1.]*4+[.5]))
    outcomes=[]
    for knob in (.6,81/99):
        t.set_resonance(knob)
        k=t.coeffs[1]
        path=data['path']
        for a,b in zip(path,path[1:]):
            if (a['r']-knob)*(b['r']-knob)>0: continue
            seed=np.array(min((a,b),key=lambda x:abs(x['r']-knob))['state'])
            out=power_bounds(c,seed,k,1e-7)
            out.update(knob=knob,arc_bracket=[a['arc'],b['arc']])
            outcomes.append(out)
    with args.out.open('x') as f: json.dump(outcomes,f,allow_nan=False)
    for r in outcomes: print(r['knob'],r['fundamental_power'],r['total_power'],r['contraction'])


if __name__=='__main__':
    main()
