"""Separate the error of the packed fit from interpolation of the original survey."""
from itertools import product
import json
from pathlib import Path
import sys
import numpy as np

def main(fit_directory, result):
    sys.path.insert(0,str(fit_directory))
    from fit_display import reference,curve_mse
    truth,edges=reference()
    audit=np.load(result/'curves.npz')
    rows=[]
    for controls,measured,fitted in zip(audit['settings'],audit['measured'],audit['fitted']):
        drive,resonance,hz=controls
        q=np.array([360*np.log(hz/20)/np.log(1000),72*drive,72*resonance])
        lo=np.minimum(np.floor(q).astype(int),np.array(truth.shape[:3])-2)
        t=q-lo
        full=np.zeros(120)
        for corner in product((0,1),repeat=3):
            full+=np.prod(np.where(corner,t,1-t))*truth[tuple(lo+corner)].astype(float)/128
        def rms(a,b):return float(np.sqrt(curve_mse(a[None],b[None],edges)[0]))
        j=np.argmax(abs(np.maximum(measured,-60)-np.maximum(fitted,-60)))
        rows.append(dict(controls=controls.tolist(),fit_to_archive=rms(fitted,full),
             archive_to_stock=rms(full,measured),fit_to_stock=rms(fitted,measured),
             worst_band_db=dict(stock=float(measured[j]),fit=float(fitted[j]),archive=float(full[j]))))
    (result/'archive-comparison.json').write_text(json.dumps(rows,indent=2)+'\n')
    print(rows)
if __name__=='__main__':main(Path(sys.argv[1]).resolve(),Path(sys.argv[2]).resolve())
