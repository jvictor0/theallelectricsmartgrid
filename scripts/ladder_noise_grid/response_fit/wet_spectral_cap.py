"""Deterministic stock wet-band caps from first differences at the 249.70 Hz challenge."""
import argparse
from fractions import Fraction as Q
import json
from math import isqrt
from pathlib import Path

import numpy as np

from finite_stock import check as check_stock, rounded_error, tiny, u
from fresh_dry_power import dyadic, sin_pi
from record_bank import bind
from survey import BAND_WEIGHTS, EDGES, EPOCH, atomic_json
from wet_block_bounds import check_analyzer, one_sided_rms
from wet_power_certificate import NATIVE


FFT = 4096


def sqrt_upper(value):
    denominator = 2**48
    return Q(isqrt(int(value*denominator**2))+1, denominator)


def up_float(value):
    return float(np.nextafter(float(value), np.inf))


def stock_slew(knobs):
    check_stock()
    words = np.empty(11, 'f4')
    bind(Path('build/tools/ladder-noise-grid/cycle.so')).Coefficients(*knobs, words)
    a, k, g, t, _, _, _, beta, reference, damping, floor = map(Q, map(float, words))
    assert 0 < a < 1 and 0 < k < 4 and 0 < t < 1 and 0 < g < 2
    assert 0 < beta < 1 and 0 < damping*reference < 1 and floor > Q(1, 2**27)

    # EMA comparison against v=0, using monotonicity of each stock operation in
    # the nonnegative rounded detector square v. The four-rounding estimate
    # below also covers a fused multiply-add for the final update.
    loss = 1-(1-u)*(1-beta*(1+u)**2)
    pad = 4*tiny
    decay = Q(1, 3500)
    assert loss < decay and pad < decay*floor
    # Thus I' >= (1-decay)I-pad and likewise for F. Since
    # (1-damping*reference)+damping < 2, both A and H decay by at most decay.
    assert 2*pad < decay*floor and 0 < reference < 8 and 0 < damping < 1
    # From H=max(A,(1-damping*reference)*I+floor+damping*F), H>=A>floor.
    # Both are below 2^90 using the finite-stock detector envelope.
    assert 2*10**26*(1+damping)+floor < 2**90
    # Hence any endpoint log ratio is <=117*log(2) < 82.
    assert sum(Q(7,10)**j/Q(d) for j,d in enumerate((1,1,2,6))) > 2
    assert Q(7,10)*117 < 82
    # -log(1-decay) <= decay/(1-decay).
    log_step = decay/(1-decay)

    # The stock saturator saturates the exact rational at +/-3. Its derivative
    # on [-3,3] is (x^2-9)^2/(9*(x^2+3)^2), hence lies in [0,1].
    # The invariant |s4|<=radius uses |Saturate(s3)|<= (1+u)/t + tiny.
    radius = Q('1.073')
    saturated = (1+u)/t+tiny
    stage_error = rounded_error(radius, rounded_error(radius, radius*rounded_error(1)))
    assert radius-a*(radius-saturated)+stage_error < radius
    assert 0 < saturated < radius
    # For the exact rational, multiplication by the input gain contributes
    # g*radius*u+tiny by its Lipschitz bound. On |x|<=3 the positive sum factors
    # make the remaining rational evaluation relative-error bounded. Values
    # with |x|<2^-50 have at most 2^-46 absolute error by direct magnitude.
    rel_sat = (1+u)**8/(1-u)**5-1
    sat_error = ((g*radius*u+tiny)+rel_sat+Q(1, 2**46))/t
    assert g*radius*(1+u)+tiny < 3

    # Compare rounded gain to K*A/H. Error in R*I shifts the rectified,
    # floor-normalized excess by at most u*R; subsequent rounding of the
    # difference, denominator, divisions, multiply and positive sum is
    # relative to the resulting positive denominator. The added absolute
    # allowance covers subnormal roundings, including division by the floor.
    rel_gain = (1+u)**9/(1-u)**9-1
    denominator_error = (1+rel_gain)*(damping*u*reference+32*tiny/floor)+rel_gain
    gain_error = k*denominator_error/(1-denominator_error)
    assert denominator_error < Q(1, 1000)
    output_error = (1+k)*sat_error+radius*gain_error+rounded_error((1+k)*radius, k*u*radius+4*tiny)
    static_step = (1+k)*g/t*(2*a*radius + stage_error)+2*output_error
    assert static_step < Q('0.132')
    return k, 1/t, static_step, log_step


