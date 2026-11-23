"""Zero-reset detector-on noise spectra. Run beside metal.so and the stock float reference."""
import argparse
import ctypes as ct
import fcntl
import hashlib
import json
import os
import shutil
import sys
import time
from pathlib import Path
import numpy as np

SRC = Path(__file__).resolve().parent
ENGINE = 192000
EPOCH = 131072
N = 4096
HIGH = 2049
LOW = 513
BINS = HIGH + LOW
RMS = .25 / np.sqrt(2.)
F32 = np.ctypeslib.ndpointer(np.float32, flags='C_CONTIGUOUS')
EDGES = np.r_[20*2**(np.arange(120)/12), 20000.]
JOB = np.dtype([('c','<f4',11), ('seed','<u4')])


def fir(n, cutoff, fs):
    t = np.arange(n) - (n-1)/2
    h = np.sinc(t*2*cutoff/fs)*np.kaiser(n,12.)
    return np.asarray(h/h.sum(),np.float32)


def atomic_json(path, value):
    tmp = path.with_suffix('.tmp')
    tmp.write_text(json.dumps(value,indent=2,allow_nan=False)+'\n')
    tmp.replace(path)


def band_weights():
    weights=[]
    for i,(lo,hi) in enumerate(zip(EDGES[:-1],EDGES[1:])):
        rate,shift=(6000,HIGH) if i<60 else (48000,0)
        width=rate/N
        left=int(np.floor(lo/width+.5))
        right=int(np.ceil(hi/width+.5))
        bins=np.arange(left,right)*width
        overlaps=np.minimum(hi,bins+width/2)-np.maximum(lo,bins-width/2)
        weights.append((slice(shift+left,shift+right),np.maximum(0,overlaps)))
    return weights


BAND_WEIGHTS=band_weights()


def band_power(psd):
    """Integrate each band directly so a loud peak cannot cancel a quiet distant band."""
    out=np.empty(psd.shape[:-1]+(len(BAND_WEIGHTS),),np.float32)
    for i,(interval,widths) in enumerate(BAND_WEIGHTS):
        out[...,i]=np.einsum('...i,i->...',psd[...,interval],widths)
    return out


def make_noise(seed, trials, epochs):
    host_count = EPOCH//4*epochs
    output = np.empty((epochs,trials,EPOCH),np.float32)
    for t in range(trials):
        rng = np.random.default_rng(np.random.SeedSequence([seed,t]))
        spectrum = np.fft.rfft(rng.standard_normal(host_count))
        spectrum[[0,-1]] = 0
        wave = np.fft.irfft(spectrum,n=4*host_count)
        wave *= RMS / np.sqrt(np.mean(wave*wave))
        output[:,t] = wave.reshape(epochs,EPOCH)
    return output


