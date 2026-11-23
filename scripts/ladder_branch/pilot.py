"""Trace the input-periodic orbit from zero resonance using full-state shooting."""
import argparse
from concurrent.futures import ThreadPoolExecutor, Future
from queue import Queue, Empty
import threading
import time
import csv
import ctypes as ct
import json
import math
import shlex
import struct
import subprocess
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[2]
ACTIVE = np.r_[0:4, 9:14]
RATE = 192000
AMPLITUDE = 10 ** (-18 / 20)
PTR = np.ctypeslib.ndpointer(dtype=np.float64, ndim=1, flags="C_CONTIGUOUS")
LIB = ct.CDLL(str(ROOT / "build/tools/ladder-branch/reference.so"))
LIB.ReturnMap.argtypes = [ct.c_double] * 3 + [PTR, ct.c_int, ct.c_int, PTR, PTR]
LIB.Coefficients.argtypes = [ct.c_double] * 3 + [PTR]
LIB.ReturnMap.restype = LIB.Coefficients.restype = None


class Tone:
    def __init__(self, q, cutoff, drive, cycles=1):
        self.q = q
        self.cycles = cycles
        self.cutoff = cutoff
        self.drive = drive
        self.phase = np.exp(-2j * np.pi * ((np.arange(q) * cycles) % q) / q)
        self.input = np.ascontiguousarray(AMPLITUDE * self.phase.real)
        self.output = np.empty(q)
        self.set_resonance(0)
        self.anchor = self.period(np.zeros(14), max(2, math.ceil(42 / (self.coeffs[7] * q))))

    def set_resonance(self, r):
        self.params = self.cutoff / RATE, np.expm1(np.log(49) * r) / 48, .25 * 20 ** self.drive
        self.coeffs = np.empty(11)
        LIB.Coefficients(*self.params, self.coeffs)

    def period(self, initial, periods=1):
        state = initial.copy()
        LIB.ReturnMap(*self.params, self.input, self.q, periods, state, self.output)
        return state

    def scales(self, z):
        scale = np.maximum(.025, abs(z[ACTIVE]))
        scale[-1] = max(1e-8, abs(z[8]), abs(z[-1]))
        return scale

    def double_jacobian(self, z, scale):
        jac = np.empty((9, 9))
        for j, i in enumerate(ACTIVE):
            dz = np.zeros(14)
            dz[i] = 2e-5 * scale[j]
            jac[:, j] = ((self.period(z + dz) - self.period(z - dz))[ACTIVE] / scale) / 4e-5
        return jac


class Metal:
    """One persistent binary protocol stream allows the solver to run locally or over SSH."""
    def __init__(self, command):
        self.process = subprocess.Popen(shlex.split(command), stdin=subprocess.PIPE, stdout=subprocess.PIPE)
        if self.process.stdout.read(4) != b"LRP1":
            raise RuntimeError("Metal evaluator did not start")

        self.jobs = Queue()
        self.worker = threading.Thread(target=self.dispatch)
        self.worker.start()

    def dispatch(self):
        while True:
            first = self.jobs.get()
            if first is None:
                return
            batch = [first]
            deadline = time.monotonic() + .001
            while len(batch) < 128:
                try:
                    batch.append(self.jobs.get(timeout=max(0, deadline - time.monotonic())))
                except Empty:
                    break
            try:
                packet = struct.pack("<I", len(batch)) + b"".join(job[0] for job in batch)
                self.process.stdin.write(packet)
                self.process.stdin.flush()
                data = self.process.stdout.read(360 * len(batch))
                if len(data) != 360 * len(batch):
                    raise RuntimeError("Metal evaluator stopped during a period")
                for i, (_, future) in enumerate(batch):
                    future.set_result(data[360*i:360*(i+1)])
            except Exception as error:
                for _, future in batch:
                    future.set_exception(error)

    def jacobian(self, tone, z, scale):
        request = np.r_[tone.coeffs, z, tone.input].astype("<f4").tobytes()
        future = Future()
        self.jobs.put((struct.pack("<I", tone.q) + request, future))
        data = future.result()
        rows = np.frombuffer(data, dtype="<f4").reshape(10, 9).astype(float)
        return rows[1:].T * scale[None, :] / scale[:, None], rows[0]

    def close(self):
        self.jobs.put(None)
        self.worker.join()
        self.process.stdin.close()
        if self.process.wait() != 0:
            raise RuntimeError("Metal evaluator failed")


