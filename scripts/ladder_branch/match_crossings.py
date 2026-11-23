"""Identify the independently enclosed periodic powers on the continued arc."""
import argparse
import json
from pathlib import Path
import numpy as np
from phase_tube import Ball, Tone
from resonance_chain import Chain


def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('charts',type=Path)
    ap.add_argument('powers',type=Path)
    ap.add_argument('out',type=Path)
    args=ap.parse_args()
    with args.charts.open() as f:
        setup=json.loads(next(f)); charts=list(map(json.loads,f))
    states=np.load(args.charts.with_suffix('.npz'))['states']
    t=Tone(setup['q'],setup['cutoff'],setup['drive'])
    scale=np.array(setup['scale'][:-1])
    chain=Chain(t,scale)
    results=[]
    for p in json.loads(args.powers.read_text()):
        t.set_resonance(p['knob'])
        k=t.coeffs[1]
        mid=np.mean(np.array(p['centers']),axis=0)
        # This exact table is also the center of the fixed-K box at the midpoint
        # if the two endpoint K values bracket k symmetrically. Include rounding
        # and any asymmetry explicitly below.
        kl,kr=map(Ball,p['feedback'])
        u=(Ball(k)-kl)/(kr-kl)
        candidate=(Ball(p['centers'][0])+(Ball(p['centers'][1])-Ball(p['centers'][0]))*u)
        inner=float(((Ball(candidate.m,candidate.r)-Ball(mid))/Ball(scale)).magnitude().max())
        inner+=(Ball(p['defect'])/(1.-Ball(p['contraction']))).hi()
        groups=[]
        current=[]
        for i,c in enumerate(charts):
            e=(Ball(c['defect'])/(1.-Ball(c['contraction']))).hi()
            a,b=c['lo'],c['hi']
            low=(Ball(min(a,b))-e).lo(); high=(Ball(max(a,b))+e).hi()
            if low<=k<=high:
                s=(Ball(k,e)-Ball(a))/(Ball(b)-a) if b!=a else Ball(.5,.5)
                low=max(0.,float(s.lo()));high=min(1.,float(s.hi()))
                if low>high: raise ArithmeticError("empty crossing enclosure")
                # Evaluate both extrema of each affine center component.
                delta=Ball(states[i+1,:,:14])-Ball(states[i,:,:14])
                origin=Ball(states[i,:,:14])-Ball(mid)
                offset=np.maximum(((origin+delta*low)/Ball(scale)).magnitude(),((origin+delta*high)/Ball(scale)).magnitude())
                current.append((i,float((Ball(offset.max())+e).hi())))
            elif current:
                groups.append(current); current=[]
        if current: groups.append(current)
        distances=[max(x[1] for x in group) for group in groups]
        best=int(np.argmin(distances))
        group=groups[best]
        first,last=group[0][0],group[-1][0]
        def sign(i,edge):
            c=charts[i]
            e=(Ball(c['defect'])/(1.-Ball(c['contraction']))).hi()
            v=Ball(c[edge],e)-k
            return -1 if v.hi()<0 else (1 if v.lo()>0 else 0)
        # The terminal chart fixes K exactly by its final hyperplane.
        if not (last==len(charts)-1 and charts[-1]['hi']==k):
            before=sign(max(first-1,0),'lo')
            after=sign(min(last+1,len(charts)-1),'hi')
            if before*after!=-1: raise ArithmeticError("missing opposite sides of crossing")
        radius=max(distances[best],inner,1e-8)*1.01
        ok,lip,_=chain.check(k,k,mid,mid,radius)
        if not ok: raise ArithmeticError(f"crossing {p['knob']} {best} needs a sharper comparison at {radius}, L={lip}")
        results.append(dict(knob=p['knob'],crossing=best+1,charts=[first,last],radius=radius,contraction=lip))
    if len({(r['knob'],r['crossing']) for r in results})!=len(results): raise ArithmeticError("duplicate crossing")
    with args.out.open('x') as f: json.dump(results,f,indent=2)
    print([(r['knob'],r['crossing']) for r in results])


if __name__=='__main__':
    main()