class GPU:
    def __init__(self,tools,capacity,trials):
        self.stock = ct.CDLL(str((tools/'stock.so').resolve()))
        self.stock.Coefficients.argtypes = [ct.c_double]*3+[F32]
        self.stock.Coefficients.restype = None
        self.stock.Reference.argtypes = [ct.c_double]*3+[ct.c_int,F32,F32]
        self.stock.Reference.restype = None
        self.lib = ct.CDLL(str((tools/'metal.so').resolve()))
        self.lib.Open.argtypes = [ct.c_char_p,ct.c_int,ct.c_int]+[F32]*4
        self.lib.Open.restype = ct.c_void_p
        self.lib.Reset.argtypes = [ct.c_void_p,ct.c_uint,ct.c_void_p]
        self.lib.Reset.restype = None
        self.lib.Epoch.argtypes = [ct.c_void_p,F32,F32]
        self.lib.Epoch.restype = ct.c_int
        self.lib.Capture.argtypes = [ct.c_void_p,ct.c_uint,F32]
        self.lib.Capture.restype = None
        self.f1 = fir(257,24000,ENGINE)
        self.f2 = fir(129,2500,48000)
        self.window = np.hanning(N+1)[:-1].astype(np.float32)
        twiddle = np.exp(-2j*np.pi*np.arange(N//2)/N).astype(np.complex64).view(np.float32)
        self.ctx = self.lib.Open(os.fsencode(SRC/'spectrum.metal'),capacity,trials,self.f1,self.f2,self.window,twiddle)
        if not self.ctx:
            raise RuntimeError('Metal initialization failed')

    def reset(self,coords,trials):
        jobs = np.zeros((len(coords)+1,trials),dtype=JOB)
        jobs['seed'][:] = np.arange(trials)
        for i,coord in enumerate(coords):
            coeff = np.empty(11,np.float32)
            self.stock.Coefficients(*coord,coeff)
            jobs['c'][i] = coeff
        jobs['c'][-1,:,0]=-1
        self.lib.Reset(self.ctx,jobs.size,jobs.ctypes.data)
        return np.empty((len(coords)+1,trials,BINS),np.float32)

    def epoch(self,noise,out):
        if self.lib.Epoch(self.ctx,noise,out):
            raise RuntimeError('Metal dispatch failed')
        if not np.isfinite(out).all() or (out<0).any():
            raise ArithmeticError('Invalid PSD')

    def capture(self,job):
        data=np.empty(EPOCH,np.float32)
        self.lib.Capture(self.ctx,job,data)
        return data


def validate(tools):
    gpu=GPU(tools,5,1)
    coords=[(.1,.1,20.),(.8,.8,655.),(1.,1.,20000.),(1.,1.,655.)]
    result=gpu.reset(coords,1)
    noise=make_noise(341,1,2)
    reference=np.empty(2*EPOCH,np.float32)
    errors=[]
    for chunk in range(2):
        gpu.epoch(noise[chunk],result)
        for i,coord in enumerate(coords):
            gpu.stock.Reference(*coord,2*EPOCH,noise.reshape(-1),reference)
            gpu_out=gpu.capture(i)
            cpu_out=reference.reshape(2,EPOCH)[chunk]
            errors.append(float(np.max(abs(gpu_out-cpu_out))/max(np.max(abs(cpu_out)),1e-12)))
        if not np.array_equal(gpu.capture(len(coords)),noise[chunk,0]):
            raise AssertionError('Dry GPU trajectory mismatch')
        # Validate causal FIR overlap, bin powers, normalization and the epoch boundary independently.
        high=np.convolve(noise.ravel(),gpu.f1,'full')[:noise.size:4]
        low=np.convolve(high,gpu.f2,'full')[:len(high):8]
        frames=np.lib.stride_tricks.sliding_window_view(high[chunk*32768:(chunk+1)*32768],N)[::2048]
        want_hi=np.mean(abs(np.fft.rfft(frames*gpu.window))**2,axis=0)*(2/(48000*1536))
        want_lo=abs(np.fft.rfft(low[chunk*N:(chunk+1)*N]*gpu.window))[:LOW]**2*(2/(6000*1536))
        want_hi[[0,-1]]*=.5
        want_lo[0]*=.5
        want=np.r_[want_hi,want_lo]
        err=np.max(abs(result[-1,0]-want))/want.max()
        assert err<2e-5,err
        assert np.min(result[-1,0])>=0
    print('Maximum relative waveform error per trajectory/epoch:',errors,flush=True)
    assert max(errors)<.002,errors
    np.testing.assert_allclose(band_power(np.ones(BINS,np.float32)),np.diff(EDGES),rtol=1e-5)
    print('Stock waveforms, consecutive epoch overlap, both 4k FFTs and band integration passed.',flush=True)


def points(grid_shape):
    ids=np.arange(int(np.prod(grid_shape)))
    coords=np.stack(np.unravel_index(ids,tuple(grid_shape)),axis=-1)
    shuffled=np.random.default_rng(85173).permutation(len(ids))
    # Complete evenly spaced nested lattices before their additional neighbors.
    intervals=grid_shape-1
    rank=np.full(len(ids),999)
    for base in [144,108,90,72,54,36,27,18,9]:
        if np.all(intervals%base==0):
            rank[np.all(coords%(intervals//base)==0,axis=1)]=base
    return coords[shuffled[np.argsort(rank[shuffled],kind='stable')]]


def measure_tile(gpu, ids, shape, seed, trials, warm, measure, pilot=False):
    epochs=warm+measure
    fraction=ids/(shape-1)
    coords=np.column_stack((fraction[:,:2],20*1000**fraction[:,2]))
    power=gpu.reset(coords,trials)
    noise=make_noise(seed,trials,epochs)
    stages=np.zeros((2,)+power.shape,dtype=np.float64)
    if pilot:
        timeline=[]
    for epoch in range(epochs):
        gpu.epoch(noise[epoch],power)
        if pilot:
            timeline.append(band_power(power))
        if epoch>=warm:
            group=int(epoch-warm>=measure//2)
            stages[group]+=power
    counts=np.array([measure//2,measure-measure//2])
    total=(stages.sum(axis=0)/measure).astype(np.float32)
    stages/=counts[:,None,None,None]
    halves=total.reshape(len(coords)+1,2,trials//2,BINS).mean(2)
    temporal=band_power(stages.mean(2)).transpose(1,0,2)
    seed_bands=band_power(total)
    values=dict(ids=ids.astype(np.uint16),halves=halves[:-1],dry_halves=halves[-1],
        band_sd=seed_bands[:-1].std(axis=1,ddof=1),time_bands=temporal[:-1],
        dry_time_bands=temporal[-1],dry_band_sd=seed_bands[-1].std(axis=0,ddof=1))
    if pilot:
        values['pilot_epochs']=np.stack(timeline)
    return values


def save_tile(file, values):
    tmp=file.with_suffix('.partial')
    with tmp.open('wb') as f:
        np.savez(f,**values)
        f.flush()
        os.fsync(f.fileno())
    tmp.replace(file)


def run(args):
    out=args.out.resolve()
    out.mkdir(parents=True,exist_ok=True)
    lock=(out/'run.lock').open('a')
    fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
    if not (args.trials>=4 and args.trials%2==0 and args.measure>=2 and args.tile>0 and args.grid>=2):
        raise ValueError('Expected even trials >=4, >=2 measured epochs, positive tile and grid >=2')
    shape=np.array([args.grid,args.grid,args.cutoffs or args.grid])
    if min(shape)<2 or args.warm<0:
        raise ValueError('Invalid grid or warm-up')
    code_hash={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in
               [SRC/'survey.py',SRC/'spectrum.metal',SRC/'metal.mm',args.tools/'stock.so',args.tools/'metal.so']}
    manifest=dict(grid=args.grid,grid_shape=shape.tolist(),order=['drive','resonance','cutoff'],knobs='i/(grid-1)',cutoff='20*1000^(i/(grid-1)) Hz',
        detector='production adaptive, power floor on; float rational saturation; 192 kHz; all states zero',
        rms=float(RMS),noise='host Gaussian, DC/Nyquist removed, Fourier interpolation to 192 kHz, fixed RMS per record',
        seeds='independent draws by tile and trial, shared by configurations in tile; dry traverses identical analysis path',
        base_seed=args.seed,trials=args.trials,tile=args.tile,warm_epochs=args.warm,measure_epochs=args.measure,
        epoch_samples=EPOCH,epoch_seconds=EPOCH/ENGINE,averaging='arithmetic mean of linear powers, one-sided density per Hz',
        fft=4096,window='periodic Hann',high='48 kHz, bins 0..2048; 15 frames per epoch at 50% overlap',
        low='6 kHz, bins 0..512; 1 nonoverlapping frame per epoch',band_edges=EDGES.tolist(),
        decimation='causal Kaiser beta=12 FIR, 257 taps at 24 kHz then 129 taps at 2.5 kHz; bass below 640 Hz',
        layout='ids rows (indices); halves[point,seed_half,bin], dry_halves[seed_half,bin]; bin order high then low; band_sd across individual late trial power means; time_bands early/late measured halves; pilot_epochs includes dry last row',
        pilot=args.pilot,code_hash=code_hash)
    mf=out/'manifest.json'
    if mf.exists() and json.loads(mf.read_text())!=manifest:
        raise ValueError('Existing run identity differs')
    atomic_json(mf,manifest)
    indices=points(shape)
    gpu=GPU(args.tools,(args.tile+1)*args.trials,args.trials)
    tiles=(len(indices)+args.tile-1)//args.tile
    run_start=time.monotonic()
    computed=0
    for tile in range(tiles):
        file=out/f'tile-{tile:06d}.npz'
        if file.exists():
            continue
        ids=indices[tile*args.tile:(tile+1)*args.tile]
        values=measure_tile(gpu,ids,shape,args.seed+tile,args.trials,args.warm,args.measure,args.pilot)
        save_tile(file,values)
        computed+=len(coords)
        elapsed=time.monotonic()-run_start
        done=min((tile+1)*args.tile,len(indices))
        progress=dict(pid=os.getpid(),done=done,total=len(indices),tiles_done=tile+1,tiles=tiles,
            rate_per_s=computed/elapsed,eta_hours=(len(indices)-done)/(computed/elapsed)/3600,
            free_gib=shutil.disk_usage(out).free/2**30,updated_unix=time.time(),complete=done==len(indices))
        atomic_json(out/'progress.json',progress)
        if tile<3 or tile%10==0 or done==len(indices):
            print(json.dumps(progress),flush=True)
        if progress['free_gib']<5 and done<len(indices):
            raise RuntimeError('Storage reserve reached; completed tiles retained')
    print('Finished',out,flush=True)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--tools',type=Path,default=Path('tools'))
    parser.add_argument('--validate',action='store_true')
    parser.add_argument('--out',type=Path)
    parser.add_argument('--grid',type=int,default=10)
    parser.add_argument('--cutoffs',type=int,help='Optional separate number of log-spaced cutoff positions')
    parser.add_argument('--trials',type=int,default=16)
    parser.add_argument('--warm',type=int,default=0)
    parser.add_argument('--measure',type=int,default=10)
    parser.add_argument('--seed',type=int,default=20261122)
    parser.add_argument('--tile',type=int,default=128)
    parser.add_argument('--pilot',action='store_true')
    args=parser.parse_args()
    if args.validate:
        validate(args.tools)
    else:
        run(args)
