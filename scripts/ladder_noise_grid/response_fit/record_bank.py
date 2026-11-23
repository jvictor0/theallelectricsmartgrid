"""Enclose a whole long-run stock spectrum for an iid finite bank of noise records."""
import argparse
import ctypes as ct
from concurrent.futures import ThreadPoolExecutor
from fractions import Fraction
from hashlib import sha256
from pathlib import Path
import sys

import numpy as np
from scipy.signal import upfirdn

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from display_metric import curve_mse
from survey import BAND_WEIGHTS, EDGES, EPOCH, F32, LOW, N, atomic_json, make_noise
from stress import SETTINGS
from cap_certificate import MODEL_HASH, at
from record_bank_check import check


def bind(library):
    lib = ct.CDLL(str(library.resolve()))
    for name, arguments, result in (
        ('CreateCycleFilter', [ct.c_double] * 3, ct.c_void_p),
        ('CopyCycleFilter', [ct.c_void_p], ct.c_void_p),
        ('DestroyCycleFilter', [ct.c_void_p], None),
        ('ReadCycleState', [ct.c_void_p, np.ctypeslib.ndpointer(np.uint32)], None),
        ('RunCycleRecord', [ct.c_void_p, ct.c_int, F32, F32], None),
        ('Reference', [ct.c_double] * 3 + [ct.c_int, F32, F32], None),
        ('Coefficients', [ct.c_double] * 3 + [F32], None),
    ):
        fn = getattr(lib, name)
        fn.argtypes, fn.restype = arguments, result
    return lib


TAIL = 1024  # 256 + 4*128 native samples of causal FIR memory, rounded up to an epoch divisor.


