#!/usr/bin/env python3
"""Check replay ordering, cutoff, scene/path semantics and rejected timelines."""

import copy
import json
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    binary = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix='sgrec-replay-') as temporary:
        root = Path(temporary)
        fixture = {
            'replay_version': 1, 'sample_rate': 48000, 'frames': 4800,
            'initial_phase': 0.0,
            'initial_patch': {'blend': 0, 'faders': [0] * 16},
            'events': [
                [0, 8, '', 0, 0, [], True, 0],
                [100, 4, 'DelaySendWater', 0, 0, [], 0.9, 0],
                [100, 4, 'DelaySendWater', 0, 0, [], 0.2, 1],
                [101, 4, 'DelaySendWater', 1, 0, [], 0.7, 0],
                [102, 4, 'DelaySendWater', 0, 0, [2], -0.5, 0],
                [103, 1, 'Mute_2', 0, 0, [], [1], 0],
                [104, 4, 'DelaySendWater', 0, 0, [128], 0.4, 0],
                [104, 5, 'DelaySendWater', 0, 0, [128], True, 1],
                [105, 2, '', 0, 3, [], 0.6, 0],
                [106, 3, '', 0, 0, [], 0.3, 0],
                [3000, 4, 'DelaySendWater', 0, 0, [], 0.8, 0],
            ],
        }

        def run(data, label, success):
            source = root / f'{label}.json'
            source.write_text(json.dumps(data))
            patch = root / f'{label}-patch.json'
            result = subprocess.run([str(binary), str(source), '0.05',
                                     str(root / f'{label}.jsonl'), str(patch)],
                                    capture_output=True, text=True)
            if (result.returncode == 0) != success:
                raise AssertionError(f'{label}: {result.stdout}\n{result.stderr}')
            return json.loads(patch.read_text()) if success else None

        saved = run(fixture, 'ordered', True)
        cell = saved['squiggleBoy']['DelaySendWater']
        assert abs(cell['values']['values'][0] - 0.2) < 1e-6
        assert abs(cell['values']['values'][1] - 0.7) < 1e-6
        assert cell['modulators'][2]['values']['values'][0] == -0.5
        assert abs(cell['gestures'][0]['values']['values'][0] - 0.4) < 1e-6
        assert cell['gestures'][0]['active'][0]
        assert saved['nonagon']['Mute_2'][0] == 1
        assert 0 < saved['faders'][3] <= 0.600001
        assert 0 < saved['blend'] <= 0.300001
        assert saved == run(fixture, 'repeat', True)

        for label, mutate in [
            ('order', lambda x: x['events'][2].__setitem__(7, 0)),
            ('unknown-name', lambda x: x['events'][1].__setitem__(2, 'unknown')),
            ('state-width', lambda x: x['events'][5].__setitem__(6, [1, 0])),
            ('bad-hop', lambda x: x['events'][4].__setitem__(5, [16])),
            ('bulk-load', lambda x: x['events'][1].__setitem__(1, 6)),
            ('outside', lambda x: x['events'][-1].__setitem__(0, 4800)),
        ]:
            invalid = copy.deepcopy(fixture)
            mutate(invalid)
            run(invalid, label, False)
    print('SGREC replay protocol: 8 cases passed')


if __name__ == '__main__':
    main()
