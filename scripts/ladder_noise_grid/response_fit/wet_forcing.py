"""Certify a three-record event erasing the entire stock ladder at 249.70 Hz."""
import argparse
import ctypes as ct
from pathlib import Path
from fractions import Fraction as Q
import json
from hashlib import sha256
import numpy as np
from scipy.ndimage import gaussian_filter1d
from record_bank import TAIL, bind
from dry_power_lower import check_constants
from dry_reset_word import certify as certify_dry, certify_word
from forcing_word import certify as certify_segment
from wet_detector_cap import report as detector_cap
from wet_detector_forcing import check as check_wet_tube
from fresh_dry_power import HOST, NATIVE
from survey import EPOCH, atomic_json, make_noise

TOOLS=Path('build/tools/ladder-noise-grid')
QUIET=18000

def prefix(index):
    rng=np.random.default_rng(99221+index)
    period=rng.uniform(650,820)
    length=rng.integers(1100,3300)
    amplitude=rng.uniform(2.2,4)
    first,second=rng.uniform(4.8,7.),rng.uniform(3.9,6.1)
    transition,period2=rng.uniform(400,2200),rng.uniform(400,2000)
    depth=rng.uniform(.15,1.9)
    phase,p1,p2=rng.uniform(0,6.28,3)
    t=np.arange(7200)-length
    bias=second+(first-second)*np.clip((transition-t)/180,0,1)
    return gaussian_filter1d(np.where(t<0,amplitude*np.sin(2*np.pi*t/period+p1),
        bias+depth*np.sin(2*np.pi*t/period2+phase)+.12*np.sin(t*.036+p2)),7)

