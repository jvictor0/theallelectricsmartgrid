"""Compare staggered stage coefficients in the delayed linearization and the driven production processor."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import csv
import ctypes
from pathlib import Path

import numpy as np
from scipy.optimize import brentq

from drive_turns import ORIGINAL,drive_knob,resonance_knob
from pilot import AMPLITUDE,LIB,PTR,RATE,Tone
from switch_frequency import CUTOFF,TARGET

LIB.SpreadResponse.argtypes=[ctypes.c_double]*4+[PTR,ctypes.c_int,PTR]
LIB.SpreadResponse.restype=None
EXPONENT=np.array([-2.,-1.,1.,2.])


def limits(alpha,spread):
    a=alpha*spread**EXPONENT
    if min(a)<=0 or max(a)>1:
        raise ValueError('spread outside the monotone stage range')
    b=1-a
    c=np.poly(b)
    # At the unit circle, -prod(z-b_i)/z^3 must be real and positive.
    # Its imaginary part contains sin(w) times a quadratic in cos(w).
    roots=np.roots([4*c[4],2*c[3],c[2]-c[4]-1])
    angles=[np.arccos(r.real) for r in roots if abs(r.imag)<1e-9 and -1<r.real<1]+[np.pi]
    candidates=[]
    for w in angles:
        z=np.exp(1j*w)
        value=-np.prod(z-b)/(z**3*np.prod(a))
        if value.real>0:
            candidates.append((value.real,w))
    gain,w=min(candidates)
    def radius(ratio):
        poly=c.copy()
        poly[1]+=ratio*gain*np.prod(a)
        return max(abs(np.roots(poly)))
    if not radius(.999)<1<radius(1.001):
        raise ArithmeticError('characteristic roots do not bracket instability')
    return gain,w*RATE/(2*np.pi)


def write_csv(file,rows):
    with file.open('w',newline='') as f:
        writer=csv.DictWriter(f,fieldnames=rows[0])
        writer.writeheader()
        writer.writerows(rows)


def linear(out):
    t=Tone(562,CUTOFF,drive_knob(ORIGINAL))
    t.set_resonance(TARGET)
    a,k,g,norm=t.coeffs[:4]
    s=g/norm
    L=k*s**3
    critical_spread=brentq(lambda v:limits(a,v)[0]-L,4.,4.5)
    rows=[]
    for v in sorted([1.,1.25,1.5,2.,2.5,3.,3.5,4.,critical_spread,4.5,5.,5.5]):
        G,f=limits(a,v)
        alphas=a*v**EXPONENT
        def stage_db(hz):
            w=2*np.pi*hz/RATE
            P=np.prod(alphas/(1-(1-alphas)*np.exp(-1j*w)))
            return 20*np.log10(abs(P))
        corner=brentq(lambda hz:stage_db(hz)+20*np.log10(4.),1,20000)
        rows.append(dict(spread=v,critical_loop_gain=G,osc_hz=f,open_at_fc_db=stage_db(CUTOFF),uncompensated_12db_hz=corner,
                         slow_hz=RATE/np.pi*np.arcsin(alphas[0]/(2*np.sqrt(1-alphas[0]))),
                         fastest_alpha=alphas[-1],origin=('threshold' if v==critical_spread else 'stable' if G>L else 'unstable'),
                         example_gain=L))
    write_csv(out/'spread-limits.csv',rows)
    f=np.geomspace(20,20000,900)
    delay=np.exp(-2j*np.pi*f/RATE)
    response=[]
    for v in [1.,2.,3.,4.,4.5,5.]:
        alphas=a*v**EXPONENT
        P=np.prod(alphas[:,None]/(1-(1-alphas[:,None])*delay),axis=0)
        G,_=limits(a,v)
        for hz,stage,H in zip(f,P,s**4*(1+k)*P/(1+L*delay*P)):
            response.append(dict(spread=v,hz=hz,open_db=20*np.log10(abs(stage)),stable_origin=G>L,
                                 closed_db=20*np.log10(abs(H))))
    write_csv(out/'linear.csv',response)
    return critical_spread


def measurements(y,hz):
    n=np.arange(len(y))
    theta=2*np.pi*hz*n/RATE
    half=len(y)//2
    out={}
    for label,start,end in [('early',half//2,half),('late',half,len(y))]:
        w=np.hanning(end-start)
        w/=w.sum()
        a=theta[start:end]
        values=y[start:end]
        # A fixed sample window gives the same observation protocol at all frequencies.
        power=w@(values*values)
        proj=2*np.sum(w*values*np.exp(-1j*a))
        third=2*np.sum(w*values*np.exp(-3j*a))
        out.update({label+'_fund_db':20*np.log10(abs(proj)/AMPLITUDE),
                    label+'_total_db':10*np.log10(2*power/AMPLITUDE**2),
                    label+'_third_share':abs(third)**2/(2*power)})
    return out


def sweep(out):
    t=Tone(562,CUTOFF,drive_knob(ORIGINAL))
    t.set_resonance(TARGET)
    L=t.coeffs[1]*(ORIGINAL/t.coeffs[3])**3
    settings=[(ORIGINAL,L,sp) for sp in (1.,2.,4.5)]+[(3.,L,1.),(3.,60.,1.),(2.5,60.,1.)]
    frequencies=sorted(set([*np.geomspace(100.,10000.,270),*np.arange(295.,386.,1.),
                            *np.linspace(341.6,342.3,15),*np.arange(945.,1176.,2.),RATE/562,RATE/561]))
    n=np.arange(RATE)
    def test(args):
        g,loop,spread,hz=args
        t=Tone(562,CUTOFF,drive_knob(g))
        s=g/t.coeffs[3]
        r=resonance_knob(loop/s**3)
        t.set_resonance(r)
        u=np.ascontiguousarray(AMPLITUDE*np.cos(2*np.pi*hz*n/RATE))
        y=np.empty_like(u)
        LIB.SpreadResponse(*t.params,spread,u,len(u),y)
        return dict(g=g,L=loop,spread=spread,hz=hz,knob=r,**measurements(y,hz))
    # The stock case must agree sample by sample with the original reference.
    u=np.ascontiguousarray(AMPLITUDE*np.cos(2*np.pi*342*n[:5000]/RATE))
    pre=np.zeros(14);y=np.empty_like(u);stock=np.empty_like(u)
    LIB.SpreadResponse(*t.params,1.,u,len(u),y)
    LIB.ReturnMap(*t.params,u,len(u),1,pre,stock)
    np.testing.assert_array_equal(y,stock)
    with ThreadPoolExecutor(5) as pool:
        rows=list(pool.map(test,[(g,L,s,f) for g,L,s in settings for f in frequencies]))
    write_csv(out/'from-reset.csv',rows)


if __name__=='__main__':
    ap=argparse.ArgumentParser()
    ap.add_argument('out',type=Path)
    a=ap.parse_args()
    print('critical spread',linear(a.out),flush=True)
    sweep(a.out)
