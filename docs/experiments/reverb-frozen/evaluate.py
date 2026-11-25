"""Deterministic frozen RMS response of the 192 kHz quad reverb."""
import argparse
import csv
import json
from pathlib import Path

import numpy as np
from numpy.polynomial.legendre import leggauss
from scipy.stats import qmc

OUT = Path(__file__).resolve().parents[3] / "build" / "results" / "reverb-magnitude"
FS = 192_000
DEPTH = 128
APF_GAIN = 0.6
INTRO_TAPS = np.array([293, 487, 907, 1433, 4493, 6899, 9811, 14033])
TANK_TAPS = np.array([[2039, 3719, 6359, 2753], [11923, 16879, 9041, 23447]]).ravel()
WIDENERS = np.array([0.978615137775, 1.0, 0.986803398875, 0.989701964951])
TERMS = (1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64, 96, 128)
SETTINGS = (
    ("Short / dark", 8, 0.65, 180, 3500),
    ("Medium", 40, 0.80, 90, 9000),
    ("Long / wide", 160, 0.80, 55, 14000),
    ("High feedback", 40, 0.90, 90, 9000),
)
SAT_GAIN = 0.5
DB_TOL = 0.5


def rotation(i, j, c, s):
    result = np.eye(4)
    result[i, i] = result[j, j] = c
    result[i, j], result[j, i] = -s, s
    return result


MIX = (rotation(0, 2, 55/73, -48/73) @ rotation(1, 3, 65/97, 72/97)
       @ rotation(0, 1, 21/29, 20/29) @ rotation(2, 3, 45/53, 28/53))
assert np.max(np.abs(MIX.T @ MIX - np.eye(4))) < 1e-14


def knot_weights(t):
    return np.array([(1-t)**3, 4-6*t*t+3*t**3,
                     1+3*t+3*t*t-3*t**3, t**3]) / 6


def lfo_cdf(phase_order, minor_order, grid=4097):
    """Integrate the CDF of two weighted uniforms over phase and two smaller knots."""
    phase, phase_w = leggauss(phase_order)
    minor, minor_w = leggauss(minor_order)
    minor, minor_w = (minor + 1)/2, minor_w/2
    left_grid = np.linspace(0, 0.5, grid)
    left_cdf = np.zeros_like(left_grid)
    for t, weight in zip((phase + 1)/4, phase_w/2):
        small1, small2, b, a = np.sort(knot_weights(t))
        shift = (small1*minor[:, None] + small2*minor[None, :]).ravel()
        w = weight * (minor_w[:, None]*minor_w[None, :]).ravel()
        mass_position = left_grid - shift[:, None]
        cdf = (np.clip(mass_position, b, a)-b/2)/a
        tail = np.minimum(np.maximum(mass_position-a, 0), b)
        cdf += -b/(2*a) + np.minimum(np.maximum(mass_position, 0), b)**2/(2*a*b)
        cdf += tail/a - tail*tail/(2*a*b)
        left_cdf += np.sum(w[:, None] * cdf, axis=0)
    left_cdf[0], left_cdf[-1] = 0, 0.5
    assert np.min(np.diff(left_cdf)) >= -1e-13, np.min(np.diff(left_cdf))
    return np.r_[left_grid, 1-left_grid[-2::-1]], np.r_[left_cdf, 1-left_cdf[-2::-1]]


def cubic_weights(t):
    return np.stack([-t*(1-t)**2/2, 1-5*t*t/2+3*t**3/2,
                     t/2+2*t*t-3*t**3/2, -t*t*(1-t)/2], axis=-1)


def taps(arg):
    whole = np.ceil(arg)
    return whole, cubic_weights(whole-arg)


def allpass(z):
    return (z - APF_GAIN) / (1 - APF_GAIN*z)


def angular_order(z):
    """Order one scalar marginal by output phase, with the cut in its largest gap."""
    angles = np.angle(z)
    indices = np.argsort(angles)
    sorted_angles = angles[indices]
    gap = np.diff(sorted_angles, append=sorted_angles[0] + 2*np.pi)
    return np.roll(indices, -int(gap.argmax())-1)


def damping(w, high_hz, low_hz):
    q = np.exp(-1j*w)
    high_g = np.tan(np.pi * high_hz/FS)
    low_g = np.tan(np.pi * low_hz/FS)
    high = high_g/(1+high_g)
    low = low_g/(1+low_g)
    return ((1-high)*(1-q)/(1+(2*high-1)*q)
            * (low*(1+q)/(1+(2*low-1)*q)))


def db(pow_):
    return 10*np.log10(pow_)


