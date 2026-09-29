import argparse
import datetime
import json
from pathlib import Path
import re

import numpy as np

parser = argparse.ArgumentParser()
parser.add_argument('--log', required=True)
parser.add_argument('--meta', required=True)
parser.add_argument('--output', required=True)
args = parser.parse_args()
metadata = json.loads(Path(args.meta).read_text())
start = datetime.datetime.fromisoformat(metadata['started_local']).timestamp()
end = datetime.datetime.fromisoformat(metadata['ended_local']).timestamp()
keys = ['cb', 't_us', 'gap_us', 'prev_budget_us', 'dsp_us', 'n', 'sr', 'xr', 'thermal', 'lp', 'muted']
fields = re.compile(r'(\w+)=(-?\d+(?:\.\d+)?)')
callbacks, clocks, misses = [], [], []
for line in Path(args.log).read_text().splitlines():
    if 'Audio cb=' in line:
        values = dict(fields.findall(line))
        if all(key in values for key in keys):
            callbacks.append([float(values[key]) for key in keys])
    elif 'Audio system ' in line:
        values = dict(fields.findall(line))
        if 't_us' in values and 'wall_ms' in values:
            clocks.append([float(values['t_us']) / 1e6, float(values['wall_ms']) / 1000])
    elif 'Missed ' in line:
        misses.append(line)
if not callbacks or len(clocks) < 2:
    raise ValueError('Need callbacks and at least two monotonic/wall-clock anchors')
a = np.array(callbacks)
c = np.array(clocks)
reference = c[0, 0]
fit = np.polyfit(c[:, 0] - reference, c[:, 1] - c[:, 0], 1)
epochs = a[:, 1] / 1e6 + np.polyval(fit, a[:, 1] / 1e6 - reference)
selected = (epochs >= start) & (epochs <= end)
a, epochs = a[selected], epochs[selected]
if not len(a):
    raise ValueError('No callbacks overlap the recording')

def event(index):
    return {'time': datetime.datetime.fromtimestamp(float(epochs[index])).astimezone().isoformat(),
            'recording_seconds': float(epochs[index] - start), 'callback': int(a[index, 0]),
            'gap_us': float(a[index, 2]), 'dsp_us': float(a[index, 4]), 'xr': int(a[index, 7])}

report = {'app_log': args.log, 'recording_metadata': args.meta, 'callbacks': len(a),
    'sample_rates': np.unique(a[:, 6]).tolist(), 'frame_sizes': np.unique(a[:, 5]).tolist(),
    'thermal_states': np.unique(a[:, 8]).tolist(), 'low_power_states': np.unique(a[:, 9]).tolist(),
    'muted_states': np.unique(a[:, 10]).tolist(),
    'sequence_holes': int(np.count_nonzero(np.diff(a[:, 0]) != 1)),
    'logger_miss_lines_entire_log': misses,
    'dsp_ms_p50_p99_max': (np.percentile(a[:, 4], [50, 99, 100]) / 1000).tolist(),
    'gap_ms_p50_p99_max': (np.percentile(a[:, 2], [50, 99, 100]) / 1000).tolist(),
    'measured_dsp_overruns': int(np.count_nonzero(a[:, 4] > a[:, 5] / a[:, 6] * 1e6)),
    'xrun_start_end': [int(a[0, 7]), int(a[-1, 7])],
    'callback_gaps_gt20ms': [event(i) for i in np.flatnonzero(a[:, 2] > 20000)],
    'xrun_increments': [event(i) for i in np.flatnonzero(np.diff(a[:, 7]) > 0) + 1],
    'clock_offset_fit_slope': float(fit[0]),
    'clock_fit_max_residual_ms': float(np.max(np.abs(c[:, 1] - c[:, 0] - np.polyval(fit, c[:, 0] - reference))) * 1000),
    'limit': 'dsp_us measures the instrumented processing region; it excludes subsequent callback logging and outer JUCE/native work. xr is the native device counter.'}
with Path(args.output).open('x') as f:
    json.dump(report, f, indent=2)
print(json.dumps(report, indent=2))
