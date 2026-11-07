"""Run reproducible neutral-delay comparisons and render scientific figures."""
import argparse
import concurrent.futures
import csv
import dataclasses
import html
import json
import math
from pathlib import Path
import subprocess

import numpy as np
from scipy import signal
from scipy.io import wavfile

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

RATE = 48000
ONSET = 8192
MODES = ("conventional", "oracle", "current", "schmitt", "floor60")
COLORS = {"conventional": "#252a34", "current": "#d24c40", "schmitt": "#147d92", "oracle": "#9583b4", "floor60": "#ce9d30"}
LABELS = {"conventional": "Conventional", "current": "Current PVDR", "schmitt": "Schmitt snap", "oracle": "Copy analysis phase", "floor60": "−60 dB floor only"}


@dataclasses.dataclass
class Case:
    id: str
    signal: str
    hz: float
    note: int
    attack: int
    delay: int
    feedback: float
    pattern: str
    ratio: float
    comb: str
    seed: int = 104729


def cases():
    result = []
    for name, hz, kind in (("onbin", 750., "sine"), ("offbin", 997., "sine"), ("harmonic", 220., "harmonic")):
        for length in (.16, .64):
            for ratio in (.75, 1., 1.5):
                for comb, phase in (("constructive", 0.), ("destructive", .5)):
                    cycles = round(length * ratio * hz - phase) + phase
                    delay = round(cycles * RATE / hz)
                    for feedback in (0., .5, .9, 1.2):
                        identity = f"{name}-{round(length*1000)}ms-r{ratio:g}-{comb}-g{feedback:g}"
                        result.append(Case(identity, kind, hz, round(length*RATE), round(length*RATE/8), delay, feedback, "single", ratio, comb))
    for pattern in ("repeated", "alternating"):
        for feedback in (.5, .9, 1.2):
            for attack in (.02, .08):
                result.append(Case(f"rhythm-{pattern}-a{round(attack*1000)}-g{feedback:g}", "sine", 997., 19200,
                                   round(attack*RATE), 12012, feedback, pattern, 12012/19200, "destructive"))
    return result


def spectrum(audio):
    frequencies, times, values = signal.stft(audio, fs=RATE, nperseg=4096, noverlap=3072,
                                            boundary=None, padded=False)
    return frequencies, times, values


def db(value):
    return 20 * np.log10(max(float(value), 1e-16))


def render(case, mode, initial, binaries, scratch):
    prefix = scratch / f"{case.id}-{mode}-{initial}"
    executable = binaries / f"pvdr_{mode if mode in ('schmitt', 'floor60') else 'current'}"
    command = [str(executable), mode if mode in ("conventional", "oracle") else "pv", initial, case.signal,
               str(case.hz), str(case.note), str(case.attack), str(case.delay), str(case.feedback), case.pattern,
               str(case.seed), str(prefix)]
    subprocess.run(command, check=True, stdout=subprocess.DEVNULL)
    audio_path = Path(str(prefix)+".f32")
    audio = np.fromfile(audio_path, dtype="<f4")
    audio_path.unlink()
    diagnostics_path = Path(str(prefix)+".csv")
    diagnostics = np.genfromtxt(diagnostics_path, delimiter=",", names=True)
    diagnostics_path.unlink()
    dry = None
    if mode == "conventional":
        dry_path = Path(str(prefix)+"-dry.f32")
        dry = np.fromfile(dry_path, dtype="<f4")
        dry_path.unlink()
    assert np.all(np.isfinite(audio))
    return audio, diagnostics, dry


