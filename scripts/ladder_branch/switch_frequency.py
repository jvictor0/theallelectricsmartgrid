"""Track the first resonance maximum and the coexisting forced responses on a common input phase."""
import argparse
import csv
import json
from pathlib import Path

import numpy as np
from scipy.interpolate import CubicSpline
from scipy.optimize import brentq
from scipy import sparse
from scipy.sparse.linalg import splu

from multiple_shooting import LIB
from pilot import ACTIVE, AMPLITUDE, RATE, Tone

TARGET = 81/99
CUTOFF = 20*1000**(56/99)
DRIVE = 90/99
SCALE = np.array([1.]*8+[.5])


def one_cycle(t, row):
    starts = row.get('block_states', [row['state']])
    edges = row.get('edges', [0,t.q])
    buf = np.empty(1)
    wave = np.empty((t.q,14))
    for seed,lo,hi in zip(starts,edges,edges[1:]):
        z = np.array(seed)
        for j in range(lo,hi):
            wave[j] = z
            LIB.ReturnMap(*t.params,t.input[j:j+1],1,1,z,buf)
    return wave


def resample(wave, phase):
    return CubicSpline(np.linspace(0.,1.,len(wave)+1),np.vstack((wave,wave[:1])),bc_type='periodic')(phase)


class PhaseGrid:
    def __init__(self,m,gauge):
        self.m = m
        self.phase = np.arange(m)/m
        self.osc = np.exp(2j*np.pi*self.phase)
        self.input = np.ascontiguousarray(AMPLITUDE*self.osc.real)
        self.edges = np.arange(m+1,dtype=np.int32)
        self.starts = np.zeros((m,14))
        self.ends = self.starts.copy()
        self.jac = np.empty((m,9,9))
        self.ctrl = np.empty((m,9))
        self.y = np.empty(m)
        self.t = Tone(182,CUTOFF,DRIVE)
        self.gauge = gauge
        self.gauge_row = sparse.csr_matrix(([1.],([0],[gauge])),shape=(1,9*m))

    def frequency(self,hz):
        # Dry detector is a cascade of two identical linear bandpasses, followed
        # by an exponential average of their squared sinusoidal output.
        z = np.exp(2j*np.pi*hz/RATE)
        a1,a2,a3,ema = self.t.coeffs[4:8]
        mat = np.array([[2*a1-1,-2*a2],[2*a2,1-2*a3]])
        state = np.linalg.solve(z*np.eye(2)-mat,[2*a2,2*a3])
        bp = a2 + np.dot([a1,-a2],state)
        self.starts[:,4:8] = (AMPLITUDE*self.osc[:,None]*np.r_[state,bp*state]).real
        self.starts[:,8] = AMPLITUDE**2/2*(abs(bp)**4+(self.osc**2*bp**4*ema/(z*z-1+ema)).real)
        # Local quintic interpolation for an arbitrary phase advance. At a
        # commensurate frequency an exact permutation replaces interpolation.
        shift = self.m*hz/RATE
        if abs(shift-round(shift)) < 2e-12:
            offsets,weights = [round(shift)],[1.]
        else:
            offsets = np.arange(-2,4)+int(np.floor(shift))
            weights = [np.prod((shift-offsets[offsets!=j])/(j-offsets[offsets!=j])) for j in offsets]
        i = np.arange(self.m)
        move = sum(w*sparse.csr_matrix((np.ones(self.m),(i,(i+j)%self.m)),shape=(self.m,self.m))
                   for j,w in zip(offsets,weights))
        self.move = move
        self.shift = sparse.kron(move,sparse.eye(9),format='csc')

    def system(self,x,r):
        if not -.01<r<1.04:
            raise ArithmeticError(f'resonance proposal outside search range: {r:g}')
        self.starts[:,ACTIVE] = x*SCALE
        knobs = np.expm1(np.log(49)*np.array([r,r+1e-6,r-1e-6]))/48
        LIB.ShootingBlocks(self.t.cutoff/RATE,knobs,self.t.params[2],self.input,self.edges,self.m,
                           self.starts,SCALE,self.ends,self.jac,self.ctrl,self.y)
        residual = self.ends[:,ACTIVE]/SCALE - self.move@x
        j = sparse.bsr_matrix((self.jac,np.arange(self.m),self.edges),shape=self.shift.shape).tocsc()
        j.eliminate_zeros()
        return residual.ravel(),j-self.shift

    def correct(self,x,r,s=None):
        x = x.copy()
        for _ in range(16):
            residual,j = self.system(x,r)
            if s is not None:
                j = sparse.bmat([[j,sparse.csc_matrix(self.ctrl.reshape(-1,1))],[self.gauge_row,None]],format='csc')
                residual = np.r_[residual,x[0,self.gauge]-s]
            err = max(abs(residual))
            lu = splu(j)
            if err < 2e-13:
                slope = None
                if s is not None:
                    unit = np.zeros(len(residual))
                    unit[-1] = 1.
                    slope = lu.solve(unit)[-1]
                return x,r,slope
            delta = lu.solve(residual)
            if s is not None:
                r -= delta[-1]
                delta = delta[:-1]
            x -= delta.reshape(self.m,9)
        raise ArithmeticError(f'Newton residual {err:g}')

    def fold(self,seed):
        x,r = seed
        s = x[0,self.gauge]
        for _ in range(10):
            x,r,slope = self.correct(x,r,s)
            if abs(slope)<3e-8:
                return x,r
            h = 1e-4
            _,_,next_slope = self.correct(x,r,s+h)
            curvature = (next_slope-slope)/h
            if curvature>=0:
                raise ArithmeticError(f'expected a maximum; curvature {curvature}')
            s -= np.clip(slope/curvature,-.025,.025)
        raise ArithmeticError('fold refinement failed')

    def statistics(self,x,r):
        gap,_ = self.system(x,r)
        v = 2*np.mean(self.y*self.osc.conj())
        return dict(fund_db=20*np.log10(abs(v)/AMPLITUDE),total_db=10*np.log10(2*np.mean(self.y**2)/AMPLITUDE**2),
                    phase_deg=np.angle(v,deg=True),residual=max(abs(gap)))

    def crossings(self,fold):
        x,r = fold
        if r<=TARGET:
            return []
        s0 = x[0,self.gauge]
        out = []
        for direction in [-1,1]:
            visited = {0.:(x,r,None)}
            def chart(offset):
                if offset not in visited:
                    near = min(visited,key=lambda u:abs(u-offset))
                    xx,rr,_ = visited[near]
                    visited[offset] = self.correct(xx,rr,s0+direction*offset)
                return visited[offset]
            lo,step = 0.,.004
            while chart(step)[1]>TARGET:
                lo,step = step,step+min(.025,step*.6)
                if step>.8:
                    raise ArithmeticError('target outside local gauge chart')
            offset = brentq(lambda v:chart(v)[1]-TARGET,lo,step,xtol=1e-12)
            y,k,_ = chart(offset)
            out.append((y,k))
        return out