def truncation_budget(w, contraction):
    # Catmull–Rom |tap|² ≥ 1 - delta. The Schroeder map then gives
    # |APF|² ≥ 1 - [(1+g)/(1-g)] delta.
    u = 0.25
    delta = u*u * (1-np.cos(w))**2 * (3+2*u*(1-np.cos(w)))
    a = np.sqrt(1 - (1+APF_GAIN)/(1-APF_GAIN)*delta)
    intro_condition = a**(-len(INTRO_TAPS))
    tol = 1 - 10**(-DB_TOL/20)
    return int(np.ceil(np.log(tol/intro_condition)/np.log(contraction)))


def band_truncation_budget(feedback, high_hz, low_hz):
    # With t = tan(w / 2), the TPT bandpass peaks at t² = k_high k_low.
    # Bound each frequency interval by the peak loop gain over it and the
    # cubic loss at its upper endpoint.
    edges = np.geomspace(20, 20_000, 1025)
    k_high, k_low = np.tan(np.pi * np.array([high_hz, low_hz]) / FS)
    peak_hz = (FS / np.pi) * np.arctan(np.sqrt(k_high * k_low))
    peak_angle = (2 * np.pi / FS) * np.clip(peak_hz, edges[:-1], edges[1:])
    r = np.abs(damping(peak_angle, high_hz, low_hz)) * 1.25 * (9**feedback - 1) / 8
    return max(truncation_budget((2 * np.pi / FS) * hz, gain)
               for hz, gain in zip(edges[1:], r))


def finite_powers(full, ret):
    """Return each truncated H_N by H_N = (I-R^N) H, using shared powers."""
    square = ret @ ret
    cube = square @ ret
    even, triple = ret, cube
    for step in range(8):
        n = 1 << step
        yield n, full-even@full
        if 3*n in TERMS:
            yield 3*n, full-triple@full
        if step != 7:
            even = square if step == 0 else even@even
        if 3*(n*2) in TERMS:
            triple = triple@triple


def rows_power(h):
    return np.sum(np.abs(h)**2, axis=-1) / 4