def compare(case, mode, initial, audio, reference, diagnostics, ref_spectrum):
    frequencies, times, ref_z = ref_spectrum
    _, _, z = spectrum(audio)
    magnitude = np.abs(z)
    ref_magnitude = np.abs(ref_z)
    source_end = ONSET + case.note * (1 if case.pattern == "single" else 7)
    tail = slice(source_end + case.delay, None)
    norm = np.linalg.norm(reference.astype(np.float64))
    error = np.linalg.norm(audio.astype(np.float64)-reference)
    tail_norm = np.linalg.norm(reference[tail].astype(np.float64))
    tail_error = np.linalg.norm(audio[tail].astype(np.float64)-reference[tail])
    near_fundamental = np.flatnonzero(np.abs(frequencies-case.hz) < 60)
    fundamental = magnitude[near_fundamental]
    ref_fundamental = ref_magnitude[near_fundamental]
    def ridge(m):
        indices = np.argmax(m, axis=0) + near_fundamental[0]
        logs = np.log(np.maximum(magnitude if m is fundamental else ref_magnitude, 1e-20))
        cols = np.arange(len(indices))
        left, center, right = logs[indices-1, cols], logs[indices, cols], logs[indices+1, cols]
        correction = .5*(left-right)/(left-2*center+right+1e-20)
        return (indices+np.clip(correction, -.5, .5)) * RATE / 4096
    ridge_hz = ridge(fundamental)
    reference_hz = ridge(ref_fundamental)
    active = np.max(ref_fundamental, axis=0) > np.max(ref_fundamental)*1e-3
    active &= np.max(fundamental, axis=0) > np.max(ref_fundamental)*1e-3
    harmonics = np.arange(1, 9 if case.signal == "harmonic" else 2) * case.hz
    outside = np.min(np.abs(frequencies[:, None]-harmonics[None, :]), axis=1) > 35
    spectral_energy = float(np.sum(magnitude.astype(np.float64)**2))
    offband = float(np.sum(magnitude[outside].astype(np.float64)**2)) / max(spectral_energy, 1e-30)
    phase_rms = math.sqrt(float(np.sum(diagnostics["weighted_phase_error"])) /
                          max(float(np.sum(diagnostics["energy"])), 1e-30))*360
    return dict(case=case.id, mode=mode, initial=initial, seed=case.seed, hz=case.hz,
                note_ms=case.note/RATE*1000, attack_ms=case.attack/RATE*1000,
                delay_ms=case.delay/RATE*1000, delay_ratio=case.delay/case.note,
                feedback=case.feedback, comb=case.comb, pattern=case.pattern,
                relative_error=error/max(norm, 1e-30), error_db=db(error/max(norm, 1e-30)),
                tail_error_db=db(tail_error/max(tail_norm, 1e-30)),
                magnitude_error=float(np.linalg.norm(magnitude-ref_magnitude)/max(np.linalg.norm(ref_magnitude), 1e-30)),
                rms_ratio=float(np.linalg.norm(audio.astype(np.float64))/max(norm, 1e-30)),
                peak=float(np.max(np.abs(audio))), offband_fraction=offband,
                ridge_rms_hz=float(np.sqrt(np.mean((ridge_hz[active]-reference_hz[active])**2))) if np.any(active) else 0.,
                phase_rms_degrees=phase_rms, triggers=int(np.sum(diagnostics["triggers"])),
                triggered_followers=int(np.sum(diagnostics["triggered_followers"])))


def run_case(case, binaries, output, retain=False):
    reference, _, dry = render(case, "conventional", "aligned", binaries, output/"scratch")
    reference_spectrum = spectrum(reference)
    arrays = {"conventional": reference, "dry": dry}
    rows = []
    for mode in ("oracle", "current", "schmitt", "floor60"):
        for initial in (("aligned",) if mode == "oracle" else ("aligned", "random")):
            audio, diagnostics, _ = render(case, mode, initial, binaries, output/"scratch")
            rows.append(compare(case, mode, initial, audio, reference, diagnostics, reference_spectrum))
            if retain:
                arrays[f"{mode}_{initial}"] = audio
                arrays[f"diag_{mode}_{initial}"] = diagnostics
    if retain:
        np.savez_compressed(output/"audio"/(case.id+".npz"), **arrays)
    return rows


