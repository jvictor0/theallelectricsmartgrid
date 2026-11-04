#!/usr/bin/env python3
"""Exercise benchmark input validation and repeatable capture/replay (no timing thresholds)."""
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from run import FIXTURE, ROOT, child_env, derive_fixtures, digest, execute


def main():
    binaries = Path(sys.argv[1]).resolve()
    def reject(arguments, expected):
        result = subprocess.run(list(map(str, arguments)), env=child_env(), capture_output=True, text=True)
        if result.returncode != 2 or expected not in result.stderr:
            raise AssertionError((arguments, result.returncode, result.stdout[-500:], result.stderr[-1000:]))
    voices = binaries / 'perf_voices'
    reject([voices, 'ladder', 3, 1, 'nan'], 'Seconds must')
    reject([voices, 'ladder', 3, 1, '1junk'], 'Seconds must')
    reject([voices, 'ladder', 2, 1, '.02'], 'Voices require')
    reject([voices, 'typo', 3, 1, '.02'], 'Unknown voice')
    reject([voices, 'ladder', 3, 9, '.02'], 'MODE must')
    reject([voices, 'ladder', 3, 1, '.000001'], 'Duration must')
    encoder = binaries / 'perf_encoders'
    reject([encoder, 'full', FIXTURE, 'typo', '.02'], 'Unknown scene')
    reject([encoder, 'full', '/missing-patch.json', 'fixed', '.02'], 'Cannot open patch')
    output = ROOT / 'build/results/perf-protocol'
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=output) as temporary:
        folder = Path(temporary)
        replay = folder / 'replay.bin'
        execute([encoder, 'record', FIXTURE, 'sweep', '.01', replay])
        other = folder / 'other.bin'
        execute([encoder, 'record', FIXTURE, 'sweep', '.01', other])
        assert digest(replay) == digest(other), 'Seeded capture changed between runs'
        payload = replay.read_bytes()
        assert payload[:8] == b'SGPERF1\0'
        bad = folder / 'bad.bin'
        variants = [
            (b'', 'Cannot read replay header'),
            (payload[:8] + struct.pack('<Q', 1) + payload[16:], 'Incompatible replay header'),
            (payload[:-1], 'Truncated replay payload'),
            (payload + b'\0', 'Trailing replay data'),
            (payload[:64] + struct.pack('<f', float('nan')) + payload[68:], 'Nonfinite replay input'),
        ]
        for data, expected in variants:
            bad.write_bytes(data)
            reject([encoder, 'matrix', FIXTURE, 'mid', '.02', bad], expected)
        patches = derive_fixtures(folder, FIXTURE)
        assert sum('gestures' in node for node in json.loads(patches['gestures'].read_text())['squiggleBoy'].values()) > 0
        for kind, patch in (('matrix', FIXTURE), ('matrix_gestures', patches['gestures']), ('full', FIXTURE)):
            dumps = []
            for repeat in range(2):
                dump = folder / f'{kind}-{repeat}.f32'
                command = [encoder, kind, patch, 'sweep', '.02']
                if kind != 'full':
                    command.append(replay)
                execute(command + [dump])
                dumps.append(dump)
            assert dumps[0].stat().st_size > 0
            assert digest(dumps[0]) == digest(dumps[1]), f'{kind} output changed between runs'
    print('CLI errors, replay compatibility/truncation/nonfinite checks, deterministic capture and output: passed')


if __name__ == '__main__':
    main()
