"""Search for bit-exact return of every stock feedback memory under one repeated record."""
from pathlib import Path
import sys,json,hashlib,ctypes as ct
from concurrent.futures import ThreadPoolExecutor
import numpy as np

here=Path(__file__).resolve().parent
prev=here.parent/'physics-v1'
sys.path.insert(0,str(prev))
from stock_audit import make_noise, stock, host_bands, F32, first, second, window, N, LOW, band_power, EPOCH, EDGES
from fit_display import curve_mse

lib=ct.CDLL(str(here/'cycle.so'))
lib.CreateCycleFilter.argtypes=[ct.c_double]*3;lib.CreateCycleFilter.restype=ct.c_void_p
lib.ReadCycleState.argtypes=[ct.c_void_p,np.ctypeslib.ndpointer(np.uint32,flags='C_CONTIGUOUS')];lib.ReadCycleState.restype=None
lib.RunCycleRecord.argtypes=[ct.c_void_p,ct.c_int,F32,F32];lib.RunCycleRecord.restype=None
lib.DestroyCycleFilter.argtypes=[ct.c_void_p];lib.DestroyCycleFilter.restype=None

def all_phases(x,tail):
    # A 1024-native-sample prefix exceeds both causal FIR histories and preserves the decimation phase.
    native=np.r_[tail,x].astype(float)
    def dec(a,k,s):
        out=np.zeros(len(a)//s)
        for j in range(s):
            v=np.convolve(a[(-j)%s::s],k[j::s].astype(float),'full')
            if j:out[1:]+=v[:len(out)-1]
            else:out+=v[:len(out)]
        return out
    h=dec(native,first,4);l=dec(h,second,8)[32:];h=h[256:]
    frames=np.lib.stride_tricks.sliding_window_view(h,N)[::N//2]
    frames=frames[np.arange(len(frames))%16!=15]
    a=(abs(np.fft.rfft(frames*window))**2).mean(0)*2/(48000*1536)
    b=(abs(np.fft.rfft(l.reshape(-1,N)*window))**2).mean(0)[:LOW]*2/(6000*1536)
    a[[0,-1]]*=.5;b[0]*=.5
    return band_power(np.r_[a,b]).astype(float)

def run(p,x,max_periods):
    f=lib.CreateCycleFilter(*p);y=np.empty_like(x);bits=np.empty(14,np.uint32)
    def state():
        lib.ReadCycleState(f,bits)
        if not np.isfinite(bits.view(np.float32)).all(): raise ArithmeticError('nonfinite state')
        return bits.tobytes()
    seen={state():0};cold=None
    try:
        for n in range(1,max_periods+1):
            lib.RunCycleRecord(f,len(x),x,y)
            if n==1:
                check=np.empty_like(x);stock.Reference(*p,len(x),x,check)
                if not np.array_equal(check.view(np.uint32),y.view(np.uint32)):raise ValueError('stock output mismatch')
                cold=host_bands(y)
            key=state()
            if key in seen:
                p0=seen[key];length=n-p0
                final=key
                tail=y[-1024:].copy()
                phase=np.zeros(120);late=np.zeros(120);digest=hashlib.sha256()
                for j in range(length):
                    lib.RunCycleRecord(f,len(x),x,y)
                    phase+=all_phases(y,tail);late+=host_bands(y)
                    digest.update(y.view(np.uint8));tail=y[-1024:].copy()
                if state()!=final:raise ValueError('cycle failed replay')
                return p0,length,final.hex(),digest.hexdigest(),cold,phase/length,late/length
            seen[key]=n
        return -1,0,'','',cold,np.full(120,np.nan),np.full(120,np.nan)
    finally:
        lib.DestroyCycleFilter(f)

def main():
    rng=np.random.default_rng(7136701)
    controls=np.load(prev/'challenge.npz')['coords'].tolist()
    controls+= [[1,1,20],[1,1,20000],[1,0,20]]
    for d,r,c in rng.random((24,3)):
        controls.append([d,r,20*1000**c])
    groups=[]
    for j in range(4):
        # The structured settings receive 32 independent records; random settings receive eight.
        xx=make_noise(85130319+j,8,14).transpose(1,0,2).copy().reshape(8,-1)
        dry=[all_phases(x,x[-1024:]) for x in xx];drylate=[host_bands(x) for x in xx]
        pairs=[(k,t) for k in range(len(controls) if j==0 else 6) for t in range(8)]
        with ThreadPoolExecutor(10) as pool:
            rows=list(pool.map(lambda v:run(controls[v[0]],xx[v[1]],20),pairs))
        for (k,t),(a,b,state,digest,cold,phase,late) in zip(pairs,rows):
            groups.append((k,j,t,a,b,state,digest,cold,phase,late,dry[t],drylate[t]))
        print('group',j,'done',sum(r[4]>0 for r in groups),'of',len(groups),flush=True)
    keys=['k','group','trial','start','period','bits','wave_sha','cold','phase','late','dry','drylate']
    np.savez_compressed(here/'cycles.npz',controls=np.array(controls),**{s:np.array(col) for s,col in zip(keys,zip(*groups))})
if __name__=='__main__':main()
