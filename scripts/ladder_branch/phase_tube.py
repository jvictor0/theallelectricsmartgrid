"""Enclose a forced ladder graph on the phase circle, including detuned inputs."""
import argparse
import gzip
import json
import math
from pathlib import Path

import numpy as np
from scipy.interpolate import CubicSpline
from scipy.ndimage import maximum_filter1d, minimum_filter1d

from pilot import AMPLITUDE, LIB, RATE, Tone
from check_fit import atan_inverse
from enclose import I, UNIT

# Binary64 roundoff padding. Dot products below have at most 14 terms.
EPS = 2.0**-48
TINY = 1.e-290


class Ball:
    __array_priority__ = 1000

    def __init__(self, mid, rad=0.):
        self.m, self.r = np.asarray(mid), np.asarray(rad)

    def __add__(a, b):
        if not isinstance(b, Ball):
            b = Ball(b)
        m = a.m + b.m
        return Ball(m, (a.r+b.r+EPS*abs(m)+TINY)*(1+EPS))

    __radd__ = __add__

    def __neg__(a):
        return Ball(-a.m, a.r)

    def __sub__(a, b):
        return a + (-b)

    def __rsub__(a, b):
        return (-a)+b

    def __mul__(a, b):
        if not isinstance(b, Ball):
            b = Ball(b)
        m = a.m*b.m
        rad = (abs(a.m)+a.r)*b.r + a.r*abs(b.m) + EPS*abs(m)
        return Ball(m, (rad+TINY)*(1+4*EPS))

    __rmul__ = __mul__

    def inv(a):
        d = abs(a.m)-a.r
        if np.any(d <= TINY):
            raise ArithmeticError("denominator interval includes zero")
        m = 1/a.m
        return Ball(m, (abs(m)*a.r/d + EPS*abs(m)+TINY)*(1+4*EPS))

    def __truediv__(a, b):
        return a*(b.inv() if isinstance(b, Ball) else Ball(b).inv())

    def magnitude(a):
        return np.nextafter(abs(a.m)+a.r, np.inf)

    def lo(a):
        return np.nextafter(a.m-a.r, -np.inf)

    def hi(a):
        return np.nextafter(a.m+a.r, np.inf)

    def col(a):
        return Ball(a.m[..., None], a.r[..., None])


def hull(lo, hi):
    m = lo*.5+hi*.5
    return Ball(m, np.nextafter(np.maximum(m-lo,hi-m), np.inf)+TINY)


class Jet:
    def __init__(self, v, d):
        self.v, self.d = v, d

    def __add__(a, b):
        return Jet(a.v+b.v, a.d+b.d)

    def __sub__(a, b):
        return Jet(a.v-b.v, a.d-b.d)

    def times(a, c):
        return Jet(a.v*c, a.d*c.col())

    def __mul__(a, b):
        return Jet(a.v*b.v, a.d*b.v.col()+b.d*a.v.col())

    def __truediv__(a, b):
        inv = b.v.inv()
        v = a.v*inv
        return Jet(v, (a.d-b.d*v.col())*inv.col())


def constant(c):
    return Jet(c, Ball(0.))


def saturate(state, gain, norm):
    # The rational saturator is monotone. Its derivative decreases with |x| up to 3.
    x = state.v*gain
    lo, hi = x.lo(), x.hi()
    def value(t):
        t = Ball(np.clip(t, -3., 3.))
        xx = t*t
        return t*(27.+xx)/(27.+9.*xx)
    def slope(t):
        t = Ball(np.clip(t, 0., 3.))
        xx = t*t
        d = 3.*(9.-xx)/(27.+9.*xx)
        return d*d
    der = hull(slope(np.maximum(abs(lo),abs(hi))).lo(),
               slope(np.maximum(0.,np.maximum(lo,-hi))).hi())*gain/norm
    return Jet(hull(value(lo).lo(),value(hi).hi())/norm, state.d*der.col())


