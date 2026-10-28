import io
import json
import math
import os
from pathlib import Path
import struct
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import sgrec
from test_sgrec import header_bytes, checksum_record, completion, parameter_group, x_fixtures


class TimelineTests(unittest.TestCase):
    def reader(self, payload, frames=4, encoding=1, groups=()):
        header = json.loads((x_fixtures / 'golden.json').read_text())['header']
        header.update(format_version=6, parameters=['global_phase_unmodulated'], initial_patch={'blend': 0.25})
        data = struct.pack('<BI', encoding, len(payload)) + payload
        data += struct.pack('<I', len(groups)) + b''.join(groups)
        record = checksum_record(struct.pack('<4sIQIH', b'BLK6', 26 + len(data), 0, frames, 0) + data)
        return sgrec.Reader(io.BytesIO(header_bytes(header) + record + completion(frames)))

    def test_independent_double_goldens_and_transport_preserve_patch_queries(self):
        payload = struct.pack('<d', 1.0) + b'\x02\x00\x02'
        groups = [parameter_group(8, 0, '', [struct.pack('<II', 0, 0)]),
                  parameter_group(9, 0, '', [struct.pack('<II', 3, 1)])]
        reader = self.reader(payload, groups=groups)
        blocks = list(reader.Blocks((), include_events=True, include_parameters=True))
        self.assertTrue(reader.m_complete)
        self.assertEqual(blocks[0].m_tracks, {})
        self.assertEqual(list(blocks[0].m_parameters['global_phase_unmodulated']),
                         [1.0 + index * 2**-52 for index in range(4)])
        events = list(sgrec.param_events(blocks[0], 6))
        self.assertEqual([(event.m_type, event.m_sample, event.m_value) for event in events],
                         [(8, 0, True), (9, 3, False)])
        self.assertEqual(reader.PatchAtSample(3), {'blend': 0.25})
        self.assertEqual(list(self.reader(payload).Blocks())[0].m_parameters, {})

    def test_constant_raw_extremes_and_signed_zero_are_exact(self):
        payload = struct.pack('<d', -0.0) + b'\0\x03'
        decoded = sgrec.decode_double_stream(payload, 4, 1)
        self.assertEqual(struct.pack('<4d', *decoded), struct.pack('<d', -0.0) * 4)
        expected = [sys.float_info.max, -sys.float_info.max, math.ulp(0.0), -0.0]
        raw = struct.pack('<4d', *expected)
        block, = self.reader(raw, encoding=0).Blocks((), include_parameters=True)
        self.assertEqual(struct.pack('<4d', *block.m_parameters['global_phase_unmodulated']), raw)

    def test_bad_predictors_lengths_nonfinite_and_varints_are_rejected(self):
        first = struct.pack('<d', 1.0)
        cases = [first, first + b'\0', first + b'\0\0', first + b'\0\x04',
                 first + b'\x80\x00', first + b'\xff' * 9 + b'\x02',
                 first + b'\0\x03\0', struct.pack('<d', math.inf) + b'\0\x03',
                 struct.pack('<d', math.nan) + b'\0\x03']
        for payload in cases:
            with self.subTest(payload=payload.hex()), self.assertRaises(sgrec.RecordingError):
                list(self.reader(payload).Blocks((), include_parameters=True))
        for encoding, payload in [(3, first), (0, first), (1, b'\0' * 7)]:
            with self.subTest(encoding=encoding), self.assertRaises(sgrec.RecordingError):
                list(self.reader(payload, encoding=encoding).Blocks(()))

    def test_transport_requires_v6_empty_names_and_zero_width(self):
        for width, name in [(1, ''), (0, 'start')]:
            group = parameter_group(8, width, name, [struct.pack('<II', 0, 0)])
            block, = self.reader(struct.pack('<d', 0.0) + b'\0\x03', groups=[group]).Blocks((), include_events=True)
            with self.assertRaises(sgrec.RecordingError):
                list(sgrec.param_events(block, 6))
        group = parameter_group(8, 0, '', [struct.pack('<II', 0, 0)])
        block = sgrec.Block(0, 4, {}, struct.pack('<I', 1) + group)
        with self.assertRaises(sgrec.RecordingError):
            list(sgrec.param_events(block, 5))

    def test_parameter_metadata_and_decoded_memory_are_bounded(self):
        base = json.loads((x_fixtures / 'golden.json').read_text())['header']
        for names in [None, {}, ['x', 'x'], [''], [1], ['x'] * 129]:
            header = dict(base, format_version=6, parameters=names)
            with self.subTest(names=names), self.assertRaises(sgrec.RecordingError):
                sgrec.Reader(io.BytesIO(header_bytes(header)))
        header = dict(base, format_version=6, parameters=['phase'], block_frames=sgrec.x_max_decoded_bytes // 40)
        with self.assertRaises(sgrec.RecordingError):
            sgrec.Reader(io.BytesIO(header_bytes(header)))

    @unittest.skipUnless(os.environ.get('SMARTGRID_DOUBLE_CODEC_FIXTURE'), 'Requires C++ codec fixture')
    def test_cpp_codec_is_bit_exact_across_ramps_resets_and_random_finite_doubles(self):
        data = Path(os.environ['SMARTGRID_DOUBLE_CODEC_FIXTURE']).read_bytes()
        offset = 0
        sizes = []
        while offset < len(data):
            frames, encoding, length = struct.unpack_from('<IBI', data, offset)
            offset += 9
            decoded = sgrec.decode_double_stream(data[offset:offset + length], frames, encoding)
            offset += length
            self.assertEqual(struct.pack(f'<{frames}d', *decoded), data[offset:offset + frames * 8])
            offset += frames * 8
            sizes.append(length)
        self.assertEqual(len(sizes), 6)
        self.assertLess(max(sizes[:3]), 1024)
        print(f'\nDouble codec bytes per 48000 frames (ramps, resets, random, extremes): {sizes}')

    @unittest.skipUnless(os.environ.get('SMARTGRID_TIMELINE_FIXTURE'), 'Requires C++ recorder fixture')
    def test_cpp_recorder_block_boundaries_partial_tail_and_restart(self):
        with open(os.environ['SMARTGRID_TIMELINE_FIXTURE'], 'rb') as source:
            reader = sgrec.Reader(source)
            blocks = list(reader.Blocks((), include_events=True, include_parameters=True))
            self.assertTrue(reader.m_complete)
        self.assertEqual([block.m_frame_count for block in blocks], [8, 8, 3])
        values = [value for block in blocks for value in block.m_parameters['global_phase_unmodulated']]
        self.assertEqual(values, [1048576 + frame * 0.125 if frame < 8 or frame >= 16 else 0.0 for frame in range(19)])
        events = [event for block in blocks for event in sgrec.param_events(block, 6)]
        self.assertEqual([(event.m_type, event.m_sample) for event in events], [(8, 0), (9, 8), (8, 16)])

    @unittest.skipUnless(os.environ.get('SMARTGRID_ENGINE_TIMELINE_FIXTURE'), 'Requires C++ engine fixture')
    def test_engine_timeline_matches_every_audio_sample(self):
        path = os.environ['SMARTGRID_ENGINE_TIMELINE_FIXTURE']
        expected = Path(path + '.expected').read_bytes()
        with open(path, 'rb') as source:
            reader = sgrec.Reader(source)
            values = []
            events = []
            for block in reader.Blocks((), include_events=True, include_parameters=True):
                values.extend(block.m_parameters['global_phase_unmodulated'])
                events.extend(sgrec.param_events(block, 6))
            self.assertTrue(reader.m_complete)
        expected_values = bytearray()
        transitions = []
        previous = None
        for frame, offset in enumerate(range(0, len(expected), 9)):
            expected_values.extend(expected[offset:offset + 8])
            running = bool(expected[offset + 8])
            if running != previous:
                transitions.append((8 if running else 9, frame))
            previous = running
        self.assertEqual(struct.pack(f'<{len(values)}d', *values), expected_values)
        self.assertEqual([(event.m_type, event.m_sample) for event in events if event.m_type in (8, 9)], transitions)
        self.assertGreaterEqual(len(transitions), 5)
        self.assertGreater(max(values), 0.0)


if __name__ == '__main__':
    unittest.main()
