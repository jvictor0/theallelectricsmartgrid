import os,sys,json,re
from pathlib import Path
import numpy as np
os.environ['MPLCONFIGDIR']='/private/tmp/smartgrid-mpl-config';os.environ['XDG_CACHE_HOME']='/private/tmp/smartgrid-plot-cache';sys.path.insert(0,'/private/tmp/smartgrid-analysis-libs')
import matplotlib;matplotlib.use('Agg')
import matplotlib.pyplot as plt
b='/private/tmp/smartgrid-ipad-juce815-ui-off-20260910'
f=json.loads(Path(b+'.flat-candidates.json').read_text());c=json.loads(Path(b+'.correlation.json').read_text());coverage=np.zeros(960)
for first,last in f['short_runs_frames']:
 for second in range(first//48000,min(960,(last-1)//48000+1)):
  coverage[second]+=max(0,min(last,(second+1)*48000)-max(first,second*48000))/48000
points=[(e['seconds'],int(re.search(r'ioDriftNS (-?\d+)',e['message'])[1])/1e6) for e in c['drift_events']]
fig,axes=plt.subplots(2,1,figsize=(12,6.4),sharex=True,layout='constrained')
fig.suptitle('JUCE 8.0.15: periodic blanking clears while reported timing drift keeps rising',fontsize=13,fontweight='bold')
axes[0].fill_between((np.arange(960)+.5)/60,coverage*100,color='#d97050',linewidth=0)
axes[0].set_ylabel('Short flat chunks\n(% of each second)');axes[0].set_ylim(0,52)
axes[0].text(.99,.94,'Normal DSP · UI off · 48 kHz / 512 frames\nNominal iPad thermal state throughout',transform=axes[0].transAxes,ha='right',va='top',fontsize=9)
axes[1].step([p[0]/60 for p in points]+[16],[p[1] for p in points]+[points[-1][1]],where='post',color='#297da8',lw=1.4)
axes[1].scatter([p[0]/60 for p in points],[p[1] for p in points],s=12,color='#297da8')
axes[1].set_ylabel('Driver timing drift\n(last reported, ms)');axes[1].set_ylim(0,37);axes[1].set_xlabel('Minutes from K-Mix recording start (12:27:42 PDT)')
for ax in axes:
 ax.axvline(24.20712/60,color='#65428e',ls='--',lw=1.1);ax.axvline(512.34777/60,color='#555',ls=':',lw=1.1);ax.set_xlim(0,16);ax.set_xticks(np.arange(17));ax.grid(axis='y',alpha=.25);ax.spines[['top','right']].set_visible(False)
axes[0].annotate('One USB restart\n225 ms gap',xy=(24.20712/60,2),xytext=(.8,45),color='#65428e',arrowprops={'arrowstyle':'->','color':'#65428e'},fontsize=9)
axes[0].annotate('Dense blanking ends\nNo driver restart or long callback gap',xy=(512.34777/60,.5),xytext=(8.9,28),arrowprops={'arrowstyle':'->','color':'#555'},fontsize=9)
fig.text(.01,-.02,'Short-hole detector verified against stereo waveform; widths include analog settling. Device/WAV alignment is approximate. Steps hold the last drift report.',fontsize=8,color='#555')
out=Path('/private/tmp/smartgrid-juce815-ui-off-drift-timeline-20260910.png');fig.savefig(out,dpi=170,bbox_inches='tight');print(str(out))