def advance(z, u, c, dk=None, dref=None):
    alpha, base, gain, norm, a1, a2, a3, ema, ref, damping, floor = c
    ex = z[13] - (z[8].times(ref) if dref is None else z[8]*Jet(ref,dref))
    lo, hi = ex.v.lo(), ex.v.hi()
    slope = hull((lo > 0).astype(float), (hi >= 0).astype(float))
    positive = Jet(hull(np.maximum(0.,lo), np.maximum(0.,hi)), ex.d*slope.col())
    command = constant(base) if dk is None else Jet(base,dk)
    k = command/(constant(Ball(1.))+positive.times(damping)/(z[8]+constant(floor)))
    stage = u-z[3]*k
    z = list(z)
    for i in range(4):
        z[i] = z[i].times(1.-alpha)+stage.times(alpha)
        if i < 3:
            stage = saturate(z[i], gain, norm)
    def band(x, i):
        ic1, ic2 = z[i:i+2]
        delta = x-ic2
        bp = ic1.times(a1)+delta.times(a2)
        z[i] = ic1.times(2.*a1-1.)+delta.times(2.*a2)
        z[i+1] = ic2.times(1.-2.*a3)+ic1.times(2.*a2)+x.times(2.*a3)
        return bp
    for i, v in [(4,u),(9,z[3])]:
        bp = band(band(v,i),i+2)
        z[i+4] = z[i+4].times(1.-ema)+(bp*bp).times(ema)
    out = saturate(z[3],gain,norm)*(constant(Ball(1.))+k)
    return z, out.v, k.v, ex.v


def stack(values, axis=1):
    shape = np.broadcast_shapes(*(x.shape for v in values for x in (v.m,v.r)))
    return Ball(np.stack([np.broadcast_to(v.m,shape) for v in values],axis),
                np.stack([np.broadcast_to(v.r,shape) for v in values],axis))


def graph(tone, seed, subdivisions):
    # Intermediate phase solves are proposals. The verifier checks the interpolant directly.
    pieces = []
    for offset in range(subdivisions):
        theta = (np.arange(tone.q)+offset/subdivisions)/tone.q
        u = np.ascontiguousarray(AMPLITUDE*np.cos(2*np.pi*theta))
        z = seed.copy()
        y = np.empty(tone.q)
        LIB.ReturnMap(*tone.params,u,tone.q,220,z,y)
        rows = []
        for n in range(tone.q):
            rows.append(z.copy())
            LIB.ReturnMap(*tone.params,u[n:n+1],1,1,z,y[:1])
        if max(abs(rows[0]-z))>1e-9 or (offset==0 and max(abs(z-seed))>1e-6):
            raise ArithmeticError("phase proposal left the selected orbit")
        pieces.append(rows)
    return np.array(pieces).transpose(1,0,2).reshape(-1,14)


def trig(turns):
    # Reduce to [-1/2,1/2] before calling. The interval around pi is from Machin's formula.
    pi = trig.pi
    angle = turns*pi*2.
    # Degree 44 Taylor polynomials. On |x|<3.15 the remainder is below 1e-32.
    if np.max(angle.magnitude()) > 3.15:
        raise ArithmeticError("trig reduction failed")
    xx = -angle*angle
    cos, sin = Ball(1.), Ball(1.)
    for j in range(22,0,-1):
        cos = 1.+xx*cos/((2*j-1)*(2*j))
        sin = 1.+xx*sin/((2*j)*(2*j+1))
    return cos+Ball(0.,1e-32), angle*sin+Ball(0.,1e-32)


pi_exact = I.point(16)*atan_inverse(5)-I.point(4)*atan_inverse(239)
trig.pi = hull(np.nextafter(pi_exact.l/UNIT,-np.inf),np.nextafter(pi_exact.h/UNIT,np.inf))
amp_lo, amp_hi = float(np.nextafter(AMPLITUDE,-np.inf)), float(np.nextafter(AMPLITUDE,np.inf))
from fractions import Fraction
if not (10**9 * Fraction(amp_lo)**10 < 1 < 10**9 * Fraction(amp_hi)**10):
    raise ArithmeticError("exact amplitude outside proposal")
AMP = hull(amp_lo,amp_hi)


def mv(a,x):
    return np.einsum("nij,nj->ni",a,x)


def positive_mv(a,x):
    return (mv(a,x)+TINY)*(1.+32*EPS)


def product(a,b):
    m = a.m @ b.m
    r = (abs(a.m)+a.r) @ b.r + a.r @ abs(b.m)
    roundoff = (abs(a.m) @ abs(b.m))*(32*EPS)
    return Ball(m,(r+roundoff+TINY)*(1.+64*EPS))


