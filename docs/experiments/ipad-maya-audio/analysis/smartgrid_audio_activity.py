import argparse
import datetime
import json
from pathlib import Path
import struct
import numpy as np

parser = argparse.ArgumentParser()
parser.add_argument('--wav', required=True)
parser.add_argument('--output', required=True)
args = parser.parse_args()
path = Path(args.wav)
out = Path(args.output)
if out.exists():
    raise FileExistsError(out)
with path.open('rb') as f:
    riff = f.read(12)
    if riff[:4] != b'RIFF' or riff[8:] != b'WAVE':
        raise ValueError('Expected RIFF/WAVE')
    fmt = None
    while True:
        kind, size = struct.unpack('<4sI', f.read(8))
        if kind == b'fmt ':
            fmt = f.read(size)
            f.seek(size % 2, 1)
        elif kind == b'data':
            offset = f.tell()
            break
        else:
            f.seek(size + size % 2, 1)
    _, channels, rate, _, alignment, bits = struct.unpack('<HHIIHH', fmt[:16])
    if (channels, rate, alignment, bits) != (2, 48000, 6, 24):
        raise ValueError('Expected 48 kHz stereo 24-bit PCM')
    frames = (path.stat().st_size - offset) // alignment
    frames -= frames % 48
    powers = []
    summaries = []
    for first in range(0, frames, 480000):
        n = min(480000, frames - first)
        f.seek(offset + first * alignment)
        raw = np.frombuffer(f.read(n * alignment), dtype=np.uint8).reshape(-1, 3).astype(np.int32)
        v = raw[:, 0] | (raw[:, 1] << 8) | (raw[:, 2] << 16)
        x = ((v ^ 8388608) - 8388608).reshape(-1, 2) / 8388608
        power = (x.reshape(-1, 48, 2)**2).mean(axis=1)
        powers.append(power)
        summaries.append({'start_seconds':first/rate,'seconds':n/rate,'rms_dbfs':(10*np.log10(np.maximum(1e-24,power.mean(axis=0)))).tolist(),'peak_dbfs':(20*np.log10(np.maximum(1e-12,abs(x).max(axis=0)))).tolist()})
p = np.concatenate(powers)
quiet = p.max(axis=1) < 10**(-75/10)
e = np.diff(np.r_[False,quiet,False].astype(np.int8))
starts, ends = np.flatnonzero(e==1), np.flatnonzero(e==-1)
runs=[]
for a,b in zip(starts,ends):
    if b-a>=20:
        before = p[max(0,a-100):a]
        after = p[b:min(len(p),b+100)]
        before_db = float(10*np.log10(max(1e-24,before.max()))) if len(before) else None
        after_db = float(10*np.log10(max(1e-24,after.max()))) if len(after) else None
        runs.append({'start_seconds':a/1000,'end_seconds':b/1000,'duration_ms':int(b-a),'preceding_100ms_peak_rms_dbfs':before_db,'following_100ms_peak_rms_dbfs':after_db,'surrounded_by_signal':before_db is not None and after_db is not None and before_db>-60 and after_db>-60})
info={'checked_local':datetime.datetime.now().astimezone().isoformat(),'wav':str(path),'seconds_scanned':frames/rate,'method':'Both-channel RMS below -75 dBFS in 1 ms bins for at least 20 ms. Candidates only: natural rests and user transport changes must be excluded and device logs used for fault attribution.','near_silence_intervals':runs,'ten_second_levels':summaries}
out.write_text(json.dumps(info,indent=2))
print(json.dumps({'seconds_scanned':info['seconds_scanned'],'near_silence_candidates':len(runs),'surrounded_by_signal':sum(x['surrounded_by_signal'] for x in runs),'intervals':runs[-12:],'last_levels':summaries[-1]},indent=2))
