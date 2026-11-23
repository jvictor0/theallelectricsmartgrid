"""Reduce the complete continuation grid to boundary maps and matched comparisons.

Run with the survey directory as argument. Write the compressed NumPy result to
stdout so the reduction can run on the machine holding the original records.
"""
import json
from pathlib import Path
import sys

import numpy as np


def first_true(mask):
    return np.where(mask.any(axis=-1), mask.argmax(axis=-1), 100).astype('uint8')


def main(root):
    manifest = json.loads((root / 'manifest.json').read_text())
    assert manifest['grid'] == 100 and manifest['fine_positions'] == 10000
    dtype = [('id', '<u8'), ('values', '<f4', (16,)), ('checksum', '<u8')]
    rows = np.memmap(root / 'points.bin', offset=24, dtype=dtype, mode='r')
    assert len(rows) == manifest['points']
    # Arrays run frequency, drive, cutoff, resonance, so the sweep is contiguous.
    shape = (100,) * 4
    status = np.zeros(shape, dtype='uint8')
    power = np.full(shape, np.nan, dtype='float32')
    cycle = np.full(shape, np.nan, dtype='float32')
    for start in range(0, len(rows), 500000):
        block = rows[start:start+500000]
        ids = block['id']
        where = (ids//1000000, ids//10000%100, ids%100, ids//100%100)
        values = block['values']
        # Bit 0 = observed, bit 1 = one-cycle failure, bit 2 = exact-buffer failure.
        status[where] = 1 + 2*(values[:,6] >= .01) + 4*(values[:,7] >= .01)
        power[where] = values[:,1]
        cycle[where] = values[:,6]
    assert np.count_nonzero(status) == len(rows)
    observed = status != 0
    failed = (status & 2) != 0
    first = first_true(observed)
    loss = first_true(failed)
    assert (first < 100).all()
    previous = status[:,:,:,:-1]
    current = status[:,:,:,1:]
    crossing = (previous & 3 == 1) & (current & 3 == 3)
    regain = (previous & 3 == 3) & (current & 3 == 1)
    lost = loss < 100
    left = loss == first
    info = dict(paths=int(first.size), first_already_failed=int(left.sum()),
                observed_first_crossing=int((lost & ~left).sum()), never_failed=int((~lost).sum()),
                paths_with_regain=int(regain.any(axis=-1).sum()),
                crossings=int(crossing.sum()), regains=int(regain.sum()),
                first_seen_failed_and_recovers=int((left & regain.any(axis=-1)).sum()),
                both_fail=int((status == 7).sum()), cycle_only=int((status == 3).sum()),
                buffer_only=int((status == 5).sum()), both_pass=int((status == 1).sum()))
    # Floor gains at -60 dB relative to input power so variation in near-zero
    # projections cannot dominate the neighboring-point comparison.
    gain = 10*np.log10(np.maximum(power, manifest['amplitude']**2/2*1e-6)/(manifest['amplitude']**2/2))
    for name, mask in [('first_loss', crossing & (np.arange(1,100) == loss[...,None])),
                       ('both_pass', (previous & 3 == 1) & (current & 3 == 1)),
                       ('both_fail', (previous & 3 == 3) & (current & 3 == 3))]:
        ix = np.nonzero(mask)
        delta = abs(gain[:,:,:,1:][ix] - gain[:,:,:,:-1][ix])
        info[name+'_gain_step_db'] = np.quantile(delta,[.5,.9,.99]).tolist()
        info[name+'_gt3db_fraction'] = float(np.mean(delta>3))
    # Leave one frequency out and linearly interpolate gain in log frequency.
    # The actual rounded tones differ slightly from the ideal logarithmic grid.
    logf = np.log([t['actual_hz'] for t in manifest['frequencies']])
    w = ((logf[1:-1]-logf[:-2])/(logf[2:]-logf[:-2])).reshape(-1,1,1,1)
    for name, classification in [('passing',1),('failing',3)]:
        errors = []
        for i in range(1,99):
            mask = np.all((status[i-1:i+2] & 3)==classification,axis=0)
            weight = float(w[i-1,0,0,0])
            estimated = gain[i-1][mask]*(1-weight)+gain[i+1][mask]*weight
            errors.append(abs(gain[i][mask]-estimated))
        errors = np.concatenate(errors)
        info[name+'_frequency_midpoints'] = len(errors)
        info[name+'_midpoint_error_db'] = np.quantile(errors,[.5,.9,.99]).tolist()
    half_deltas = {1:[],3:[]}
    floor = manifest['amplitude']**2/2*1e-6
    for start in range(0,len(rows),500000):
        v = rows[start:start+500000]['values']
        delta = abs(10*np.log10(np.maximum(v[:,10],floor)/np.maximum(v[:,11],floor)))
        half_deltas[1].append(delta[v[:,6]<.01])
        half_deltas[3].append(delta[v[:,6]>=.01])
    for group,name in [(1,'passing'),(3,'failing')]:
        info[name+'_half_window_db'] = np.quantile(np.concatenate(half_deltas[group]),[.5,.9,.99]).tolist()
    fc = 20*1000**(np.arange(100)/99)
    f = np.array([t['actual_hz'] for t in manifest['frequencies']])
    sine = np.sin(np.pi*fc/192000)
    a = 2*sine*(np.sqrt(1+sine*sine)-sine)
    ratio = a/(1-a)
    osc = 192000/np.pi*np.arcsin(ratio/2)
    g = .25*20**(np.arange(100)/99)
    slope = g/np.minimum(1,g*(g*g+27)/(9*g*g+27))
    kcrit = (ratio+2)**2
    critical_knob = 99*np.log1p(48/3.9*kcrit[None,:]/slope[:,None]**3)/np.log(49)
    info['one_cycle_input_bins'] = sum(t['input_cycles']==1 for t in manifest['frequencies'])
    one = np.array([t['input_cycles']==1 for t in manifest['frequencies']])
    info['integer_cycle_failed'] = int(failed[one].sum())
    info['integer_cycle_observed'] = int(observed[one].sum())
    # Fix cutoff for detailed inspection, retaining all input and knob bins.
    np.savez_compressed(sys.stdout.buffer, first=first, loss=loss, freq=f, cutoff=fc,
        osc=osc, critical_knob=critical_knob, cycle56=cycle[:,:,56,:],
        status56=status[:,:,56,:], gain56=gain[:,:,56,:], info=json.dumps(info))


if __name__ == '__main__':
    main(Path(sys.argv[1]))
