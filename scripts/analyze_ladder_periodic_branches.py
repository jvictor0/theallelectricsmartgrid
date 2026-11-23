#!/usr/bin/env python3
"""Numerical rational-ladder return-map roots, stability, and continuation.

This uses binary64 smooth-map derivatives with the GPU's rounded coefficients
and forcing. It is not an interval existence certificate or a derivative of
binary32 rounding. The native derivative is checked by central differences.
"""
import argparse
import ctypes
import csv
import json
from pathlib import Path
import numpy as np

ROOT=Path(__file__).resolve().parents[1]


class PeriodMap:
    def __init__(self,model):
        self.p=np.array(model['coefficients'],dtype=np.float64)
        self.input=np.array(model['input'],dtype=np.float64)
        self.quadrature=np.array(model['quadrature'],dtype=np.float64)
        self.scale=np.array([.25+self.p[2]/self.p[4]]+[1/self.p[4]]*3)
        self.lib=ctypes.CDLL(str(ROOT/'build/tools/ladder-rational-period/libladder_rational_period.so'))
        ptr=ctypes.POINTER(ctypes.c_double)
        self.fun=self.lib.LadderRationalCycle
        self.fun.argtypes=[ptr,ptr,ctypes.c_int,ptr,ptr,ptr];self.fun.restype=None

    def cycle(self,x,p=None):
        p=self.p if p is None else p
        state=np.array(x,dtype=np.float64);jac=np.empty((4,4));out=np.empty(len(self.input))
        ptr=ctypes.POINTER(ctypes.c_double)
        self.fun(p.ctypes.data_as(ptr),self.input.ctypes.data_as(ptr),len(self.input),
                 state.ctypes.data_as(ptr),jac.ctypes.data_as(ptr),out.ctypes.data_as(ptr))
        return state,jac,out

    def fit(self,x,p=None,iterations=100):
        x=np.array(x,dtype=float)
        for it in range(iterations):
            final,jac,_=self.cycle(x,p);res=(final-x)/self.scale
            err=float(np.linalg.norm(res))
            if err<1e-11:return x,err,it
            j=(jac-np.eye(4))*self.scale[None,:]/self.scale[:,None]
            try: step=np.linalg.solve(j,-res)
            except np.linalg.LinAlgError:step=np.linalg.lstsq(j,-res,rcond=None)[0]
            norm=np.linalg.norm(step)
            if not np.isfinite(norm):break
            if norm>2:step*=2/norm
            moved=False
            for fraction in 2.**(-np.arange(12)):
                y=x+fraction*step*self.scale
                f,_,_=self.cycle(y,p)
                if np.linalg.norm((f-y)/self.scale)<err*(1-1e-4*fraction):
                    x=y;moved=True;break
            if not moved:
                for damping in [1e-4,.01,1,100]:
                    step=np.linalg.solve(j.T@j+damping*max(1,np.linalg.norm(j)**2)*np.eye(4),-j.T@res)
                    y=x+step*self.scale;f,_,_=self.cycle(y,p)
                    if np.linalg.norm((f-y)/self.scale)<err:
                        x=y;moved=True;break
            if not moved:break
        final,_,_=self.cycle(x,p)
        return x,float(np.linalg.norm((final-x)/self.scale)),it+1

    def diagnose(self,x):
        final,jac,out=self.cycle(x)
        finite=np.empty((4,4));h=1e-7
        for i in range(4):
            delta=np.eye(4)[i]*h
            finite[:,i]=(self.cycle(x+delta)[0]-self.cycle(x-delta)[0])/(2*h)
        derivative_error=float(np.max(abs(finite-jac)))
        assert derivative_error<1e-4*max(1,np.max(abs(jac))),derivative_error
        eig=np.linalg.eigvals(jac)
        real=8*np.mean(out*self.input);imag=-8*np.mean(out*self.quadrature)
        return {'state':x.tolist(),'residual':float(np.max(abs(final-x))),
                'multipliers':[[float(z.real),float(z.imag)] for z in eig],
                'spectral_radius':float(max(abs(eig))),
                'jacobian_finite_difference_error':derivative_error,
                'power':float(np.mean(out*out)),
                'fundamental':float((real*real+imag*imag)/2),'real':float(real),'imaginary':float(imag)}


