import array
from contextlib import redirect_stdout
import copy
import importlib
import io
import json
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import wave
import sys
import unittest
from unittest import mock
import types
import tracemalloc
import zlib

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
x_fixtures = Path(__file__).resolve().parents[2] / 'private/test/fixtures/streaming-recording'


def load_reader(test):
    try:
        return importlib.import_module('sgrec')
    except ModuleNotFoundError:
        test.fail('The bounded streaming reader is not implemented')


def header_bytes(header):
    body = json.dumps(header, separators=(',', ':')).encode()
    return b'SMRTGRID' + struct.pack('<I', len(body)) + body


def checksum_record(body):
    return body + struct.pack('<I', zlib.crc32(body))


def data_record(start, frames, descriptors=b'', payload=b'', count=0):
    return checksum_record(struct.pack('<4sIQIH', b'BLK1', 26 + len(descriptors) + len(payload), start, frames, count) + descriptors + payload)


def completion(frames):
    return checksum_record(struct.pack('<4sQ', b'END1', frames))



def event_group(name, width, entries, event_type=1):
    name_bytes = name.encode('utf-8')
    return (struct.pack('<BBHI', event_type, width, len(name_bytes), len(entries)) + name_bytes
            + b''.join(struct.pack('<IB', offset, scene) + value for offset, scene, value in entries))


def event_record(start, frames, groups):
    events = struct.pack('<I', len(groups)) + b''.join(groups)
    return checksum_record(struct.pack('<4sIQIH', b'BLK2', 26 + len(events), start, frames, 0) + events)


def parameter_group(event_type, width, name, entries):
    encoded = name.encode('utf-8')
    return struct.pack('<BBHI', event_type, width, len(encoded), len(entries)) + encoded + b''.join(entries)


def ordered_record(start, frames, groups):
    events = struct.pack('<I', len(groups)) + b''.join(groups)
    return checksum_record(struct.pack('<4sIQIH', b'BLK3', 26 + len(events), start, frames, 0) + events)


def patch_entry(sample, order, patch, restore_faders=None):
    body = json.dumps(patch, separators=(',', ':')).encode('utf-8')
    flags = b'' if restore_faders is None else bytes([restore_faders])
    return struct.pack('<II', sample, order) + flags + struct.pack('<I', len(body)) + body


