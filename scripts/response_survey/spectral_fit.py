"""Nonuniform control meshes and per-slice quantized spectral bases."""
import struct

import numpy as np


def layout(d):
    knots = d.r["spectralMesh"]
    if (len(knots) != 3 or len(d.axes) != 3 or d.relative_axis is not None
            or any(not 2 <= k <= n for k, n in zip(knots,d.knots))):
        raise ValueError("Spectral fitting needs three valid mesh sizes on fixed-Hz spectra")
    nx, ny, nz = knots
    bands = len(d.freq)
    fixed = 20 + 3*24 + 8*(bands + sum(knots)) + 4*(nz+1) + 2*bands*nz
    per_mode = nx*ny + 2*bands
    count = min(2*min(36,bands)*nz, (d.r["budgetBytes"]-fixed)//per_mode)
    if count < 1:
        raise ValueError("Byte budget cannot store the spectral mesh and a basis vector")
    return fixed+count*per_mode, count


def frequency_mass(d):
    f = np.log(d.freq)
    mass = np.zeros((len(f), len(f)))
    for j, width in enumerate(np.diff(f)):
        mass[j:j+2,j:j+2] += width*np.array([[2,1],[1,2]])/6
    if d.dual_rate:
        mass[0,0] += f[0]-np.log(d.dual_rate.edges[0])
        mass[-1,-1] += np.log(d.dual_rate.edges[-1])-f[-1]
    return mass/mass.sum()


def optimal_mesh(planes, metric, count):
    n = len(planes)
    if n == count:
        return np.arange(n)
    encoded = (planes.astype(float) @ metric).reshape(n,-1)
    encoded -= encoded[:1].copy()
    gram = encoded @ encoded.T
    diag = gram.diagonal()
    costs = np.full((n,n), np.inf)
    for a in range(n-1):
        for b in range(a+1,n):
            ix = np.arange(a+1,b)
            w = (ix-a)/(b-a)
            v = 1-w
            err = diag[ix]+v*v*diag[a]+w*w*diag[b]+2*v*w*gram[a,b]-2*v*gram[ix,a]-2*w*gram[ix,b]
            costs[a,b] = max(0, err.sum())
    best = np.full(n, np.inf)
    best[0] = 0
    parents = np.zeros((count,n), dtype=int)
    for j in range(1,count):
        paths = best[:,None]+costs
        parents[j] = paths.argmin(axis=0)
        best = paths.min(axis=0)
    mesh = [n-1]
    for j in range(count-1,0,-1):
        mesh.append(parents[j,mesh[-1]])
    return np.array(mesh[::-1])


def weights(mesh, n):
    x = np.arange(n)
    i = np.clip(np.searchsorted(mesh,x,side="right")-1,0,len(mesh)-2)
    t = (x-mesh[i])/(mesh[i+1]-mesh[i])
    w = np.zeros((n,len(mesh)))
    w[x,i] = 1-t
    w[x,i+1] = t
    return w


def project_slices(values, mesh):
    if len(values) == len(mesh):
        return values
    w = weights(mesh,len(values))
    # The normal matrix of the linear interpolation weights is tridiagonal.
    rhs = np.zeros((len(mesh), *values.shape[1:]), np.float32)
    for i, row in enumerate(w):
        for j in np.flatnonzero(row):
            rhs[j] += row[j]*values[i]
    gram = w.T @ w
    diag, upper = gram.diagonal().copy(), gram.diagonal(1)
    for j in range(1,len(mesh)):
        factor = upper[j-1]/diag[j-1]
        diag[j] -= factor*upper[j-1]
        rhs[j] -= factor*rhs[j-1]
    rhs[-1] /= diag[-1]
    for j in reversed(range(len(mesh)-1)):
        rhs[j] = (rhs[j]-upper[j]*rhs[j+1])/diag[j]
    return rhs


def quantized(data, dtype):
    q = np.rint(data)
    limits = np.iinfo(dtype)
    if not np.isfinite(q).all() or q.min() < limits.min or q.max() > limits.max:
        raise ValueError("Spectral coefficient exceeds the packed representation")
    return q.astype(dtype)


def fit(d, curves):
    size, count = layout(d)
    nx, ny, nz = d.r["spectralMesh"]
    bands = len(d.freq)
    metric = np.linalg.cholesky(frequency_mass(d))
    inverse = np.linalg.inv(metric)
    clipped = np.maximum(curves,d.r["floorDb"]).transpose(2,0,1,3)
    zmesh = optimal_mesh(clipped[:,::5,::5], metric, nz)
    xmesh = optimal_mesh(clipped[::4,:,::5].swapaxes(0,1), metric, nx)
    ymesh = optimal_mesh(clipped[::4,::5,:].transpose(2,0,1,3), metric, ny)
    wx, wy = weights(xmesh,d.knots[0]), weights(ymesh,d.knots[1])
    ix, iy = np.linalg.pinv(wx), np.linalg.pinv(wy)
    values = np.empty((d.knots[2],nx,ny,bands),np.float32)
    for z, plane in enumerate(clipped):
        values[z] = iy @ ((ix @ plane.reshape(d.knots[0],-1)).reshape(nx,d.knots[1],bands))
    values = project_slices(values,zmesh)
    means, bases, scores, benefits = [], [], [], []
    gaps = np.diff(zmesh)
    widths = (np.r_[gaps[0],gaps]+np.r_[gaps,gaps[-1]])/2
    rank = min(36,bands)
    for z, plane in enumerate(values):
        dense = wy @ ((wx @ plane.reshape(nx,-1)).reshape(d.knots[0],ny,bands))
        mean = dense.mean(axis=(0,1))
        means.append(quantized(mean*128,"<i2"))
        sample = (dense.reshape(-1,bands)-mean) @ metric
        eigen, vectors = np.linalg.eigh(sample.T@sample/len(sample))
        vectors = vectors[:,::-1][:,:rank].copy()
        pivots = np.argmax(np.abs(vectors),axis=0)
        vectors *= np.sign(vectors[pivots,np.arange(rank)])
        coeff = ((plane-mean)@metric) @ vectors
        scale = np.maximum(np.max(np.abs(coeff),axis=(0,1)),1e-10)
        base = (vectors.T@inverse)*scale[:,None]
        q = quantized(127*coeff/scale,"i1")
        fine = quantized(127*128*(coeff/scale-q.astype(float)/127),"i1")
        diff = coeff - q*scale/127
        err = wy @ ((wx @ diff.reshape(nx,-1)).reshape(d.knots[0],ny,rank))
        err = (err**2).mean(axis=(0,1))
        benefit = np.maximum(0,eigen[::-1][:rank]-err)
        benefits.append(widths[z]*np.stack((benefit,np.minimum(err,benefit*.999)),axis=1))
        bases.append(np.stack((quantized(128*base,"<i2"),quantized(base,"<i2")),axis=1))
        scores.append(np.stack((np.moveaxis(q,2,0),np.moveaxis(fine,2,0)),axis=1))
    selected = np.sort(np.argsort(-np.array(benefits).ravel(),kind="stable")[:count])
    offsets = np.r_[0,np.cumsum(np.bincount(selected//(rank*2),minlength=nz))].astype("<u4")
    header = struct.pack("<4sIId",b"DSB1",3,bands,d.r["floorDb"])
    axes = b"".join(struct.pack("<IIdd",n,a["logScale"],a["low"],a["high"])
                    for n,a in zip((nx,ny,nz),d.axes))
    mesh = b"".join((x/(n-1)).astype("<f8").tobytes() for x,n in zip((xmesh,ymesh,zmesh),d.knots))
    artifact = b"".join((header,axes,d.freq.astype("<f8").tobytes(),mesh,offsets.tobytes(),
                         np.array(means,dtype="<i2").tobytes(),
                         np.array(bases,dtype="<i2").reshape(-1,bands)[selected].tobytes(),
                         np.array(scores,dtype="i1").reshape(-1,nx,ny)[selected].tobytes()))
    if len(artifact) != size:
        raise ValueError("Spectral artifact size mismatch")
    return artifact