def analyze(path):
    model=json.loads(path.read_text());point=model['point_id'];m=PeriodMap(model)
    rows=[r for r in csv.DictReader(path.with_name(f'{point}-direct.csv').open()) if r['half']=='1']
    baselines={r['family']:r for r in rows if r['family'] in ['saved_low','saved_high','reset']}
    initial=lambda r:np.array([float(r['initial_'+c]) for c in 'abcd'])
    seeds=[np.zeros(4),initial(baselines['saved_high']),initial(baselines['saved_low'])]
    rng=np.random.default_rng(20261121+point)
    seeds.extend(rng.uniform(-1,1,(512,4))*m.scale)
    roots=[];found=[];failures=0
    for x in seeds:
        solved,error,iterations=m.fit(x)
        if error>1e-9:
            failures+=1;continue
        index=next((i for i,r in enumerate(roots) if np.max(abs((r-solved)/m.scale))<1e-6),None)
        if index is None:index=len(roots);roots.append(solved)
        found.append(index)
    details=[m.diagnose(x) for x in roots]
    for i,d in enumerate(details):d['newton_seed_count']=found.count(i)
    paths={}
    for route in ['feedback','drive','both']:
        x=np.zeros(4);records=[];failed=False
        target_drive=(point//10000%100)/99
        for t in np.linspace(0,1,129):
            p=m.p.copy()
            if route in ['drive','both']:
                p[3]=.25*20**(target_drive*t)
                p[4]=np.clip(p[3]*(p[3]**2+27)/(9*p[3]**2+27),-1,1)
            if route in ['feedback','both']:p[2]=m.p[2]*t
            x,err,it=m.fit(x,p)
            if err>1e-9:
                failed=True;records.append({'fraction':float(t),'error':err});break
            f,j,_=m.cycle(x,p)
            records.append({'fraction':float(t),'state':x.tolist(),'radius':float(max(abs(np.linalg.eigvals(j)))),'error':err})
        paths[route]={'failed':failed,'steps':records,'final':None if failed else m.diagnose(x)}
    # Independent scalar formulation checks the native recurrence, including output.
    state=initial(baselines['saved_high']);other=state.copy();p=m.p
    outputs=[]
    rat=lambda z:np.clip(z*(z*z+27)/(9*z*z+27),-1,1)
    for value in m.input:
        other[0]=p[1]*other[0]+p[0]*(value-p[2]*other[3])
        for i in range(1,4):other[i]=p[1]*other[i]+p[0]*rat(p[3]*other[i-1])/p[4]
        outputs.append((1+p[2])*rat(p[3]*other[3])/p[4])
    final,j,out=m.cycle(state)
    reference_error=max(float(np.max(abs(final-other))),float(np.max(abs(out-outputs))))
    assert reference_error<1e-10,reference_error
    result={'point_id':point,'seed_count':len(seeds),'failed_seeds':failures,'period_samples':len(m.input),
            'roots':details,'continuations':paths,'reference_error':reference_error}
    path.with_name(f'{point}-periodic.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({'point':point,'seeds':len(seeds),'failed':failures,
          'roots':[{k:r[k] for k in ['spectral_radius','fundamental','power','newton_seed_count','residual']} for r in details],
          'paths':{k:{'failed':p['failed'],'final_rho':p['final']['spectral_radius'] if p['final'] else None,
                      'final_fund':p['final']['fundamental'] if p['final'] else None,
                      'max_rho':max(s.get('radius',0) for s in p['steps'])} for k,p in paths.items()}}),flush=True)


def analyze_multiple(root,point,multiple):
    model=json.loads((root/f'{point}-model.json').read_text())
    qmap=PeriodMap(model);long=PeriodMap(model)
    long.input=np.tile(long.input,multiple);long.quadrature=np.tile(long.quadrature,multiple)
    rows=[r for r in csv.DictReader((root/f'{point}-direct.csv').open()) if r['half']=='1']
    seeds=[np.array([float(r['initial_'+c]) for c in 'abcd']) for r in rows if r['family'] in ['saved_low','saved_high']]
    x=seeds[0].copy()
    for _ in range(multiple):
        x=qmap.cycle(x)[0];seeds.append(x.copy())
    seeds.extend(np.random.default_rng(20261122).uniform(-1,1,(256,4))*long.scale)
    roots=[];counts=[];failed=0
    for x in seeds:
        z,err,_=long.fit(x)
        if err>1e-9:failed+=1;continue
        i=next((i for i,v in enumerate(roots) if np.max(abs((v-z)/long.scale))<1e-6),None)
        if i is None:i=len(roots);roots.append(z);counts.append(0)
        counts[i]+=1
    records=[long.diagnose(x) for x in roots]
    for i,(x,r) in enumerate(zip(roots,records)):
        y=qmap.cycle(x)[0]
        distances=[np.linalg.norm((v-y)/long.scale) for v in roots]
        assert min(distances)<1e-6
        r['next_root_index']=int(np.argmin(distances));r['seed_count']=counts[i]
    seen=set();families=[]
    for i in range(len(roots)):
        if i in seen:continue
        cycle=[];j=i
        while j not in cycle:
            cycle.append(j);seen.add(j);j=records[j]['next_root_index']
        assert j==i
        families.append(cycle)
    escape=[]
    for cycle in families:
        i=cycle[0]
        if records[i]['spectral_radius']<=1:continue
        x=roots[i];_,jac,_=long.cycle(x)
        eig,vectors=np.linalg.eig(jac);index=np.argmax(abs(eig))
        assert abs(eig[index].imag)<1e-8
        direction=vectors[:,index].real
        direction/=np.linalg.norm(direction/long.scale)
        for sign in [-1,1]:
            state=x+sign*1e-6*direction
            for _ in range(1000):state=long.cycle(state)[0]
            final=long.diagnose(state)
            nearest=int(np.argmin([np.linalg.norm((r-state)/long.scale) for r in roots]))
            escape.append({'root':i,'sign':sign,'scaled_kick':1e-6,
                           'seconds':1000*len(long.input)/192000,
                           'nearest_root_index':nearest,'final':final})
    result={'point_id':point,'input_buffer_multiple':multiple,'seeds':len(seeds),'failed':failed,
            'roots':records,'cycle_families':families,'unstable_eigenvector_escapes':escape}
    (root/f'{point}-period-{multiple}.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({'point':point,'multiple':multiple,'families':families,'failed':failed,
          'radii':[r['spectral_radius'] for r in records],
          'escape':[{'sign':r['sign'],'nearest':r['nearest_root_index'],'power':r['final']['power']} for r in escape]}),flush=True)


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('directory',type=Path)
    parser.add_argument('--point',type=int);parser.add_argument('--multiple',type=int,default=1)
    args=parser.parse_args()
    if args.multiple!=1:
        assert args.point is not None and 1<args.multiple<=16
        analyze_multiple(args.directory,args.point,args.multiple);return
    for path in sorted(args.directory.glob('*-model.json')):
        if not path.with_name(path.name.replace('-model.json','-periodic.json')).exists():
            analyze(path)


if __name__=='__main__':main()