def frequency_response(args):
    OUT.mkdir(parents=True, exist_ok=True)
    phase_order, minor_order = args.phase_order, args.minor_order
    grid, cdf = lfo_cdf(phase_order, minor_order)
    # Test the scalar law against an independently integrated exact second moment.
    t, weight = leggauss(12)
    exact_var = np.sum(weight*np.sum(knot_weights((t+1)/2)**2, axis=0))/6
    amp_quantiles = (np.arange(1 << args.scalar_power)+0.5)/(1 << args.scalar_power)
    wander = 2*np.interp(amp_quantiles, cdf, grid)-1
    assert abs(np.mean(wander*wander)-exact_var) < 3e-6, (np.mean(wander*wander), exact_var)
    max_power = max(args.points_power, args.near_limit_power)
    sobol = qmc.Sobol(8, scramble=False).random_base2(max_power)
    rank = np.minimum((sobol*len(wander)).astype(int), len(wander)-1).T
    ordinary_nodes = 1 << args.points_power
    high_nodes = 1 << max_power
    age, weights = taps(DEPTH*(1+wander))
    age += 1-DEPTH
    frequency = np.unique(np.r_[np.geomspace(20, 20_000, args.frequencies),
                                np.arange(60, 700, args.low_spacing)])
    main_taps = [taps(ms * FS/1000 * WIDENERS) for _, ms, _, _, _ in SETTINGS]
    full_power = np.empty((len(SETTINGS), len(frequency), 4))
    coarse_power = np.empty_like(full_power)
    finite = np.empty((len(TERMS), *full_power.shape))
    r = np.empty((len(SETTINGS), len(frequency)))
    budget = np.empty_like(r, dtype=int)
    nodes_used = np.empty_like(r, dtype=int)
    drive_only_error = 0.0
    for k, hz in enumerate(frequency):
        w = (2*np.pi/FS)*hz
        phase4 = np.exp(1j*w*np.arange(4))
        q = np.exp(-1j*w)
        base_z = np.exp(-1j*w*age) * np.sum(weights * phase4, axis=1)
        intro = allpass(np.exp(-1j*w*INTRO_TAPS[:, None]) * base_z)
        intro_power = np.prod(np.mean(np.abs(intro)**2, axis=1))
        for j, (_, _, fb, lo, hi) in enumerate(SETTINGS):
            r[j, k] = abs(damping(w, lo, hi))*(1.25*(9**fb-1)/8)
            assert r[j, k] < 1
            budget[j, k] = truncation_budget(w, r[j, k])
        nodes_used[:, k] = np.where(r[:, k] > 0.90, high_nodes,
                                    np.where(r[:, k] > 0.80, max(ordinary_nodes, high_nodes//2),
                                             ordinary_nodes))
        marginals = allpass(np.exp(-1j*w*TANK_TAPS[:, None]) * base_z)
        packed = np.stack([a[angular_order(a)[indices[:nodes_used[:, k].max()]]]
                           for a, indices in zip(marginals, rank)])
        combined = (packed[:4]*packed[4:]).T
        for j, (_, _, fb, lo, hi) in enumerate(SETTINGS):
            damp = damping(w, lo, hi)
            compensated_fb = (1.25*(9**fb-1)/8)/SAT_GAIN
            nodes = nodes_used[j, k]
            main_age, main_weight = main_taps[j]
            main = np.exp(-1j*w*main_age) * np.sum(main_weight*phase4, axis=-1)
            forward = (combined[:nodes]*(damp*SAT_GAIN))[:, :, None] * (MIX*main)
            ret = (q*compensated_fb)*forward
            full = np.linalg.solve(np.eye(4)-ret, forward)
            power = rows_power(full)*intro_power
            full_power[j, k] = power.mean(axis=0)
            coarse_power[j, k] = power[:nodes//2].mean(axis=0)
            if k in (0, len(frequency)//2, len(frequency)-1):
                residual = (np.eye(4)-ret)@full-forward
                assert np.max(np.abs(residual)) < 5e-13
                first_exact = (np.mean(np.abs(marginals[:4])**2, axis=1)
                               * np.mean(np.abs(marginals[4:])**2, axis=1)
                               * np.sum(np.abs(MIX*main)**2, axis=1)
                               * abs(damp*SAT_GAIN)**2 * intro_power / 4)
                drift = np.max(np.abs(db(rows_power(forward).mean(axis=0)*intro_power)-db(first_exact)))
                drive_only_error = max(drive_only_error, drift)
            for n, partial in finite_powers(full, ret):
                finite[TERMS.index(n), j, k] = rows_power(partial).mean(axis=0)*intro_power
                if k == 0 and n == 3:
                    error = partial - (forward+ret@forward+ret@(ret@forward))
                    assert np.max(np.abs(error)) < 5e-13
        if k % 100 == 0:
            print(f"{k}/{len(frequency)} frequencies", flush=True)
    delta = np.max(np.abs(db(finite) - db(full_power[None, ...])), axis=(2, 3))
    quadrant_delta = np.max(np.abs(db(full_power) - db(coarse_power)), axis=(1, 2))
    label = args.label
    np.savez_compressed(OUT/f"data-{label}.npz", frequency=frequency, full_power=full_power,
                        finite=finite, coarse_power=coarse_power, contraction=r, budget=budget, nodes=nodes_used)
    stats = {"frequencies": len(frequency), "quadrature_nodes": len(sobol),
             "minimum_quadrature_nodes": ordinary_nodes, "lfo_variance": float(np.mean(wander*wander)),
             "lfo_exact_variance": float(exact_var), "first_pass_quad_check_db": float(drive_only_error),
             "settings": []}
    for j, (name, ms, fb, lo, hi) in enumerate(SETTINGS):
        continuously_good = np.maximum.accumulate(delta[::-1, j])[::-1] <= DB_TOL
        stats["settings"].append({"name": name, "ms": ms, "feedback_knob": fb, "high_cut_hz": lo,
                                 "low_cut_hz": hi, "max_contraction": float(r[j].max()),
                                 "checked_min_terms": min((n for n, good in zip(TERMS, continuously_good)
                                                           if good), default=None),
                                 "conservative_terms": band_truncation_budget(fb, lo, hi),
                                 "max_refinement_delta_db": float(quadrant_delta[j]),
                                 "max_truncation_delta_db": dict(zip(TERMS, delta[:, j].tolist()))})
    (OUT/f"summary-{label}.json").write_text(json.dumps(stats, indent=2) + "\n")
    with (OUT/f"reverb-magnitude-{label}.csv").open("w", newline="") as stream:
        writer = csv.writer(stream)
        writer.writerow(("setting", "frequency_hz", "output_lane", "full_rms_db",
                         "contraction_envelope", "terms_for_half_db_bound")
                        + tuple(f"rms_db_with_{n}_terms" for n in TERMS))
        for j, (name, *_) in enumerate(SETTINGS):
            for k, hz in enumerate(frequency):
                for lane in range(4):
                    writer.writerow((name, f"{hz:.7f}", lane+1, f"{db(full_power[j,k,lane]):.7f}",
                                     f"{r[j,k]:.7f}", budget[j,k])
                                    + tuple(f"{value:.7f}" for value in db(finite[:,j,k,lane])))
    print(json.dumps(stats, indent=2), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--points-power", type=int, default=13)
    parser.add_argument("--near-limit-power", type=int, default=16)
    parser.add_argument("--scalar-power", type=int, default=15)
    parser.add_argument("--frequencies", type=int, default=431)
    parser.add_argument("--low-spacing", type=float, default=3.0)
    parser.add_argument("--phase-order", type=int, default=64)
    parser.add_argument("--minor-order", type=int, default=12)
    parser.add_argument("--label", default="curves")
    frequency_response(parser.parse_args())