def difference_energy(m, k, sat, static_step, log_step):
    # For a sequence L, TV(L)=2*sum of its downward steps + L_end-L_start.
    # For log(A) and log(H) the downward step is bounded by log_step.
    # As exp is 1-Lipschitz on (-inf,0], TV(k) <= K*(TV(log A)+TV(log H)).
    # |delta k|<=K bounds its squared variation by K times TV(k).
    change2 = k*k*min(Q(1), 4*log_step+Q(164,m))
    return (static_step+sat*sqrt_upper(change2))**2


def cap_bands(root, knobs):
    check_analyzer(root, knobs)
    k, sat, static_step, log_step = stock_slew(knobs)
    coefficients = root.parent.parent/'physics-v1/analysis-coefficients.npz'
    with np.load(coefficients) as z:
        h, h_scale = dyadic(z['first'])
        b, b_scale = dyadic(z['second'])
    expanded = np.zeros(4*(len(b)-1)+1, dtype=object)
    expanded[::4] = b
    banks = {}
    for stride, taps, scale in [(4,h,h_scale), (32,np.convolve(h,expanded),h_scale*b_scale)]:
        # For the l2 difference of decimated output, apply the FIR to each of
        # stride offsets of native first differences. The FIR work in one
        # frame touches at most m consecutive differences (pad to m).
        m = FFT*stride+len(taps)
        budget = difference_energy(m,k,sat,static_step,log_step)
        center = len(taps)-1
        corr = np.correlate(taps,taps,'full')
        lags = np.arange(-(center//stride),center//stride+1)*stride
        norm2 = Q(sum(abs(corr[center+lags])),scale**2)
        l1 = Q(sum(abs(taps)),scale)
        per_frame = stride*sqrt_upper(norm2*m*budget)

        # Also aggregate differences over one whole record. For each of
        # stride offsets, commute the ideal Hann to native time about the
        # FIR center. The squared native Hann norm is 1536*stride.
        # Where frames overlap by half a window, their squared Hann values
        # add to at most one. The bass frames do not overlap.
        epochs = NATIVE//EPOCH
        assert epochs*EPOCH == NATIVE
        frames = epochs*(15 if stride == 4 else 1)
        span = NATIVE+len(taps)+stride
        gain_energy = k*k*(4*span*log_step+164)/frames
        native_norm = static_step*sqrt_upper(Q(1536*stride))+sat*sqrt_upper(gain_energy)
        moment = Q(sum(abs(taps)*abs(np.arange(len(taps))-center//2)),scale)
        commute = Q(22,7*FFT*stride)*moment*(static_step+sat*k)*sqrt_upper(Q(FFT))
        per_record = stride*(sqrt_upper(norm2)*native_norm+commute)
        norm = min(per_frame,per_record)
        # The periodic ideal Hann is zero at j=0, including the wrap where
        # the telescoping difference would use a nonlocal sample.
        # Δ(wz)_j = w_j Δz_j + Δw_j z_(j-1), |Δw|<=pi/FFT.
        norm += Q(22,7*FFT)*sqrt_upper(Q(FFT))*l1*5
        # Bound the DFT error relative to the ideal Hann via the previous
        # per-bin arithmetic budget, plus stored-window mismatch.
        bin_error = Q(1,10**6)+Q(1,10**7)*FFT*l1*5
        banks[stride] = norm, bin_error
    caps = []
    for i,(part,_) in enumerate(BAND_WEIGHTS):
        rate, stride, offset = (6000,32,2049) if i<60 else (48000,4,0)
        norm, bin_error = banks[stride]
        first = part.start-offset
        frequency_gain = 2*Q(float(sin_pi(np.array(first), FFT)[0]))
        assert frequency_gain > 0
        # Positive bins and their mirrored negative bins together use at most
        # the full Parseval norm. Dividing by the smallest difference gain
        # bounds the tapered band norm even for fractional bin weights <= df.
        signal = sqrt_upper(Q(FFT,2))*norm/frequency_gain
        error = sqrt_upper(Q(part.stop-part.start))*bin_error
        caps.append(min(Q(40), 2*(signal+error)**2/(FFT*1536)))
    return caps


def support_obstruction(witness_path, wet, dry_upper, clipped_upper):
    # The repeated admissible 46.875 Hz word attains this ratio under the
    # checked wet-block analyzer. Verify the conclusion with rational bounds.
    witness = json.loads(witness_path.read_text())
    j = 14
    assert witness['checked_fft'] and witness['worst_band'] == j
    assert witness['frequency_hz'] == 46.875 and 46 < witness['worst_band_hz'] < 47
    edges = list(map(Q, map(float, EDGES)))
    width = edges[j+1]-edges[j]
    measured = Q(witness['max_ratio'])
    radius = sqrt_upper(2*width/(10**12*6000*1536*Q(float(wet[j]))))
    assert measured < 117**2
    witness_lower = 13574
    assert measured-234*radius-radius**2-Q(1,10**6) > witness_lower
    ratio = Q(float(clipped_upper[j]))+Q(6,1000)*(witness_lower-2)
    # 19.16 dB is 10*479/250. Check it without evaluating logarithms.
    assert ratio**250 > 10**479
    # Both curves lie above the display floor on the two segments adjacent
    # to the witness. At the other two endpoints, the difference is >=0.
    for i in range(j-1,j+2):
        assert Q(float(wet[i])) > Q(dry_upper[i])/10**6
        assert clipped_upper[i] >= 1
    # The normalized log span between the outer band centers is greater than
    # 0.01672 = 209/12500. The span is half the log of this edge product ratio.
    span_ratio = edges[j+1]*edges[j+2]/(edges[j-1]*edges[j])
    assert span_ratio**6250 > (edges[-1]/edges[0])**209
    # Integrating each linear segment against its triangular lower bound gives
    # one third of its log span times the squared peak.
    assert Q('19.16')**2*Q('0.01672')/3 > Q('1.43')**2
    # Even granting both 1,024-history audits as independent, zero hits do
    # not exclude a 0.002 event at the remaining 1.5% failure budget.
    optimistic_mass = Q(2,1000)
    assert (1-optimistic_mass)**2048 > Q(15,1000)
    optimistic_ratio = Q(float(clipped_upper[j])) + optimistic_mass*(witness_lower-2)
    assert optimistic_ratio**1000 > 10**1449
    assert Q('14.49')**2*Q('0.01672')/3 > Q('1.08')**2
    return dict(witness_band=j, witness_ratio_lower=witness_lower,
                display_knot_error_db_lower=19.16, whole_rms_db_lower=1.43,
                pooled_zero_hit_count=2048, pooled_failure_budget=.015,
                pooled_display_knot_error_db_lower=14.49, pooled_whole_rms_db_lower=1.08)


def main(root, output, witness):
    with np.load(root/'confirmation.npz') as z:
        assert bool(z['checked_fft'])
        caps = cap_bands(root,z['knobs'])
    with np.load(root.parent/'survivor-replay-250/fresh-parents.npz') as z:
        wet = (z['base']+z['delta'].sum(1)).mean(0)[0]
    dry_report = json.loads((root.parent/'survivor-replay-250/dry-power.json').read_text())
    dry = np.sqrt(np.array(dry_report['dry_power_lower'])*dry_report['dry_power_upper'])
    confirmation = json.loads((root/'confirmation-bounds.json').read_text())
    upper = np.array(confirmation['band_capped_mean_upper_multiple'])
    lower = np.array(confirmation['band_mean_lower_multiple'])
    cap = np.array(list(map(up_float,caps)))
    ref = 10*np.log10(wet/dry)
    raw = upper+.006*(cap/wet-2)
    assert np.all(cap >= 2*wet)
    rms = np.hypot(one_sided_rms(ref,ref+10*np.log10(lower),False),
                   one_sided_rms(ref,ref+10*np.log10(raw),True))
    data = dict(band_edges_hz=EDGES.tolist(), band_power_caps=cap.tolist(),
                exact_caps=[str(x) for x in caps], bands_improved=int(sum(x < 40 for x in caps)),
                smallest_band_cap=float(cap.min()),
                largest_cap_to_anchor_ratio=float((cap/wet).max()),
                raw_finite_whole_rms_estimate=float(rms))
    if witness is not None:
        first = json.loads((root/'mean-bounds.json').read_text())
        assert first['seed'] != confirmation['seed']
        assert all(r['histories'] == 1024 and r['observed_cap_hits'] == 0 and
                   r['tail_cutoff_multiple'] == 2 for r in (first,confirmation))
        data['support_obstruction'] = support_obstruction(witness, wet, dry_report['dry_power_upper'], upper)
    output.parent.mkdir(parents=True,exist_ok=True)
    atomic_json(output,data)
    print({key:value for key,value in data.items() if not isinstance(value,list)})


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('root',type=Path)
    parser.add_argument('output',type=Path)
    parser.add_argument('--checked-witness',type=Path)
    args = parser.parse_args()
    main(args.root,args.output,args.checked_witness)