class PatchLoadReplayTests(unittest.TestCase):
    def setUp(self):
        self.m_header = json.loads((x_fixtures / 'golden.json').read_text())['header']
        self.m_values = {'values': {'values': [[0.25, 0.5] for _ in range(8)]}}
        self.m_patch = {
            'nonagon': {'Counter': [7] * 16, 'Mute_0': [0] * 8},
            'stateSaver': {'activeTrio': [0] * 4,
                           **{f'sourceWidth_{i}': [0] * 4 for i in range(4)},
                           **{f'sourceMonitor_{i}': [1] for i in range(4)},
                           **{f'sourceSelected_{trio}_{i}': [int(i == 0)] for trio in range(3) for i in range(4)}},
            'configGrid': {'sourceStereo': [False] * 4,
                           'sourceSelected': [[True, False, False, False] for _ in range(3)],
                           'sampleDirectoryRelative': ['kept'] * 9},
            'squiggleBoy': {'Carrier': self.m_values, 'Untouched': self.m_values},
            'faders': [0.125] * 16, 'blend': 0.25,
        }
        self.m_header.update(format_version=3, initial_patch=self.m_patch)

    def reader(self, groups):
        return load_reader(self).Reader(io.BytesIO(header_bytes(self.m_header)
                                                   + ordered_record(0, 4, groups) + completion(4)))

    def test_capture_order_preserves_same_sample_edits_around_multiple_loads(self):
        groups = [
            parameter_group(1, 1, 'Mute_0', [struct.pack('<IIBB', 1, 0, 0, 1),
                                            struct.pack('<IIBB', 1, 4, 0, 1)]),
            parameter_group(2, 4, '', [struct.pack('<IIBf', 1, 2, 3, 0.75)]),
            parameter_group(4, 4, 'Carrier', [struct.pack('<IIBBBf', 1, 5, 0, 1, 0, 0.875)]),
            parameter_group(6, 0, '', [
                patch_entry(1, 1, {'nonagon': {'Mute_0': [0]}, 'faders': [0.5] * 16, 'blend': 0.75}, True),
                patch_entry(1, 3, {'nonagon': {'Mute_0': [0]}, 'faders': [0.0] * 16, 'blend': 0.0,
                                   'squiggleBoy': {'Carrier': {'values': {'values': [[0.5, 0.25] for _ in range(8)]}}}}, False),
            ]),
        ]
        reader = self.reader(groups)
        self.assertEqual(reader.PatchAtSample(0), self.m_patch)
        patch = reader.PatchAtSample(1)
        self.assertEqual(patch['nonagon']['Mute_0'], [1, 0, 0, 0, 0, 0, 0, 0])
        self.assertEqual(patch['faders'], [0.5] * 3 + [0.75] + [0.5] * 12)
        self.assertEqual(patch['blend'], 0.75)
        self.assertEqual(patch['squiggleBoy']['Carrier']['values']['values'][0], [0.5, 0.875])
        self.assertEqual(reader.PatchAtSample(0), self.m_patch)

    def test_partial_load_preserves_missing_fields_and_replaces_provided_encoder_children(self):
        carrier = copy.deepcopy(self.m_values)
        carrier['modulators'] = [copy.deepcopy(self.m_values)] + [None] * 14
        carrier['gestures'] = [dict(copy.deepcopy(self.m_values), active=[True] * 128)] + [None] * 15
        self.m_patch['squiggleBoy']['Carrier'] = carrier
        loaded = {'nonagon': {'Counter': [255, 128, 3], 'unknown': [4]},
                  'stateSaver': {'activeTrio': [2]}, 'squiggleBoy': {'Carrier': {}, 'unknown': {}},
                  'faders': [1.0], 'sampleAsset': 'ignored'}
        patch = self.reader([parameter_group(6, 0, '', [patch_entry(1, 0, loaded, True)])]).PatchAtSample(1)
        self.assertEqual(patch['nonagon']['Counter'], [-1, -128, 3, 0] + [7] * 12)
        self.assertEqual(patch['stateSaver']['activeTrio'], [2, 0, 0, 0])
        self.assertEqual(patch['squiggleBoy']['Carrier'], self.m_values)
        self.assertEqual(patch['squiggleBoy']['Untouched'], self.m_values)
        self.assertEqual(patch['faders'], [0.125] * 16)
        self.assertEqual(patch['blend'], 0.25)
        self.assertEqual(set(patch), set(self.m_patch))
        self.assertNotIn('unknown', patch['nonagon'])
        self.assertNotIn('unknown', patch['squiggleBoy'])

    def test_loaded_nested_children_are_garbage_collected_like_the_engine(self):
        zero = {'values': {'values': [[0.0, 0.0] for _ in range(8)]}}
        active = [False] * 128
        active[17] = True
        gesture = dict(self.m_values, active=active)
        nested = dict(zero, gestures=[gesture, dict(self.m_values, active=[False] * 128)])
        loaded = {'squiggleBoy': {'Carrier': dict(self.m_values, modulators=[zero, nested])}}
        patch = self.reader([parameter_group(6, 0, '', [patch_entry(1, 0, loaded, False)])]).PatchAtSample(1)
        modulators = patch['squiggleBoy']['Carrier']['modulators']
        self.assertEqual(len(modulators), 15)
        self.assertIsNone(modulators[0])
        self.assertEqual(modulators[1]['gestures'], [gesture] + [None] * 15)
        self.assertEqual(modulators[1]['values'], zero['values'])

    def test_same_sample_load_removes_old_gesture_before_new_activation(self):
        groups = [
            parameter_group(3, 4, '', [struct.pack('<IIf', 1, 5, 0.625)]),
            parameter_group(4, 4, 'Carrier', [struct.pack('<IIBBBBf', 1, 0, 0, 1, 1, 128, 0.75),
                                             struct.pack('<IIBBBBf', 1, 3, 0, 1, 1, 129, 0.5)]),
            parameter_group(5, 1, 'Carrier', [struct.pack('<IIBBBBB', 1, 1, 0, 1, 1, 128, 1),
                                             struct.pack('<IIBBBBB', 1, 4, 0, 1, 1, 129, 1)]),
            parameter_group(6, 0, '', [patch_entry(1, 2, {'squiggleBoy': {'Carrier': {}}, 'blend': 0.0}, True)]),
        ]
        patch = self.reader(groups).PatchAtSample(1)
        gestures = patch['squiggleBoy']['Carrier']['gestures']
        self.assertIsNone(gestures[0])
        self.assertTrue(gestures[1]['active'][1])
        self.assertEqual(gestures[1]['values']['values'][0], [0.0, 0.5])
        self.assertEqual(patch['blend'], 0.625)

    def test_config_aliases_override_state_and_normalize_channel_limit(self):
        loaded = {'stateSaver': {'sourceWidth_0': [0] * 4, 'sourceMonitor_0': [1], 'sourceSelected_1_2': [1]},
                  'configGrid': {'sourceStereo': [True, True], 'sourceSelected': [[True] * 4],
                                 'sourceMonitor': [False], 'sampleDirectoryRelative': ['excluded']}}
        patch = self.reader([parameter_group(6, 0, '', [patch_entry(1, 0, loaded, False)])]).PatchAtSample(1)
        self.assertEqual(patch['stateSaver']['sourceWidth_0'], [1, 0, 0, 0])
        self.assertEqual(patch['stateSaver']['sourceMonitor_0'], [0])
        self.assertEqual(patch['configGrid']['sourceStereo'], [True, True, False, False])
        self.assertEqual(patch['configGrid']['sourceSelected'], [[False, False, True, True], [False] * 4, [False] * 4])
        self.assertEqual(patch['stateSaver']['sourceSelected_1_2'], [0])
        self.assertEqual(patch['stateSaver']['sourceSelected_0_2'], [1])
        self.assertEqual(patch['configGrid']['sampleDirectoryRelative'], ['kept'] * 9)
        self.assertNotIn('sourceMonitor', patch['configGrid'])

    def test_present_config_without_selection_clears_selection_and_state_only_updates_aliases(self):
        groups = [parameter_group(6, 0, '', [
            patch_entry(1, 0, {'configGrid': {}}, False),
            patch_entry(2, 1, {'stateSaver': {'sourceWidth_1': [1], 'sourceSelected_1_2': [1]}}, False),
        ])]
        reader = self.reader(groups)
        self.assertEqual(reader.PatchAtSample(1)['configGrid']['sourceSelected'], [[False] * 4 for _ in range(3)])
        patch = reader.PatchAtSample(2)
        self.assertEqual(patch['configGrid']['sourceStereo'], [False, True, False, False])
        self.assertEqual(patch['configGrid']['sourceSelected'][1], [False, False, True, False])

    def test_snapshot_replaces_complete_patch_before_later_same_sample_edits(self):
        snapshot = {'faders': [0.0] * 16, 'blend': 0.0, 'nonagon': {'Mute_0': [0] * 8}}
        groups = [parameter_group(1, 1, 'Mute_0', [struct.pack('<IIBB', 2, 1, 0, 1)]),
                  parameter_group(7, 0, '', [patch_entry(2, 0, snapshot)])]
        reader = self.reader(groups)
        self.assertEqual(reader.PatchAtSample(1), self.m_patch)
        patch = reader.PatchAtSample(2)
        self.assertEqual(set(patch), set(snapshot))
        self.assertEqual(patch['nonagon']['Mute_0'], [1, 0, 0, 0, 0, 0, 0, 0])

    def test_bad_patch_records_reject_replay_without_affecting_audio(self):
        sgrec = load_reader(self)
        bad_entries = [
            struct.pack('<I', 0),
            struct.pack('<IIBI', 0, 0, 2, 2) + b'{}',
            struct.pack('<IIBI', 0, 0, 1, 20) + b'{}',
            struct.pack('<IIBI', 0, 0, 1, 1) + b'\xff',
            patch_entry(0, 0, [], True),
            patch_entry(4, 0, {}, True),
            patch_entry(0, 0, {'blend': float('nan')}, True),
            patch_entry(0, 0, {'blend': 'invalid'}, True),
        ]
        for entry in bad_entries:
            groups = [parameter_group(6, 0, '', [entry])]
            with self.subTest(entry=entry), self.assertRaises(sgrec.RecordingError):
                self.reader(groups).PatchAtSample(0)
            self.assertEqual(len(list(self.reader(groups).Blocks())), 1)
        for event_type, width, name in ((6, 4, ''), (7, 0, 'named')):
            with self.subTest(event_type=event_type), self.assertRaises(sgrec.RecordingError):
                self.reader([parameter_group(event_type, width, name, [patch_entry(0, 0, {})])]).PatchAtSample(0)
        with self.assertRaises(sgrec.RecordingError):
            self.reader([parameter_group(7, 0, '', [patch_entry(0, 0, {})[:-1]])]).PatchAtSample(0)


