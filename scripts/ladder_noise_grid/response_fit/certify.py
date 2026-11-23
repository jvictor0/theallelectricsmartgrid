"""Integer enclosure of displayed error over every knob and log-frequency cell."""
from itertools import product
from fractions import Fraction
from hashlib import sha256
from pathlib import Path
import json, math
import sys
import numpy as np

here=Path(__file__).resolve().parent
fit_dir = here.parent/'whole-curve-v1'
if not fit_dir.is_dir():
    fit_dir = here.parent/'whole-curve-fit'
sys.path.insert(0,str(fit_dir))
from fit import reference
from verify import get_slice
F=-7680
# Twice the maximum segment widths in twelfths of an octave.
width=np.full(121,2,np.int64);width[[0,-1]]=1
corners=list(product(range(2),repeat=3))

def padded(v):
    return np.pad(v,((0,0),(0,0),(1,1)),mode='edge')

def bounds(planes):
    ranges=[]
    for plane in planes:
        ends=[plane[d:d+72,r:r+72] for d,r in product(range(2),repeat=2)]
        ranges.append((np.minimum.reduce(ends),np.maximum.reduce(ends)))
    lower=np.minimum(ranges[0][0],ranges[1][0])
    upper=np.maximum(ranges[0][1],ranges[1][1])
    return np.minimum(lower[:,:,:-1],lower[:,:,1:]),np.maximum(upper[:,:,:-1],upper[:,:,1:])

def kink(lo,hi):
    up=np.maximum(0,hi-F);down=np.maximum(0,F-lo)
    den=np.maximum(1,up+down)
    return (up*down+den-1)//den

def inspect(r,q):
    rl,ru=bounds(r);ql,qu=bounds(q)
    J=np.maximum(kink(rl,ru),kink(ql,qu))
    visible=[np.abs(np.maximum(x,F)-np.maximum(y,F)) for x,y in zip(r,q)]
    total=0;worst=0;maxpoint=0
    for c,d,k in corners:
        v=visible[c][d:d+72,k:k+72]
        l=v[:,:,:-1]+J;h=v[:,:,1:]+J
        assert max(l.max(),h.max())<100000
        energy=np.sum((l*l+h*h)*width,axis=2)
        total+=int(energy.sum());worst=max(worst,int(energy.max()))
        maxpoint=max(maxpoint,int(l.max()),int(h.max()))
    return total,worst,maxpoint

def upper_centidb(S,D):
    # Both archive and fitted vertices are rounded with error at most 1/256 dB.
    cents=0
    while Fraction(cents,100)<Fraction(1,128) or (Fraction(cents,100)-Fraction(1,128))**2*128**2*D<S:
        cents+=1
    return cents

def check_grid(truth, path, factor):
    if truth.shape != (360*factor+1,73,73,120) or truth.dtype != np.int16:
        raise ValueError('Unexpected reference grid')
    digest=sha256(memoryview(truth).cast('B')).hexdigest()
    with np.load(path) as data:
        m=dict(data)
    for key,limit in [('ca',360),('da',72),('ra',72)]:
        a=m[key];assert a[0]==0 and a[-1]==limit and (np.diff(a)>0).all()
    # The stored fit is identical. Express its cutoff knots in units of the finer grid.
    m['ca']=m['ca'].astype(np.int64)*factor
    cache={};previous=None;total=0;worst=point=0
    cutoff_bounds=[]
    for c in range(len(truth)):
        num,den=get_slice(m,c,cache)
        assert np.abs(num).max()<2**50 and den.min()>0 and den.max()<2**36
        q=(2*num+den)//(2*den)
        now=(padded(truth[c].astype(np.int64)),padded(q))
        if previous is not None:
            a,b,f=inspect([previous[0],now[0]],[previous[1],now[1]])
            total+=a;worst=max(worst,b);point=max(point,f)
            cutoff_bounds.append(dict(interval=c-1,worst_numerator=b,
                uniform_curve_centidb=upper_centidb(b,2*239)))
        previous=now
    cells=(len(truth)-1)*72**2
    result=dict(bytes=path.stat().st_size,model_hash=sha256(path.read_bytes()).hexdigest(),archive_hash=digest,
        control_cells=cells,freq_segments=121,global_numerator=total,global_denominator=16*239*cells,
        worst_numerator=worst,worst_denominator=2*239,global_centidb=upper_centidb(total,16*239*cells),
        uniform_curve_centidb=upper_centidb(worst,2*239),pointwise_db=(point+1)/128)
    per_cutoff=dict(model_hash=result['model_hash'],archive_hash=digest,
        edge_hz=f'20 * 1000^(i/{len(truth)-1}), i=0..{len(truth)-1}',worst_denominator=2*239,intervals=cutoff_bounds)
    return result,per_cutoff

def main():
    truth,_=reference()
    result,per_cutoff=check_grid(truth,here/'display-model.npz',1)
    assert result['archive_hash']=='4f1e55bb9044a9549fd790eb2b43370250ee7c4fa1a82054740ada5372cc9b66'
    (here/'certificate-by-cutoff.json').write_text(json.dumps(per_cutoff,indent=2)+'\n')
    (here/'certificate.json').write_text(json.dumps(result,indent=2));print(result,flush=True)

if __name__=='__main__':main()