def normalize_word(host):
    # The unused energy goes into a remote smooth pulse. The resulting dyadic
    # host direction is projected and normalized exactly by the support checker.
    i=np.arange(512)
    filler=np.sin(np.pi*i/512)**4*np.cos(2*np.pi*i/64)
    projected=np.fft.rfft(host)
    projected[[0,-1]]=0
    energy=np.sum(np.fft.irfft(projected)**2)
    assert energy < HOST/32
    host[HOST//2:HOST//2+len(i)] += np.sqrt((HOST/32-energy)/sum(filler*filler))*filler
    return host

def native_record(host):
    projected = np.fft.rfft(host)
    projected[[0,-1]] = 0
    x = np.fft.irfft(projected,n=NATIVE)
    x *= np.sqrt((NATIVE/32)/np.sum(x*x))
    return x.astype('f4')

def design(index, cut, tail_level, tail_length, first_energy=56200):
    x=prefix(index)
    # Join after the four audio registers have approached each other. The second
    # portion drives the input detector near its center while its wet peer settles.
    t=np.arange(tail_length)
    tail=tail_level*(np.cos(2*np.pi*t/765+.2)+.13*np.sin(2*np.pi*t/519+.7)
                     +.26*np.sin(2*np.pi*t/838+1.2))
    transition=80
    tail[:transition]=x[cut-1]+(tail[:transition]-x[cut-1])*(1-np.cos(np.pi*np.arange(transition)/transition))/2
    signal=np.r_[x[:cut],tail]
    boundary=min(cut-1,int(np.searchsorted(np.cumsum(signal*signal),first_energy)))
    boundary-=boundary%4
    length=(len(signal)-boundary)//4*4
    first=np.zeros(HOST)
    first[-boundary//4:]=signal[:boundary:4]
    second=np.zeros(HOST)
    second[:length//4]=signal[boundary:boundary+length:4]
    first,second=normalize_word(first),normalize_word(second)
    f,s=native_record(first),native_record(second)
    begin=NATIVE-boundary-QUIET
    return (first,second),begin,np.r_[f[begin:],s[:length]]

def cfunc(name,fn,argtypes):
    lib=ct.CDLL(str((TOOLS/name).resolve()))
    run=getattr(lib,fn)
    run.argtypes=argtypes
    run.restype=ct.c_int
    return run

ptr=np.ctypeslib.ndpointer
FP=ptr('f4',flags='C_CONTIGUOUS')
DARGS=[ct.c_double]*3+[ct.c_int,FP]

def check_audio_rounding(words):
    # Work at the rounded value of gain*x, a monotone function of x. The
    # derivative of z(27+z²)/(27+9z²) is (z²-9)²/[9(z²+3)²].
    # For |z| >= 2^-60 all nonzero positive factors are normal. The three
    # numerator roundings, two denominator roundings and quotient rounding
    # give the relative error below. Clipping a nonnegative ratio at one
    # turns this into an absolute error even on the flat portion. Smaller
    # |z| gives absolute error <2^-55 directly, including underflow.
    alpha, tanh_gain = Q(float(words[0])), Q(float(words[3]))
    u = Q(1,2**24)
    rel_hi = (1+u)**4/(1-u)**2
    rel_lo = (1-u)**4/(1+u)**2
    deviation = max(rel_hi-1, 1/rel_lo-1)
    per_endpoint = (deviation+u)/tanh_gain + Q(1,2**55)
    assert 2*per_endpoint < Q('0.000003')
    assert Q(float(words[2]))*500 < 800 and 0 < alpha < 1
    # The shared pole bound for stages 2 through 4 is invariant under the
    # rounded positive complement, its product, and the final stock FMA.
    bound = Q('1.073')
    top = (1+u)/tanh_gain + Q(1,2**80)
    complement = Q(float(np.float32(1)-words[0]))
    assert (complement*bound*(1+u)+alpha*top)*(1+u)+Q(1,2**70) < bound

def erase_wet(knobs, coeff, w):
    dry = np.empty_like(w)
    assert cfunc('dry-power-lower.so','DryPowerLower',DARGS+[FP])(*knobs,len(w),w,dry)==0
    hit = np.full(1,-1,'i4')
    low, high = np.empty_like(w), np.empty_like(w)
    cap = np.nextafter(np.float32(2.078),np.float32(np.inf))
    fn = cfunc('wet-forcing.so','WetForcing',DARGS+[FP,ct.c_float,ptr('i4'),FP,FP])
    assert fn(*knobs,len(w),w,dry,cap,hit,low,high)==0
    start = int(hit[0])
    assert 0 < start < len(w)
    np.testing.assert_array_equal(low[start:].view('u4'),high[start:].view('u4'))

    # Validate the provisional full-feedback waveform by induction. Each
    # upper bound uses only earlier stage-four intervals, so this comparison
    # has no circular assumption about the controller on the current sample.
    upper = np.empty_like(w)
    fn = cfunc('wet-detector-forcing.so','WetDetectorBoxUpper',DARGS+[FP,FP])
    assert fn(*knobs,len(w),low,high,upper)==0
    ref = np.multiply(coeff[8],dry[start:],dtype=np.float32)
    margin = ref.astype(float)-upper[start:].astype(float)
    assert np.min(margin)>0
    stats, cuts = np.zeros(9,'i4'), np.full(3,-1,'i4')
    pre = low[start:]
    diagnostic_upper = np.empty_like(pre)
    fn = cfunc('wet-detector-forcing.so','WetDetectorForcingStats',DARGS+[FP,ptr('i4'),ptr('i4')])
    assert fn(*knobs,len(pre),pre,diagnostic_upper,cuts,stats)==0
    assert 0 < cuts[0] < cuts[1] < cuts[2] <= len(pre)
    return start,cuts,stats,float(np.min(margin))

def replay_stock(stock, knobs, dry, hosts, hit):
    """Cross-check the certified meeting time through the ordinary stock entry point."""
    first,_ = certify_word(dry['period'],dry['modes'],dry['weights'],dry['phases'])
    second,third = map(native_record,hosts)
    records = (first,second,third[:hit])
    before,at_hit,after = [],[],[]
    for seed in (71189,247109):
        history = make_noise(seed,1,NATIVE//EPOCH).ravel()
        state = np.empty(14,'u4')
        handle = stock.CreateCycleFilter(*knobs)
        try:
            stock.RunCycleRecord(handle,len(history),history,np.empty_like(history))
            stock.ReadCycleState(handle,state)
            before.append(state.copy())
            for record in records:
                stock.RunCycleRecord(handle,len(record),record,np.empty_like(record))
            stock.ReadCycleState(handle,state)
            at_hit.append(state.copy())
            tail = third[hit:]
            output = np.empty_like(tail)
            stock.RunCycleRecord(handle,len(tail),tail,output)
            after.append(sha256(output.astype('<f4',copy=False).tobytes()).hexdigest())
        finally:
            stock.DestroyCycleFilter(handle)
    assert np.any(before[0] != before[1])
    np.testing.assert_array_equal(at_hit[0],at_hit[1])
    assert after[0] == after[1]
    return dict(seeds=[71189,247109],distinct_initial_registers=int(np.sum(before[0] != before[1])),
                common_register_words=[hex(int(x)) for x in at_hit[0]],common_tail_sha256=after[0])

def main(grid,out):
    out.mkdir(parents=True,exist_ok=True)
    with np.load(grid/'screen-v3/survivor-replay-250/fresh-parents.npz') as z:
        knobs = z['settings'][0]
    coeff = np.empty(11,'f4')
    stock = bind(TOOLS/'cycle.so')
    stock.Coefficients(*knobs,coeff)
    check_constants(coeff)
    assert detector_cap(knobs,coeff)['wet_ema_upper']=='1039/500'
    check_wet_tube(knobs,coeff)
    check_audio_rounding(coeff)
    dry_path = out/'dry-reset.json'
    certify_dry(grid,TOOLS/'detector-memory.so',TOOLS/'cycle.so',dry_path,7168,
        [3,5,8,9,11],[78,75,52,53,98],[2,5,3,9,10])
    dry = json.loads(dry_path.read_text())
    hosts,begin,w = design(119,4690,.55,78000)
    split = NATIVE-begin
    probabilities = []
    for j,(host,offset,chosen) in enumerate(zip(hosts,(begin,0),(w[:split],w[split:]))):
        indices = np.flatnonzero(host)
        proof = certify_segment(indices,host[indices],offset,chosen)
        assert proof['strict_support']
        proof['native_words_sha256'] = sha256(chosen.astype('<f4',copy=False).tobytes()).hexdigest()
        atomic_json(out/f'word-{j}-probability.json',proof)
        probabilities.append(proof)
    start,cuts,stats,margin = erase_wet(knobs,coeff,w)
    last = start+int(cuts[-1])-split
    assert 0 < last < len(w)-split
    # At a fixed phase even an unfinished analyzer epoch and its FIR prefix
    # agree by the third boundary. The rest of that record is shared.
    assert NATIVE-last >= TAIL+EPOCH
    assert 2**1000 < 10**302
    dry_exponent = (dry['gaussian_probability_lower_negative_power_of_two']*302+999)//1000
    def exponent(p,key):
        mantissa,exp=p[key].split('e-')
        assert mantissa=='1'
        return int(exp)
    low_exp = dry_exponent+sum(exponent(p,'probability_lower') for p in probabilities)
    high_exp = sum(exponent(p,'probability_upper') for p in probabilities)
    report=dict(knobs=knobs.tolist(),record_samples=NATIVE,
        dry_reset_boundary=dry['detector']['steps'][-1],shaped_slices=[split,len(w)-split],
        audio_boundary_in_record_three=start-split,
        wet_detector_boundaries_in_record_three=(cuts+start-split).tolist(),
        enumeration_start_size_final=stats.reshape(3,3).tolist(),min_full_feedback_margin=margin,
        samples_left_for_analyzer=NATIVE-last,analyzer_history_upper=TAIL+EPOCH,
        event_probability_lower=f'1e-{low_exp}',event_probability_upper=f'1e-{high_exp}',
        stock_replay=replay_stock(stock,knobs,dry,hosts,last))
    atomic_json(out/'full-reset.json',report)
    print(report)

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('grid',type=Path)
    parser.add_argument('out',type=Path)
    args=parser.parse_args()
    main(args.grid,args.out)