def frequency_tangent(tone, knots):
    # Differentiate X(theta + f/Fs) = F(X(theta), u(theta)) at the seed.
    # This is a numerical proposal. Its equation defect is checked over the entire cell.
    size = len(knots)
    stride = size//tone.q
    spline = CubicSpline(np.arange(size+1)/size,np.vstack((knots,knots[0])),bc_type='periodic')
    phi = np.arange(size)/size
    eye = np.eye(14)
    state = [Jet(Ball(knots[:,j]),Ball(eye[j])) for j in range(14)]
    states,_,_,_ = advance(state,constant(Ball(AMPLITUDE*np.cos(2*np.pi*phi))),
                           [Ball(v) for v in tone.coeffs])
    jac = stack([x.d for x in states]).m.reshape(tone.q,stride,14,14)
    forcing = -spline((phi+1/tone.q)%1,1).reshape(tone.q,stride,14)/RATE
    mat = np.broadcast_to(eye,(stride,14,14))
    end = np.zeros((stride,14))
    for a,b in zip(jac,forcing,strict=True):
        end = mv(a,end)+b
        mat = a@mat
    v = np.linalg.solve(eye-mat,end[:,:,None])[:,:,0]
    rows = []
    for a,b in zip(jac,forcing,strict=True):
        rows.append(v)
        v = mv(a,v)+b
    return np.asarray(rows).reshape(size,14)


