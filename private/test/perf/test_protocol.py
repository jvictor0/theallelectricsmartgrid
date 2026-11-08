#!/usr/bin/env python3
"""Exercise benchmark input validation and repeatable capture/replay (no timing thresholds)."""
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from run import FIXTURE, SAVED_FIXTURE, ROOT, child_env, derive_fixtures, digest, execute


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
        mixed = json.loads(patches['mixed'].read_text())
        roots = mixed['squiggleBoy']
        gestures = {name for name, node in roots.items() if any(node.get('gestures', []))}
        modulators = {name for name, node in roots.items() if any(node.get('modulators', []))}
        assert len(roots) == 329 and len(gestures) == 165 and len(modulators) == 164
        assert gestures.isdisjoint(modulators) and gestures | modulators == roots.keys()
        assert mixed['faders'] == [0.5] * 16
        original = json.loads(FIXTURE.read_text())['squiggleBoy']
        for name in modulators:
            assert roots[name] == original[name], 'Mixed fixture changed a modulation-only parameter'
        for name in gestures:
            assigned = [node for node in roots[name]['gestures'] if node]
            assert len(assigned) == 2
            assert all(any(node['active'][scene] for node in assigned) for scene in range(8))
            assert all(value in original[name]['values']['values']
                       for node in assigned for value in node['values']['values'])
        workloads = [('matrix', 'matrix', FIXTURE, 'sweep'),
                     ('matrix_fixed', 'matrix', FIXTURE, 'fixed'),
                     ('matrix_knobs', 'matrix', FIXTURE, 'knobs'),
                     ('matrix_gestures', 'matrix_gestures', patches['gestures'], 'sweep'),
                     ('matrix_mixed', 'matrix_gestures', patches['mixed'], 'sweep'),
                     ('consumer', 'consumer', FIXTURE, 'sweep'),
                     ('consumer_gestures', 'consumer_gestures', patches['mixed'], 'sweep'),
                     ('full', 'full', FIXTURE, 'sweep')]
        workloads += [(name, kind, patches['mixed'], mode) for name, kind, mode in
                      [('mixed_fixed', 'full', 'fixed'), ('mixed_blend', 'full', 'sweep'),
                       ('mixed_gestures', 'full_gestures', 'fixed'),
                       ('mixed_both', 'full_gestures', 'sweep'),
                       ('mixed_scenes', 'full_gestures', 'scenes')]]
        gesture_checksums = {}
        loaded_roots = None
        for name, kind, patch, mode in workloads:
            dumps = []
            for repeat in range(2):
                dump = folder / f'{name}-{repeat}.f32'
                command = [encoder, kind, patch, mode, '.02']
                if kind in ('matrix', 'matrix_gestures', 'consumer', 'consumer_gestures'):
                    command.append(replay)
                row = execute(command + [dump])
                if kind in ('consumer', 'consumer_gestures'):
                    assert row['parameter_updates'] == 3840
                if name == 'full':
                    loaded_roots = row['roots']
                    assert loaded_roots >= len(roots)
                if name.startswith('mixed_'):
                    # Parameters added since this fixture load at their defaults.
                    # Both full-rig fixtures must contain the same current catalogue.
                    assert row['roots'] == loaded_roots
                    assert row['gesture_roots'] == len(gestures) and row['modulated_roots'] == len(modulators)
                    assert row['gesture_sweep'] == (kind == 'full_gestures')
                    gesture_checksums[name] = row['gesture_checksum']
                dumps.append(dump)
            assert dumps[0].stat().st_size > 0
            assert digest(dumps[0]) == digest(dumps[1]), f'{name} output changed between runs'
        assert digest(folder / 'matrix-0.f32') == digest(folder / 'consumer-0.f32')
        assert digest(folder / 'matrix_fixed-0.f32') != digest(folder / 'matrix_knobs-0.f32')
        assert digest(folder / 'matrix_mixed-0.f32') == digest(folder / 'consumer_gestures-0.f32')
        assert gesture_checksums['mixed_fixed'] != gesture_checksums['mixed_gestures']
        assert gesture_checksums['mixed_blend'] != gesture_checksums['mixed_both']
        saved = json.loads(SAVED_FIXTURE.read_text())
        saved['blend'] = 0.375
        saved_path = folder / 'saved.json'
        saved_path.write_text(json.dumps(saved))
        saved_hash = digest(saved_path)
        for mode in ('hold', 'sweep'):
            for kind in ('full', 'full_saved_gestures'):
                dumps = []
                for repeat in range(2):
                    dump = folder / f'saved-{kind}-{mode}-{repeat}.f32'
                    row = execute([encoder, kind, saved_path, mode, '.02', dump])
                    assert row['gesture_roots'] > 0
                    if mode == 'hold':
                        assert row['initial_blend'] == row['final_blend'] == 0.375
                    if kind == 'full_saved_gestures':
                        assert 0 < row['gesture_fader_mask'] < 65535
                    else:
                        assert row['gesture_fader_mask'] == 0
                    dumps.append(dump)
                assert digest(dumps[0]) == digest(dumps[1])
        assert digest(saved_path) == saved_hash, 'Saved-patch benchmark modified its input'
        def clear_gestures(node):
            node.pop('gestures', None)
            for amount in node.get('modulators', []):
                if amount:
                    clear_gestures(amount)
        for root in saved['squiggleBoy'].values():
            clear_gestures(root)
        saved_path.write_text(json.dumps(saved))
        plain, gesture = folder / 'plain.f32', folder / 'no-gestures.f32'
        execute([encoder, 'full', saved_path, 'hold', '.02', plain])
        row = execute([encoder, 'full_saved_gestures', saved_path, 'hold', '.02', gesture])
        assert row['gesture_fader_mask'] == 0
        assert digest(plain) == digest(gesture), 'A patch without gestures moved unrelated faders'
    print('CLI errors, replay compatibility/truncation/nonfinite checks, deterministic capture and output: passed')


if __name__ == '__main__':
    main()
