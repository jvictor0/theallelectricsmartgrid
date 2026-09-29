import datetime
import json
from pathlib import Path
import re
import sys
from collections import Counter
import numpy as np
sys.path.insert(0, '/private/tmp/smartgrid-analysis-libs')
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

root = Path('/private/tmp')
summary = {}
for label in ['ui-on-r3', 'ui-off']:
    base = root / ('smartgrid-ipad-autoplay-' + label + '-20260910')
    flat = json.loads(Path(str(base) + '.flat-candidates.json').read_text())
    correlation = json.loads(Path(str(base) + '.correlation.json').read_text())
    runs = np.asarray(flat['short_runs_frames'], dtype=np.int64)
    bins = Counter((runs[:, 0] // 48000).tolist())
    groups = []
    for second in sorted(s for s, count in bins.items() if count >= 10):
        if groups and second == groups[-1][-1] + 1:
            groups[-1].append(second)
        else:
            groups.append([second])
    episodes = []
    for group in groups:
        selected = runs[(runs[:, 0] >= group[0] * 48000) & (runs[:, 0] < (group[-1] + 1) * 48000)]
        episodes.append({'bin_start_seconds': group[0], 'bin_end_seconds': group[-1]+1,
            'first_hole_seconds': selected[0, 0] / 48000,
            'last_hole_end_seconds': selected[-1, 1] / 48000,
            'short_holes': len(selected),
            'width_ms_min_median_p99_max': np.percentile((selected[:, 1]-selected[:, 0])/48, [0,50,99,100]).tolist(),
            'flat_fraction_within_episode_envelope': float(np.sum(selected[:, 1]-selected[:, 0])/(selected[-1, 1]-selected[0, 0]))})
    stages = []
    windows = [(40, 50),(120,125),(140,150),(350,360),(450,460),(510,520),(560,570),(650,660),(940,950)] if label == 'ui-off' else [(177,181)]
    for start,end in windows:
        r = runs[(runs[:,0] >= start*48000) & (runs[:,0] < end*48000)]
        stages.append({'start_seconds':start,'end_seconds':end,'holes':len(r),
            'holes_per_second':len(r)/(end-start),
            'flat_fraction':float(np.sum(r[:,1]-r[:,0])/(48000*(end-start))) if len(r) else 0,
            'median_width_ms':float(np.median(r[:,1]-r[:,0])/48) if len(r) else None,
            'median_onset_spacing_ms':float(np.median(np.diff(r[:,0]))/48) if len(r)>1 else None})
    summary[label] = {'base': str(base), 'episodes':episodes,'representative_windows':stages,
        'method':'Dense episodes are consecutive one-second bins with >=10 short flat candidates. Candidate detection uses both channels; isolated candidates may be natural music. Dense waveform and cadence were independently inspected. Analog settling affects measured boundaries and widths.',
        'timing_limit':'WAV timestamps have recorder startup/latency and clock offset. Association with device events is approximate; no sub-millisecond cross-device alignment is claimed.'}

base = root / 'smartgrid-ipad-autoplay-ui-off-20260910'
flat = json.loads(Path(str(base) + '.flat-candidates.json').read_text())
c = json.loads(Path(str(base) + '.correlation.json').read_text())
seconds = int(flat['seconds'])
coverage = np.zeros(seconds)
for first,last in flat['short_runs_frames']:
    for second in range(first//48000, min(seconds, (last-1)//48000+1)):
        coverage[second] += max(0,min(last,(second+1)*48000)-max(first,second*48000))/48000
restart = c['rows'][0]['usb_error_seconds']
drift = [(e['seconds'],int(re.search(r'ioDriftNS (\d+)',e['message']).group(1))/1e6) for e in c['drift_events']]
plt.rcParams.update({'font.family':'DejaVu Sans','font.size':10})
fig,axes=plt.subplots(2,1,figsize=(12,6.5),sharex=True,gridspec_kw={'height_ratios':[1.3,1]},layout='constrained')
fig.suptitle('UI rendering off: periodic blanking clears at an audio restart, then returns',fontsize=14,fontweight='bold')
axes[0].fill_between((np.arange(seconds)+.5)/60,coverage*100,color='#dd5e46',alpha=.8,linewidth=0)
axes[0].set_ylabel('Short flat chunks\n(% of each second)')
axes[0].set_ylim(0,55)
axes[0].text(.025,.93,'Normal DSP · MAYA 48 kHz / 512 frames · K-Mix analog capture',transform=axes[0].transAxes,va='top',fontsize=9)
axes[0].annotate('Periodic damage',xy=(2,35),xytext=(.65,43),arrowprops={'arrowstyle':'->','color':'#333'})
axes[0].annotate('No dense periodic blanking',xy=(6.5,.5),xytext=(5.0,23),arrowprops={'arrowstyle':'->','color':'#333'})
axes[0].annotate('Periodic damage returns',xy=(10,6),xytext=(10,46),arrowprops={'arrowstyle':'->','color':'#333'})
for segment in [[p for p in drift if p[0]<restart],[p for p in drift if p[0]>restart]]:
    stop = restart if segment[0][0]<restart else seconds
    xs=[p[0]/60 for p in segment]+[stop/60]
    ys=[p[1] for p in segment]+[segment[-1][1]]
    axes[1].step(xs,ys,where='post',color='#287ea6',linewidth=1.4,label='Last reported ioDriftNS' if segment[0][0]<restart else None)
    axes[1].scatter([p[0]/60 for p in segment],[p[1] for p in segment],color='#287ea6',s=13,zorder=3)
axes[1].axhline(8,color='#888',linestyle=':',linewidth=1)
axes[1].text(15.7,8.35,'8 ms',ha='right',color='#666',fontsize=9)
axes[1].set_ylabel('Driver timing drift\n(last reported, ms)')
axes[1].set_ylim(0,14)
axes[1].set_xlabel('Minutes from recording start (11:02:09 PDT, September 10)')
for ax in axes:
    ax.axvline(restart/60,color='#57317f',linestyle='--',linewidth=1.4)
    ax.set_xlim(0,16)
    ax.set_xticks(np.arange(0,17,1))
    ax.grid(axis='y',color='#ddd',linewidth=.5)
    ax.spines[['top','right']].set_visible(False)
axes[1].text(restart/60+.12,12.9,'USB error → I/O restart\n229 ms interruption',color='#57317f',va='top',fontsize=9)
fig.text(.02,-.025,'Dots are device log reports; steps hold the last report. Drift is not directly logged at the restart. Wave/device clocks are only approximately aligned.',fontsize=8,color='#555')
fig.savefig(root/'smartgrid-ui-off-drift-timeline-20260910.png',dpi=170,bbox_inches='tight')
(root/'smartgrid-ui-periodic-summary-20260910.json').write_text(json.dumps(summary,indent=2))
print(json.dumps(summary,indent=2))
