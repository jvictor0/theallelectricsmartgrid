"""Check whether one fresh noise record erases all reachable input-detector histories."""
import argparse
import ctypes as ct
from concurrent.futures import ThreadPoolExecutor
from fractions import Fraction as Q
from pathlib import Path

import numpy as np

from record_bank import bind
from survey import make_noise, atomic_json


def check_tube(coeff):
    a,b,c,beta = map(Q,map(float,coeff[4:8]))
    u = Q(1,2**24)
    tiny = Q(1,2**80)
    # P = [[1,1/2],[1/2,1]]. Check the exact Sylvester minors.
    x,y,z = 2*a-1, 2*b, 1-2*c
    rho = Q(996,1000)
    xx = rho**2-(x*x+x*y+y*y)
    yy = rho**2-(y*y-y*z+z*z)
    xy = rho**2/2-(-x*y+(x*z-y*y)/2+y*z)
    assert xx>0 and xx*yy>xy**2 and Q('1.16')**2>Q(4,3)
    def error(X,Y,I,t):
        def rnd(v,e=0):
            return u*v+(1+u)*e+t
        diff = I+Y
        bp,lp = a*X+b*diff, Y+b*X+c*diff
        e_bp = rnd(bp,rnd(a*X)+b*rnd(diff))
        e_lp = rnd(lp,rnd(Y+b*X)+c*rnd(diff))
        return rnd(2*bp+X,2*e_bp)+rnd(2*lp+Y,2*e_lp)
    # Compare two rounded trajectories term by term in |cx|, |cy|, |input|,
    # radius and underflow. The positive binary64 recursion has excess margin.
    for j in range(5):
        X,Y,I,R,t = [Q(int(j==i)) for i in range(5)]
        bound = rho*R+error(X,Y,I,t*tiny)+error(X+Q('1.16')*R,Y+Q('1.16')*R,I,t*tiny)
        impl = Q('.99601')*R+17*u*(X+Y+Q('.02')*I)+Q('1e-18')*t
        assert bound < impl*(1-Q(1,10**13))
    # p + beta*(v-p), for exactly common, already rounded squared input v.
    for j in range(4):
        P,V,R,t = [Q(int(j==i)) for i in range(4)]
        def ema(p):
            ed = u*(V+p)+t*tiny
            et = u*beta*(V+p)+(1+u)*beta*ed+t*tiny
            return u*(p+beta*(V+p))+(1+u)*et+t*tiny
        bound = (1-beta)*R+ema(P)+ema(P+R)
        impl = (1-beta+6*u)*R+10*u*(P+beta*V)+Q('1e-18')*t
        assert bound < impl*(1-Q(1,10**13))


def main(root, library, workers, count):
    stock = bind(Path('build/tools/ladder-noise-grid/cycle.so'))
    with np.load(root.parent/'survivor-replay-250/fresh-parents.npz') as z:
        knobs = z['settings'][0]
    coeff = np.empty(11,'f4')
    stock.Coefficients(*knobs,coeff)
    check_tube(coeff)
    f = ct.CDLL(str(library.resolve())).DetectorMemory
    f.argtypes = [ct.c_double]*3+[ct.c_int]+[np.ctypeslib.ndpointer(t) for t in ('f4','i4','u8','u4')]
    f.restype = ct.c_int
    def test(i):
        noise = make_noise(3187164523+i,1,14).ravel()
        steps, sizes, terminal = np.zeros(3,'i4'),np.zeros(3,'u8'),np.zeros(1,'u4')
        code = f(*knobs,len(noise),noise,steps,sizes,terminal)
        if code == 4:
            raise ArithmeticError('Exhaustive terminal disagrees with stock from reset')
        return dict(code=code, steps=steps.tolist(), sizes=sizes.tolist(), terminal=int(terminal[0]))
    with ThreadPoolExecutor(workers) as pool:
        rows = []
        for row in pool.map(test,range(count)):
            rows.append(row)
            print(len(rows),row,flush=True)
    atomic_json(root/f'detector-{count}.json',dict(seed=3187164523, rows=rows))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('root',type=Path)
    parser.add_argument('--library',type=Path,default=Path('build/tools/ladder-noise-grid/detector-memory.so'))
    parser.add_argument('--workers',type=int,default=2)
    parser.add_argument('--count',type=int,default=16)
    args=parser.parse_args()
    main(args.root,args.library,args.workers,args.count)