def bands(samples, prefix, analyzer):
    first, second, window = analyzer
    x = np.concatenate((prefix, samples)).astype(np.float64)
    host = upfirdn(first, x, down=4)[:len(x)//4]
    bass = upfirdn(second, host, down=8)[:len(x)//32][TAIL//32:]
    host = host[TAIL//4:].reshape(-1, EPOCH//4)
    frames = np.lib.stride_tricks.sliding_window_view(host, N, axis=-1)[:, ::N//2]
    high = (abs(np.fft.rfft(frames*window))**2).mean(axis=(0, 1)) * (2/(48000*1536))
    low = (abs(np.fft.rfft(bass.reshape(-1, N)*window))**2).mean(axis=0)[:LOW] * (2/(6000*1536))
    high[[0, -1]] *= .5
    low[0] *= .5
    psd = np.r_[high, low]
    return np.array([np.dot(psd[part], weights) for part, weights in BAND_WEIGHTS])


def power_enclosure(edges, rewards, steps=256):
    """Integer outward rounding of P^m r, simultaneously for wet and dry columns."""
    largest = np.max(rewards, axis=(0, 1))
    if not np.all(np.isfinite(rewards)) or np.any(rewards < 0) or np.any(largest <= 0):
        raise ValueError('Invalid spectrum')
    nletters = edges.shape[1]
    if nletters > 128:
        raise ValueError('Int64 headroom allows up to 128 letters')
    exponent = np.frexp(largest)[1] - 48
    scaled = np.ldexp(rewards, -exponent)
    def mean(lo, hi):
        return lo.sum(axis=1)//nletters, (hi.sum(axis=1)+nletters-1)//nletters
    lo, hi = mean(np.floor(scaled).astype(np.int64), np.ceil(scaled).astype(np.int64))
    for _ in range(steps):
        lo, hi = mean(lo[edges], hi[edges])
    return np.ldexp(lo.min(axis=0).astype(float), exponent), np.ldexp(hi.max(axis=0).astype(float), exponent)


def solve(lib, settings, records, dry, analyzer, limit):
    # Equality uses bytes, including signed zero. Every queued node is reached from reset.
    zero = np.zeros(TAIL, np.float32)
    pending = [(lib.CreateCycleFilter(*settings), -1, zero)]
    keys = {}
    transitions, powers = [], []
    y = np.empty_like(records[0])
    bits = np.empty(14, np.uint32)
    size = len(records)
    try:
        for node, (f, previous, prefix) in enumerate(pending):
            row, level = [], []
            for letter, x in enumerate(records):
                child = lib.CopyCycleFilter(f)
                try:
                    lib.RunCycleRecord(child, len(x), x, y)
                    if node == 0:
                        ref = np.empty_like(y)
                        lib.Reference(*settings, len(x), x, ref)
                        if not np.array_equal(ref.view('u4'), y.view('u4')):
                            raise ArithmeticError('Reset edge does not match stock Reference')
                    lib.ReadCycleState(child, bits)
                    if not np.isfinite(bits.view('f4')).all() or not np.isfinite(y).all():
                        raise ArithmeticError('Nonfinite stock result')
                    key = (letter, bits.tobytes(), y[-TAIL:].tobytes())
                    dest = keys.get(key)
                    if dest is None:
                        if len(pending) >= limit:
                            return dict(closed=False, states=len(pending), examined_nodes=node, bank=size), None
                        dest = len(pending)
                        keys[key] = dest
                        pending.append((child, letter, y[-TAIL:].copy()))
                        child = None
                    row.append(dest)
                    level.append(np.r_[bands(y, prefix, analyzer), dry[previous, letter]])
                finally:
                    if child is not None:
                        lib.DestroyCycleFilter(child)
            transitions.append(row)
            powers.append(level)
            lib.DestroyCycleFilter(f)
            pending[node] = (None, previous, None)
    finally:
        for f, _, _ in pending:
            if f is not None:
                lib.DestroyCycleFilter(f)
    edges, rewards = np.asarray(transitions), np.asarray(powers)
    lo, hi = power_enclosure(edges, rewards)
    wet_lo, dry_lo = lo.reshape(2, -1)
    wet_hi, dry_hi = hi.reshape(2, -1)
    if np.any(lo <= 0):
        raise ArithmeticError('Insufficient depth to resolve a positive lower bound')
    lower = 10*np.log10(wet_lo/dry_hi)
    upper = 10*np.log10(wet_hi/dry_lo)
    coeff = np.empty(11, np.float32)
    lib.Coefficients(*settings, coeff)
    summary = dict(closed=True, states=len(edges), edges=edges.size, bank=size,
                   ks3=float(coeff[1])*(float(coeff[2])/float(coeff[3]))**3,
                   max_bracket_width_db=float(np.max(upper-lower)))
    boundary_bits = np.zeros((len(edges), 14), dtype='uint32')
    wet_tails = np.zeros((len(edges), TAIL), dtype='uint32')
    last_letter = np.full(len(edges), -1)
    for (letter, state, tail), j in keys.items():
        boundary_bits[j] = np.frombuffer(state, dtype='uint32')
        wet_tails[j] = np.frombuffer(tail, dtype='uint32')
        last_letter[j] = letter
    arrays = dict(edges=edges, rewards=rewards, lo=lo, hi=hi, boundary_bits=boundary_bits,
                  wet_tails=wet_tails, last_letter=last_letter, lower_db=lower, upper_db=upper)
    return summary, arrays


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('results', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--library', type=Path, default=Path('build/tools/ladder-noise-grid/cycle.so'))
    parser.add_argument('--seed', type=int, default=20261122)
    parser.add_argument('--bank', type=int, default=2)
    parser.add_argument('--limit', type=int, default=256)
    parser.add_argument('--settings', type=int, nargs='+', default=[0, 1, 2],
                        help='0–3 use the stress settings, 4 covers the capped resonance face')
    args = parser.parse_args()
    if not 1 <= args.bank <= 128 or args.limit < 1:
        parser.error('Invalid bank or state limit')
    lib = bind(args.library)
    records = make_noise(args.seed, args.bank, 14).transpose(1, 0, 2).copy().reshape(args.bank, -1)
    with np.load(args.results/'physics-v1/analysis-coefficients.npz') as z:
        analyzer = [z[k].astype(float) for k in ('first', 'second', 'window')]
    with np.load(args.results/'screen-v3/physical-stress/curves.npz') as z:
        fits, long_run = z['fitted'], z['measured']
        np.testing.assert_array_equal(z['settings'], SETTINGS)
    args.output.mkdir(parents=True, exist_ok=True)
    tails = [x[-TAIL:] for x in records] + [np.zeros(TAIL, np.float32)]
    dry = np.array([[bands(x, tail, analyzer) for x in records] for tail in tails])
    face = [66/72, 1., 20*1000**(355/360)]
    def run(j):
        p = face if j == 4 else SETTINGS[j].tolist()
        row, arrays = solve(lib, p, records, dry, analyzer, args.limit)
        row.update(setting=p, seed=args.seed,
                   record_sha256=[sha256(x.view(np.uint8)).hexdigest() for x in records])
        if arrays is not None:
            row.update(check(*(arrays[k] for k in ('edges', 'rewards', 'lo', 'hi'))))
            curve = (arrays['lower_db']+arrays['upper_db'])/2
            def rms(reference):
                return float(np.sqrt(curve_mse(curve[None], reference[None], EDGES)[0]))
            if j == 4:
                # The stock cap and all-visible frozen fit over this slab are certified
                # by CapAudit.cpp and cap_certificate.py. At fixed drive and cutoff the
                # signed error is affine between resonance knots, and its norm is convex.
                model = args.results/'screen-v3/display-model.npz'
                if sha256(model.read_bytes()).hexdigest() != MODEL_HASH:
                    raise ValueError('Wrong frozen model')
                with np.load(model) as z:
                    knots = sorted({57, 72, *(int(r) for r in z['ra'] if r >= 57)})
                    lines = [np.array(at(z, Fraction(66), Fraction(r), Fraction(355)), float) for r in knots]
                if min(np.min(q) for q in lines) <= -60 or np.min(arrays['lower_db']) <= -60:
                    raise ValueError('A floor crossing requires the general clipped-cell bound')
                c0, c1 = np.zeros(11, np.float32), np.zeros(11, np.float32)
                lib.Coefficients(p[0], 57/72, p[2], c0)
                lib.Coefficients(*p, c1)
                np.testing.assert_array_equal(c0.view('u4'), c1.view('u4'))
                row.update(resonance_interval=[57/72, 1.], resonance_knots=knots,
                           max_knot_rms_db=max(map(rms, lines)), rms_from_fit_db=rms(lines[-1]))
            else:
                row.update(rms_from_fit_db=rms(fits[j]), rms_from_64_long_records_db=rms(long_run[j]))
            np.savez_compressed(args.output/f'setting-{j}.npz', **arrays)
        atomic_json(args.output/f'setting-{j}.json', row)
        print(j, row, flush=True)
    with ThreadPoolExecutor(min(4, len(args.settings))) as pool:
        list(pool.map(run, args.settings))


if __name__ == '__main__':
    main()
