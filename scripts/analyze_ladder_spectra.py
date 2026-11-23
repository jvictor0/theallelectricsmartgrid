#!/usr/bin/env python3
"""Measure complete spectra of saved-state continuations; no DSP is reimplemented."""
import argparse
from fractions import Fraction
import json
from pathlib import Path
import numpy as np


def analyze(root,r):
    fs=192000; pin=.25**2/2
    y=np.fromfile(root/r['file'],dtype='<f4').astype(np.float64)
    n=len(y); assert n==r['samples'] and np.all(np.isfinite(y))
    total=float(np.mean(y*y)); dc=float(y.mean())
    fft=np.fft.rfft(y); rect=2*np.abs(fft/n)**2
    rect[0]*=.5
    if n%2==0: rect[-1]*=.5
    assert abs(float(rect.sum())/total-1)<1e-12
    fundamental_bin=r['cycles_per_period']*(n//r['period_samples'])
    p1=float(rect[fundamental_bin])
    window=np.hanning(n)
    hfft=np.fft.rfft(y*window)
    line=2*np.abs(hfft/window.sum())**2
    weighted_power=2*np.abs(hfft)**2/(n*np.sum(window**2))
    weighted_power[0]*=.5
    if n%2==0: weighted_power[-1]*=.5
    candidates=np.flatnonzero((line[1:-1]>line[:-2]) & (line[1:-1]>line[2:]))+1
    candidates=candidates[np.argsort(line[candidates])[::-1]]
    peaks=[]; selected=[]
    for k in candidates:
        k=int(k)
        if any(abs(k-old)<=5 for old in selected): continue
        if line[k]<total*1e-9 or len(peaks)>=24: break
        selected.append(k)
        logs=np.log(np.maximum(line[k-1:k+2],1e-300))
        offset=.5*(logs[0]-logs[2])/(logs[0]-2*logs[1]+logs[2])
        hz=(k+float(offset))*fs/n
        integrated=float(weighted_power[max(1,k-3):k+4].sum())
        ratio=hz/r['frequency_hz']; approx=Fraction(ratio).limit_denominator(16)
        peaks.append({'frequency_hz':hz,'input_ratio':ratio,'near_ratio':str(approx),
                      'near_ratio_error_hz':hz-float(approx)*r['frequency_hz'],
                      'power_fraction':integrated/total,
                      'power_gain_db':float(10*np.log10(integrated/pin))})
    # Removing DFT bins on the repeating input-buffer grid is the orthogonal
    # residual after averaging the waveform by its input-buffer phase.
    residual_bins=rect.copy()
    residual_bins[::n//r['period_samples']]=0
    outside=float(residual_bins.sum()/total)
    top100=float(np.partition(residual_bins,-100)[-100:].sum()/max(residual_bins.sum(),1e-300))
    periodic=[]
    for m in [1,2,3,4,5,6,7,8,12,16,24,32,64]:
        lag=m*r['period_samples']
        if lag>=n//2: continue
        diff=y[lag:]-y[:-lag]
        periodic.append({'input_buffer_periods':m,'lag_samples':lag,
                         'relative_rms_difference':float(np.sqrt(np.mean(diff*diff)/total))})
    chunks=[]
    q=r['period_samples']; half=(n//(2*q))*q
    for a,b in [(0,half),(half,n)]:
        segment=y[a:b]; spectrum=np.fft.rfft(segment)/(b-a)
        k=r['cycles_per_period']*((b-a)//q)
        chunks.append({'total':float(np.mean(segment*segment)),
                       'fundamental':float(2*abs(spectrum[k])**2)})
    bins=np.unique(np.r_[0,1,np.geomspace(max(1,20*n/fs),len(line),4500).astype(int),len(line)])
    bins=bins[bins<=len(line)]
    centers=(bins[:-1]+bins[1:]-1)*.5*fs/n
    maximum=np.maximum.reduceat(line,bins[:-1])
    result={**r,'duration_seconds':n/fs,'total_power':total,'fundamental_power':p1,'dc':dc,
            'fundamental_fraction':p1/total,'dc_fraction':dc*dc/total,
            'total_gain_db':float(10*np.log10(total/pin)),
            'fundamental_gain_db':float(10*np.log10(p1/pin)),
            'outside_input_buffer_grid_fraction':outside,
            'residual_top_100_fft_bins_fraction':top100,
            'power_fraction_above_24khz':float(rect[int(24000*n/fs)+1:].sum()/total),
            'strongest_peaks':peaks,'periodicity':periodic,'halves':chunks,
            'parseval_relative_error':float(abs(rect.sum()/total-1))}
    return result,centers,10*np.log10(np.maximum(maximum,1e-30)/pin)


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('root',type=Path)
    root=parser.parse_args().root
    selection=json.loads((root/'selection.json').read_text())
    rows=[]; plots={}
    for i,r in enumerate(selection['representatives']):
        row,hz,db=analyze(root,r);rows.append(row)
        plots[f'hz_{i}']=hz;plots[f'db_{i}']=db
        print(json.dumps({'point':r['point_id'],'start':r['start_index'],'level':r['level'],
                          'fund_db':row['fundamental_gain_db'],'total_db':row['total_gain_db'],
                          'top':[(round(p['frequency_hz'],3),round(100*p['power_fraction'],2)) for p in row['strongest_peaks'][:4]],
                          'repeat':min(row['periodicity'],key=lambda x:x['relative_rms_difference'])}),flush=True)
    (root/'spectral-report.json').write_text(json.dumps({'sample_rate':192000,'input_amplitude':.25,
        'capture_start_seconds':600,'line_power_estimator':'Hann main-lobe integrated +/-3 FFT bins; total/fundamental use rectangular coherent projection',
        'captures':rows},indent=2)+'\n')
    np.savez_compressed(root/'spectral-plots.npz',**plots)


if __name__=='__main__':
    main()
