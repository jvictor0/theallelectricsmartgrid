#!/usr/bin/env python3
"""Exploratory analysis of the existing Lean ladder's binary64 measurements.

Raw IEEE-bit CSV files remain authoritative. Plot data is rounded to six
significant digits; no solver or ladder implementation is duplicated here.
"""

import csv
import gzip
import itertools
import json
import math
from pathlib import Path
import struct

import numpy as np


ROOT = Path(__file__).resolve().parents[1]
RESULTS = ROOT / "build/results/naive-ladder-finite-level"
FS = 192000
CUTOFF = 1000


def from_bits(value):
    return struct.unpack("<d", struct.pack("<Q", int(value)))[0]


def read_measurements(name):
    result = []
    with (RESULTS / f"{name}-bits.csv").open() as stream:
        for row in csv.DictReader(stream):
            decoded = {}
            for key, value in row.items():
                if key.endswith("_bits"):
                    decoded[key[:-5]] = from_bits(value)
                elif key in ("model", "detector"):
                    decoded[key] = value
                else:
                    decoded[key] = float(value)
            decoded.setdefault("resonanceKnob", 1.0)
            decoded["inputPower"] = decoded["amplitude"] ** 2 / 2
            decoded["powerDriftDb"] = 10 * math.log10(decoded["totalPower"] / decoded["previousPower"])
            decoded["coherentDrift"] = math.hypot(
                decoded["re"] - decoded["previousRe"], decoded["im"] - decoded["previousIm"]
            ) / math.sqrt(2 * decoded["totalPower"])
            assert all(math.isfinite(v) for v in decoded.values() if isinstance(v, float))
            assert decoded["fundamentalPower"] + decoded["thirdPower"] + decoded["dcPower"] <= (
                decoded["totalPower"] * (1 + 1e-10) + 1e-20
            )
            assert decoded["kMin"] >= 0
            assert decoded["kMax"] <= decoded["kBase"] * (1 + 1e-12)
            result.append(decoded)
    return result


def group_key(drive, resonance, amplitude, detector):
    return f"{round(drive * 100)}|{round(resonance * 100)}|{amplitude:g}|{detector}"


def row_key(row):
    return group_key(row["driveKnob"], row["resonanceKnob"], row["amplitude"], row["detector"])


def compact(value):
    if isinstance(value, float):
        return float(f"{value:.6g}")
    if isinstance(value, list):
        return [compact(v) for v in value]
    if isinstance(value, dict):
        return {k: compact(v) for k, v in value.items()}
    return value