def read_seeds(q,previous,output):
    data = next(row for row in map(json.loads,(output/'first-turn-seeds.jsonl').open()) if row['q']==q)
    path = data['path']
    i = next(i for i,p in enumerate(path) if p['tangent_r']<0)
    idx = int(np.argmax(abs((np.array(path[i+2]['state'])-path[i-2]['state'])[ACTIVE]/SCALE)))
    t = Tone(q,CUTOFF,DRIVE)
    r = path[i]['r']
    t.set_resonance(r)
    fold = one_cycle(t,path[i])
    name = 'edge-arcs.jsonl' if q==183 else 'segments.jsonl'
    quiet = next(row for row in map(json.loads,(previous/name).open()) if row['q']==q and row['step']==.02)
    t.set_resonance(TARGET)
    quiet = one_cycle(t,quiet.get('target',quiet))
    return idx,fold,r,quiet


def run(q,m,previous,output):
    gauge,w,r,quiet = read_seeds(q,previous,output)
    grid = PhaseGrid(m,gauge)
    seed = resample(w,grid.phase)[:,ACTIVE]/SCALE,r
    quiet = resample(quiet,grid.phase)[:,ACTIVE]/SCALE
    start,end = RATE/q,RATE/(182 if q==183 else 562)
    seen = {}
    def fold_at(hz):
        if hz not in seen:
            grid.frequency(hz)
            near = seen[min(seen,key=lambda x:abs(x-hz))] if seen else seed
            seen[hz] = grid.fold(near)
        return seen[hz]
    for hz in np.linspace(start,end,7):
        fold_at(hz)
    switch = brentq(lambda f:fold_at(f)[1]-TARGET,min(start,end),max(start,end),xtol=1e-9)
    records = []
    orbit_samples = {}
    for hz in sorted({*np.linspace(start,end,11),switch},reverse=start>end):
        fold = fold_at(hz)
        grid.frequency(hz)
        quiet,_,_ = grid.correct(quiet,TARGET)
        roots = [('quiet',quiet,TARGET),('first maximum',*fold)]
        if abs(hz-switch)>1e-8:
            roots += [(label,*root) for label,root in zip(('outgoing','returning'),grid.crossings(fold))]
        for label,x,k in roots:
            info = grid.statistics(x,k)
            records.append(dict(frequency_hz=hz,branch=label,knob=k,m=m,switch_hz=switch,**info))
            if abs(hz-switch)<1e-8 or hz in (start,end):
                orbit_samples[f'{hz:.9f}-{label}'] = grid.starts.copy()
    stem = output/f'phase-{q}-{m}'
    with stem.with_suffix('.csv').open('w',newline='') as f:
        writer = csv.DictWriter(f,fieldnames=records[0])
        writer.writeheader()
        writer.writerows(records)
    np.savez_compressed(stem.with_suffix('.npz'),**orbit_samples)
    print(q,m,switch,[(row['branch'],row['fund_db'],row['total_db']) for row in records if row['frequency_hz']==switch],flush=True)


if __name__=='__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('previous',type=Path)
    ap.add_argument('output',type=Path)
    ap.add_argument('--q',type=int,choices=(183,561),required=True)
    ap.add_argument('--nodes',type=int,required=True)
    a = ap.parse_args()
    run(a.q,a.nodes,a.previous,a.output)
