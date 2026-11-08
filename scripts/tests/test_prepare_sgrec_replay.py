import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from prepare_sgrec_replay import prepare
from sgrec import RecordingError
from test_sgrec import parameter_group, patch_entry
import test_sgrec_timeline as timeline


class PrepareReplayTests(unittest.TestCase):
    def recording(self, extra=()):
        groups = [parameter_group(8, 0, '', [struct.pack('<II', 0, 0)])]
        groups.extend(extra)
        reader = timeline.TimelineTests().reader(struct.pack('<d', 2.0) + b'\0\x03', groups=groups)
        return reader.m_source.getvalue()

    def test_capture_order_phase_and_existing_output(self):
        # Physical event groups differ from temporal order.
        groups = [parameter_group(3, 4, '', [struct.pack('<IIf', 2, 2, 0.8)]),
                  parameter_group(2, 4, '', [struct.pack('<IIBf', 2, 1, 3, 0.4)])]
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / 'input.sgrec'
            output = Path(directory) / 'output.json'
            source.write_bytes(self.recording(groups))
            result = prepare(source, output)
            self.assertEqual(result['initial_phase'], 2.0)
            self.assertEqual(result['frames'], 4)
            self.assertEqual([row[1] for row in result['events']], [8, 2, 3])
            self.assertEqual(json.loads(output.read_text()), result)
            with self.assertRaises(FileExistsError):
                prepare(source, output)

    def test_incomplete_corrupt_and_unsupported_do_not_create_output(self):
        valid = self.recording()
        corrupt = bytearray(valid)
        corrupt[-5] ^= 1
        bulk = self.recording([parameter_group(6, 0, '', [patch_entry(1, 1, {}, True)])])
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / 'input.sgrec'
            output = Path(directory) / 'output.json'
            for data in [valid[:-16], corrupt, bulk]:
                with self.subTest(length=len(data)):
                    source.write_bytes(data)
                    with self.assertRaises(RecordingError):
                        prepare(source, output)
                    self.assertFalse(output.exists())


if __name__ == '__main__':
    unittest.main()
