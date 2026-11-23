"""Simultaneous finite-block mean bounds and an audit of the remaining upper tail."""
import argparse
from decimal import Decimal, localcontext
from fractions import Fraction as Q
from hashlib import sha256
import json
from math import prod, isqrt
from pathlib import Path

import numpy as np

from record_bank import EDGES, curve_mse, bind
from fresh_dry_power import dyadic, sin_pi
from survey import BAND_WEIGHTS, atomic_json


def check_analyzer(root, knobs):
    coeff=np.empty(11,'f4')
    bind(Path('build/tools/ladder-noise-grid/cycle.so')).Coefficients(*knobs,coeff)
    # Positive detector denominator, clamped saturator, two binary32 roundings.
    assert (1+Q(float(coeff[1])))/Q(float(coeff[3]))*(1+Q(1,2**24))**3 < 5
    coefficients=root.parent.parent/'physics-v1/analysis-coefficients.npz'
    assert sha256(coefficients.read_bytes()).hexdigest() == '2beacba39454322758b8c2a2fa37150c989bacab21d44ee81fa71d7b2082caa0'
    with np.load(coefficients) as z:
        h,a=dyadic(z['first'])
        h2,b=dyadic(z['second'])
        hann=z['window'].astype(float)
        np.testing.assert_array_equal(z['edges'],EDGES)
    assert len(h)==257 and len(h2)==129
    assert Q(sum(abs(h)),a)<Q('2.04') and Q(sum(abs(h2)),b)<Q('1.47')
    for i,(part,widths) in enumerate(BAND_WEIGHTS):
        lo,hi=map(Q,map(float,EDGES[i:i+2]))
        rate,offset=(6000,2049) if i<60 else (48000,0)
        bin_width=Q(rate,4096)
        indices=range(part.start-offset,part.stop-offset)
        assert 0 < indices.start < indices.stop <= 2048
        exact=[min(hi,(k+Q(1,2))*bin_width)-max(lo,(k-Q(1,2))*bin_width) for k in indices]
        assert list(map(Q,map(float,widths)))==exact and sum(exact)==hi-lo
    cs=sin_pi(2*np.arange(4096)+2048,4096)
    assert max(np.max(abs(hann-(1-x)/2)) for x in cs)<1e-7
    eps=Q(1,2**53)
    def gamma(n):
        return n*eps/(1-n*eps)
    # Direct FIR products and sums, followed by one stored Hann multiplication.
    err = gamma(514)*Q('10.2')
    err = Q('1.47')*err+gamma(258)*(15+Q('1.47')*err)
    err += eps*(16+err)+Q('1e-200')
    size=Q(16)
    for _ in range(12):
        err=2*err+Q('1e-13')*(size+err)+Q('1e-200')
        size*=2
    assert err+gamma(2000)*(size+err) < Q('1e-6')
    second=np.zeros(4*(len(h2)-1)+1,dtype=object)
    second[::4]=h2
    for taps,scale,stride in [(h,a,4),(np.convolve(h,second),a*b,32)]:
        m=(len(taps)-1)//2
        corr=np.correlate(taps,taps,'full')
        # Squared l2 operator norm of decimated FIR <= l1 row sum of H H*.
        lags=np.arange(-((2*m)//stride),(2*m)//stride+1)*stride
        norm=Q(sum(abs(corr[2*m+lags])),scale**2)
        assert stride*norm < Q('1.55')
        moment=Q(sum(abs(taps)*abs(np.arange(len(taps))-m)),scale)
        delta=Q(22,7)*moment/(4096*stride)+Q(1,10**7)*Q(sum(abs(taps)),scale)
        # Commute the Hann window with the FIR. The continuous native Hann
        # has squared norm 1536*stride and Lipschitz constant pi/(4096*stride).
        assert Q('1.245')**2>Q('1.55') and Q('1.634')**2>Q(8,3)
        assert 25*(Q('1.245')+Q('1.634')*delta)**2 < 40


def lower_mean(sample, threshold):
    # X is nonnegative, stored as a downward-rounded multiple of 1/65536.
    # Under E[X] <= m each factor (m + 3X)/(4m) has mean at most one.
    # Search in float, then certify the reported grid point as an integer product.
    scale = 65536
    y = np.asarray(sample, dtype=np.int64)
    lo, hi = 0, int(np.ceil(y.mean()))+1
    while hi-lo > 1:
        mid = (hi+lo)//2
        if np.log1p(.75*(y/mid-1)).sum() >= np.log(threshold):
            lo = mid
        else:
            hi = mid
    if lo:
        numerator = prod(int(lo+3*x) for x in y)
        assert numerator >= threshold*(4*lo)**len(y)
    return Q(lo, scale)


def one_sided_rms(ref, side, upper):
    # Inserting the crossing of the two raw lines separates positive and negative
    # parts exactly. The clamped dB curves are affine between successive cuts.
    from display_metric import displayed_db
    centers = np.sqrt(EDGES[:-1]*EDGES[1:])
    hz = np.log(np.r_[EDGES[0],centers,EDGES[-1]])
    a,b = np.pad(ref,1,'edge'), np.pad(side,1,'edge')
    total = 0.
    for j,width in enumerate(np.diff(hz)):
        ra, rb = a[j:j+2], b[j:j+2]
        cuts = [0.,1.]
        for x in (ra+60,rb+60,ra-rb):
            if x[0]*x[1]<0:
                cuts.append(x[0]/(x[0]-x[1]))
        cuts = sorted(cuts)
        e = displayed_db(np.interp(cuts,[0.,1.],rb))-displayed_db(np.interp(cuts,[0.,1.],ra))
        e = np.maximum(0.,e if upper else -e)
        total += width*np.sum(np.diff(cuts)*(e[:-1]**2+e[:-1]*e[1:]+e[1:]**2)/3)
    return np.sqrt(total/(hz[-1]-hz[0]))


def verify_display(w, dry, l, u, report):
    # Enclose the compiled display bounds independently of libm. Decimal.ln is
    # correctly rounded. The integer logs here are below 1000, so subtracting
    # two 55-digit log results has absolute error below 1e-49.
    def log_q(x):
        vals=(Decimal(x.numerator).ln(),Decimal(x.denominator).ln())
        assert max(map(abs,vals)) < 1000
        return vals[0]-vals[1]
    def grid(x):
        return Q(int(x * 2**40), 2**40)
    with localcontext() as ctx:
        ctx.prec=55
        edge=[log_q(Q(float(x))) for x in EDGES]
        hz=list(map(grid,[edge[0],*((a+b)/2 for a,b in zip(edge,edge[1:])),edge[-1]]))
        uncertainty=Q(4,2**40)
        weights=[(b-a+uncertainty)/(hz[-1]-hz[0]-uncertainty) for a,b in zip(hz,hz[1:])]
        ln10=Decimal(10).ln()
        ws=list(map(Q,map(float,w)))
        ref=[(log_q(x)-(log_q(Q(a))+log_q(Q(b)))/2)*10/ln10 for x,a,b in
             zip(ws,dry['dry_power_lower'],dry['dry_power_upper'])]
        def energy(side, upper):
            change=[10*log_q(x)/ln10 for x in side]
            a=list(map(grid,ref))
            b=[grid(x+d) for x,d in zip(ref,change)]
            a=[a[0],*a,a[-1]]
            b=[b[0],*b,b[-1]]
            total=Q(0)
            for j,weight in enumerate(weights):
                r0,r1=a[j:j+2]
                v0,v1=b[j:j+2]
                cuts={Q(0),Q(1)}
                for start,stop in ((r0+60,r1+60),(v0+60,v1+60),(r0-v0,r1-v1)):
                    if start*stop<0:
                        cuts.add(start/(start-stop))
                pts=sorted(cuts)
                def value(t):
                    diff=max(-60,v0+(v1-v0)*t)-max(-60,r0+(r1-r0)*t)
                    return max(0,diff if upper else -diff)
                vals=list(map(value,pts))
                for start,stop,x,y in zip(pts,pts[1:],vals,vals[1:]):
                    total+=weight*(stop-start)*(x*x+x*y+y*y)/3
            return total
        lower=list(map(Q,map(float,l)))
        clipped=list(map(Q,map(float,u)))
        raw=[c+Q(6,1000)*(40/x-2) for c,x in zip(clipped,ws)]
        extra=Q(str(report['sufficient_mean_excess_fraction_if_no_time_bias']))
        residual=Q(str(report['signed_residual_fraction']))
        history=Q(str(report['history_allowance_fraction']))
        excess=Q(str(report['clipping_allowance_fraction']))
        down=energy(lower,False)
        up=energy(clipped,True)
        # Each endpoint error is < 2^-39 dB. Clamp and linear interpolation are
        # nonexpansive. Both one-sided errors together cost < 1e-8 dB in RMS.
        for err,limit in ((down,report['lower_one_sided_rms_upper']),
                (up,report['upper_one_sided_capped_rms_upper']),
                (up+down,report['capped_whole_rms_upper']),
                (energy(raw,True)+down, report['raw_finite_whole_rms_upper']),
                (energy([x-residual for x in lower],False)+energy([x+residual for x in clipped],True),
                 report['whole_rms_if_residual_certified']),
                (energy([x-history for x in lower],False)+energy([x+history+excess for x in clipped],True),
                 report['whole_rms_if_moment_and_history_certified']),
                (energy([x+extra for x in clipped],True)+down,.346784)):
            assert err < (Q(str(limit))-Q(1,10**8))**2
        peak=int(np.argmax(w))
        lo,hi=map(lambda x:Decimal(str(x)),report['raw_peak_bounds_db'])
        assert lo+Decimal('1e-40') < 10*log_q(lower[peak])/ln10
        assert 10*log_q(raw[peak])/ln10 < hi-Decimal('1e-40')


def main(root, confirmation):
    old = root.parent/'survivor-replay-250'
    with np.load(old/'fresh-parents.npz') as z:
        delta = z['delta']
        w = (z['base']+delta.sum(1)).mean(0)[0]
        assert np.all((z['meetings'] > 0) & (z['meetings'] <= delta.shape[1]))
        ages = np.arange(4,delta.shape[1])
        ramp = np.minimum((ages-3)/4,1)
        age_sums = np.cumsum(ramp[:,None]*delta[:,4:,0,:].mean(0),axis=0)
        # Index 2 is the terminal age 7. Subsequent corrections vanish on
        # each archived pair after its recorded time of agreement.
        seen_history = float(np.max(np.abs(age_sums[2:]/w)))
    archive=root/('confirmation.npz' if confirmation else 'verified.npz')
    with np.load(archive) as z:
        assert z['checked_fft']
        observations = z['powers']
        check_analyzer(root,z['knobs'])
        seed=int(z['seed'])
    if not confirmation:
        with np.load(root/'powers.npz') as z:
            np.testing.assert_allclose(observations,z['powers'],rtol=1e-9,atol=1e-25)
    dry = json.loads((old/'dry-power.json').read_text())
    d = np.sqrt(np.array(dry['dry_power_lower'])*dry['dry_power_upper'])
    # Bonferroni, 120 bands x two directions, family failure 1/100.
    threshold = 24000
    ratio = observations.mean(axis=1)/w
    lower, upper = [], []
    for i,(ref, band) in enumerate(zip(w, observations.transpose(2,0,1))):
        width=Q(float(EDGES[i+1]))-Q(float(EDGES[i]))
        rate=6000 if i<60 else 48000
        radius2=Q(1,10**12)*2*width/(rate*1536*Q(float(ref)))
        t=Q(isqrt(int(radius2*2**96))+1,2**48)
        guard=3*t+t*t
        raw=[sum(map(Q,map(float,row)))/(len(row)*Q(float(ref))) for row in band]
        assert max(raw)+guard < 2
        lower.append(float(lower_mean([int(max(0,x-guard)*65536) for x in raw],threshold)))
        upper.append(float(2-lower_mean([int(max(0,2-x-guard)*65536) for x in raw],threshold)))
    l,u = np.array(lower),np.array(upper)
    ref_db = 10*np.log10(w/d)
    def score(high):
        low_rms = one_sided_rms(ref_db,ref_db+10*np.log10(l),False)
        hi_rms = one_sided_rms(ref_db,ref_db+10*np.log10(high),True)
        return low_rms,hi_rms,np.hypot(low_rms,hi_rms)
    lo_score, hi_score, capped_score = score(u)
    # An additive stationary correction can point either way independently
    # in each band. The physical bound on that correction is a separate
    # hypothesis. Check the resulting displayed RMS against the survey budget.
    residual = .048
    residual_score = np.hypot(one_sided_rms(ref_db,ref_db+10*np.log10(l-residual),False),
                              one_sided_rms(ref_db,ref_db+10*np.log10(u+residual),True))
    residual_upper = float(np.ceil(1000*residual_score)/1000)
    # Power lost to upper clipping has a known sign. A 3% power allowance
    # for each of clipping and history widens the lower edge by only 3%.
    history, excess = .03, .03
    split_score = np.hypot(one_sided_rms(ref_db,ref_db+10*np.log10(l-history),False),
                           one_sided_rms(ref_db,ref_db+10*np.log10(u+history+excess),True))
    split_upper = float(np.ceil(1000*split_score)/1000)
    # Solve for one sufficient uniform relative excess budget above the cap.
    a,b = 0.,1.
    for _ in range(36):
        m = (a+b)/2
        if score(u+m)[2] <= .346784:
            a=m
        else:
            b=m
    raw = 10*np.log10(observations.mean(axis=(0,1))/d)
    # A whole-vector cap overflow was absent on all checked draws. Include its
    # full physical payoff, even if every future overflow occupies the same bin.
    assert Q(994,1000)**len(ratio) < Q(1,400)
    raw_upper = u+.006*(40/w-2)
    peak = int(np.argmax(w))
    detector=json.loads((root/'detector-16.json').read_text())
    assert len(detector['rows'])==16 and all(x['code']==0 for x in detector['rows'])
    q=Q(283,1000)
    assert (1-q)**16 < Q(1,200)
    late_dry=40*q**40/(1-q)
    result = dict(histories=len(observations), records_per_block=observations.shape[1], seed=seed,
            family_failure=0.01, tail_cutoff_multiple=2,
            observed_max_multiple=float(ratio.max()), observed_cap_hits=int(np.sum(ratio>2)),
            finite_block_empirical_rms=float(np.sqrt(curve_mse(ref_db[None],raw[None],EDGES)[0])),
            lower_one_sided_rms_upper=float(np.ceil(1000*lo_score)/1000),
            upper_one_sided_capped_rms_upper=float(np.ceil(1000*hi_score)/1000),
            capped_whole_rms_upper=float(np.ceil(1000*capped_score)/1000),
            signed_residual_fraction=residual, whole_rms_if_residual_certified=residual_upper,
            history_allowance_fraction=history, clipping_allowance_fraction=excess,
            whole_rms_if_moment_and_history_certified=split_upper,
            observed_max_band_relative_second_moment=float(np.max(np.mean((ratio-1)**2,axis=0))),
            observed_max_band_and_horizon_signed_history_fraction=seen_history,
            sufficient_mean_excess_fraction_if_no_time_bias=float(np.floor(10000*a)/10000),
            raw_family_failure=.0125, raw_finite_whole_rms_upper=float(np.ceil(1000*score(raw_upper)[2])/1000),
            peak_band_center_hz=float(np.sqrt(EDGES[peak]*EDGES[peak+1])),
            raw_peak_bounds_db=[float(np.floor(1000*10*np.log10(l[peak]))/1000),
                                float(np.ceil(1000*10*np.log10(raw_upper[peak]))/1000)],
            dry_mismatch_after_age_40_power_allowance=float(late_dry),
            dry_mismatch_max_fraction_of_anchor=float(late_dry/min(map(Q,map(float,w)))),
            band_mean_lower_multiple=lower, band_capped_mean_upper_multiple=upper,
            input_sha256=sha256(archive.read_bytes()).hexdigest())
    verify_display(w,dry,l,u,result)
    atomic_json(root/('confirmation-bounds.json' if confirmation else 'mean-bounds.json'),result)
    print({k:v for k,v in result.items() if not isinstance(v,list)})


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('root', type=Path)
    parser.add_argument('--confirmation', action='store_true')
    args=parser.parse_args()
    main(args.root,args.confirmation)