class ParamReplayTests(unittest.TestCase):
    def setUp(self):
        self.m_header = json.loads((x_fixtures / 'golden.json').read_text())['header']
        self.m_patch = {'faders': [0.0] * 16, 'blend': 0.125,
                        'squiggleBoy': {'Carrier': {'values': {'values': [[0.25, 0.25] for _ in range(8)]}}}}
        self.m_header.update(format_version=2, initial_patch=self.m_patch)

    def reader(self, groups):
        sgrec = load_reader(self)
        return sgrec.Reader(io.BytesIO(header_bytes(self.m_header) + event_record(0, 4, groups) + completion(4)))

    def test_all_parameter_types_replay_nested_encoder_values_and_activation(self):
        groups = [
            parameter_group(2, 4, '', [struct.pack('<IBf', 0, 3, 0.25), struct.pack('<IBf', 3, 3, 0.75)]),
            parameter_group(3, 4, '', [struct.pack('<If', 1, 0.5)]),
            parameter_group(4, 4, 'Carrier', [
                struct.pack('<IBBBf', 1, 2, 1, 0, 0.75),
                struct.pack('<IBBBBBf', 2, 2, 1, 2, 1, 3, -0.5),
                struct.pack('<IBBBBBf', 3, 1, 0, 2, 1, 128, 0.25)]),
            parameter_group(5, 1, 'Carrier', [
                struct.pack('<IBBBBB', 2, 2, 1, 1, 130, 1),
                struct.pack('<IBBBBBB', 3, 1, 0, 2, 1, 128, 1)]),
        ]
        reader = self.reader(groups)
        patch = reader.PatchAtSample(2)
        self.assertEqual(patch['faders'][3], 0.25)
        self.assertEqual(patch['blend'], 0.5)
        carrier = patch['squiggleBoy']['Carrier']
        self.assertEqual(carrier['values']['values'][2], [0.25, 0.75])
        gesture = carrier['gestures'][2]
        self.assertTrue(gesture['active'][33])
        self.assertFalse(gesture['active'][32])
        self.assertEqual(carrier['modulators'][1]['modulators'][3]['values']['values'][2], [0.0, -0.5])
        final = reader.PatchAtSample(3)
        self.assertEqual(final['faders'][3], 0.75)
        nested_gesture = final['squiggleBoy']['Carrier']['modulators'][1]['gestures'][0]
        self.assertEqual(nested_gesture['values']['values'][1], [0.25, 0.0])
        self.assertTrue(nested_gesture['active'][16])
        self.assertEqual(reader.PatchAtSample(0)['blend'], 0.125)
        self.assertEqual(reader.m_header['initial_patch'], self.m_patch)

    def test_bad_parameter_payloads_reject_replay_but_do_not_affect_audio(self):
        sgrec = load_reader(self)
        bad_groups = [
            parameter_group(2, 4, '', [struct.pack('<IBf', 0, 16, 0.5)]),
            parameter_group(3, 4, '', [struct.pack('<If', 0, float('nan'))]),
            parameter_group(4, 4, 'Carrier', [struct.pack('<IBBB', 0, 0, 0, 17)]),
            parameter_group(4, 4, 'Carrier', [struct.pack('<IBBBBf', 0, 0, 0, 1, 127, 0.5)]),
            parameter_group(5, 1, 'Carrier', [struct.pack('<IBBBBB', 0, 0, 0, 1, 128, 2)]),
        ]
        for group in bad_groups:
            with self.subTest(group=group), self.assertRaises(sgrec.RecordingError):
                self.reader([group]).PatchAtSample(0)
            self.assertEqual(len(list(self.reader([group]).Blocks())), 1)


