"""Exact checks on the frozen response in the portion where resonance is capped."""
import argparse
from fractions import Fraction as Q
from hashlib import sha256
from itertools import product
from pathlib import Path
import json
import numpy as np

MODEL_HASH = '1e3c6fb810e4dc8ef373235ec398b691fd930fa94d577a22b8997eacd284e2ec'
# 2^(249/25) < 1000 < 2^10, checked without logarithms.
LENGTHS = [Q(249,25),Q(10)]
SCALE = 127*128

def energy(v,t):
    x=[Q(0)]+[Q(2*j+1,24) for j in range(119)]+[(Q(119,12)+t)/2,t]
    y=[v[0],*v,v[-1]]
    return sum((b-a)*(l*l+l*r+r*r) for a,b,l,r in zip(x,x[1:],y,y[1:]))/(3*t)

def quantized_curve(m,c,d,r):
    lo,hi=m['offset'][c:c+2]
    return (m['score'][lo:hi,d,r].astype(np.int64)@m['basis'][lo:hi].astype(np.int64)
            +127*m['center'][c].astype(np.int64))

def neighbors(knots,x):
    i=min(len(knots)-2,max(0,int(np.searchsorted(knots,float(x),side='right')-1)))
    t=(x-int(knots[i]))/int(knots[i+1]-knots[i])
    return ((i,1-t),(i+1,t))

def at(m, d, r, c):
    out=[Q(0)]*120
    for (ci,wc),(di,wd),(ri,wr) in product(*(neighbors(m[k],v) for k,v in [('ca',c),('da',d),('ra',r)])):
        w=wc*wd*wr/SCALE
        out=[a+w*int(b) for a,b in zip(out,quantized_curve(m,ci,di,ri))]
    return out

def main(model_path):
    assert sha256(model_path.read_bytes()).hexdigest()==MODEL_HASH
    assert 2**249 < 1000**25 and 1000 < 2**10
    m=dict(np.load(model_path))
    # Coordinates in the original 72,72,360 lattice. The two resonance settings
    # are on the same side of the physical cap at this drive and cutoff.
    a,b=[at(m,Q(327,5),r,Q(713,2)) for r in (Q(40),Q(45))]
    assert min(a+b)>-60
    diff=[x-y for x,y in zip(a,b)]
    lower=min(energy(diff,t) for t in LENGTHS)
    assert lower>Q(2,5)**2
    # Across the rectangular slab, compare the fitted curve to the one at
    # maximum resonance at the SAME drive and cutoff. Their difference is
    # multi-affine. Convexity bounds its integrated square by its worst vertex.
    axes=[np.flatnonzero(m[k]>=lo) for k,lo in [('ca',345),('da',65),('ra',57)]]
    assert [int(m[k][ix[0]]) for k,ix in zip(('ca','da','ra'),axes)]==[345,65,57]
    worst=Q(0);largest=Q(0);count=0
    for c,d,r in product(*axes):
        v=quantized_curve(m,c,d,r)
        ref=quantized_curve(m,c,d,-1)
        assert min(v.min(),ref.min()) > -60*SCALE
        delta=[Q(int(n),SCALE) for n in v-ref]
        worst=max(worst,*(energy(delta,t) for t in LENGTHS))
        largest=max(largest,max(map(abs,delta)))
        count+=1
    assert worst<Q(12,100)**2 and largest<Q(31,100)
    return dict(model=MODEL_HASH,witness_mean_square_lower=str(lower),
       rms_lower_db=float(lower)**.5,unavoidable_uniform_rms_lower_db=float(lower)**.5/2,
       box_control_vertices=count,box_mean_square_upper=str(worst),
       box_rms_upper_db=float(worst)**.5,box_pointwise_upper_db=float(largest))

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('model',type=Path)
    print(json.dumps(main(p.parse_args().model),indent=2))