def spectral_records():
    def key(row):
        return tuple(row[1:7])

    with gzip.open(RESULTS / "spectra-bits.csv.gz", "rt") as stream:
        for fields, rows in itertools.groupby(csv.reader(stream), key=key):
            model, detector, drive, amplitude, resonance, period = fields
            period = int(period)
            samples = list(rows)
            indices = [int(r[7]) for r in samples]
            assert all(i == indices[0] + j for j, i in enumerate(indices))
            signal = np.array([from_bits(r[8]) for r in samples])
            assert len(signal) == period * 32
            count = len(signal)
            spectrum = np.fft.rfft(signal) / count
            powers = 2 * abs(spectrum) ** 2
            powers[0] /= 2
            if count % 2 == 0:
                powers[-1] /= 2
            assert np.isclose(np.sum(powers), np.mean(signal**2), rtol=1e-12)
            frequencies = np.fft.rfftfreq(count, 1 / FS)
            peak_index = np.argmax(powers[1:]) + 1
            # Preserve dominant narrow components with the most energetic FFT
            # bins, plus all driven harmonics. This is a discrete line spectrum;
            # it is not an interpolated power spectral density.
            audible = np.where((frequencies >= 40) & (frequencies <= 24000))[0]
            strongest = audible[np.argsort(powers[audible])[-48:]]
            harmonic_bins = [h * 32 for h in range(1, int(24000 / (FS / period)) + 1)]
            total_power = float(np.sum(powers))
            selected = sorted(i for i in set(strongest.tolist() + harmonic_bins)
                              if powers[i] >= total_power * 1e-6)
            selected_power = sum(powers[i] for i in selected)
            yield group_key(float(drive), float(resonance), float(amplitude), detector), {
                "period": period,
                "inputHz": FS / period,
                "samples": count,
                "startSeconds": (max(32, (15360 + period - 1) // period) * period * 8 +
                                 2 * max(16, (7680 + period - 1) // period) * period * 8 - count) / FS,
                "durationSeconds": count / FS,
                "resolutionHz": FS / count,
                "peakHz": float(frequencies[peak_index]),
                "fundamentalPower": float(powers[32]),
                "thirdPower": float(powers[96]),
                "totalPower": total_power,
                "shownPowerFraction": float(selected_power / total_power),
                "audiblePowerFraction": float(np.sum(powers[frequencies <= 24000]) / total_power),
                "rows": [[float(frequencies[i]), float(powers[i])] for i in selected],
            }


def main():
    original_measured = read_measurements("sweep") + read_measurements("mid")
    replacements = {}
    refined = read_measurements("refined") + read_measurements("worst")
    for row in sorted(refined, key=lambda r: r["measuredSamples"]):
        replacements[(row_key(row), row["period"])] = row
    measured = [replacements.get((row_key(r), r["period"]), r) for r in original_measured]
    groups = {}
    for row in measured:
        key = row_key(row)
        groups.setdefault(key, {"measurements": [], "spectra": []})["measurements"].append(row)
    for key, spectrum in spectral_records():
        groups[key]["spectra"].append(spectrum)
    assert len(groups) == 54
    for group in groups.values():
        rows = sorted(group.pop("measurements"), key=lambda r: r["frequencyHz"])
        assert len(rows) == 140
        assert len(group["spectra"]) == 3
        group["spectra"].sort(key=lambda s: s["inputHz"])
        group["rows"] = [[r["frequencyHz"], r["fundamentalPower"], r["totalPower"],
                          r["thirdPower"], r["residualPower"], r["kMean"],
                          r["powerDriftDb"], r["coherentDrift"]] for r in rows]
        group["maxPowerDriftDb"] = max(abs(r["powerDriftDb"]) for r in rows)
        group["maxCoherentDrift"] = max(r["coherentDrift"] for r in rows)
        group["warmRangeSeconds"] = [min(r["warmSamples"] for r in rows) / FS,
                                     max(r["warmSamples"] for r in rows) / FS]
        group["measurementRangeSeconds"] = [min(r["measuredSamples"] for r in rows) / FS,
                                            max(r["measuredSamples"] for r in rows) / FS]
        group["gain"] = rows[0]["gain"]
        group["baseFeedback"] = rows[0]["kBase"]

    long = read_measurements("long") + read_measurements("mid-long")
    short_lookup = {(r["model"], row_key(r), r["period"]): r for r in original_measured + read_measurements("tanh")}
    checks = []
    for row in long:
        earlier = short_lookup[(row["model"], row_key(row), row["period"])]
        checks.append({"model": row["model"], "group": row_key(row), "frequencyHz": row["frequencyHz"],
                       "totalPowerChangeDb": 10 * math.log10(row["totalPower"] / earlier["totalPower"]),
                       "coherentChange": math.hypot(row["re"] - earlier["re"], row["im"] - earlier["im"]) /
                       math.sqrt(2 * row["totalPower"]), "longWindowDriftDb": row["powerDriftDb"]})
    sine = math.sin(math.pi * CUTOFF / FS)
    ratio = 2 * sine * (math.sqrt(1 + sine * sine) + sine)
    data = {"schema": "naive-ladder-finite-level-v1", "sampleRate": FS, "hostRate": 48000,
            "cutoffHz": CUTOFF, "crossingHz": math.asin(ratio / 2) * FS / math.pi,
            "groups": groups}
    (RESULTS / "plot-data.json").write_text(json.dumps(compact(data), separators=(",", ":")))
    (RESULTS / "validation.json").write_text(json.dumps(checks, indent=2))
    (RESULTS / "measurements.json").write_text(json.dumps(measured + long, separators=(",", ":")))
    print(json.dumps({"sweeps": len(groups), "measuredPoints": len(measured),
                      "spectra": sum(len(g["spectra"]) for g in groups.values()),
                      "longChecks": len(checks), "plotBytes": (RESULTS / "plot-data.json").stat().st_size,
                      "maxLongPowerChangeDb": max(abs(c["totalPowerChangeDb"]) for c in checks),
                      "maxLongCoherentChange": max(c["coherentChange"] for c in checks)}, indent=2))


if __name__ == "__main__":
    main()
