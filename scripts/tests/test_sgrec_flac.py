import copy
import io
import os
import struct
import unittest
from unittest import mock

from test_sgrec import checksum_record, completion, header_bytes, load_reader, x_fixtures


def expected_values():
    return [-8388608, -1, 0, 1, 8388607] + [i * 123 - 200000 for i in range(2048)]


class FlacReaderTests(unittest.TestCase):
    def setUp(self):
        self.m_flac = (x_fixtures / 'flac-pcm24.flac').read_bytes()
        self.m_frames = len(expected_values())
        self.m_header = {
            'format_version': 4, 'initial_patch': {'blend': 0.25},
            'recorded_at_utc': '2026-09-23T00:00:00Z',
            'git_commit_sha': 'a' * 40, 'sample_rate': 48000,
            'block_frames': 48000,
            'tracks': [{'id': 0, 'name': 'signal', 'type': 'mono',
                        'role': 'input', 'tap': 'post_fader'}],
        }

    def reader(self, flac=None, *, payload=None, header=None, descriptor=b'\x02\0', frames=None):
        header = self.m_header if header is None else header
        frames = self.m_frames if frames is None else frames
        flac = self.m_flac if flac is None else flac
        payload = struct.pack('<I', len(flac)) + flac if payload is None else payload
        descriptors = struct.pack('<I', 0) + descriptor
        body = (struct.pack('<4sIQIH', f'BLK{header["format_version"]}'.encode(),
                            30 + len(descriptors) + len(payload), 0, frames, 1)
                + descriptors + payload + b'\0' * 4)
        return load_reader(self).Reader(io.BytesIO(header_bytes(header) + checksum_record(body)
                                                   + completion(frames)))

    def test_independent_cli_fixture_preserves_pcm24_endpoints_and_partial_flac_frame(self):
        reader = self.reader()
        block, = reader.Blocks()
        self.assertEqual(list(block.m_tracks[0][0]), expected_values())
        self.assertTrue(reader.m_complete)

    def test_skipping_flac_requires_no_library_and_preserves_event_boundary(self):
        with mock.patch('sgrec_flac._load_library', side_effect=ValueError('not installed')):
            reader = self.reader()
            block, = reader.Blocks((), include_events=True)
            self.assertEqual(block.m_tracks, {})
            self.assertEqual(bytes(block.m_event_data), b'\0' * 4)
            self.assertEqual(self.reader().PatchAtSample(100), {'blend': 0.25})
            with self.assertRaisesRegex(load_reader(self).RecordingError, 'not installed'):
                list(self.reader().Blocks())

    def test_payload_framing_and_streaminfo_must_match_container_even_when_skipped(self):
        corrupt = []
        corrupt.extend([b'', b'fLaC', self.m_flac[:41], b'xxxx' + self.m_flac[4:]])
        for offset, mask in ((4, 1), (7, 1), (8, 128), (18, 1), (20, 2), (20, 16), (25, 1)):
            data = bytearray(self.m_flac)
            data[offset] ^= mask
            corrupt.append(data)
        for data in corrupt:
            for selected in (None, ()):
                with self.subTest(offset=len(data), selected=selected), self.assertRaises(load_reader(self).RecordingError):
                    list(self.reader(data).Blocks(selected))
        for length in (0, 3, len(self.m_flac) + 10, 0xffffffff):
            for selected in (None, ()):
                with self.subTest(length=length, selected=selected), self.assertRaises(load_reader(self).RecordingError):
                    list(self.reader(payload=struct.pack('<I', length) + self.m_flac).Blocks(selected))

    def test_decoder_rejects_corruption_truncation_and_trailing_bytes(self):
        damaged = bytearray(self.m_flac)
        damaged[-4] ^= 0x40
        for data in (self.m_flac[:-1], self.m_flac[:-15], damaged,
                     self.m_flac + b'\0', self.m_flac + self.m_flac):
            with self.subTest(length=len(data)), self.assertRaises(load_reader(self).RecordingError):
                list(self.reader(data).Blocks())

    def test_md5_is_checked_when_streaminfo_supplies_it(self):
        data = bytearray(self.m_flac)
        data[26:42] = b'\x01' * 16
        with self.assertRaises(load_reader(self).RecordingError):
            list(self.reader(data).Blocks())

    def test_coordinate_stream_rejects_negative_values(self):
        header = copy.deepcopy(self.m_header)
        header['tracks'][0]['type'] = 'panned_mono'
        payload = b'\x01\0\0' + struct.pack('<I', len(self.m_flac)) + self.m_flac + b'\0\0\0'
        with self.assertRaises(load_reader(self).RecordingError):
            list(self.reader(payload=payload, header=header, descriptor=b'\x01\0\x02\0\x01\0').Blocks())

    def test_encoding_requires_v4_and_zero_descriptor_width(self):
        for version, descriptor in ((3, b'\x02\0'), (4, b'\x02\x01')):
            with self.subTest(version=version), self.assertRaises(load_reader(self).RecordingError):
                list(self.reader(header=dict(self.m_header, format_version=version), descriptor=descriptor).Blocks())

    @unittest.skipUnless(os.environ.get('SMARTGRID_FLAC_CODEC_FIXTURE'), 'C++ FLAC fixture not requested')
    def test_cpp_fixture_preserves_all_streams(self):
        with open(os.environ['SMARTGRID_FLAC_CODEC_FIXTURE'], 'rb') as source:
            reader = load_reader(self).Reader(source)
            block, = reader.Blocks()
        frames = block.m_frame_count
        expected = [frame % 4 for frame in range(frames)]
        expected[63:65] = [-8388608, 8388607]
        self.assertEqual(list(block.m_tracks[0][0]), expected)
        self.assertEqual(list(block.m_tracks[0][1]), [0] * (frames // 2) + [8388607] * (frames - frames // 2))
        self.assertEqual(list(block.m_tracks[0][2]), [4194304] * frames)
        self.assertEqual([list(stream) for stream in block.m_tracks[1]], [[0] * frames] * 2)
        self.assertTrue(reader.m_complete)