def figure(case, output):
    plt.rcParams.update({"font.size": 10, "axes.spines.top": False, "axes.spines.right": False})
    data = np.load(output/"audio"/(case.id+".npz"))
    fig, axes = plt.subplots(3, 2, figsize=(14, 9), layout="constrained", sharex=True, sharey=True)
    spectra = {}
    top = 0
    for initial in ("aligned", "random"):
        for mode in ("conventional", "schmitt", "current"):
            key = "conventional" if mode == "conventional" else f"{mode}_{initial}"
            f, t, z = spectrum(data[key])
            levels = 20*np.log10(np.maximum(2*np.abs(z), 1e-7))
            spectra[(initial, mode)] = (f, t, levels)
            top = max(top, math.ceil(float(levels.max())/6)*6)
    for column, initial in enumerate(("aligned", "random")):
        for row, mode in enumerate(("conventional", "schmitt", "current")):
            ax = axes[row, column]
            f, t, levels = spectra[(initial, mode)]
            im = ax.pcolormesh(t, f, levels, cmap="magma", vmin=-100, vmax=top, shading="auto", rasterized=True)
            ax.set_ylim(0, 2400)
            ax.set_title(f"{LABELS[mode]} · {initial} initial phase" if mode != "conventional" else "Conventional · shared reference")
            ax.set_ylabel("Frequency (Hz)")
            if row == 2:
                ax.set_xlabel("Time (seconds)")
    fig.colorbar(im, ax=axes, label="Amplitude (dBFS), one shared scale", shrink=.8)
    title = (f"{case.signal.capitalize()} {case.hz:g} Hz · note {case.note/RATE*1000:.0f} ms · "
             f"attack {case.attack/RATE*1000:.0f} ms · delay {case.delay/RATE*1000:.2f} ms\n"
             f"Feedback {case.feedback:g} · {case.comb} · {case.pattern}")
    fig.suptitle(title, fontsize=15)
    fig.savefig(output/"figures"/(case.id+"-spectrogram.png"), dpi=140)
    plt.close(fig)

    fig, axes = plt.subplots(3, 2, figsize=(14, 9), layout="constrained", sharex=True, sharey=True)
    for column, initial in enumerate(("aligned", "random")):
        for row, mode in enumerate(("conventional", "schmitt", "current")):
            key = "conventional" if mode == "conventional" else f"{mode}_{initial}"
            f, t, z = signal.stft(data[key], fs=RATE, nperseg=16384, noverlap=15360, boundary=None, padded=False)
            levels = 20*np.log10(np.maximum(2*np.abs(z), 1e-7))
            ax = axes[row, column]
            im = ax.pcolormesh(t, f, levels, cmap="magma", vmin=-100, vmax=top, shading="auto", rasterized=True)
            ax.set_ylim(case.hz-45, case.hz+45)
            ax.set_title(f"{LABELS[mode]} · {initial}")
            ax.set_ylabel("Frequency (Hz)")
            if row == 2:
                ax.set_xlabel("Time (seconds)")
    fig.colorbar(im, ax=axes, label="Amplitude (dBFS), one shared scale", shrink=.8)
    fig.suptitle(title+"\nFrequency detail: 16384-sample / 341 ms analysis windows", fontsize=14)
    fig.savefig(output/"figures"/(case.id+"-frequency-detail.png"), dpi=140)
    plt.close(fig)

    fig, axes = plt.subplots(3, 2, figsize=(14, 9), layout="constrained", sharex=True)
    for column, initial in enumerate(("aligned", "random")):
        for mode in ("conventional", "schmitt", "current"):
            key = "conventional" if mode == "conventional" else f"{mode}_{initial}"
            f, t, z = spectrum(data[key])
            power = np.abs(z)**2
            band = np.abs(f-case.hz) < 60
            local = power[band]
            centroid = np.sum(f[band, None]*local, axis=0)/np.maximum(np.sum(local, axis=0), 1e-30)
            active = np.sum(local, axis=0) > np.max(np.sum(local, axis=0))*1e-6
            centroid[~active] = np.nan
            axes[0, column].plot(t, centroid-case.hz, color=COLORS[mode], label=LABELS[mode], lw=1.2)
            amplitude = np.sqrt(np.sum(power, axis=0))
            axes[1, column].plot(t, 20*np.log10(np.maximum(amplitude, 1e-8)), color=COLORS[mode], lw=1.2)
            if mode != "conventional":
                d = data[f"diag_{mode}_{initial}"]
                phase = np.sqrt(d["weighted_phase_error"]/np.maximum(d["energy"], 1e-30))*360
                phase[d["energy"] < np.max(d["energy"])*1e-8] = np.nan
                axes[2, column].plot(d["sample"]/RATE, phase, color=COLORS[mode], lw=1.2)
        axes[0, column].set_title(f"{initial.capitalize()} initial phase")
        axes[0, column].set_ylabel("Fundamental-band centroid\noffset from note (Hz)")
        axes[0, column].set_ylim(-60, 60)
        axes[0, column].legend(loc="upper right", fontsize=8)
        axes[1, column].set_ylabel("Spectral level (dB)")
        axes[1, column].set_ylim(-100, 20)
        axes[2, column].set_ylabel("Own-analysis phase error\nweighted RMS (degrees)")
        axes[2, column].set_ylim(0, 180)
        axes[2, column].set_xlabel("Time (seconds)")
        for ax in axes[:, column]:
            ax.grid(alpha=.2)
    fig.suptitle(title, fontsize=15)
    fig.savefig(output/"figures"/(case.id+"-evolution.png"), dpi=140)
    plt.close(fig)

    center = ONSET + case.note//2 + 2*case.delay
    half_width = round(3*RATE/case.hz)
    sample_slice = slice(center-half_width, center+half_width)
    milliseconds = np.arange(-half_width, half_width)/RATE*1000
    fig, axes = plt.subplots(1, 2, figsize=(14, 3.8), layout="constrained", sharey=True)
    for ax, initial in zip(axes, ("aligned", "random")):
        for mode in ("conventional", "schmitt", "current"):
            key = "conventional" if mode == "conventional" else f"{mode}_{initial}"
            ax.plot(milliseconds, data[key][sample_slice], color=COLORS[mode], label=LABELS[mode], lw=1.5,
                    linestyle="--" if mode == "schmitt" else "-")
        ax.set_title(f"{initial.capitalize()} initial phase")
        ax.set_xlabel(f"Milliseconds relative to {center/RATE:.4f} s")
        ax.set_ylabel("Amplitude (no fitted phase or gain)")
        ax.grid(alpha=.2)
        ax.legend(fontsize=8)
    fig.suptitle("Waveform detail · "+case.id, fontsize=14)
    fig.savefig(output/"figures"/(case.id+"-waveform.png"), dpi=150)
    plt.close(fig)

    # All listening copies for a case share one gain. Raw float data is retained.
    keys = ["conventional", "schmitt_random", "current_random", "schmitt_aligned", "current_aligned"]
    gain = min(1., .9/max(float(np.max(np.abs(data[key]))) for key in keys))
    for key in keys:
        wavfile.write(output/"audio"/(case.id+"-"+key+".wav"), RATE, (data[key]*gain).astype(np.float32))
    return gain


