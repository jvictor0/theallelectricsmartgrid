"""Validate powers and a local resonance maximum at an exact p-cycle, q-sample input."""
import argparse
import json
from pathlib import Path

import numpy as np
from scipy import sparse
from scipy.sparse.linalg import spsolve

from arc_certificate import Arc
from orbit_power import measure_tube
from phase_tube import AMP,Ball
from resonance_chain import Chain
from segmented_inverse import inverse_bounds as segment_inverse
from switch_frequency import ACTIVE,CUTOFF,DRIVE,RATE,SCALE,TARGET,PhaseGrid,Tone,resample


def bounds(ball):
    return [float(ball.lo()),float(ball.hi())]


def refine_cycle(exact,x,r,order,gauge):
    """Use short time-ordered blocks to correct a long exact digital cycle."""
    q = exact.m
    edges = np.r_[np.arange(0,q,32),q]
    count = len(edges)-1
    initial_gauge = None if gauge is None else x[0,gauge]
    for _ in range(5):
        gap,_ = exact.system(x,r)
        if np.max(abs(gap))<2e-14:
            return exact.starts[order].copy(),r
        residual = gap.reshape(q,9)[order]
        matrices = exact.jac[order]
        control = exact.ctrl[order]
        tails = np.empty((count,9,9))
        controls = np.empty((count,9))
        sources = np.empty((count,9))
        for i,(lo,hi) in enumerate(zip(edges,edges[1:])):
            group = np.column_stack((np.eye(9),np.zeros((9,2))))
            for j in range(lo,hi):
                group = matrices[j]@group
                group[:,-2] += control[j]
                group[:,-1] += residual[j]
            tails[i],controls[i],sources[i] = group[:,:9],group[:,-2],group[:,-1]
        idx = (np.arange(count)-1)%count
        size = count*9
        mat = sparse.eye(size,format='csc')-sparse.bsr_matrix((tails[idx],idx,np.arange(count+1)),shape=(size,size)).tocsc()
        rhs = sources[idx].ravel()
        if gauge is not None:
            row = sparse.csc_matrix(([1.],([0],[gauge])),shape=(1,size))
            mat = sparse.bmat([[mat,sparse.csc_matrix(-controls[idx].reshape(-1,1))],[row,None]],format='csc')
            rhs = np.r_[rhs,initial_gauge-x[0,gauge]]
        correction = spsolve(mat,rhs)
        dr = 0. if gauge is None else correction[-1]
        for i,(lo,hi) in enumerate(zip(edges,edges[1:])):
            delta = correction[9*i:9*i+9]
            for j in range(lo,hi):
                x[order[j]] += delta
                delta = matrices[j]@delta + control[j]*dr + residual[j]
        r += dr
    raise ArithmeticError('segmented exact cycle correction failed')


