"""Compare a one-sixth-octave power measurement to saved post-settling replays."""
import argparse
import json
from pathlib import Path

import numpy as np


def main(knobs):
    replays = knobs.parent
    amplitude = 10**(-18/20)
    result = []
    for case in ('high','near','double','regain','control'):
        report = json.loads((replays/f'{case}-replay.json').read_text())
        tone = report['frequency']
        with np.load(replays/f'{case}-audio.npz') as waves:
            for level in waves:
                wave = waves[level].astype(float)
                window = np.hanning(len(wave))
                power = 2*abs(np.fft.rfft(wave*window))**2/(len(wave)*(window@window))
                power[[0,-1]] *= .5
                assert abs(power.sum()-np.sum((wave*window)**2)/(window@window)) < 1e-10
                hz = np.fft.rfftfreq(len(wave),1/192000)
                band = (hz >= tone*2**(-1/12)) & (hz <= tone*2**(1/12))
                db = lambda p: float(10*np.log10(p/(amplitude**2/2)))
                result.append(dict(case=case,level=int(level),fundamental_db=report['levels'][level]['after_12s']['gain'],
                    probe_band_db=db(power[band].sum()),all_band_db=db(power.sum())))
    (knobs/'probe-band.json').write_text(json.dumps(result,indent=2)+'\n')
    for row in result:
        print(row['case'],row['level'],*[round(row[k],2) for k in ('fundamental_db','probe_band_db','all_band_db')])


if __name__=='__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory',type=Path)
    main(parser.parse_args().directory)
