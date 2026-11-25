"""Batched two-rate spectral measurement for an exported mono processor."""
import ctypes as ct
from pathlib import Path

import numpy as np

FFT = 4096
EPOCH = FFT * 32
HIGH = FFT // 2 + 1
LOW = FFT // 8 + 1
F32 = np.ctypeslib.ndpointer(dtype=np.float32, flags="C_CONTIGUOUS")


def noise_bank(rng, records, count, rms, divisor):
    output = np.empty((count // EPOCH, records, EPOCH), np.float32)
    for record in range(records):
        wave = rng.standard_normal(count // divisor)
        if divisor == 4:
            # Fourier-interpolate a host-rate record to the engine rate.
            spectrum = np.fft.rfft(wave)
            spectrum[[0, -1]] = 0
            wave = np.fft.irfft(spectrum, n=count)
        else:
            # Remove DC and the Nyquist coefficient at the native rate.
            wave[::2] -= wave[::2].mean()
            wave[1::2] -= wave[1::2].mean()
        wave *= rms / np.sqrt(np.mean(wave * wave))
        output[:, record] = wave.reshape(-1, EPOCH)
    return output


def lowpass(length, cutoff):
    t = np.arange(length) - (length-1)/2
    taps = np.sinc(2*cutoff*t)*np.kaiser(length, 12)
    return np.ascontiguousarray(taps/taps.sum(), np.float32)


class DualRate:
    def __init__(self, recipe, frequencies):
        rule = recipe["dualRate"]
        rate = recipe["sampleRate"]
        self.edges = np.asarray(rule["edges"], dtype=float)
        self.tile = rule["trainingTile"]
        self.records = recipe["records"]
        self.warm = recipe["warm"]
        self.count = self.warm + recipe["frames"]*recipe["fft"]
        self.rate = rate
        if (recipe["fft"] != FFT or self.warm % EPOCH or self.count % EPOCH
                or any(s["prelude"] % EPOCH for s in recipe["stimuli"])
                or recipe["bandLimitHz"] not in (0, rate/8, rate/2) or self.tile < 1
                or len(self.edges) != len(frequencies)+1 or not np.isfinite(self.edges).all()
                or self.edges[0] <= 0 or self.edges[-1] > rate/2 or np.any(np.diff(self.edges) <= 0)
                or not np.allclose(frequencies, np.sqrt(self.edges[:-1]*self.edges[1:]), rtol=1e-6)
                or not 0 < rule["splitHz"] <= rate/256):
            raise ValueError("Invalid two-rate spectrum recipe")
        self.noise_divisor = 4 if recipe["bandLimitHz"] == rate/8 else 1
        self.engine_spectrum = self.edges[-1] > rate/8
        if self.engine_spectrum and self.noise_divisor == 4:
            raise ValueError("Wet/dry bands above host Nyquist require a full-band stimulus")
        self.bin_count = HIGH + LOW + (HIGH if self.engine_spectrum else 0)
        self.fast = lowpass(257, 1/8)
        self.slow = lowpass(129, 2500/48000)
        self.window = np.hanning(FFT+1)[:-1].astype(np.float32)
        self.twiddle = np.exp(-2j*np.pi*np.arange(FFT//2)/FFT).astype(np.complex64).view(np.float32)
        self.bands = []
        for lo, hi in zip(self.edges, self.edges[1:]):
            bass = hi <= rule["splitHz"]
            engine = self.engine_spectrum and hi > rate/8
            width = rate / ((32 if bass else (1 if engine else 4)) * FFT)
            bins = np.arange(LOW if bass else HIGH)*width
            weights = np.maximum(0, np.minimum(hi, bins+width/2) - np.maximum(lo, bins-width/2))
            active = np.flatnonzero(weights)
            first, last = active[0], active[-1]+1
            offset = HIGH if bass else (HIGH + LOW if engine else 0)
            self.bands.append((slice(offset+first, offset+last), weights[first:last]))

    def prepare_noise(self, generators, stimulus):
        segments = []
        if stimulus["prelude"]:
            segments.append((stimulus["prelude"], stimulus["preludeRms"]))
        segments.append((self.count, stimulus["rms"]))
        banks = []
        for n, rms in segments:
            if len(generators) == 1:
                banks.append(noise_bank(generators[0], self.records, n, rms, self.noise_divisor))
            else:
                bank = np.empty((n//EPOCH, len(generators)*self.records, EPOCH), np.float32)
                for i,rng in enumerate(generators):
                    bank[:,i*self.records:(i+1)*self.records] = noise_bank(rng,self.records,n,rms,self.noise_divisor)
                banks.append(bank)
        return banks[0] if len(banks) == 1 else np.concatenate(banks)

    def band_power(self, bins):
        return np.stack([np.sum(bins[..., section]*weights, axis=-1)
                         for section, weights in self.bands], axis=-1)

    def reference(self, signal, warm):
        # Independent host implementation of the causal FIRs, FFTs and normalization.
        high = np.convolve(signal, self.fast, "full")[:len(signal):4]
        low = np.convolve(high, self.slow, "full")[:len(high):8]
        def analyze(samples, stride, band_count, divisor, offset=0):
            epochs = samples[warm//divisor:].reshape(-1, EPOCH//divisor)
            frames = np.lib.stride_tricks.sliding_window_view(epochs[:,offset:], FFT, axis=1)[:, ::stride]
            bins = np.abs(np.fft.rfft(frames*self.window))**2
            one_sided = bins.mean(axis=(0,1)) * (2*divisor/(self.rate*1536))
            one_sided[[0,-1]] *= .5
            return one_sided[:band_count]
        bins = [analyze(high, FFT//2, HIGH, 4), analyze(low, FFT, LOW, 32)]
        if self.engine_spectrum:
            bins.append(analyze(signal, 2*FFT, HIGH, 1, FFT//2))
        return self.band_power(np.concatenate(bins))

    def measure(self, backend, configs, stimulus, generators):
        waves = len(generators)*self.records
        shared = len(generators) == 1
        if not shared and len(generators) != len(configs):
            raise ValueError("Expected one training noise bank or independent banks for each setting")
        noise = self.prepare_noise(generators, stimulus)
        warm = self.warm + stimulus["prelude"]
        if backend.gpu:
            spectra = backend.spectrum.measure(configs, self, noise, warm//EPOCH)
            spectra = spectra.reshape(-1,self.records,self.bin_count).mean(axis=1, dtype=np.float64)
            powers = self.band_power(spectra)
            dry = powers[len(configs):]
            result = np.stack((powers[:len(configs)], np.broadcast_to(dry,(len(configs),len(self.bands)))),axis=1)
        else:
            rows = np.ascontiguousarray(noise.transpose(1,0,2).reshape(waves,-1))
            dry = np.asarray([self.reference(wave,warm) for wave in rows]).reshape(-1,self.records,len(self.bands)).mean(1)
            output = []
            for i, config in enumerate(configs):
                x = rows if shared else rows[i*self.records:(i+1)*self.records]
                y = backend.process([config],self.records,x)
                output.append(np.mean([self.reference(wave,warm) for wave in y],axis=0))
            result = np.stack((output,np.broadcast_to(dry,np.shape(output))),axis=1)
        result[(result[:,1] <= 0).any(axis=1) | ~np.isfinite(result).all(axis=(1,2))] = np.nan
        return result


class MetalSpectrum:
    def __init__(self, backend, directory, design):
        self.backend = backend
        self.rule = design.dual_rate
        self.words = design.state_size
        self.context = None
        self.capacity = (0,0)
        self.source = ((directory/"processor.metal").read_text() +
                       f"\n#define SURVEY_STATE_WORDS {self.words}\n#define SURVEY_SAMPLE_RATE {self.rule.rate}.0f\n" +
                       Path(__file__).with_name("Spectrum.metal").read_text()).encode()
        lib = backend.lib
        lib.OpenSpectrum.argtypes = [ct.c_char_p, ct.c_uint, ct.c_uint, ct.c_uint, ct.c_bool]+[F32]*4
        lib.OpenSpectrum.restype = ct.c_void_p
        lib.CloseSpectrum.argtypes = [ct.c_void_p]
        lib.CloseSpectrum.restype = None
        lib.ResetSpectrum.argtypes = [ct.c_void_p, ct.c_uint, ct.c_uint, F32]
        lib.ResetSpectrum.restype = ct.c_bool
        lib.SpectrumEpoch.argtypes = [ct.c_void_p, F32, ct.c_bool, ct.c_bool, ct.c_bool]
        lib.SpectrumEpoch.restype = ct.c_bool
        lib.ReadSpectrum.argtypes = [ct.c_void_p, F32]
        lib.ReadSpectrum.restype = None

    def close(self):
        if self.context:
            self.backend.lib.CloseSpectrum(self.context)
            self.context = None

    def measure(self, configs, rule, noise, warm_epochs):
        lib = self.backend.lib
        waves = noise.shape[1]
        wet = len(configs)*rule.records
        rows = wet + waves
        if rows > self.capacity[0] or waves > self.capacity[1]:
            self.close()
            self.context = lib.OpenSpectrum(self.source,rows,waves,self.words,rule.engine_spectrum,
                                            rule.fast,rule.slow,rule.window,rule.twiddle)
            if not self.context:
                raise RuntimeError("Could not create the tiled Metal spectrum pipeline")
            self.capacity = (rows,waves)
        states = self.backend.initial_states(configs, rule.records)
        if not lib.ResetSpectrum(self.context,wet,waves,states):
            raise RuntimeError("Invalid tile dimensions")
        for i, epoch in enumerate(noise):
            if not lib.SpectrumEpoch(self.context,epoch,i >= warm_epochs-1,i >= warm_epochs,i == warm_epochs):
                raise RuntimeError("Metal spectrum epoch failed")
        bins = np.empty((rows,rule.bin_count),np.float32)
        lib.ReadSpectrum(self.context,bins)
        return bins / (len(noise)-warm_epochs)