def solve(tone, start, metal):
    z = start.copy()
    for iteration in range(14):
        scale = tone.scales(z)
        end = tone.period(z)
        residual = (end - z)[ACTIVE] / scale
        if metal:
            jac, _ = metal.jacobian(tone, z, scale)
        else:
            jac = tone.double_jacobian(z, scale)
        if not np.isfinite(jac).all():
            return None
        mat = np.eye(9) - jac
        try:
            delta = np.linalg.solve(mat, residual)
        except np.linalg.LinAlgError:
            return None
        if np.max(abs(residual)) < 2e-9 and np.max(abs(delta)) < 2e-8:
            return z, iteration, float(max(abs(residual)))
        if iteration == 13:
            return None
        for factor in (1., .5, .25, .125, .0625):
            trial = z.copy()
            trial[ACTIVE] += factor * delta * scale
            if trial[-1] < 0:
                continue
            gap = (tone.period(trial) - trial)[ACTIVE] / scale
            if np.max(abs(gap)) < np.max(abs(residual)):
                z = trial
                break
        else:
            return None


def diagnostics(tone, z, metal):
    scale = tone.scales(z)
    jac = tone.double_jacobian(z, scale)
    mat = np.eye(9) - jac
    singular = np.linalg.svd(mat, compute_uv=False)
    end = tone.period(z)
    residual = np.max(abs(end - z))
    power = np.mean(tone.output ** 2)
    phasor = 2 * np.mean(tone.output * tone.phase)
    result = dict(state=z.tolist(), full_return_residual=float(residual), total_power=float(power),
                  fundamental_power=float(abs(phasor)**2 / 2), fundamental_real=float(phasor.real), fundamental_imag=float(phasor.imag), phase_rad=float(np.angle(phasor)),
                  rho_wet=float(max(abs(np.linalg.eigvals(jac)))), sigma_min=float(singular[-1]),
                  cond=float(singular[0] / singular[-1]), base_feedback=float(tone.coeffs[1]))
    if metal:
        gpu_jac, gpu_end = metal.jacobian(tone, z, scale)
        result['metal_jac_relative_inf'] = float(np.linalg.norm(gpu_jac - jac, np.inf) / max(1, np.linalg.norm(jac, np.inf)))
        result['metal_return_scaled_inf'] = float(max(abs((gpu_end - end[ACTIVE]) / scale)))
    return result


def trace(tone, spacing, targets, metal, record):
    accepted = []
    r = 0.
    z = tone.anchor.copy()
    prev = None
    prev_r = None
    output = {}
    step = spacing
    failures = 0
    attempts = 0
    for target in targets:
        while r < target - 1e-13:
            attempts += 1
            if attempts > 600:
                record(dict(kind="stalled", reason="attempt_budget", r=r, target=target, state=z.tolist(), steps=accepted))
                return output
            following = min(r + step, target)
            prediction = z.copy() if prev is None else z + (z - prev) * ((following - r) / (r - prev_r))
            tone.set_resonance(following)
            solved = solve(tone, prediction, metal)
            deviation = (np.max(abs((solved[0] - prediction)[ACTIVE]) / tone.scales(z))
                         if solved else float("inf"))
            if deviation > .16:
                failures += 1
                step = (following - r) / 2
                if step < spacing / 1024:
                    record(dict(kind="stalled", reason="minimum_step", r=r, attempted=following, target=target, rejections=failures, state=z.tolist(), steps=accepted))
                    return output
                continue
            prev_r, prev, r, z = r, z, following, solved[0]
            accepted.append(dict(r=r, iterations=solved[1], prediction_error=deviation, scaled_residual=solved[2], state=z.tolist()))
            step = min(spacing, step * 1.5)
        tone.set_resonance(target)
        row = diagnostics(tone, z, metal)
        row.update(kind="target", r=target, steps=accepted, rejections=failures)
        record(row)
        output[target] = z.copy()
        accepted = []
    return output


