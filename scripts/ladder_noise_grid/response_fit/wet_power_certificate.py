"""Bound the physical band powers on a low-feedback box and compare with the frozen UI fit.

The input law and the error accounting are specified in docs/ladder-noise-response.md.
The positive Fourier sums below enclose the expectation over the noise law.
"""
import argparse
from decimal import Decimal, localcontext
from fractions import Fraction as Q
from hashlib import sha256
from itertools import product
from math import comb, isqrt
import json
from pathlib import Path

import numpy as np

from cap_certificate import MODEL_HASH, at

HOST = 14 * 32768
NATIVE = 4 * HOST
FFT = 4096
P = 2**28
COEF = 2**21
U = Q(1, 2**24)
TINY = Q(1, 2**80)    # absorbs propagated underflow, including flush-to-zero below 2^-126
DOWN = lambda x: np.nextafter(x, -np.inf)
UP = lambda x: np.nextafter(x, np.inf)


def cosine_table():
    # Range reduce to [0,pi/2]. Fixed-point Taylor with integer arithmetic.
    # x is within 3/P of the true argument. In the first quadrant the errors
    # in successive positive Taylor terms obey e_j <= 1/P +
    # (2.5*e_(j-1) + 1.25/P)/(2j(2j-1)). Through degree 16 the accumulated
    # error, including angle error and Taylor remainder, is below 32/P.
    j = np.arange(NATIVE//4 + 1, dtype=np.int64)
    pi_floor = 843314856  # floor(pi*2^28)
    assert Q(3141592653589793, 10**15)*P > pi_floor
    assert Q(3141592653589794, 10**15)*P < pi_floor+1
    x = 2*pi_floor*j//NATIVE
    x2 = x*x//P
    term = np.full_like(x, P)
    result = term.copy()
    for i in range(1, 9):
        term = term*x2//(P*(2*i)*(2*i-1))
        result += (-1)**i*term
    return np.concatenate([result[:-1], -result[:0:-1], -result[:-1], result[:0:-1]])


def fourier_amplitude(taps, modes, period, cos):
    # All supplied FIRs and the centered periodic Hann are even. Compute signed
    # cosine dot products in int64, add coefficient and trig error in real units.
    assert NATIVE % period == 0
    q = np.rint(taps.astype(np.float64)*COEF).astype(np.int64)
    mid = len(taps)//2
    if len(taps)%2 == 0:
        assert taps[0] == 0 and np.array_equal(taps[1:mid], taps[:mid:-1])
    else:
        assert np.array_equal(taps[:mid], taps[:mid:-1])
    norm = sum(abs(int(t)) for t in q)
    assert norm*(P+32) < 2**63
    aq = np.full(len(modes), int(q[mid])*P, np.int64)
    for i in range(1,len(taps)-mid):
        aq += 2*int(q[mid+i])*cos[(modes*(i*(NATIVE//period)))%NATIVE]
    rounding = sum(abs(Q(float(v))-Q(int(w),COEF)) for v,w in zip(taps,q))
    radius = UP(float(rounding + Q(norm*32,COEF*P)))
    val = abs(aq.astype(np.float64)/(COEF*P))
    return np.maximum(0,DOWN(DOWN(val)-radius)), UP(UP(val)+radius)


def window_power(window, period, cos):
    offset = np.arange(period//2+1, dtype=np.int64)
    near = 16*(period//FFT)
    lo,hi = fourier_amplitude(window,offset[:near+1],period,cos)
    w = [Q(float(x)) for x in [0,0,*window,0,0]]
    diff2 = sum(abs(a-2*b+c) for a,b,c in zip(w,w[1:],w[2:]))
    # Enclose every frequency within half a native Fourier mode of the center.
    first_moment = sum(abs(Q(float(v))*(i-FFT//2)) for i,v in enumerate(window))
    radius = UP(float(Q(22,7)*first_moment/period))
    lo = np.r_[np.maximum(0,DOWN(lo-radius)),np.zeros(len(offset)-near-1)]
    hi = np.r_[UP(hi+radius),np.full(len(offset)-near-1,np.inf)]
    chord = np.maximum(0,np.sqrt(np.maximum(0,2*(P-cos[offset*(NATIVE//period)]-32)/P))*(1-1e-14)-UP(float(Q(22,7)/period)))
    bound = np.full(len(offset),np.inf)
    ok = chord>0
    bound[ok] = UP(float(diff2)) / chord[ok]**2*(1+1e-12)
    hi = np.minimum(hi,bound)
    return [np.r_[v,v[-2:0:-1]] for v in (np.maximum(0,DOWN(lo*lo)),UP(hi*hi))]


def spectral_intervals(z, alpha, slope):
    cos = cosine_table()
    j = np.arange(NATIVE//2+1, dtype=np.int64)
    # Fractions of one full spectral-density unit in a native Fourier cell.
    # This covers fresh IID normalized records with random record phase, and
    # the discrete Fourier lines of one periodically repeated normalized record.
    boundary=HOST//2
    inside=j<boundary
    distance=np.where(inside,np.minimum(j,boundary-j),j-boundary+1)
    tail=.5/np.maximum(.5,distance-1.5)
    density=[np.where(inside,np.maximum(0,1-tail-1e-14),0), np.where(inside,1,np.minimum(1,tail+1e-14))]
    # Squared pole gain is monotone in alpha. Evaluate its frequency chord as
    # 2*sin(pi*j/N), including odd j and the half-cell angular uncertainty.
    chord=2*cos[(NATIVE//4-j//2)%NATIVE]/P
    radius=UP(float(Q(64,P)+3*Q(22,7)/NATIVE))
    gain=[]
    for a,s,h,lower in ((alpha[0],slope[0],chord+radius,True),
                        (alpha[1],slope[1],np.maximum(0,chord-radius),False)):
        a,s=float(a),float(s)
        value = s**8*(a*a/(a*a+(1-a)*h*h))**4
        gain.append(value*(1-1e-12 if lower else 1+1e-12))
    def response(taps,period):
        center=len(taps)//2
        r=UP(float(Q(22,7)*sum(abs(Q(float(v))*(i-center)) for i,v in enumerate(taps))/period))
        lo,hi=fourier_amplitude(taps,j,period,cos)
        return np.maximum(0,DOWN(lo-r)),UP(hi+r)
    fir = response(z['first'],NATIVE)
    secondary = response(z['second'],HOST)
    result = []
    for rate, count in ((6000,60),(48000,60)):
        period=HOST*rate//48000
        starts=0 if rate==6000 else 60
        windows = window_power(z['window'],period,cos)
        masks=[]
        for side in (0,1):
            amp=fir[side]*(secondary[side] if rate==6000 else 1.)
            for spectrum in (amp*amp,amp*amp*gain[side]):
                # Distinct positive and negative host frequencies have zero cross-covariance.
                folded=np.bincount(np.r_[j%period,(-j[1:-1])%period],
                                   weights=np.r_[spectrum*density[side],(spectrum*density[side])[1:-1]],minlength=period)
                masks.append(np.r_[folded,folded])
        for lo,hi in zip(z['edges'][starts:starts+count], z['edges'][starts+1:starts+count+1]):
            bw=rate/FFT
            bins=np.arange(int(np.floor(lo/bw+.5)),int(np.ceil(hi/bw+.5)))
            power=np.zeros(4)
            for k in bins:
                weight=min(hi,(k+.5)*bw)-max(lo,(k-.5)*bw)
                if weight<=0:
                    continue
                i=k*(period//FFT)
                for m,mask in enumerate(masks):
                    power[m] += weight*np.dot(mask[i:i+period],windows[m//2])
            # Positive-only sums and products: even serial binary64 summation of
            # period terms costs < 1e-10 relative. Allow 1e-8 for all reductions.
            power *= 2/(rate*1536*32*(HOST-2))
            power[:2] *= 1-1e-8
            power[2:] *= 1+1e-8
            result.append(power)
    return np.asarray(result)


def waveform_budget():
    # Prepared coefficient box. Ideal reference tangent uses stored g / stored T.
    amin,amax=Q(29,10000),Q(34,10000)
    s=Q(1022,1000)
    gain=Q(27,100)
    K=Q(25,10**6)
    # The m-pole impulse is negative binomial. Its squared norm follows by
    # summing binom(t+m-1,m-1)^2 (1-a)^(2t). The full circular DFT average
    # differs from the integral by at most 2/NATIVE by monotone rectangle sums.
    ref=[]
    b=1-amax
    for m in range(1,5):
        integral=amax**(2*m)*sum(comb(m-1,j)**2*b**(2*j) for j in range(m))/(1-b*b)**(2*m-1)
        variance=Q(4,32)*Q(HOST,HOST-2)*s**(2*(m-1))*(integral+Q(2,NATIVE))
        ref.append(Q(isqrt((variance*10**20).__floor__())+1,10**10))
        assert ref[-1]**2>=variance
    # Check an invariant box for the four per-sample ensemble RMS errors.
    # Sizes are outward rounded by integer ceiling to 1e-10 RMS.
    def ceil10(x):
        return Q(-((-x*10**10)//1),10**10)
    def advance(old, rounding):
        rnd=U if rounding else Q(0)
        tiny=TINY if rounding else Q(0)
        xs=Q(177,1000)
        feed=K*(ref[3]+old[3])
        err=feed+4*rnd*(xs+feed)+8*tiny
        inputs=xs+err
        new=[]
        for i in range(4):
            # Multiplication forming 1-alpha and the old-state product, then FMA.
            # Per-step local rounding < 3.01u*|old| + 1.01u*alpha*|input| + 6 tiny.
            extra=(Q(301,100)*rnd*(ref[i]+old[i])+6*tiny)/amin+Q(101,100)*rnd*inputs
            d=ceil10(err+extra)
            new.append(d)
            cubic=Q(8,27)*gain**2*s*Q(3873,1000)*ref[i]**3
            err=s*d+cubic+12*rnd*s*(ref[i]+d)+128*tiny
            inputs=s*ref[i]+err
        out=(1+K)*err+K*s*ref[3]+4*rnd*(1+K)*inputs+8*tiny
        return new,ceil10(out)
    answers=[]
    for rounding in (False,True):
        ds=[Q(0)]*4
        for _ in range(30):
            nxt,e=advance(ds,rounding)
            if nxt==ds: break
            ds=nxt
        assert advance(ds,rounding)[0]==ds
        answers.append(e)
    return (amin,amax),(Q(1017,1000),s),answers


def log_interval(gain, upper):
    if gain<=0: return float('-inf')
    with localcontext() as ctx:
        ctx.prec=42
        db=Decimal(10)*Decimal.from_float(float(gain)).log10()
        # Decimal.log10 is correctly rounded. Extra guard exceeds binary64 conversion.
        return float(db)+(1e-10 if upper else -1e-10)


def main(coefficients, model):
    assert sha256(model.read_bytes()).hexdigest()==MODEL_HASH
    coefficient_hash=sha256(coefficients.read_bytes()).hexdigest()
    if coefficient_hash!='2beacba39454322758b8c2a2fa37150c989bacab21d44ee81fa71d7b2082caa0':
        raise ValueError('Analyzer coefficients differ from the frozen survey')
    with np.load(coefficients) as file: z=dict(file)
    alpha,slope,eps=waveform_budget()
    bounds=spectral_intervals(z,alpha,slope)
    f1=sum(abs(Q(float(x))) for x in z['first'])
    f2=sum(abs(Q(float(x))) for x in z['second'])
    window=sum(Q(float(x))**2 for x in z['window'])
    assert window/Q(1536)<Q(1000001,1000000)**2
    # sqrt(E band error power) <= max_t ||wave error_t||_2 times the FIR L1
    # and the Hann RMS factor, by one-sided Parseval and bin overlaps <= bin width.
    A=np.array([float((f1*f2 if b<60 else f1)*Q(1000001,1000000)) for b in range(120)])*(1+1e-12)
    dry_lo,wet_lo,dry_hi,wet_hi=bounds.T
    # Also enclose rounding the ideal normalized input once to binary32 in the dry path.
    dry_eps=float(2*U*Q(177,1000)+TINY)
    dry_l=np.sqrt(dry_lo)*(1-1e-12)-A*dry_eps
    dry_h=np.sqrt(dry_hi)*(1+1e-12)+A*dry_eps
    assert np.min(dry_l)>0
    W=A*float(eps[1])
    lower=np.maximum(0,np.sqrt(wet_lo)*(1-1e-12)-W)
    upper=np.sqrt(wet_hi)*(1+1e-12)+W
    dblo=[log_interval(q,False) for q in (lower/dry_h)**2*(1-1e-12)]
    dbhi=[log_interval(q,True) for q in (upper/dry_l)**2*(1+1e-12)]
    # The fit is multiaffine inside each native model cell in these lattice coordinates.
    m=dict(np.load(model))
    axes=[]
    for key,lo,hi in [('da',Q(0),Q(1)),('ra',Q(0),Q(1,200)),('ca',Q(80),Q(84))]:
        axes.append(sorted({lo,hi,*[Q(int(k)) for k in m[key] if lo<int(k)<hi]}))
    curves=[at(m,*pt) for pt in product(*axes)]
    flo=[float(min(v))+(-1e-10) for v in zip(*curves)]
    fhi=[float(max(v))+1e-10 for v in zip(*curves)]
    # Enclose clipped differences along each log-frequency segment; include the
    # two flat end segments. Never interpolate a prematurely clipped lower bound.
    err=[]
    for i in range(121):
        a,b=max(0,i-1),min(i,119)
        # Split exactly at floor crossings of every bounding line. The square
        # of the larger of the two signed errors is convex on each subsegment.
        # Trapezoidal integration of that upper envelope is conservative.
        lines=[(dbhi[a],dbhi[b]),(dblo[a],dblo[b]),(flo[a],flo[b]),(fhi[a],fhi[b])]
        lines=[(Q(v),Q(w)) if np.isfinite(v) and np.isfinite(w) else (Q(-60),Q(-60)) for v,w in lines]
        knots=sorted({Q(0),Q(1),*[(Q(-60)-v)/(w-v) for v,w in lines if min(v,w)<-60<max(v,w)]})
        def discrepancy(t):
            h,l,fl,fh=[max(Q(-60),v+(w-v)*t) for v,w in lines]
            return max(h-fl,fh-l,0)
        values=[discrepancy(t) for t in knots]
        err.append((max(values),sum((hi-lo)*(v*v+w*w)/2
                                     for lo,hi,v,w in zip(knots,knots[1:],values,values[1:]))))
    # Twice each interval width in twelfths of an octave is at most its weight.
    weights=[1]+[2]*119+[1]
    assert 2**239<1000**24
    mse=sum(w*area for w,(_,area) in zip(weights,err))/239
    err=[float(UP(float(v))) for v,_ in err]
    cent=0
    while Q(cent,100)**2<mse: cent+=1
    hidden=next((i for i in range(120) if max(dbhi[i:])<-60),120)
    return dict(model=MODEL_HASH,coefficients_sha256=coefficient_hash,
                control_box_lattice=dict(drive=[0,1],resonance=[0,0.005],cutoff=[80,84]),
                waveform_rms_budgets=[str(v) for v in eps],
                stationary_model_to_physical_rms_db_upper=cent/100,
                pointwise_db_upper=float(UP(max(err))),first_hidden_band_index=hidden,
                tail_model_db_error_upper=float(UP(max(err[hidden+1:]))) if hidden<120 else None,
                tail_start_center_hz=float(np.sqrt(z['edges'][hidden]*z['edges'][hidden+1])) if hidden<120 else None,
                dry_power_lower=(dry_l**2*(1-1e-12)).tolist(),
                dry_power_upper=(dry_h**2*(1+1e-12)).tolist(),
                physical_db_lower=[v if np.isfinite(v) else None for v in dblo],physical_db_upper=dbhi,
                segment_error_db=err)

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('coefficients',type=Path);p.add_argument('model',type=Path);p.add_argument('output',type=Path)
    a=p.parse_args()
    report=main(a.coefficients,a.model)
    a.output.write_text(json.dumps(report,indent=2,allow_nan=False)+'\n')
    print({k:v for k,v in report.items() if not isinstance(v,list) or len(v)<5})
