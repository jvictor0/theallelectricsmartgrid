"""A finite mono noise experiment driven entirely by the exported recipe."""
import ctypes as ct
import json
import math
import subprocess
from pathlib import Path

import numpy as np

from spectrum import DualRate, MetalSpectrum
import spectral_fit

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
F32 = np.ctypeslib.ndpointer(dtype=np.float32, flags="C_CONTIGUOUS")
F64 = np.ctypeslib.ndpointer(dtype=np.float64, flags="C_CONTIGUOUS")


class Design:
    def __init__(self, directory):
        self.r = r = json.loads((directory / "recipe.json").read_text())
        processor = json.loads((directory / "processor.json").read_text())
        self.state_size = max(1, processor["stateSize"])
        self.axes = r["axes"]
        self.stimuli = r["stimuli"]
        self.freq = np.array(r["frequencies"], dtype=float)
        names = [a["name"] for a in self.axes]
        if not names or names != processor["axes"] or len(names) != len(set(names)):
            raise ValueError("Recipe axes must match the Lean configuration fields in order")
        for a in self.axes:
            if not (math.isfinite(a["low"]) and math.isfinite(a["high"]) and a["low"] < a["high"]
                    and (not a["logScale"] or a["low"] > 0) and a["knots"] >= 2
                    and a["cells"] >= (2 if a["points"] else 1)):
                raise ValueError(f"Invalid axis {a['name']}")
        reference = r.get("frequencyRelativeTo")
        self.relative_axis = None
        if reference is not None:
            if reference not in names or self.axes[names.index(reference)]["low"] <= 0:
                raise ValueError("frequencyRelativeTo must name a strictly positive axis")
            self.relative_axis = names.index(reference)
        if (not self.stimuli or any(s["mass"] <= 0 or not math.isfinite(s["rms"]) or s["rms"] <= 0
                                    or not math.isfinite(s["preludeRms"]) or s["preludeRms"] < 0
                                    for s in self.stimuli)
                or len({s["name"] for s in self.stimuli}) != len(self.stimuli)):
            raise ValueError("Invalid stimulus distribution")
        self.shape = tuple(a["cells"] for a in self.axes) + (len(self.stimuli),)
        weights = r["masses"] or [s["mass"] for s in self.stimuli] * math.prod(self.shape[:-1])
        if len(weights) != math.prod(self.shape) or min(weights) < 0 or max(weights) <= 0:
            raise ValueError("Invalid joint cell masses")
        self.weights = tuple(weights)
        self.active = [i for i, weight in enumerate(weights) if weight]
        if (min(r[k] for k in ("records", "frames", "training", "audit", "sampleRate")) <= 0
                or r["fft"] < 4 or (r["fft"] & (r["fft"] - 1)) or len(self.freq) < 2
                or not np.isfinite(self.freq).all() or np.any(np.diff(self.freq) <= 0)
                or self.freq[0] <= 0 or self.freq[-1] > r["sampleRate"] / 2
                or not -240 <= r["floorDb"] <= 0
                or not 0 <= r["bandLimitHz"] <= r["sampleRate"]/2
                or not r["thresholdsDb"] or not r["coveragePPM"] or not 0 < r["riskPPM"] < 1000000
                or any(not 0 < q < 1000000 for q in r["coveragePPM"])
                or any(not math.isfinite(t) or t < 0 for t in r["thresholdsDb"])
                or any(len(set(r[k])) != len(r[k]) for k in ("coveragePPM", "thresholdsDb"))):
            raise ValueError("Invalid measurement or audit plan")
        self.knots = tuple(a["knots"] for a in self.axes)
        self.storage_shape = list(self.knots)
        overhead = 20 + 24*len(self.axes) + 8*len(self.freq)
        def size():
            return overhead + 2*math.prod(self.storage_shape)*len(self.freq)
        if r.get("spectralMesh") is not None:
            self.nbytes, _ = spectral_fit.layout(self)
            self.storage_shape = r["spectralMesh"]
        else:
            while size() > r["budgetBytes"]:
                i = max(range(len(self.axes)), key=self.storage_shape.__getitem__)
                if self.storage_shape[i] == 2:
                    raise ValueError("Byte budget cannot store even the corner table")
                self.storage_shape[i] -= 1
            self.nbytes = size()
        self.tests = len(self.active)*len(r["thresholdsDb"])*len(r["coveragePPM"])
        self.dual_rate = DualRate(r, self.freq) if r.get("dualRate") else None
        if self.dual_rate:
            return
        self.window = np.hanning(r["fft"] + 1)[:-1]
        bins = np.fft.rfftfreq(r["fft"], 1/r["sampleRate"])
        # Integrate positive-frequency bin power between adjacent geometric midpoints.
        edges = np.r_[self.freq[0], np.sqrt(self.freq[:-1]*self.freq[1:]), self.freq[-1]]
        half = r["sampleRate"]/(2*r["fft"])
        self.bands = np.maximum(0, np.minimum(edges[1:, None], bins+half)
                                - np.maximum(edges[:-1, None], bins-half))
        self.bands[:, -1] *= 0.5
        self.bands[:, 0] *= 0.5

    def score(self, predicted, measured):
        if self.dual_rate:
            # The legacy display extends each endpoint ordinate to the outer band edge.
            f = np.r_[self.dual_rate.edges[0], self.freq, self.dual_rate.edges[-1]]
            return visible_rms(np.pad(predicted,1,mode="edge"),np.pad(measured,1,mode="edge"),f,self.r["floorDb"])
        return visible_rms(predicted, measured, self.freq, self.r["floorDb"])

    def config(self, u):
        return np.array([a["low"]*(a["high"]/a["low"])**x if a["logScale"]
                         else a["low"] + (a["high"]-a["low"])*x for a,x in zip(self.axes,u)])

    def draw(self, cell, rng):
        indices = np.unravel_index(cell, self.shape)
        u = [i/(a["cells"]-1) if a["points"] else (i+rng.random())/a["cells"]
             for a,i in zip(self.axes,indices)]
        return self.config(u), indices[-1]

    def noise(self, rng, count, rms):
        x = rng.standard_normal((self.r["records"], count))
        if self.r["bandLimitHz"] > 0:
            spectrum = np.fft.rfft(x, axis=-1)
            spectrum[:, np.fft.rfftfreq(count, 1/self.r["sampleRate"]) > self.r["bandLimitHz"]] = 0
            x = np.fft.irfft(spectrum, n=count, axis=-1)
        return (rms * x / np.sqrt(np.mean(x*x, axis=1, keepdims=True))).astype(np.float32)

    def batch_size(self, stimulus):
        r = self.r
        samples = self.stimuli[stimulus]["prelude"] + r["warm"] + r["frames"] * r["fft"]
        if self.dual_rate:
            # Independent audit banks are much larger than the shared training bank.
            return max(1, min(16, self.dual_rate.tile, (2*1024**3) // (samples*r["records"]*4)))
        # Keep the waveform arrays bounded even for long trials. A single
        # complete trial is the minimum independently measurable unit.
        return max(1, min(128, (8 * 1024**2) // (samples * r["records"] * 4)))

    def measure_batch(self, backend, configs, stimulus, generators):
        r = self.r
        s = self.stimuli[stimulus]
        if self.dual_rate:
            return self.dual_rate.measure(backend, configs, s, generators)
        count = r["warm"] + r["frames"] * r["fft"]
        records = []
        for rng in generators:
            block = self.noise(rng, count, s["rms"])
            if s["prelude"]:
                block = np.concatenate((self.noise(rng, s["prelude"], s["preludeRms"]), block), axis=1)
            records.append(block)
        x = np.concatenate(records)
        y = backend.process(configs, r["records"], x)

        def power(signal):
            frames = signal[:, -r["frames"] * r["fft"]:].reshape(len(configs), -1, r["fft"])
            spectrum = np.abs(np.fft.rfft(frames * self.window)) ** 2
            return spectrum.mean(axis=1) @ self.bands.T

        wet, dry = power(y), power(x)
        powers = np.stack((wet, dry), axis=1)
        invalid = (dry <= 0).any(axis=1) | ~np.isfinite(powers).all(axis=(1, 2))
        powers[invalid] = np.nan
        return powers


def db(powers):
    return 10*np.log10(np.maximum(1e-24, powers[0]/powers[1]))


def visible_rms(a, b, frequencies, floor):
    # Between knots both raw curves are affine in log Hz. Split each segment at
    # both floor crossings, then integrate the squared linear difference exactly.
    frequencies = np.asarray(frequencies)
    widths = np.diff(np.log(frequencies))
    # Distinct, nearby knots can round to the same logarithm. Recover those
    # widths without changing scores from normally spaced saved surveys.
    collapsed = widths == 0
    lower = frequencies[:-1][collapsed]
    upper = frequencies[1:][collapsed]
    widths[collapsed] = np.log1p((upper - lower) / lower)
    span = widths.sum()
    if not np.isfinite(span) or span <= 0:
        raise ValueError("Survey frequency span must be positive and finite")

    def cross(v):
        slope = np.diff(v)
        return np.clip(np.divide(floor-v[:-1], slope, out=np.zeros_like(slope), where=slope!=0),0,1)
    x, y = cross(a), cross(b)
    points = (np.zeros_like(x), np.minimum(x,y), np.maximum(x,y), np.ones_like(x))
    values = [np.maximum(floor,a[:-1]+p*np.diff(a))-np.maximum(floor,b[:-1]+p*np.diff(b)) for p in points]
    area = sum((v*v+v*w+w*w)*(hi-lo)*widths/3 for lo,hi,v,w in zip(points,points[1:],values,values[1:]))
    mean_square = area.sum()/span
    return float(np.sqrt(max(0.0, mean_square))) if np.isfinite(mean_square) else None


class Backend:
    def __init__(self, directory, design, metal):
        self.words = design.state_size
        sources = [directory/"processor.cpp", HERE/"Table.cpp"]
        flags = []
        if metal:
            sources.append(HERE/"Metal.mm")
            if design.dual_rate:
                sources.append(HERE/"Spectrum.mm")
            flags = ["-fobjc-arc","-framework","Metal","-framework","Foundation"]
        library = directory/"processor.dylib"
        subprocess.run(["clang++","-std=c++20","-O2","-fPIC","-shared","-fno-fast-math","-ffp-contract=off",
                        "-I",str(ROOT/"private/src"),"-I",str(ROOT/"formal/DSP/generated"),
                        *map(str,sources),*flags,"-o",str(library)],check=True)
        self.lib = ct.CDLL(str(library.resolve()))
        self.lib.SurveyPrepare.argtypes = [F32,F32]
        self.lib.SurveyRun.argtypes = [F32,F32,F32,ct.c_uint]
        self.lib.SurveyKnots.argtypes = [ct.c_char_p,ct.c_size_t,F64,ct.c_uint,F64,ct.c_uint]
        self.lib.SurveyKnots.restype = ct.c_bool
        self.gpu = None
        if metal:
            self.lib.SurveyMetalError.restype = ct.c_char_p
            self.lib.SurveyMetalOpen.argtypes = [ct.c_char_p]
            self.lib.SurveyMetalOpen.restype = ct.c_void_p
            self.lib.SurveyMetalRun.argtypes = [ct.c_void_p,F32,ct.c_uint,F32,F32,ct.c_uint,ct.c_uint]
            self.lib.SurveyMetalRun.restype = ct.c_bool
            self.lib.SurveyMetalClose.argtypes = [ct.c_void_p]
            self.gpu = self.lib.SurveyMetalOpen((directory/"processor.metal").read_bytes())
            if not self.gpu:
                raise RuntimeError(self.lib.SurveyMetalError().decode())

        self.spectrum = MetalSpectrum(self, directory, design) if self.gpu and design.dual_rate else None

    def close(self):
        if self.spectrum:
            self.spectrum.close()
        if self.gpu:
            self.lib.SurveyMetalClose(self.gpu)
            self.gpu = None

    def initial_states(self, configs, records):
        states = np.empty((len(configs)*records,self.words),dtype=np.float32)
        for i, config in enumerate(configs):
            group = states[i*records:(i+1)*records]
            self.lib.SurveyPrepare(config.astype(np.float32),group[0])
            group[:] = group[0]
        return states

    def process(self, configs, records, x):
        states = self.initial_states(configs, records)
        y = np.empty_like(x)
        if self.gpu:
            if not self.lib.SurveyMetalRun(self.gpu,states,self.words,x,y,len(x),x.shape[1]):
                raise RuntimeError(self.lib.SurveyMetalError().decode())
        else:
            for state,src,dst in zip(states,x,y):
                self.lib.SurveyRun(state,src,dst,len(src))
        return y

    def knots(self, packed, config, count):
        y = np.empty(count)
        if not self.lib.SurveyKnots(packed,len(packed),config,len(config),y,count):
            raise ValueError("Packed runtime table rejected")
        return y
