import array
from contextlib import redirect_stdout
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

    def test_corrupt_block_never_exposes_even_its_valid_selected_track(self):
        sgrec = load_reader(self)
        corrupted = bytearray(self.m_records[0])
        corrupted[-1] ^= 1
        reader = sgrec.Reader(io.BytesIO(self.m_prefix + corrupted + completion(4)))
        with self.assertRaisesRegex(sgrec.RecordingError, 'CRC'):
            next(reader.Blocks())
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
            dict(self.m_header, format_version=2),
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
        bad_positions = data_record(4, 4, struct.pack('<I', 3)+bytes([1,0,1,0,1,0]), bytes.fromhex('010000ffffff000000'), 1)
        for suffix in (b'', b'BLK1'+b'\0'*5, bad_positions, completion(4)[:-1]):
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
    for name, member in (('pymobiledevice3.lockdown', 'create_using_usbmux'), ('pymobiledevice3.services.house_arrest', 'HouseArrestService'), ('pymobiledevice3.usbmux', 'list_devices')):
        module = types.ModuleType(name)
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

    def stat(self, path):
        return {'st_ifmt': 'S_IFREG', 'st_size': len(self.m_data)}

    def fopen(self, path, mode):
        return 1

    def fread(self, handle, count):
        if self.m_early_eof:
            return b''
        data = self.m_data[self.m_offset:self.m_offset+count]
        self.m_offset += len(data)
        return data

    def fclose(self, handle):
        self.m_closed = True

    def rm(self, path):
        self.m_deleted.append(path)


class SyncTests(unittest.TestCase):
    def setUp(self):
        self.m_sync = load_sync()
        self.m_directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.m_directory.cleanup)
        self.m_root = Path(self.m_directory.name)

    def test_new_container_dispatches_by_magic_and_deletes_only_complete_download(self):
        afc = FakeAfc((x_fixtures/'golden.sgrec').read_bytes())
        with mock.patch.object(self.m_sync, 'MAC_RECORDINGS_DIR', self.m_root), mock.patch.object(self.m_sync, 'list_files_recursive', return_value=[('recording.bin', False, '/recording.bin')]), mock.patch.object(self.m_sync.subprocess, 'check_output', side_effect=AssertionError('Custom container passed to SoX')), redirect_stdout(io.StringIO()):
            self.m_sync.sync_recordings_from_ipad(afc, '/')
        self.assertEqual(afc.m_deleted, ['/recording.bin'])
        self.assertTrue(afc.m_closed)
        self.assertEqual((self.m_root/'recording.bin').read_bytes(), afc.m_data)
        with wave.open(str(self.m_root/'recording_stereo.wav'), 'rb') as wav:
            self.assertEqual((wav.getnchannels(), wav.getnframes()), (2,9))

    def test_playable_partial_export_keeps_remote_recording(self):
        afc = FakeAfc((x_fixtures/'golden.sgrec').read_bytes()[:-16])
        with mock.patch.object(self.m_sync, 'MAC_RECORDINGS_DIR', self.m_root), mock.patch.object(self.m_sync, 'list_files_recursive', return_value=[('partial.sgrec', False, '/partial.sgrec')]), mock.patch.object(self.m_sync.subprocess, 'check_output', side_effect=AssertionError('Custom container passed to SoX')), redirect_stdout(io.StringIO()):
            self.m_sync.sync_recordings_from_ipad(afc, '/')
        self.assertEqual(afc.m_deleted, [])
        with wave.open(str(self.m_root/'partial_stereo.wav'), 'rb') as wav:
            self.assertEqual(wav.getnframes(), 9)

    def test_retry_replaces_partial_sync_export_only_after_complete_download(self):
        complete = (x_fixtures/'golden.sgrec').read_bytes()
        first_block = 12 + struct.unpack_from('<I', complete, 8)[0]
        first_block_end = first_block + struct.unpack_from('<I', complete, first_block+4)[0]
        afc = FakeAfc(complete[:first_block_end])
        with mock.patch.object(self.m_sync, 'MAC_RECORDINGS_DIR', self.m_root), mock.patch.object(self.m_sync, 'list_files_recursive', return_value=[('retry.sgrec', False, '/retry.sgrec')]), redirect_stdout(io.StringIO()):
            self.m_sync.sync_recordings_from_ipad(afc, '/')
            self.assertEqual(afc.m_deleted, [])
            with wave.open(str(self.m_root/'retry_stereo.wav'), 'rb') as wav:
                self.assertEqual(wav.getnframes(), 4)
            afc.m_data = complete
            afc.m_offset = 0
            self.m_sync.sync_recordings_from_ipad(afc, '/')
        self.assertEqual(afc.m_deleted, ['/retry.sgrec'])
        with wave.open(str(self.m_root/'retry_stereo.wav'), 'rb') as wav:
            self.assertEqual(wav.getnframes(), 9)

    def test_retry_after_remote_delete_failure_reextracts_and_deletes(self):
        afc = FakeAfc((x_fixtures/'golden.sgrec').read_bytes())
        with mock.patch.object(self.m_sync, 'MAC_RECORDINGS_DIR', self.m_root), mock.patch.object(self.m_sync, 'list_files_recursive', return_value=[('retry.sgrec', False, '/retry.sgrec')]), redirect_stdout(io.StringIO()):
            with mock.patch.object(afc, 'rm', side_effect=OSError('Connection lost during delete')):
                self.m_sync.sync_recordings_from_ipad(afc, '/')
            self.assertEqual(afc.m_deleted, [])
            self.assertTrue((self.m_root/'retry_stereo.wav').exists())
            afc.m_offset = 0
            self.m_sync.sync_recordings_from_ipad(afc, '/')
        self.assertEqual(afc.m_deleted, ['/retry.sgrec'])
        with wave.open(str(self.m_root/'retry_stereo.wav'), 'rb') as wav:
            self.assertEqual(wav.getnframes(), 9)

    def test_nonzero_extraction_status_keeps_remote_recording(self):
        afc = FakeAfc(b'RIFF'+bytes(4)+b'WAVE')
        with mock.patch.object(self.m_sync, 'MAC_RECORDINGS_DIR', self.m_root), mock.patch.object(self.m_sync, 'list_files_recursive', return_value=[('failed.wav', False, '/failed.wav')]), mock.patch.object(self.m_sync, 'extract_stereo_recording', return_value=1), redirect_stdout(io.StringIO()):
            self.m_sync.sync_recordings_from_ipad(afc, '/')
        self.assertEqual(afc.m_deleted, [])

    def test_truncated_download_raises_and_closes_instead_of_looping(self):
        afc = FakeAfc(b'promised bytes')
        with mock.patch.object(afc, 'fread', side_effect=[b'', AssertionError('Read again after premature EOF')]):
            with self.assertRaisesRegex(OSError, 'download|Download'):
                self.m_sync.download_afc_file(afc, '/source', self.m_root/'copy')
        self.assertTrue(afc.m_closed)
        self.assertEqual(afc.m_deleted, [])

    def test_legacy_riff_and_rf64_keep_last_two_channel_sox_route(self):
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


if __name__ == '__main__':
    unittest.main()
