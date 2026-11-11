#!/usr/bin/env python3
"""Serial paired tuning cases, including a deliberately saturated inverse reader."""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import random
import subprocess

ROOT = Path(__file__).resolve().parents[3]
BINARY = ROOT / 'build/tools/perf/Release/perf_density'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--label', required=True)
    parser.add_argument('--repeats', type=int, default=5)
    args = parser.parse_args()
    if args.repeats < 1 or not args.label.replace('-', '').isalnum():
        parser.error('positive repeats and an alphanumeric/hyphen label required')
    out = ROOT / 'build/results/inverse-optimization' / args.label
    out.mkdir(parents=True, exist_ok=True)
    if (out / 'metadata.json').exists():
        parser.error('refusing to overwrite results')
    files = [ROOT / 'private/src/TimeWarpInverter.hpp', Path(__file__).resolve(),
             ROOT / 'private/test/perf/density.cpp',
             ROOT / 'private/test/perf/reference/LandedTimeWarpInverter.hpp']
    snapshots = {}
    for source in files:
        relative = source.relative_to(ROOT)
        target = out / 'sources' / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(source.read_bytes())
        snapshots[str(relative)] = hashlib.sha256(source.read_bytes()).hexdigest()
    metadata = {'label': args.label, 'platform': platform.platform(),
                'baseline': 'de51539eb16f929321c7a3dec275be5b051424ac',
                'build': json.loads((BINARY.parent / 'build.json').read_text()),
                'sources': snapshots, 'repeats': args.repeats, 'cycles': 3.25,
                'binary_sha256': hashlib.sha256(BINARY.read_bytes()).hexdigest(),
                'machine': subprocess.check_output(['sysctl', '-n', 'machdep.cpu.brand_string'], text=True).strip(),
                'processes': subprocess.check_output(['ps', '-Ao', 'pid,pcpu,comm', '-r'], text=True)}
    (out / 'metadata.json').write_text(json.dumps(metadata, indent=2) + '\n')
    (out / 'perf_density').write_bytes(BINARY.read_bytes())
    cases = [(0, 16), (1, 16), (2, 18), (3, 16), (3, 20), (3, 24),
             (5, 16), (7, 16), (16, 16), (11, 16)]
    random.Random(20261111).shuffle(cases)
    cases = [('write-performance', *case) for case in cases]
    cases += [('contention', setting, 16) for setting in [1, 16, 11]]
    rows = []
    for mode, setting, bits in cases:
        command = [str(BINARY), mode, str(setting), str(bits), str(args.repeats), '3.25']
        result = subprocess.run(command, cwd=ROOT, text=True, capture_output=True, check=True)
        (out / f'{mode}-{setting}-{bits}.jsonl').write_text(result.stdout)
        new = [json.loads(line) for line in result.stdout.splitlines()]
        if mode == 'write-performance':
            for repeat in range(args.repeats):
                pair = [r for r in new if r['repeat'] == repeat]
                assert len(pair) == 2 and pair[0]['checksum'] == pair[1]['checksum']
                assert pair[0]['unavailable'] == pair[1]['unavailable']
        rows.extend(new)
        (out / 'all.json').write_text(json.dumps(rows, indent=2) + '\n')
        print(json.dumps({'label': args.label, 'mode': mode, 'case': new[0]['case']}), flush=True)
    print(json.dumps({'label': args.label, 'status': 'complete'}), flush=True)


if __name__ == '__main__':
    main()