def proof_chunk(table, tangent, q, offsets, c, radius, scale, freq_radius, drift):
    m = len(table)
    stride = m//q
    idx = (np.arange(q)[None,:]*stride + offsets[:,None]).ravel()
    shift = lambda x,n: np.roll(x.reshape(len(offsets),q,*x.shape[1:]),-n,axis=1).reshape(x.shape)
    slip = float((Ball(freq_radius)/RATE*m).hi())
    if slip >= 1:
        raise ArithmeticError("one sample of detuning spans multiple phase bins")
    left, right = table[idx],table[(idx+1)%m]
    vl, vr = tangent[idx], tangent[(idx+1)%m]
    vseg = hull(np.minimum(vl,vr),np.maximum(vl,vr))
    segment = hull(np.minimum(left,right),np.maximum(left,right))
    delta_f = Ball(0.,freq_radius)
    moving = segment+vseg*delta_f
    tube = moving+Ball(0.,(Ball(radius)*Ball(scale)).magnitude())
    if drift:
        def extra(graph):
            slopes=[(Ball(graph[(idx+j+1)%m])-Ball(graph[(idx+j)%m])).magnitude() for j in (-1,1)]
            return np.maximum(*slopes)
        expansion=(Ball(extra(table))+Ball(extra(tangent))*freq_radius)*drift
        tube=tube+Ball(0.,expansion.magnitude())
    turns = Ball(np.where(idx <= m//2,idx,idx-m))/m
    cos, sin = trig(turns)
    step_angle = trig.pi*2./m
    phase_pad = (step_angle*(Ball(1.)+drift)).hi()
    u_all = AMP*(cos+Ball(0.,phase_pad))
    eye = np.eye(15)
    def jets(state):
        return [Jet(Ball(state.m[:,j],state.r[:,j]), Ball(eye[j])) for j in range(14)]
    z, y, kk, excess = advance(jets(tube), Jet(u_all,Ball(eye[14])), c)
    D = stack([v.d for v in z])
    # Obtain the defect on the entire piecewise-linear chart with a centered
    # derivative bound; adjacent affine pieces coincide under the base rotation.
    edge,_,_,_ = advance(jets(Ball(left,np.zeros_like(left))),Jet(AMP*cos,Ball(eye[14])),c)
    edge_gap = (stack([v.v for v in edge])-Ball(shift(left,1))).magnitude()
    slope = Ball(right)-Ball(left)
    du = -AMP*step_angle*(sin+Ball(0.,step_angle.hi()))
    segment = hull(np.minimum(left,right),np.maximum(left,right))
    directed = [Jet(Ball(segment.m[:,j],segment.r[:,j]), Ball(slope.m[:,j,None],slope.r[:,j,None])) for j in range(14)]
    direction,_,_,_ = advance(directed,Jet(u_all,du.col()),c)
    velocity = stack([v.d for v in direction])
    vel = Ball(velocity.m[:,:,0],velocity.r[:,:,0])-(Ball(shift(right,1))-Ball(shift(left,1)))
    # Frequency derivative of the residual. At a spline corner enclose both
    # one-sided slopes; a Lipschitz bound along the frequency interval still follows.
    cols = lambda b,j: Ball(b.m[:,j,None],b.r[:,j,None])
    velocity_state = [Jet(Ball(moving.m[:,j],moving.r[:,j]),cols(vseg,j)) for j in range(14)]
    freq_deriv,_,_,_ = advance(velocity_state,constant(u_all),c)
    delta = stack([v.d for v in freq_deriv])
    def next_slopes(graph):
        ds = [Ball(graph[(idx+stride+j+1)%m])-Ball(graph[(idx+stride+j)%m]) for j in (-1,0,1)]
        return hull(np.minimum.reduce([x.lo() for x in ds]),np.maximum.reduce([x.hi() for x in ds]))
    next_v = tangent[(idx[:,None]+stride+np.arange(-1,3)[None,:])%m]
    offset = (next_slopes(table)+delta_f*next_slopes(tangent))*m/RATE
    freq_gap = (Ball(delta.m[:,:,0],delta.r[:,:,0])-offset-
                hull(next_v.min(axis=1),next_v.max(axis=1))).magnitude()
    defect = (edge_gap+vel.magnitude()+freq_gap*freq_radius+TINY)*(1.+16*EPS)
    observables = [y*y,2.*y*(cos+Ball(0.,phase_pad)),-2.*y*(sin+Ball(0.,phase_pad))]
    # Work in weighted infinity norm; all floating diagonal scales here are dyadic.
    J = Ball(D.m[:,:,:14],D.r[:,:,:14])
    ratio = Ball(scale[None,:])/Ball(scale[:,None])
    J = J*ratio
    return J, (defect/scale+TINY)*(1+EPS), observables, [float(kk.lo().min()),float(kk.hi().max())], float(excess.lo().min())


def phase_window(x,width,maximum):
    fun = maximum_filter1d if maximum else minimum_filter1d
    return fun(x,size=2*width+1,axis=0,mode='wrap') if width else x


def compose_grid(jac, defect, q, levels, freq_radius):
    # After each half-block, recenter at every successor phase bin reachable
    # for this frequency interval. This keeps drift from accumulating in early products.
    m = len(defect)
    for k in range(levels):
        length = 2**k
        shift = length*(m//q)
        width = math.ceil(float((Ball(freq_radius)*length*m/RATE).hi())) if freq_radius else 0
        lower = phase_window(jac.lo(),width,False)
        upper = phase_window(jac.hi(),width,True)
        next_m = np.empty_like(jac.m)
        next_r = np.empty_like(jac.r)
        next_d = np.empty_like(defect)
        dmax = phase_window(defect,width,True)
        for start in range(0,m,1024):
            ids = np.arange(start,min(m,start+1024))
            second_ids = (ids+shift)%m
            second = hull(lower[second_ids],upper[second_ids])
            prod = product(second,Ball(jac.m[ids],jac.r[ids]))
            next_m[ids],next_r[ids] = prod.m,prod.r
            next_d[ids] = (dmax[second_ids]+positive_mv(second.magnitude(),defect[ids])+TINY)*(1+EPS)
        jac,defect = Ball(next_m,next_r),next_d
        if not np.isfinite(jac.m).all() or np.max(jac.magnitude())>1e9:
            raise ArithmeticError(f"matrix wrapping at block {length*2}")
    return float(np.max(jac.magnitude().sum(axis=2))*(1+32*EPS)),float(defect.max())


def check(table, tangent, coeff, q, radius, scale, freq_radius, k_radius, levels, chunk):
    m = len(table)
    stride = m//q
    if m%q or radius<=0 or freq_radius<0 or k_radius<0 or 2**levels<q:
        raise ValueError("invalid tube configuration")
    c = [Ball(v,2**-48) for v in coeff]
    # R(K)=(Kcrit-K)^-2. If R at the center is below .9 and |delta K|<.01,
    # R stays below one and |R'|=2 R^(3/2) stays below three.
    if coeff[8] >= .9 or k_radius >= .01:
        raise ValueError("reference sensitivity range exceeded")
    c[8] += Ball(0.,3*k_radius*(1+EPS))
    c[1] += Ball(0.,k_radius)
    slip = (Ball(freq_radius)*(2**levels)*m/RATE).hi()
    drift = float(slip) if 0 < slip <= 1 else 0.
    data = np.empty((m,14))
    jac = Ball(np.empty((m,14,14)),np.empty((m,14,14)))
    outputs = np.empty((m,3,2))
    kvals,knees = [],[]
    for start in range(0,stride,chunk):
        offsets = np.arange(start,min(stride,start+chunk))
        jj,defect,ob,kr,knee = proof_chunk(table,tangent,q,offsets,c,radius,scale,freq_radius,drift)
        ix = (np.arange(q)[None,:]*stride+offsets[:,None]).ravel()
        data[ix],jac.m[ix],jac.r[ix] = defect,jj.m,jj.r
        for j,b in enumerate(ob):
            outputs[ix,j,0],outputs[ix,j,1] = b.lo(),b.hi()
        kvals.append(kr)
        knees.append(knee)
    if drift:
        data=phase_window(data,1,True)
    contraction,forcing = compose_grid(jac,data,q,levels,0. if drift else freq_radius)
    # Each q-block samples each nominal phase on the orbit once. Include the
    # phase slip within that block and bound any partial trailing block separately.
    width = math.ceil(float((Ball(freq_radius)*q*m/RATE).hi())) if freq_radius else 0
    low = phase_window(outputs[:,:,0],width,False).reshape(q,stride,3).mean(axis=0)
    high = phase_window(outputs[:,:,1],width,True).reshape(q,stride,3).mean(axis=0)
    guard = ((np.max(abs(outputs),axis=(0,2))+TINY)*(q+2)*EPS)*(1+EPS)
    obs = np.stack([low.min(axis=0)-guard,high.max(axis=0)+guard],axis=1)
    accepted = np.isfinite([contraction,forcing]).all() and (Ball(contraction)*radius+forcing).hi()<radius
    return dict(accepted=bool(accepted), phase_drift_in_bins=drift, radius=radius, freq_radius_hz=freq_radius, base_feedback_radius=k_radius,
                contraction=contraction, accumulated_defect=forcing,
                total_power=obs[0].tolist(), projection_real=obs[1].tolist(),
                projection_imag=obs[2].tolist(), effective_feedback=[np.min(kvals),np.max(kvals)],
                minimum_excess=min(knees), phase_segments=m, block_samples=2**levels)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('sweep',type=Path)
    ap.add_argument('out',type=Path)
    ap.add_argument('--q',type=int,default=180)
    ap.add_argument('--subdivisions',type=int,default=64)
    ap.add_argument('--radius',type=float,default=.0002)
    ap.add_argument('--frequency-radius',type=float,default=.00001)
    ap.add_argument('--feedback-radius',type=float,default=1e-8)
    ap.add_argument('--levels',type=int,default=11)
    ap.add_argument('--chunk',type=int,default=8)
    args=ap.parse_args()
    with gzip.open(args.sweep) as stream:
        row=next(r for r in map(json.loads,stream) if r['kind']=='target' and r['q']==args.q and r['spacing']==.01 and r['r']>.8)
    t=Tone(args.q,row['cutoff_hz'],row['drive'])
    t.set_resonance(row['r'])
    knots=graph(t,np.array(row['state']),8)
    spline=CubicSpline(np.arange(len(knots)+1)/len(knots),np.vstack((knots,knots[0])),bc_type='periodic')
    table=spline(np.arange(t.q*args.subdivisions)/(t.q*args.subdivisions))
    # The dry detector drives the wet loop unidirectionally. Scale its power
    # coordinate to the measured dry level before taking matrix norms.
    dry_power = 2.**math.floor(math.log2(np.max(abs(table[:,8]))))
    scale=np.array([1.]*4+[math.sqrt(dry_power)/4]*4+[dry_power/8]+[1.]*4+[.5])
    tangent_knots=frequency_tangent(t,knots)
    tangent=CubicSpline(np.arange(len(knots)+1)/len(knots),np.vstack((tangent_knots,tangent_knots[0])),bc_type='periodic')(np.arange(len(table))/len(table))
    res=check(table,tangent,t.coeffs,t.q,args.radius,scale,args.frequency_radius,args.feedback_radius,args.levels,args.chunk)
    res.update(q=t.q, frequency_hz=RATE/t.q, cutoff_hz=t.cutoff, drive=t.drive, resonance=row['r'], base_feedback=float(t.coeffs[1]))
    print(res,flush=True)
    args.out.write_text(json.dumps(res,indent=2,allow_nan=False))
    if res['accepted']:
        np.savez_compressed(args.out.with_suffix('.npz'),table=table,tangent=tangent,scale=scale,coefficients=t.coeffs,q=t.q)


if __name__=='__main__':
    main()
