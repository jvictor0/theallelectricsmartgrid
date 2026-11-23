#!/usr/bin/env python3
"""Validate continuation measurements, coherent phases, and checkpoint recovery."""
import argparse
import fcntl
import json
from pathlib import Path
import shutil
import struct
import subprocess
import numpy as np

DTYPE=np.dtype([('point','<u8'),('values','<f4',(16,)),('checksum','<u8')])
MAGIC=0x314e54434c44414c


def fnv(data):
    x=14695981039346656037
    for byte in data:
        x=((x^byte)*1099511628211)&((1<<64)-1)
    return x


def read_records(root):
    raw=(root/'points.bin').read_bytes()
    magic,identity,stride=struct.unpack_from('<QQQ',raw)
    assert magic==MAGIC and stride==80 and (len(raw)-24)%80==0
    data=np.frombuffer(raw,DTYPE,offset=24).copy()
    assert len(set(data['point']))==len(data)
    for i,r in enumerate(data):
        assert int(r['checksum'])==fnv(raw[24+80*i:24+80*i+72])
    return data[np.argsort(data['point'])]


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary',type=Path);parser.add_argument('output',type=Path)
    args=parser.parse_args();binary=args.binary.resolve();root=args.output
    root.mkdir(parents=True,exist_ok=False)
    common=['--grid','4','--fine','7','--samples-per-step','60000','--warm-samples','48000','--threshold','-1']
    def run(name,extra=(),success=True,base=None):
        command=[str(binary),*(common if base is None else base),'--output',str(root/name),*map(str,extra)]
        p=subprocess.run(command,text=True,capture_output=True)
        (root/(name+'.log')).write_text(p.stdout+p.stderr)
        assert (p.returncode==0)==success,(name,p.returncode,p.stdout[-1000:],p.stderr)
        return p
    run('gpu',['--batch','7'])
    run('cpu',['--batch','64','--cpu-reference'])
    run('resumed',['--batch','11','--max-steps','3'])
    with (root/'resumed/points.bin').open('ab') as f:f.write(b'uncommitted bytes after the checkpoint')
    run('resumed',['--batch','5'])
    gpu=read_records(root/'gpu');cpu=read_records(root/'cpu');resumed=read_records(root/'resumed')
    assert len(gpu)==256 and np.array_equal(gpu,resumed)
    assert np.all(np.isfinite(gpu['values']))
    # At zero feedback trajectories are contractive; comparison is meaningful.
    zero=(gpu['point']//4%4)==0
    assert np.array_equal(gpu['point'],cpu['point'])
    scale=np.maximum(cpu['values'][zero,0],10**(-18/10)/2)
    field_error=np.max(abs(gpu['values'][zero,:6]-cpu['values'][zero,:6])/scale[:,None])
    assert field_error<2e-5,field_error
    run('synthetic',['--batch','64','--synthetic'])
    synthetic=read_records(root/'synthetic')
    expected=np.array([.560625,.5,.6,.8,.125,.045])
    synthetic_error=float(np.max(abs(synthetic['values'][:,:6]-expected)))
    assert synthetic_error<2e-6,synthetic_error
    assert np.max(synthetic['values'][:,7])==0
    manifest=json.loads((root/'synthetic/manifest.json').read_text())
    interpolation_errors=[]
    for tone in manifest['frequencies']:
        f=tone['index'];q=tone['period_samples'];p=tone['input_cycles']
        delay=q/p;lag=int(delay);fraction=delay-lag
        weights=np.array([np.prod([(fraction-(k-7))/(j-k) for k in range(16) if j!=k]) for j in range(16)],dtype=np.float32)
        omega=2*np.pi*p/q
        errors=[]
        for harmonic in [1,3]:
            response=np.sum(weights*np.exp(1j*harmonic*omega*(lag+np.arange(16)-7)))
            errors.append(abs(response-1)**2)
        expected_rms=np.sqrt((.5*errors[0]+.045*errors[1])/.560625)
        measured=synthetic['values'][synthetic['point']//64==f,6]
        interpolation_errors.append(float(np.max(abs(measured-expected_rms))))
    assert max(interpolation_errors)<2e-6,interpolation_errors
    run('resumed',['--amplitude','.2'],success=False)
    (root/'orphan-header').mkdir()
    (root/'orphan-header/points.bin').write_bytes(struct.pack('<QQQ',MAGIC,0,80))
    run('orphan-header',success=False)
    shutil.copytree(root/'gpu',root/'corrupt')
    with (root/'corrupt/points.bin').open('r+b') as f:
        f.seek(32);b=f.read(1);f.seek(32);f.write(bytes([b[0]^1]))
    run('corrupt',success=False)
    shutil.copytree(root/'gpu',root/'checkpoint-corrupt')
    with (root/'checkpoint-corrupt/checkpoint.bin').open('r+b') as f:
        f.seek(16);b=f.read(1);f.seek(16);f.write(bytes([b[0]^1]))
    run('checkpoint-corrupt',success=False)
    with (root/'gpu/run.lock').open('r+') as f:
        fcntl.flock(f,fcntl.LOCK_EX|fcntl.LOCK_NB)
        run('gpu',success=False)
    run('filtered',['--threshold','3.5','--batch','13'])
    filtered=read_records(root/'filtered')
    common_points=np.isin(gpu['point'],filtered['point'])
    assert np.array_equal(gpu[common_points],filtered)
    result={'records':len(gpu),'resume_different_batch_bit_exact':True,
            'rollback_uncommitted_tail':True,'zero_feedback_cpu_gpu_scaled_error':float(field_error),
            'synthetic_metric_error':synthetic_error,'synthetic_exact_buffer_error':0,
            'fractional_cycle_diagnostic_error':max(interpolation_errors),
            'configuration_corruption_and_lock_rejected':True,'filtered_points':len(filtered),
            'filtered_records_bit_exact':True}
    (root/'validation.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result))


if __name__=='__main__':main()
