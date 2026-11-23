"""Enclose the cyclic inverse by solving for block boundaries before recovering their interiors."""
import numpy as np
from scipy.sparse import bsr_matrix, eye
from scipy.sparse.linalg import splu

from phase_tube import Ball, EPS, TINY, product


def inverse_bounds(matrices, forcing, block_size=32):
    q,d,_ = matrices.shape
    edges = np.arange(0,q,block_size)
    count = len(edges)
    lengths = np.minimum(block_size,q-edges)
    nb = forcing.shape[-1]
    identity = np.eye(d)
    p = Ball(np.broadcast_to(identity,(count,d,d)),np.zeros((count,d,d)))
    # Prefix products retain signs. Repeated scalar absolute products would
    # discard the damped rotations inside each bandpass.
    prefixes = []
    for j in range(block_size):
        idx = np.minimum(edges+j,q-1)
        active = (j<lengths)[:,None,None]
        mat = np.where(active,matrices[idx],identity)
        p = product(Ball(mat,np.zeros_like(mat)),p)
        # Evaluate the positive source by summing signed suffix-product bounds
        # below. Propagating v through |A| sample by sample is unnecessarily loose.
        prefixes.append(p)
    # Binary grouping retains cancellation in the local inverse error bound.
    def block_source(start,length):
        if length==1:
            index = edges+start
            active = (index<q)[:,None,None]
            index = np.minimum(index,q-1)
            mat = np.where(active,matrices[index],identity)
            return Ball(mat,np.zeros_like(mat)),np.where(active,forcing[index],0.)
        half = length//2
        early,ev = block_source(start,half)
        late,lv = block_source(start+half,length-half)
        return product(late,early),((late.magnitude()@ev)*(1+32*EPS)+lv+TINY)*(1+8*EPS)
    p,v = block_source(0,block_size)
    # Boundary equation indexed by its destination: x_j-P_{j-1}x_{j-1}=b_j.
    idx = (np.arange(count)-1)%count
    closure = eye(count*d,format='csc')-bsr_matrix((p.m[idx],idx,np.arange(count+1)),shape=(count*d,)*2).tocsc()
    lu = splu(closure)
    source = np.roll(v,1,axis=0)
    rhs_bound = np.empty((count,d,nb))
    err_by_row = np.empty((count,d,1))
    guard = 1+(count*d+8)*EPS
    for lo in range(0,count,4):
        hi = min(count,lo+4)
        width = (hi-lo)*d
        e = np.zeros((count*d,width))
        e[lo*d:hi*d] = np.eye(width)
        b = lu.solve(e,trans='T').T.reshape(hi-lo,d,count,d).transpose(0,2,1,3)
        bm = product(Ball(np.roll(b,-1,axis=1),np.zeros_like(b)),p)
        error = (Ball(e.T.reshape(hi-lo,d,count,d).transpose(0,2,1,3))-Ball(b)+bm).magnitude()
        err_by_row[lo:hi,:,0] = error.sum(axis=(1,3))*guard+TINY
        rhs_bound[lo:hi] = (abs(b)@source).sum(axis=1)*guard+TINY
    defect = err_by_row.max()
    if defect>=1:
        raise ArithmeticError(f'boundary inverse defect {defect:g}')
    radius = (Ball(rhs_bound.max(axis=(0,1)))/(1-Ball(defect))).hi()
    bounds = (Ball(rhs_bound)+Ball(err_by_row)*Ball(radius)).hi()
    worst = bounds.max(axis=(0,1))
    for j,prefix in enumerate(prefixes[:-1],start=1):
        _,disturbance = block_source(0,j)
        inside = (prefix.magnitude()@bounds)*(1+32*EPS)+disturbance
        worst = np.maximum(worst,inside[j<lengths].max(axis=(0,1),initial=0.))
    return (worst+TINY)*(1+8*EPS)
