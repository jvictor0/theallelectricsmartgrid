#!/usr/bin/env python3
"""Validate small GPU survey fixtures, checkpoint recovery and CPU agreement."""

import argparse
import fcntl
import json
import math
from pathlib import Path
import shutil
import struct
import subprocess

MAGIC = 0x334752454444414c
RECORD = struct.Struct('<Q864fQ')
HEADER = struct.Struct('<QQQ')
ROOT = Path(__file__).resolve().parents[1]
BINARY = ROOT/'build/tools/ladder-gpu-grid/ladder-gpu-grid'


def checksum(data):
    value = 14695981039346656037
    for byte in data:
        value = ((value ^ byte)*1099511628211) & ((1 << 64)-1)
    return value


def records(folder):
    result = {}
    data = (folder/'worker-000.bin').read_bytes()
    magic, _, size = HEADER.unpack(data[:HEADER.size])
    assert magic == MAGIC and size == RECORD.size
    assert (len(data)-HEADER.size) % size == 0
    for offset in range(HEADER.size, len(data), size):
        raw = data[offset:offset+size]
        record = RECORD.unpack(raw)
        assert record[0] not in result
        assert checksum(raw[:-8]) == record[-1]
        result[record[0]] = raw
    return result


def run(folder, extra, success=True):
    command = [str(BINARY), '--grid', '4', '--starts', '3', '--samples', '191999',
               '--threads', '2', '--batch-jobs', '2', '--output', str(folder)] + extra
    result = subprocess.run(command, capture_output=True, text=True)
    assert (result.returncode == 0) == success, (command, result.stdout, result.stderr)
    return result.stdout+result.stderr


def validate(root):
    control = records(root/'control')
    scalar = records(root/'scalar')
    resumed = records(root/'resumed')
    assert len(control) == len(scalar) == 64 and control.keys() == scalar.keys()
    assert len(resumed) == 5
    for job, raw in resumed.items():
        assert control[job] == raw
    with (root/'resumed/worker-000.bin').open('ab') as stream:
        stream.write(b'incomplete-tail')
    log = run(root/'resumed', [])
    assert 'recovered_partial_record' in log
    (root/'resume.log').write_text(log)
    assert records(root/'resumed') == control
    assert json.loads((root/'resumed/progress.json').read_text())['status'] == 'complete'
    for label, extra in [('starts', ['--starts', '4']), ('seed', ['--seed', '123']),
                         ('execution', ['--cpu-reference'])]:
        log = run(root/'resumed', extra, success=False)
        assert 'configuration or format mismatch' in log
        (root/(label+'-rejection.log')).write_text(log)
    with (root/'resumed/run.lock').open('a') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        log = run(root/'resumed', [], success=False)
        assert 'Another process owns' in log
    assert records(root/'resumed') == control
    for label in ['corrupt', 'wrong-format']:
        folder = root/label
        folder.mkdir()
        data = bytearray((root/'control/worker-000.bin').read_bytes())
        data[HEADER.size+16 if label == 'corrupt' else 7] ^= 1
        (folder/'worker-000.bin').write_bytes(data)
        log = run(folder, [], success=False)
        assert ('Corrupt or duplicate' if label == 'corrupt' else 'configuration or format mismatch') in log
        assert (folder/'worker-000.bin').read_bytes() == data
        (root/(label+'-rejection.log')).write_text(log)
    log = run(root/'filtered', ['--min-effective-resonance', '3.5'])
    (root/'filtered.log').write_text(log)
    filtered = records(root/'filtered')
    manifest = json.loads((root/'filtered/manifest.json').read_text())
    assert 0 < len(filtered) < len(control) and len(filtered)*4 == manifest['selected_points']
    assert all(control[job] == raw for job, raw in filtered.items())
    maximum = 0
    points = 0
    fields = 0
    for job, raw in control.items():
        if job % 4 != 0:
            continue
        gpu_values = RECORD.unpack(raw)[1:-1]
        cpu_values = RECORD.unpack(scalar[job])[1:-1]
        for a, b in zip(gpu_values, cpu_values):
            assert math.isnan(a) == math.isnan(b)
            if math.isnan(a):
                continue
            assert math.isfinite(a) and math.isfinite(b)
            scaled = abs(a-b)/max(1, abs(b))
            maximum = max(maximum, scaled)
            assert scaled < 2e-4, (job, a, b, scaled)
            fields += 1
        points += 4
    return dict(status='passed', total_control_points=256, partial_jobs_preserved=5,
                recovered_partial_record=True, changed_batch_size_bitwise_identical=True,
                incompatible_settings_rejected=True, corrupt_record_and_wrong_format_rejected=True,
                concurrent_writer_rejected=True, filtered_points=len(filtered)*4,
                filtered_records_bitwise_identical=True, stable_cpu_reference_points=points,
                compared_finite_fields=fields, maximum_stable_scaled_error=maximum)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('folder', type=Path)
    args = parser.parse_args()
    result = validate(args.folder)
    (args.folder/'validation.json').write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