def aggregate(rows, output):
    fig, axes = plt.subplots(2, 2, figsize=(12, 8), layout="constrained")
    for column, initial in enumerate(("aligned", "random")):
        for mode in ("current", "schmitt", "floor60", "oracle"):
            selected = [r for r in rows if r["mode"] == mode and (r["initial"] == initial or mode == "oracle") and r["pattern"] == "single"]
            gains = sorted(set(r["feedback"] for r in selected))
            medians, lo, hi, spectral = [], [], [], []
            for gain in gains:
                group = [r for r in selected if r["feedback"] == gain]
                errors = [r["error_db"] for r in group]
                medians.append(np.median(errors))
                lo.append(min(errors))
                hi.append(max(errors))
                spectral.append(np.median([100*r["magnitude_error"] for r in group]))
            axes[0, column].plot(gains, medians, "o-", color=COLORS[mode], label=LABELS[mode])
            axes[0, column].fill_between(gains, lo, hi, color=COLORS[mode], alpha=.09)
            axes[1, column].plot(gains, spectral, "o-", color=COLORS[mode], label=LABELS[mode])
        axes[0, column].set_title(f"{initial.capitalize()} initial phase")
        axes[0, column].set_ylabel("Waveform error / conventional (dB)\nmedian line; full-range shading")
        axes[0, column].legend(fontsize=8)
        axes[1, column].set_ylabel("Spectrogram magnitude error (%)\nmedian across single-note cases")
        for ax in axes[:, column]:
            ax.set_xlabel("Feedback gain")
            ax.grid(alpha=.2)
    fig.suptitle("Neutral delay comparison · same timing, same input, linear feedback", fontsize=15)
    fig.savefig(output/"figures"/"overview.png", dpi=160)
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--binaries", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--workers", type=int, default=4)
    parser.add_argument("--quick", action="store_true")
    args = parser.parse_args()
    for directory in (args.output, args.output/"scratch", args.output/"audio", args.output/"figures"):
        directory.mkdir(parents=True, exist_ok=True)
    matrix = cases()
    if args.quick:
        matrix = [matrix[0], matrix[3], matrix[7], matrix[48+7], matrix[-1]]
    (args.output/"cases.json").write_text(json.dumps([dataclasses.asdict(case) for case in matrix], indent=2))
    rows = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.workers) as pool:
        futures = {pool.submit(run_case, case, args.binaries, args.output): case for case in matrix}
        for count, future in enumerate(concurrent.futures.as_completed(futures), 1):
            rows.extend(future.result())
            if count % 8 == 0 or count == len(matrix):
                print(f"Rendered {count}/{len(matrix)} cases ({count*8} comparisons)", flush=True)
    rows.sort(key=lambda r: (r["case"], r["mode"], r["initial"]))
    with (args.output/"metrics.csv").open("w") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    summary = {}
    for mode in ("oracle", "current", "schmitt", "floor60"):
        for initial in (("aligned",) if mode == "oracle" else ("aligned", "random")):
            group = [r for r in rows if r["mode"] == mode and r["initial"] == initial]
            summary[f"{mode}_{initial}"] = {"median_error_db": float(np.median([r["error_db"] for r in group])),
                                          "worst_error_db": max(r["error_db"] for r in group),
                                          "median_magnitude_error": float(np.median([r["magnitude_error"] for r in group])),
                                          "median_ridge_rms_hz": float(np.median([r["ridge_rms_hz"] for r in group]))}
    (args.output/"summary.json").write_text(json.dumps(summary, indent=2))
    print(json.dumps(summary, indent=2), flush=True)
    chosen = set()
    for initial in ("aligned", "random"):
        for mode in ("current", "schmitt"):
            group = [r for r in rows if r["mode"] == mode and r["initial"] == initial]
            chosen.add(max(group, key=lambda r: r["relative_error"])["case"])
    for case in matrix:
        if case.id in ("onbin-160ms-r1.5-constructive-g0.9", "offbin-640ms-r0.75-destructive-g1.2",
                       "harmonic-640ms-r0.75-destructive-g0.9", "rhythm-alternating-a80-g1.2",
                       "offbin-160ms-r1.5-destructive-g0.5", "rhythm-repeated-a20-g0.9"):
            chosen.add(case.id)
    if args.quick:
        chosen = {case.id for case in matrix}
    selected = [case for case in matrix if case.id in chosen]
    gains = {}
    for index, case in enumerate(selected):
        run_case(case, args.binaries, args.output, retain=True)
        gains[case.id] = figure(case, args.output)
        print(f"Plotted {index+1}/{len(selected)}: {case.id}", flush=True)
    (args.output/"listening-gains.json").write_text(json.dumps(gains, indent=2))
    aggregate(rows, args.output)
    blocks = []
    for case in selected:
        filename = html.escape(case.id)
        blocks.append(f'<section id="{filename}"><h2>{filename}</h2><img src="figures/{filename}-spectrogram.png"><img src="figures/{filename}-frequency-detail.png"><img src="figures/{filename}-evolution.png"><img src="figures/{filename}-waveform.png">'
                      + ''.join(f'<p>{html.escape(label)} <audio controls preload="none" src="audio/{filename}-{key}.wav"></audio></p>'
                                for key, label in (("conventional", "Conventional"), ("schmitt_random", "Schmitt / random initial"), ("current_random", "Current / random initial")))
                      + '</section>')
    body = '''<!doctype html><html><head><meta charset="utf-8"><title>PVDR phase experiment</title>
<style>body{max-width:1400px;margin:30px auto;padding:0 20px;font:16px system-ui;color:#252a34}img{width:100%;height:auto}section{margin:50px 0;border-top:1px solid #ccc}audio{vertical-align:middle}table{border-collapse:collapse}td,th{padding:8px;border:1px solid #ddd}code{font-size:.9em}</style></head><body>
<h1>Neutral feedback delay: phase initialization and Schmitt resets</h1>
<p>Actual C++ Resynthesizer FFT, PVDR and overlap-add; existing 48 kHz passthrough-rate test configuration. N=4096, H=1024. No warp, pitch shift, detune, magnitude slew, filter, saturation or clipping. One sample of launch latency is compensated in the source position. Conventional delay and every spectral variant close their own feedback loops.</p>
<p>Schmitt: −60/−72 dB relative to the largest current/reference magnitude. Low bins retain the existing random-increment behavior. A triggered leader is assigned its current analysis phase; followers use existing offsets. There is no growth retrigger, peak tracker, or transient detector. The −60 dB floor-only ablation changes the cutoff without adding resets.</p>
<p>Aligned initial phase seeds all bins from reference analysis at the first nonzero input window; random initial phase adds independent uniform offsets at that same instant. The random seed is recorded per case. Copy-analysis control reassigns all bin phases every frame. DC and Nyquist are omitted as in production. Output is the wet signal. Feedback above unity is intentionally unclipped and rendered for a finite duration.</p>
<p>Inputs: 0.2 peak-scale carrier, cosine attack/release, 750 Hz bin-centered sine, 997 Hz off-bin sine, and 220 Hz note with eight 1/h harmonics. The matrix spans 160/640 ms notes, 20/80 ms attacks, nominal delay/note ratios 0.75/1/1.5, gains 0/0.5/0.9/1.2. Delay is rounded to the nearest integer sample near constructive or destructive carrier phase. Additional repeated/alternating-polarity notes exercise cancellation.</p>
<p>Waveform error is measured sample-for-sample without fitting lag, gain or phase. Spectrogram magnitude error is the relative Frobenius norm of linear magnitudes. Ridge error uses a parabolic log-magnitude peak near the fundamental; it is a descriptor, not an instantaneous-frequency ground truth. Evolution plots show the fundamental-band spectral centroid and phase error against each variant’s own analysis. Spectrograms use one absolute color scale per figure; there is no per-panel normalization. Listening copies share a safety gain within each case; raw float outputs are in the NPZ files.</p>
<p><a href="metrics.csv">All metrics</a> · <a href="cases.json">Exact cases</a> · <a href="summary.json">Summary</a> · <a href="listening-gains.json">Listening gains</a></p>
<img src="figures/overview.png">
'''
    (args.output/"index.html").write_text(body+'\n'.join(blocks)+'</body></html>')


if __name__ == "__main__":
    main()