class StateReplayTests(unittest.TestCase):
    def setUp(self):
        self.m_header = json.loads((x_fixtures / 'golden.json').read_text())['header']
        self.m_patch = {
            'nonagon': {'Mute_0': [0] * 8, 'Counter': [0] * 8},
            'stateSaver': {'activeTrio': [0] * 4, 'sourceWidth_0': [0] * 4, 'sourceSelected_0_1': [0]},
            'configGrid': {'sourceStereo': [False], 'sourceSelected': [[True, False]]},
            'squiggleBoy': {'untouched': [1, 2, 3]},
        }
        self.m_header.update(format_version=2, initial_patch=self.m_patch)
        self.m_records = [
            event_record(0, 4, [
                event_group('Counter', 2, [(2, 1, bytes([255, 128]))]),
                event_group('Mute_0', 1, [(1, 0, b'\x01'), (1, 0, b'\x00'), (1, 0, b'\x01'), (3, 2, b'\x01')]),
                event_group('activeTrio', 4, [(3, 0, b'\x02\x00\x00\x00')]),
                event_group('sourceSelected_0_1', 1, [(2, 0, b'\x01')]),
                event_group('sourceWidth_0', 4, [(2, 0, b'\x01\x00\x00\x00')]),
            ]),
            event_record(4, 2, [event_group('Mute_0', 1, [(0, 0, b'\x00')])]),
        ]
        self.m_data = header_bytes(self.m_header) + b''.join(self.m_records) + completion(6)

    def test_patch_queries_apply_through_sample_and_restart_from_initial_patch(self):
        sgrec = load_reader(self)
        reader = sgrec.Reader(io.BytesIO(self.m_data))
        self.assertEqual(reader.PatchAtSample(0), self.m_patch)
        self.assertEqual(reader.PatchAtSample(1)['nonagon']['Mute_0'], [1, 0, 0, 0, 0, 0, 0, 0])
        patch = reader.PatchAtSample(3)
        self.assertEqual(patch['nonagon']['Mute_0'], [1, 0, 1, 0, 0, 0, 0, 0])
        self.assertEqual(patch['nonagon']['Counter'], [0, 0, -1, -128, 0, 0, 0, 0])
        self.assertEqual(patch['stateSaver']['activeTrio'], [2, 0, 0, 0])
        self.assertEqual(patch['configGrid'], {'sourceStereo': [True], 'sourceSelected': [[True, True]]})
        self.assertEqual(patch['squiggleBoy'], self.m_patch['squiggleBoy'])
        self.assertEqual(reader.PatchAtSample(4)['nonagon']['Mute_0'], [0, 0, 1, 0, 0, 0, 0, 0])
        self.assertEqual(reader.PatchAtSample(0), self.m_patch)
        self.assertEqual(reader.m_header['initial_patch'], self.m_patch)

    def test_audio_extraction_ignores_patch_and_event_semantics(self):
        sgrec = load_reader(self)
        header = dict(self.m_header)
        header['initial_patch'] = 'not a patch'
        unknown = event_record(0, 4, [event_group('future', 1, [(0, 0, b'\x01')], event_type=99)])
        reader = sgrec.Reader(io.BytesIO(header_bytes(header) + unknown + completion(4)))
        blocks = list(reader.Blocks())
        self.assertTrue(reader.m_complete)
        self.assertEqual(blocks[0].m_frame_count, 4)
        self.assertTrue(all(value == 0 for streams in blocks[0].m_tracks.values() for stream in streams for value in stream))
        del header['initial_patch']
        self.assertEqual(len(list(sgrec.Reader(io.BytesIO(header_bytes(header) + unknown + completion(4))).Blocks())), 1)
        with self.assertRaises(sgrec.RecordingError):
            sgrec.Reader(io.BytesIO(header_bytes(self.m_header) + unknown + completion(4))).PatchAtSample(0)

    def test_patch_query_requires_recorded_sample_but_not_later_clean_completion(self):
        sgrec = load_reader(self)
        reader = sgrec.Reader(io.BytesIO(self.m_data))
        for sample in (-1, True, 6):
            with self.subTest(sample=sample), self.assertRaises(sgrec.RecordingError):
                reader.PatchAtSample(sample)
        truncated = header_bytes(self.m_header) + self.m_records[0]
        self.assertEqual(sgrec.Reader(io.BytesIO(truncated)).PatchAtSample(1)['nonagon']['Mute_0'][0], 1)
        with self.assertRaises(sgrec.RecordingError):
            sgrec.Reader(io.BytesIO(truncated)).PatchAtSample(4)
        empty = sgrec.Reader(io.BytesIO(header_bytes(self.m_header) + completion(0)))
        self.assertEqual(empty.PatchAtSample(0), self.m_patch)

    def test_patch_replay_checks_truncated_delta_and_whole_block_crc(self):
        sgrec = load_reader(self)
        malformed = event_record(0, 4, [event_group('Mute_0', 1, [(0, 0, b'')])])
        with self.assertRaises(sgrec.RecordingError):
            sgrec.Reader(io.BytesIO(header_bytes(self.m_header) + malformed + completion(4))).PatchAtSample(0)
        corrupted = bytearray(self.m_records[0])
        corrupted[-5] ^= 1
        for replay in (False, True):
            with self.subTest(replay=replay), self.assertRaises(sgrec.RecordingError):
                reader = sgrec.Reader(io.BytesIO(header_bytes(self.m_header) + corrupted + completion(4)))
                reader.PatchAtSample(0) if replay else list(reader.Blocks())

    def test_patch_cli_writes_json_at_requested_sample(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / 'take.sgrec'
            output = Path(directory) / 'patch.json'
            source.write_bytes(self.m_data)
            result = subprocess.run([sys.executable, str(Path(__file__).resolve().parents[1] / 'extract_recording.py'),
                                     'patch', str(source), '--sample', '1', '-o', str(output)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(json.loads(output.read_text())['nonagon']['Mute_0'][0], 1)


class ReaderTests(unittest.TestCase):
    def setUp(self):
        self.m_expected = json.loads((x_fixtures / 'golden.json').read_text())
        self.m_header = self.m_expected['header']
        self.m_prefix = header_bytes(self.m_header)
        self.m_records = [bytes.fromhex(block['record_hex']) for block in self.m_expected['blocks']]

    def test_independent_golden_preserves_all_streams_and_silent_timeline(self):
        sgrec = load_reader(self)
        with (x_fixtures / 'golden.sgrec').open('rb') as source:
            reader = sgrec.Reader(source)
            self.assertEqual(reader.m_header, self.m_header)
            blocks = list(reader.Blocks())
        self.assertTrue(reader.m_complete)
        self.assertEqual(reader.m_total_frames, 9)
        for actual, expected in zip(blocks, self.m_expected['blocks'], strict=True):
            self.assertEqual(actual.m_start_frame, expected['start_frame'])
            self.assertEqual(actual.m_frame_count, expected['frame_count'])
            self.assertEqual({str(key): [list(stream) for stream in streams] for key, streams in actual.m_tracks.items()}, expected['tracks'])
            for streams in actual.m_tracks.values():
                for stream in streams:
                    self.assertIsInstance(stream, array.array)
                    self.assertEqual(stream.itemsize, 4)

    def test_selected_track_decodes_only_that_track(self):
        sgrec = load_reader(self)
        with (x_fixtures / 'golden.sgrec').open('rb') as source:
            reader = sgrec.Reader(source)
            with mock.patch.object(sgrec, 'decode_stream', wraps=sgrec.decode_stream) as decode:
                blocks = list(reader.Blocks({7}))
        self.assertTrue(reader.m_complete)
        self.assertEqual(decode.call_count, 4)
        for actual, expected in zip(blocks, self.m_expected['blocks'], strict=True):
            self.assertEqual(set(actual.m_tracks), {7})
            self.assertEqual(
                [list(stream) for stream in actual.m_tracks[7]],
                expected['tracks']['7'],
            )

    def test_corrupt_block_never_exposes_even_its_valid_selected_track(self):
        sgrec = load_reader(self)
        corrupted = bytearray(self.m_records[0])
        corrupted[-1] ^= 1
        reader = sgrec.Reader(io.BytesIO(self.m_prefix + corrupted + completion(4)))
        with self.assertRaisesRegex(sgrec.RecordingError, 'CRC'):
            next(reader.Blocks({7}))
        self.assertEqual(reader.m_total_frames, 0)
        self.assertFalse(reader.m_complete)

    def test_clean_empty_session(self):
        sgrec = load_reader(self)
        reader = sgrec.Reader(io.BytesIO(self.m_prefix + completion(0)))
        self.assertEqual(list(reader.Blocks()), [])
        self.assertTrue(reader.m_complete)

    def test_damaged_suffix_keeps_only_whole_contiguous_blocks(self):
        sgrec = load_reader(self)
        cases = {
            'missing completion': self.m_records[0],
            'truncated block': self.m_records[0] + self.m_records[1][:-1],
            'missing whole block': self.m_records[0] + data_record(8, 4) + completion(12),
            'short nonfinal': data_record(0, 2) + data_record(2, 4) + completion(6),
            'wrong end count': self.m_records[0] + completion(3),
            'trailing bytes': self.m_records[0] + completion(4) + b'junk',
            'bad end CRC': self.m_records[0] + completion(4)[:-1] + b'\xff',
        }
        for name, data in cases.items():
            with self.subTest(name=name):
                reader = sgrec.Reader(io.BytesIO(self.m_prefix + data))
                blocks = reader.Blocks()
                first = next(blocks)
                self.assertEqual(first.m_start_frame, 0)
                with self.assertRaises(sgrec.RecordingError):
                    next(blocks)
                self.assertFalse(reader.m_complete)

    def test_metadata_and_allocation_limits_fail_before_payload_reads(self):
        sgrec = load_reader(self)
        variants = [
            dict(self.m_header, format_version=4),
            dict(self.m_header, format_version=True),
            dict(self.m_header, git_commit_sha='short'),
            dict(self.m_header, recorded_at_utc='yesterday'),
            dict(self.m_header, sample_rate=0),
            dict(self.m_header, block_frames=0),
            dict(self.m_header, block_frames=2**32),
            dict(self.m_header, block_frames=64*1024*1024),
            dict(self.m_header, tracks=self.m_header['tracks']*2),
            dict(self.m_header, tracks=[dict(self.m_header['tracks'][0], type='surround')]),
            dict(self.m_header, tracks=[dict(self.m_header['tracks'][0], id=-1)]),
            dict(self.m_header, tracks=[dict(self.m_header['tracks'][0], role='master_stereo')]),
            dict(self.m_header, tracks=[dict(self.m_header['tracks'][0], id=index) for index in range(129)]),
        ]
        for header in variants:
            with self.subTest(header=header):
                with self.assertRaises(sgrec.RecordingError):
                    sgrec.Reader(io.BytesIO(header_bytes(header)))
        for bad in (b'BADMAGIC', b'SMRTGRID'+struct.pack('<I', 1048577), b'SMRTGRID'+struct.pack('<I', 1)+b'\xff'):
            with self.assertRaises(sgrec.RecordingError):
                sgrec.Reader(io.BytesIO(bad))
        reader = sgrec.Reader(io.BytesIO(self.m_prefix + b'BLK1' + struct.pack('<I', 64*1024*1024+1)))
        with self.assertRaisesRegex(sgrec.RecordingError, 'size|limit'):
            next(reader.Blocks())

    def test_invalid_descriptors_ranges_padding_and_derived_lengths(self):
        sgrec = load_reader(self)
        cases = [
            data_record(0, 4, struct.pack('<IBB', 999, 0, 24), b'\0'*12, 1),
            data_record(0, 4, struct.pack('<IBB', 1, 0, 23), b'\0'*12, 1),
            data_record(0, 4, struct.pack('<IBB', 1, 1, 26), b'\0'*13, 1),
            data_record(0, 4, struct.pack('<IBB', 1, 2, 24), b'\0'*12, 1),
            data_record(0, 4, struct.pack('<IBB', 1, 1, 1), bytes.fromhex('00000080'), 1),
            data_record(0, 4, struct.pack('<IBB', 1, 1, 2), bytes.fromhex('ffff7f01'), 1),
            data_record(0, 4, struct.pack('<IBB', 1, 0, 24), b'\0'*11, 1),
            data_record(0, 4, struct.pack('<IBB', 1, 1, 0)*2, b'\0'*6, 2),
            data_record(0, 4, struct.pack('<I', 3)+bytes([1,0,1,0,1,0]), bytes.fromhex('010000ffffff000000'), 1),
            data_record(0, 0), data_record(0, 5), data_record(1, 4),
        ]
        for data in cases:
            with self.subTest(record=data.hex()):
                reader = sgrec.Reader(io.BytesIO(self.m_prefix + data))
                with self.assertRaises(sgrec.RecordingError):
                    next(reader.Blocks())
                self.assertEqual(reader.m_total_frames, 0)

    def test_encoded_record_is_not_duplicated_while_reading(self):
        sgrec = load_reader(self)
        data = data_record(0, 4, payload=bytes(4*1024*1024))
        reader = sgrec.Reader(io.BytesIO(self.m_prefix + data))
        tracemalloc.start()
        try:
            with self.assertRaises(sgrec.RecordingError):
                next(reader.Blocks())
            _, peak = tracemalloc.get_traced_memory()
        finally:
            tracemalloc.stop()
        self.assertLess(peak, len(data) + 512*1024)

    def test_all_signed_delta_widths_are_accepted_including_nonminimal(self):
        sgrec = load_reader(self)
        for width in range(26):
            with self.subTest(width=width):
                if width == 25:
                    values = [-8388608, 8388607]
                    payload = bytes.fromhex('000080ffffff00')
                elif width == 0:
                    values = [123, 123]
                    payload = bytes.fromhex('7b0000')
                else:
                    delta = -(1 << (width-1))
                    values = [0, delta]
                    payload = b'\0'*3 + (delta & ((1 << width)-1)).to_bytes((width+7)//8, 'little')
                record = data_record(0, 2, struct.pack('<IBB', 1, 1, width), payload, 1)
                reader = sgrec.Reader(io.BytesIO(self.m_prefix + record + completion(2)))
                self.assertEqual(list(next(reader.Blocks()).m_tracks[1][0]), values)


def load_extractor(test):
    try:
        return importlib.import_module('extract_recording')
    except ModuleNotFoundError:
        test.fail('PCM24 recording extraction is not implemented')


def wav_chunks(path):
    data = path.read_bytes()
    chunks = {}
    offset = 12
    while offset + 8 <= len(data):
        tag, size = struct.unpack_from('<4sI', data, offset)
        chunks[tag] = data[offset + 8:offset + 8 + size]
        offset += 8 + size + size % 2
    return data, chunks


class ExtractionTests(unittest.TestCase):
    def setUp(self):
        self.m_directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.m_directory.cleanup)
        self.m_root = Path(self.m_directory.name)
        self.m_source = x_fixtures / 'golden.sgrec'
        self.m_expected = json.loads((x_fixtures / 'golden.json').read_text())

    def test_pcm24_packing_interleaves_signed_channels(self):
        extractor = load_extractor(self)
        streams = (
            array.array('i', [1, -2, 0x123456]),
            array.array('i', [3, -4, -0x123456]),
        )
        self.assertEqual(
            extractor.pack_pcm24(streams, 0, 3),
            bytes.fromhex('010000030000fefffffcffff563412aacbed'),
        )

    def test_direct_master_and_stems_preserve_pcm_order_and_true_length(self):
        extractor = load_extractor(self)
        cases = [('mono', {'track_id': 1}, 1, 1), ('panned', {'track_id': 3}, 3, 1), ('stereo', {'master': 'stereo'}, 7, 2), ('quad', {'master': 'quad'}, 9, 4)]
        for name, selector, track_id, channels in cases:
            with self.subTest(track=name):
                output = self.m_root / f'{name}.wav'
                result = extractor.extract(self.m_source, output, **selector)
                self.assertTrue(result.m_complete)
                self.assertEqual(result.m_frames, 9)
                data, chunks = wav_chunks(output)
                expected = bytearray()
                for block in self.m_expected['blocks']:
                    for index in range(block['frame_count']):
                        for stream in block['tracks'][str(track_id)][:channels]:
                            expected.extend(struct.pack('<i', stream[index])[:3])
                self.assertEqual(chunks[b'data'], expected)
                self.assertEqual(struct.unpack_from('<I', data, 4)[0], len(data)-8)
                self.assertEqual(struct.unpack_from('<HHI', chunks[b'fmt ']), (0xfffe if channels==4 else 1, channels, 48000))
                if channels == 4:
                    self.assertEqual(struct.unpack_from('<I', chunks[b'fmt '], 20)[0], 0)
                else:
                    with wave.open(str(output), 'rb') as wav:
                        self.assertEqual((wav.getnchannels(), wav.getsampwidth(), wav.getnframes()), (channels, 3, 9))

    def test_incomplete_suffix_produces_playable_validated_prefix_and_failure(self):
        extractor = load_extractor(self)
        prefix = header_bytes(self.m_expected['header'])
        first = bytes.fromhex(self.m_expected['blocks'][0]['record_hex'])
        bad_selected = data_record(4, 4, struct.pack('<I', 7)+bytes([1,1,1,1]), bytes.fromhex('0000008000000000'), 1)
        for suffix in (b'', b'BLK1'+b'\0'*5, bad_selected, completion(4)[:-1]):
            with self.subTest(suffix=suffix):
                source = self.m_root / 'damaged.sgrec'
                output = self.m_root / 'partial.wav'
                source.write_bytes(prefix + first + suffix)
                result = extractor.extract(source, output, master='stereo', overwrite=True)
                self.assertFalse(result.m_complete)
                self.assertEqual(result.m_frames, 4)
                self.assertTrue(result.m_error)
                with wave.open(str(output), 'rb') as wav:
                    self.assertEqual(wav.getnframes(), 4)
                    self.assertEqual(len(wav.readframes(9)), 24)

    def test_bad_header_or_missing_selector_creates_no_wav(self):
        extractor = load_extractor(self)
        sgrec = load_reader(self)
        output = self.m_root / 'absent.wav'
        broken = self.m_root / 'broken.sgrec'
        broken.write_bytes(b'SMRTGRID'+struct.pack('<I', 2)+b'{}')
        for source, selector in ((broken, {'master':'stereo'}), (self.m_source, {'track_id':999})):
            with self.subTest(selector=selector):
                with self.assertRaises(sgrec.RecordingError):
                    extractor.extract(source, output, **selector)
                self.assertFalse(output.exists())

    def test_overwrite_requires_explicit_opt_in_and_cannot_replace_input(self):
        extractor = load_extractor(self)
        output = self.m_root / 'existing.wav'
        output.write_bytes(b'preserve me')
        with self.assertRaises(FileExistsError):
            extractor.extract(self.m_source, output, master='stereo')
        self.assertEqual(output.read_bytes(), b'preserve me')
        extractor.extract(self.m_source, output, master='stereo', overwrite=True)
        self.assertEqual(output.read_bytes()[:4], b'RIFF')
        with self.assertRaises(ValueError):
            extractor.extract(self.m_source, self.m_source, master='stereo', overwrite=True)

    def test_clean_zero_frame_session_exports_empty_wav(self):
        extractor = load_extractor(self)
        source = self.m_root / 'empty.sgrec'
        source.write_bytes(header_bytes(self.m_expected['header']) + completion(0))
        output = self.m_root / 'empty.wav'
        result = extractor.extract(source, output, master='stereo')
        self.assertTrue(result.m_complete)
        with wave.open(str(output), 'rb') as wav:
            self.assertEqual(wav.getnframes(), 0)

    def test_riff_boundary_promotes_to_rf64_with_exact_64bit_sizes(self):
        extractor = load_extractor(self)
        for channels, header_length in ((1,80),(2,80),(4,104)):
            first_rf64_frames = (0xfffffffe - (header_length-8)) // (channels*3) + 1
            small = extractor.wav_header(48000, channels, first_rf64_frames-1)
            large = extractor.wav_header(48000, channels, first_rf64_frames)
            self.assertEqual(small[:4], b'RIFF')
            self.assertEqual(large[:4], b'RF64')
            self.assertEqual(len(large), header_length)
            self.assertEqual(large[12:16], b'ds64')
            riff_size, data_size, frame_count, table_length = struct.unpack_from('<QQQI', large, 20)
            self.assertEqual(frame_count, first_rf64_frames)
            self.assertEqual(data_size, first_rf64_frames*channels*3)
            self.assertEqual(riff_size, header_length-8+data_size+data_size%2)
            self.assertEqual(table_length, 0)
            self.assertEqual(large[4:8], b'\xff'*4)
            self.assertEqual(large[-4:], b'\xff'*4)

    def test_cli_info_is_header_only_and_verify_checks_completion(self):
        load_extractor(self)
        source = self.m_root / 'header_only.sgrec'
        source.write_bytes(header_bytes(self.m_expected['header']))
        script = str(Path(__file__).resolve().parents[1] / 'extract_recording.py')
        result = subprocess.run([sys.executable, script, 'info', str(source)], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        for field in ('48000', '0123456789abcdef0123456789abcdef01234567', 'panned_mono', 'sample_val', 'x', 'y', '(0, 1)', 'post_mastering_pre_master_volume'):
            self.assertIn(field, result.stdout)
        result = subprocess.run([sys.executable, script, 'info', str(source), '--verify'], capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('END1', result.stderr)
        result = subprocess.run([sys.executable, script, 'extract', str(source), '--master', 'stereo'], capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('0 frames', result.stderr)
        self.assertTrue((self.m_root / 'header_only_stereo.wav').exists())


def load_sync():
    modules = {}
    for name, members in (
        ('pymobiledevice3.services.house_arrest', ('HouseArrestService',)),
        ('ipad_device', ('connect_ipad', 'device_host', 'device_udid')),
    ):
        module = types.ModuleType(name)
        for member in members:
            setattr(module, member, None)
        modules[name] = module
    with mock.patch.dict(sys.modules, modules):
        return importlib.import_module('sync_ipad')


class FakeAfc:
    def __init__(self, data, early_eof=False):
        self.m_data = data
        self.m_offset = 0
        self.m_deleted = []
        self.m_closed = False
        self.m_early_eof = early_eof

    async def stat(self, path):
        return {'st_ifmt': 'S_IFREG', 'st_size': len(self.m_data)}

    async def fopen(self, path, mode):
        return 1

    async def fread(self, handle, count):
        if self.m_early_eof:
            return b''
        data = self.m_data[self.m_offset:self.m_offset+count]
        self.m_offset += len(data)
        return data

    async def fclose(self, handle):
        self.m_closed = True

    async def rm(self, path):
        self.m_deleted.append(path)


class SyncTests(unittest.IsolatedAsyncioTestCase):
    def setUp(self):
        self.m_sync = load_sync()
        self.m_directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.m_directory.cleanup)
        self.m_root = Path(self.m_directory.name)

    async def test_new_container_dispatches_by_magic_and_deletes_only_complete_download(self):
        afc = FakeAfc((x_fixtures/'golden.sgrec').read_bytes())
        with mock.patch.object(self.m_sync, 'MAC_RECORDINGS_DIR', self.m_root), mock.patch.object(self.m_sync, 'list_files_recursive', return_value=[('recording.bin', False, '/recording.bin')]), mock.patch.object(self.m_sync.subprocess, 'check_output', side_effect=AssertionError('Custom container passed to SoX')), redirect_stdout(io.StringIO()):
            transfers = await self.m_sync.download_recordings_from_ipad(afc, '/')
            self.assertEqual(afc.m_deleted, [])
            self.m_sync.extract_recording(transfers[0])
            await self.m_sync.delete_recording(afc, transfers[0])
        self.assertEqual(afc.m_deleted, ['/recording.bin'])
        self.assertTrue(afc.m_closed)
        self.assertEqual((self.m_root/'recording.bin').read_bytes(), afc.m_data)
        with wave.open(str(self.m_root/'recording_stereo.wav'), 'rb') as wav:
            self.assertEqual((wav.getnchannels(), wav.getnframes()), (2,9))

    async def test_playable_partial_export_keeps_remote_recording(self):
        afc = FakeAfc((x_fixtures/'golden.sgrec').read_bytes()[:-16])
        with mock.patch.object(self.m_sync, 'MAC_RECORDINGS_DIR', self.m_root), mock.patch.object(self.m_sync, 'list_files_recursive', return_value=[('partial.sgrec', False, '/partial.sgrec')]), mock.patch.object(self.m_sync.subprocess, 'check_output', side_effect=AssertionError('Custom container passed to SoX')), redirect_stdout(io.StringIO()):
            transfers = await self.m_sync.download_recordings_from_ipad(afc, '/')
            with self.assertRaisesRegex(RuntimeError, 'Extraction failed'):
                self.m_sync.extract_recording(transfers[0])
        self.assertEqual(afc.m_deleted, [])
        with wave.open(str(self.m_root/'partial_stereo.wav'), 'rb') as wav:
            self.assertEqual(wav.getnframes(), 9)

    async def test_retry_replaces_partial_sync_export_only_after_complete_download(self):
        complete = (x_fixtures/'golden.sgrec').read_bytes()
        first_block = 12 + struct.unpack_from('<I', complete, 8)[0]
        first_block_end = first_block + struct.unpack_from('<I', complete, first_block+4)[0]
        afc = FakeAfc(complete[:first_block_end])
        with mock.patch.object(self.m_sync, 'MAC_RECORDINGS_DIR', self.m_root), mock.patch.object(self.m_sync, 'list_files_recursive', return_value=[('retry.sgrec', False, '/retry.sgrec')]), redirect_stdout(io.StringIO()):
            transfers = await self.m_sync.download_recordings_from_ipad(afc, '/')
            with self.assertRaisesRegex(RuntimeError, 'Extraction failed'):
                self.m_sync.extract_recording(transfers[0])
            self.assertEqual(afc.m_deleted, [])
            with wave.open(str(self.m_root/'retry_stereo.wav'), 'rb') as wav:
                self.assertEqual(wav.getnframes(), 4)
            afc.m_data = complete
            afc.m_offset = 0
            transfers = await self.m_sync.download_recordings_from_ipad(afc, '/')
            self.m_sync.extract_recording(transfers[0])
            await self.m_sync.delete_recording(afc, transfers[0])
        self.assertEqual(afc.m_deleted, ['/retry.sgrec'])
        with wave.open(str(self.m_root/'retry_stereo.wav'), 'rb') as wav:
            self.assertEqual(wav.getnframes(), 9)

    async def test_retry_after_remote_delete_failure_reextracts_and_deletes(self):
        afc = FakeAfc((x_fixtures/'golden.sgrec').read_bytes())
        with mock.patch.object(self.m_sync, 'MAC_RECORDINGS_DIR', self.m_root), mock.patch.object(self.m_sync, 'list_files_recursive', return_value=[('retry.sgrec', False, '/retry.sgrec')]), redirect_stdout(io.StringIO()):
            transfers = await self.m_sync.download_recordings_from_ipad(afc, '/')
            self.m_sync.extract_recording(transfers[0])
            with mock.patch.object(afc, 'rm', side_effect=OSError('Connection lost during delete')), self.assertRaisesRegex(OSError, 'Connection lost during delete'):
                await self.m_sync.delete_recording(afc, transfers[0])
            self.assertEqual(afc.m_deleted, [])
            self.assertTrue((self.m_root/'retry_stereo.wav').exists())
            afc.m_offset = 0
            transfers = await self.m_sync.download_recordings_from_ipad(afc, '/')
            self.m_sync.extract_recording(transfers[0])
            await self.m_sync.delete_recording(afc, transfers[0])
        self.assertEqual(afc.m_deleted, ['/retry.sgrec'])
        with wave.open(str(self.m_root/'retry_stereo.wav'), 'rb') as wav:
            self.assertEqual(wav.getnframes(), 9)

    async def test_nonzero_extraction_status_keeps_remote_recording(self):
        afc = FakeAfc(b'RIFF'+bytes(4)+b'WAVE')
        transfer = await self.m_sync.download_recording(afc, '/failed.wav', self.m_root/'failed.wav')
        with self.assertRaisesRegex(RuntimeError, 'Extraction failed'), redirect_stdout(io.StringIO()):
            self.m_sync.extract_recording(transfer, mock.Mock(return_value=1))
        self.assertEqual(afc.m_deleted, [])

    async def test_remote_size_change_keeps_even_complete_recording(self):
        afc = FakeAfc((x_fixtures/'golden.sgrec').read_bytes())
        sizes = [{'st_size': len(afc.m_data)}, {'st_size': len(afc.m_data) + 1}]
        with mock.patch.object(afc, 'stat', side_effect=sizes), redirect_stdout(io.StringIO()):
            transfer = await self.m_sync.download_recording(afc, '/growing.sgrec', self.m_root/'growing.sgrec')
            self.m_sync.extract_recording(transfer)
            with self.assertRaisesRegex(OSError, 'changed size during transfer'):
                await self.m_sync.delete_recording(afc, transfer)
        self.assertEqual(afc.m_deleted, [])
        with wave.open(str(self.m_root/'growing_stereo.wav'), 'rb') as wav:
            self.assertEqual(wav.getnframes(), 9)

    async def test_truncated_download_raises_and_closes_instead_of_looping(self):
        afc = FakeAfc(b'promised bytes')
        with mock.patch.object(afc, 'fread', side_effect=[b'', AssertionError('Read again after premature EOF')]):
            with self.assertRaisesRegex(OSError, 'short read'):
                await self.m_sync.download_afc_file(afc, '/source', self.m_root/'copy')
        self.assertTrue(afc.m_closed)
        self.assertEqual(afc.m_deleted, [])

    async def test_legacy_riff_and_rf64_keep_last_two_channel_sox_route(self):
        for magic in (b'RIFF', b'RF64'):
            source = self.m_root/f'{magic.decode()}.wav'
            source.write_bytes(magic+bytes(4)+b'WAVE')
            calls = []
            def run_sox(command, check):
                calls.append(command)
                self.assertTrue(check)
                self.assertEqual(command, ['sox', str(source), str(source.with_stem(source.stem+'_stereo')), 'remix', '5', '6'])
                return subprocess.CompletedProcess(command, 0)
            with mock.patch.object(self.m_sync.subprocess, 'check_output', return_value='6'), mock.patch.object(self.m_sync.subprocess, 'run', side_effect=run_sox), redirect_stdout(io.StringIO()):
                status = self.m_sync.extract_stereo_recording(source)
            self.assertEqual(status, 0)
            self.assertEqual(len(calls), 1)


@unittest.skipUnless(os.environ.get('SMARTGRID_RECORDING_FIXTURE_OUTPUT'), 'Set SMARTGRID_RECORDING_FIXTURE_OUTPUT to a C++ mixer fixture')
class CppMixerFixtureTests(unittest.TestCase):
    """Opt in with SMARTGRID_RECORDING_FIXTURE_OUTPUT=/path/mixer.sgrec.

    The C++ fixture generator writes the observed scalar integers alongside the
    recording at /path/mixer.sgrec.expected.json. Generate both files in the same
    run before invoking Python unittest discovery with that environment value.
    """

    def setUp(self):
        self.m_source = Path(os.environ['SMARTGRID_RECORDING_FIXTURE_OUTPUT'])
        self.assertTrue(self.m_source.is_file(), f'Missing C++ recording fixture: {self.m_source}')
        expected_path = Path(str(self.m_source) + '.expected.json')
        self.assertTrue(expected_path.is_file(), f'Missing independent mixer expectations: {expected_path}')
        self.m_expected = json.loads(expected_path.read_text())
        self.m_offsets = {}
        offset = 0
        for track in self.m_expected['tracks']:
            self.m_offsets[track['id']] = offset
            offset += track['stream_count']
        self.assertTrue(self.m_expected['frames'])
        for frame in self.m_expected['frames']:
            self.assertEqual(len(frame), offset)

    def test_every_mixer_track_matches_observed_cpp_integers(self):
        sgrec = load_reader(self)
        expected_frames = self.m_expected['frames']
        with self.m_source.open('rb') as source:
            reader = sgrec.Reader(source)
            self.assertEqual(reader.m_header['git_commit_sha'], self.m_expected['git_commit_sha'])
            self.assertEqual(reader.m_header['sample_rate'], self.m_expected['sample_rate'])
            declared = {track['id']: len(sgrec.x_stream_names[track['type']]) for track in reader.m_header['tracks']}
            self.assertEqual(declared, {track['id']: track['stream_count'] for track in self.m_expected['tracks']})
            for block in reader.Blocks():
                self.assertLessEqual(block.m_start_frame + block.m_frame_count, len(expected_frames))
                for track_id, streams in block.m_tracks.items():
                    for stream_index, actual in enumerate(streams):
                        scalar = self.m_offsets[track_id] + stream_index
                        expected = [expected_frames[index][scalar] for index in range(block.m_start_frame, block.m_start_frame + block.m_frame_count)]
                        self.assertEqual(list(actual), expected, f'track {track_id}, stream {stream_index}, block at {block.m_start_frame}')
                del block
        self.assertTrue(reader.m_complete)
        self.assertEqual(reader.m_total_frames, len(expected_frames))

    def test_both_master_wavs_match_observed_cpp_integers(self):
        sgrec = load_reader(self)
        extractor = load_extractor(self)
        with self.m_source.open('rb') as source:
            header = sgrec.Reader(source).m_header
        with tempfile.TemporaryDirectory() as directory:
            for master, channels in (('stereo', 2), ('quad', 4)):
                with self.subTest(master=master):
                    matches = [track for track in header['tracks'] if track['role'] == f'master_{master}']
                    self.assertEqual(len(matches), 1)
                    offset = self.m_offsets[matches[0]['id']]
                    expected_pcm = bytearray()
                    for frame in self.m_expected['frames']:
                        for value in frame[offset:offset+channels]:
                            expected_pcm.extend(struct.pack('<i', value)[:3])
                    output = Path(directory) / f'{master}.wav'
                    result = extractor.extract(self.m_source, output, master=master)
                    self.assertTrue(result.m_complete, result.m_error)
                    self.assertEqual(result.m_frames, len(self.m_expected['frames']))
                    _, chunks = wav_chunks(output)
                    self.assertEqual(chunks[b'data'], expected_pcm)
                    self.assertEqual(struct.unpack_from('<HI', chunks[b'fmt '], 2), (channels, self.m_expected['sample_rate']))
                    if channels == 4:
                        self.assertEqual(struct.unpack_from('<I', chunks[b'fmt '], 20)[0], 0)


@unittest.skipUnless(os.environ.get('SMARTGRID_STATE_RECORDING_FIXTURE'), 'Set SMARTGRID_STATE_RECORDING_FIXTURE to a C++ state recording')
class CppStateFixtureTests(unittest.TestCase):
    def test_real_engine_patch_and_state_events_reconstruct_across_blocks(self):
        sgrec = load_reader(self)
        with open(os.environ['SMARTGRID_STATE_RECORDING_FIXTURE'], 'rb') as source:
            reader = sgrec.Reader(source)
            initial = reader.PatchAtSample(0)
            self.assertEqual(initial['nonagon']['Mute_0'], [0] * 8)
            self.assertEqual(initial['nonagon']['Mute_1'][0], 1)
            self.assertEqual(reader.m_header['initial_patch']['stateSaver']['sceneStateRight'][0], 1)
            self.assertEqual(initial['stateSaver']['sceneStateRight'][0], 2)
            self.assertEqual(initial['stateSaver']['sourceMonitor_0'], [1])
            self.assertEqual(set(initial), {'nonagon', 'squiggleBoy', 'stateSaver', 'configGrid', 'faders', 'blend'})
            self.assertEqual(initial['blend'], 0.125)
            first = reader.PatchAtSample(1)
            self.assertEqual(first['nonagon']['Mute_0'], [1, 0, 0, 0, 0, 0, 0, 0])
            self.assertEqual(first['stateSaver']['sourceMonitor_0'], [0])
            self.assertEqual(first['stateSaver']['sourceMonitor_1'], [1])
            self.assertAlmostEqual(first['blend'], 8192 / 16383)
            self.assertAlmostEqual(first['faders'][3], 4096 / 16383)
            third = reader.PatchAtSample(3)
            encoder = third['squiggleBoy']['Harmonics1']
            self.assertEqual(encoder['values']['values'][2][1], 0.625)
            gesture = encoder['gestures'][2]
            self.assertTrue(gesture['active'][0])
            self.assertEqual(gesture['values']['values'][0][0], 0.75)
            self.assertNotIn('modulators', gesture)
            nested = encoder['modulators'][1]['gestures'][0]
            self.assertTrue(nested['active'][16])
            self.assertEqual(nested['values']['values'][1][0], 0.25)
            last = reader.PatchAtSample(4)
            self.assertEqual(last['nonagon']['Mute_0'], [1, 0, 1, 0, 0, 0, 0, 0])
            self.assertEqual(last['stateSaver']['activeTrio'], [0, 0, 0, 0])
            self.assertEqual(last['configGrid']['sourceStereo'][0], True)
            self.assertEqual(last['configGrid']['sourceSelected'][0], [True, True, False, False])
            self.assertEqual(last['stateSaver']['sourceMonitor_0'], [1])
            self.assertEqual(last['stateSaver']['sourceMonitor_1'], [0])
            self.assertFalse(last['squiggleBoy']['Harmonics1']['gestures'][2]['active'][0])
            self.assertEqual(last['faders'][3], 1.0)
            self.assertEqual(reader.PatchAtSample(0), initial)


@unittest.skipUnless(os.environ.get('SMARTGRID_RANDOM_PARAM_FIXTURE'), 'Set SMARTGRID_RANDOM_PARAM_FIXTURE to a C++ seeded recording')
class CppRandomParamFixtureTests(unittest.TestCase):
    def test_seeded_encoder_scene_edits_reconstruct_complete_live_patches(self):
        sgrec = load_reader(self)
        path = os.environ['SMARTGRID_RANDOM_PARAM_FIXTURE']
        with open(path + '.json') as source:
            checkpoints = json.load(source)
        with open(path, 'rb') as source:
            reader = sgrec.Reader(source)
            for checkpoint in checkpoints:
                with self.subTest(sample=checkpoint['sample']):
                    self.assertEqual(reader.PatchAtSample(checkpoint['sample']), checkpoint['patch'])


@unittest.skipUnless(os.environ.get('SMARTGRID_PATCH_LOAD_FIXTURE'), 'Set SMARTGRID_PATCH_LOAD_FIXTURE to a C++ patch-load recording')
class CppPatchLoadFixtureTests(unittest.TestCase):
    def assert_patch_equal(self, actual, expected, path=()):
        if isinstance(expected, dict):
            self.assertEqual(set(actual), set(expected), path)
            for key, value in expected.items():
                if path == ('configGrid',) and key == 'sampleDirectoryRelative':
                    continue
                self.assert_patch_equal(actual[key], value, path + (key,))
        elif isinstance(expected, list):
            self.assertEqual(len(actual), len(expected), path)
            for index, value in enumerate(expected):
                self.assert_patch_equal(actual[index], value, path + (index,))
        elif isinstance(expected, float):
            self.assertAlmostEqual(actual, expected, delta=1e-6, msg=path)
        else:
            self.assertEqual(actual, expected, path)

    def test_real_loads_and_resets_reconstruct_live_patch_checkpoints(self):
        sgrec = load_reader(self)
        path = os.environ['SMARTGRID_PATCH_LOAD_FIXTURE']
        with open(path + '.json') as source:
            checkpoints = json.load(source)
        with open(path, 'rb') as source:
            reader = sgrec.Reader(source)
            events = [event for block in reader.Blocks((), include_events=True)
                      for event in sgrec.param_events(block, reader.m_header['format_version'])]
            for sample in (2, 3, 6):
                self.assertEqual([event.m_type for event in events if event.m_sample == sample], [6], sample)
            for sample, expected in ((1, [4, 6, 4]), (5, [6, 4, 6]), (7, [7, 4])):
                ordered = sorted((event for event in events if event.m_sample == sample), key=lambda event: event.m_order)
                self.assertEqual([event.m_type for event in ordered], expected, sample)
            for checkpoint in checkpoints:
                with self.subTest(sample=checkpoint['sample']):
                    self.assert_patch_equal(reader.PatchAtSample(checkpoint['sample']), checkpoint['patch'])


if __name__ == '__main__':
    unittest.main()