def frequency(target):
    # Up to eight cycles in the repeated input, within the kernel buffer limit.
    pairs = [(round(RATE * p / target), p) for p in range(1, 9)]
    q, p = min(((q, p) for q, p in pairs if 3 <= q <= 10000), key=lambda pair: abs(RATE * pair[1] / pair[0] - target))
    divisor = math.gcd(q, p)
    return q // divisor, p // divisor


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("output", type=Path)
    ap.add_argument("--suite", choices=["stress", "grid", "frequency", "frequency-edges"], default="stress")
    ap.add_argument("--grid-size", type=int, default=4)
    ap.add_argument("--previous-frequency-table", type=Path)
    ap.add_argument("--metal-command")
    ap.add_argument("--workers", type=int, default=4)
    args = ap.parse_args()
    metal = Metal(args.metal_command) if args.metal_command else None
    if args.suite == "stress":
        paths = [(q, 1, RATE/q, 20 * 1000 ** (56 / 99), 90 / 99, [0, r / 99]) for q, r in [(18, 17), (180, 81)]]
    elif args.suite == "frequency":
        cutoff, drive = 20 * 1000**(56/99), 90/99
        alpha = Tone(180, cutoff, drive).coeffs[0]
        oscillation = RATE * math.asin(alpha / (2*(1-alpha))) / math.pi
        # Exact one-cycle buffers, with both meshes anchored at zero resonance for each frequency.
        qs = {180}
        for divisor in (1, 3):
            middle = RATE * divisor / oscillation
            qs.update(round(middle * 2**offset) for offset in np.linspace(-.32, .32, 33))
        paths = [(q, 1, RATE/q, cutoff, drive, [0., 81/99]) for q in sorted(qs)]
    elif args.suite == "frequency-edges":
        if not args.previous_frequency_table:
            ap.error("frequency-edges requires a previous frequency table")
        with args.previous_frequency_table.open() as f:
            old=sorted(csv.DictReader(f),key=lambda r:int(r["period"]))
        qs=set()
        for a,b in zip(old,old[1:]):
            qa,qb=int(a["period"]),int(b["period"])
            if qb/qa<1.1 and a["total_gain_db"] and b["total_gain_db"] and abs(float(a["total_gain_db"])-float(b["total_gain_db"]))>3:
                qs.update(range(qa+1,qb))
        paths=[(q,1,RATE/q,20*1000**(56/99),90/99,[0.,81/99]) for q in sorted(qs)]
    else:
        n = args.grid_size
        if n < 2:
            raise ValueError("grid size must be at least two")
        paths = [(*frequency(f), f, fc, d, np.linspace(0, 1, n).tolist()) for f in np.geomspace(20, 20000, n)
                 for fc in np.geomspace(20, 20000, n) for d in np.linspace(0, 1, n)]
    with args.output.open("x", buffering=1) as out:
        lock = threading.Lock()
        def emit(data):
            with lock:
                print(json.dumps(data, allow_nan=False), file=out)
        def run_path(path):
            q, cycles, target_hz, cutoff, drive, targets = path
            base = dict(q=q, cycles=cycles, target_hz=target_hz, actual_hz=RATE*cycles/q, cutoff_hz=cutoff, drive=drive, amplitude=AMPLITUDE)
            solutions = []
            for spacing in [1 / 50, 1 / 100]:
                tone = Tone(q, cutoff, drive, cycles)
                solutions.append(trace(tone, spacing, targets, metal,
                                       lambda row: emit(dict(**base, spacing=spacing, **row))))
            for target in sorted(solutions[0].keys() & solutions[1].keys()):
                a, b = (solutions[i][target] for i in (0, 1))
                err = max(abs(a[ACTIVE] - b[ACTIVE]) / tone.scales(b))
                emit(dict(**base, kind="mesh_check", r=target, state_scaled_inf=float(err)))
            print(f"q={q} fc={cutoff:.3f} drive={drive:.3f}: {[len(s) for s in solutions]}", flush=True)
        try:
            with ThreadPoolExecutor(max_workers=args.workers) as pool:
                list(pool.map(run_path, paths))
        finally:
            if metal:
                metal.close()


if __name__ == "__main__":
    main()