def probe(p,q,anchors,out,reference,check_max):
    if np.gcd(p,q)!=1:
        raise ValueError('tone must be reduced')
    wave = np.load(anchors)
    hz = RATE*p/q
    t = Tone(q,CUTOFF,DRIVE,p)
    t.set_resonance(TARGET)
    k = t.coeffs[1]
    dry = 2.**np.floor(np.log2(t.anchor[8]))
    scale = np.array([1.]*4+[np.sqrt(dry)/4]*4+[dry/8]+[1.]*4+[.5])
    chain = Chain(t,scale)
    arc = Arc(t,np.r_[scale,1.])
    if q>250:
        chain.inverse = arc.inverse = segment_inverse
    gauge = 4 if reference==183 else 3
    grid = PhaseGrid(reference*2,gauge)
    grid.frequency(hz)
    exact = PhaseGrid(q,gauge)
    exact.frequency(hz)
    phase_order = np.arange(q)*p%q
    def closest(label):
        names = [name for name in wave.files if name.endswith('-'+label)]
        name = min(names,key=lambda s:abs(float(s.split('-')[0])-hz))
        return resample(wave[name],grid.phase)[:,ACTIVE]/SCALE
    def key_state(x,r,augmented):
        z,r = refine_cycle(exact,resample(x,exact.phase),r,phase_order,gauge if augmented else None)
        if augmented:
            fb = 3.9*np.expm1(np.log(49)*r)/48
            z = np.column_stack((z,np.full(q,fb)))
        return z
    def certify_power(x):
        z = key_state(x,TARGET,False)
        # Covers the prepared base feedback as well as rounding of the knob law.
        lo,hi = k-1e-12,k+1e-12
        rad = 3e-8
        ok,lip,eta = chain.check(lo,hi,z,z,rad)
        if not ok:
            raise ArithmeticError(f'power enclosure failed: radius {rad}, L {lip}, eta {eta}')
        err = Ball(eta)/(1-Ball(lip))
        report = measure_tube(chain,Ball(z,np.broadcast_to((Ball(scale)*err).magnitude(),z.shape)),lo,hi,harmonics=(3,))
        for observable in ('fundamental','total'):
            # Powers are the interval results. Gains are their presentation in dB.
            report[observable+'_gain_db'] = (10*np.log10(np.array(report[observable+'_power'])/(float(AMP.m)**2/2))).tolist()
        return dict(radius=rad,contraction=lip,defect=eta,**report)
    def finish():
        out.write_text(json.dumps(summary,indent=2,allow_nan=False)+'\n')
    x = closest('first maximum')
    fold = grid.fold((x,TARGET))
    quiet = grid.correct(closest('quiet'),TARGET)[0]
    summary = dict(p=p,q=q,input_hz=hz,fold_estimate=fold[1],powers={})
    for label,solution in [('quiet',quiet),*[(n,v[0]) for n,v in zip(('outgoing','returning'),grid.crossings(fold))]]:
        summary['powers'][label] = certify_power(solution)
        finish()
        print(p,q,label,summary['powers'][label]['total_gain_db'],flush=True)
    if not check_max:
        return
    # Use one state coordinate as a parameter on both sides of the turn. Its
    # value at phase zero is the hyperplane constraint on the augmented cycle.
    xf,rf = fold
    mid_s = xf[0,gauge]
    plane = np.zeros(15)
    plane[ACTIVE[gauge]] = 1.
    center = key_state(xf,rf,True)
    table = {mid_s:center}
    def sample(s):
        if s not in table:
            x,r,_ = grid.correct(xf,rf,s)
            table[s] = key_state(x,r,True)
        return table[s]
    cells = []
    def cover(a,b,depth=0):
        left,right = sample(a),sample(b)
        rad = max(2e-9,(b-a)**2*.6)
        for _ in range(3):
            ok,lip,eta = arc.check_arc(left,right,plane,plane,rad)
            if ok:
                err = float((Ball(eta)/(1-Ball(lip))).hi())
                cells.append(dict(a=a,b=b,left=left[0,-1],right=right[0,-1],radius=rad,defect=eta,contraction=lip,error=err))
                return
            if lip>=.85: break
            rad = max(1.4*rad,1.2*eta/(1-lip))
        if depth==14:
            raise ArithmeticError(f'failed local chart at {a} {b}: {lip} {eta}')
        h = (a+b)/2
        cover(a,h,depth+1)
        cover(h,b,depth+1)
    half = .00065
    cover(mid_s-half,mid_s)
    cover(mid_s,mid_s+half)
    lower = max((Ball(max(c['left'],c['right']))-c['error']).lo() for c in cells)
    upper = max((Ball(max(c['left'],c['right']))+c['error']).hi() for c in cells)
    # Check a tighter *point* enclosure at the three test gauges to distinguish
    # an interior maximum from an endpoint of the chart.
    points = []
    for s in (mid_s-half,mid_s,mid_s+half):
        z = sample(s)
        ok,lip,eta = arc.check_arc(z,z,plane,plane,3e-8)
        if not ok: raise ArithmeticError('failed point gauge')
        err = (Ball(eta)/(1-Ball(lip))).hi()
        points.append(bounds(Ball(z[0,-1],err)))
    assert points[1][0]>max(points[0][1],points[2][1]),points
    summary.update(max_feedback=[max(lower,points[1][0]),upper],target_feedback=k,endpoint_center_feedback=points,cells=cells)
    finish()
    print(p,q,'maximum',summary['max_feedback'],'target',k,flush=True)


if __name__=='__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('phases',type=Path)
    ap.add_argument('output',type=Path)
    ap.add_argument('p',type=int)
    ap.add_argument('q',type=int)
    ap.add_argument('--reference',required=True,type=int,choices=(183,561))
    ap.add_argument('--maximum',action='store_true')
    a = ap.parse_args()
    probe(a.p,a.q,a.phases,a.output,a.reference,a.maximum)
