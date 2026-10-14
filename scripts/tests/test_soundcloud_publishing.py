import importlib.util
import os
import struct
import sys
import tempfile
import threading
import time
import unittest
import wave
from datetime import datetime
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
if importlib.util.find_spec('soundcloud_publisher'):
    from soundcloud_publisher import PublishingService, PublishingError
    from soundcloud_api import APIError
else:
    PublishingService = None


class ExternalAPI:
    def __init__(self):
        self.tracks = []
        self.error = None
        self.upload_error = None
        self.gate = threading.Event()
        self.gate.set()
        self.uploads = []
        self.list_reads = 0
        self.account = {'id': 'account-one', 'username': 'Test Account'}

    def Account(self):
        return self.account

    def Tracks(self):
        self.list_reads += 1
        if self.error:
            raise self.error
        return self.tracks

    def Upload(self, path, title, artwork_path, progress):
        self.gate.wait(3)
        artwork = Path(artwork_path).read_bytes() if artwork_path else b''
        self.uploads.append((Path(path).read_bytes(), title, artwork))
        if self.upload_error:
            raise self.upload_error
        progress(0.5)
        return {'urn': 'soundcloud:tracks:' + str(len(self.uploads)), 'title': title,
                'permalink_url': 'https://soundcloud.com/test/' + str(len(self.uploads)), 'duration': 5000}


class PublishingTests(unittest.TestCase):
    def setUp(self):
        self.assertIsNotNone(PublishingService, 'Mac publishing backend is not implemented')
        self.directory = tempfile.TemporaryDirectory()
        self.root = Path(self.directory.name)
        (self.root / 'recordings').mkdir()
        self.api = ExternalAPI()
        self.service = PublishingService(self.root, self.api)

    def tearDown(self):
        if hasattr(self, 'service'):
            self.api.gate.set()
            self.service.Close()
            self.directory.cleanup()

    def write_stereo(self, name='recording-2026-10-01T210000_stereo.wav', seconds=5):
        path = self.root / 'recordings' / name
        with wave.open(str(path), 'wb') as output:
            output.setnchannels(2)
            output.setsampwidth(2)
            output.setframerate(100)
            output.writeframes(b'\x00' * seconds * 400)
        return path

    def wait_for(self, predicate):
        deadline = time.monotonic() + 3
        while time.monotonic() < deadline:
            if predicate():
                return
            time.sleep(0.01)
        self.fail('background operation did not complete')

    def refresh(self):
        self.service.Refresh()
        self.wait_for(lambda: not self.service.Snapshot()['refreshing'])
        return self.service.Snapshot()

    def metadata(self, row, request='request-1', title=None, retry=None):
        return {'recording_id': row['id'], 'version': row['version'], 'request_id': request,
                'title': title, 'retry_of': retry}

    def restart(self, api=True):
        self.service.Close()
        self.service = PublishingService(self.root, self.api if api else None)

    def test_next_title_and_receipt_survive_restart(self):
        self.write_stereo()
        self.api.tracks = [{'id': '11', 'title': 'Smart Grid Nightly 2026/10/01',
                            'permalink_url': 'https://soundcloud.com/test/one', 'duration': 5000}]
        row = self.refresh()['recordings'][0]
        self.assertEqual(row['suggested_title'], 'Smart Grid Nightly 2026/10/01 #2')
        self.assertEqual(row['candidate_id'], '11')
        self.assertEqual(row['state'], 'ready')
        job = self.service.Submit(self.metadata(row), b'', '')
        self.wait_for(lambda: self.service.Snapshot()['recordings'][0]['state'] == 'uploaded')
        self.restart()
        receipt = self.service.Snapshot()['recordings'][0]
        self.assertEqual((receipt['state'], receipt['job_id'], receipt['permalink']),
                         ('uploaded', job['job_id'], 'https://soundcloud.com/test/1'))

    def test_successful_prior_receipt_durably_claims_new_request_identity(self):
        self.write_stereo()
        self.write_stereo('recording-2026-10-02T210000_stereo.wav')
        rows = self.refresh()['recordings']
        self.service.Submit(self.metadata(rows[0]), b'', '')
        self.wait_for(lambda: self.service.Snapshot()['recordings'][0]['state'] == 'uploaded')
        receipt = self.service.Submit(self.metadata(rows[0], 'receipt-request'), b'', '')
        self.assertEqual(receipt['state'], 'uploaded')
        self.restart()
        with self.assertRaises(PublishingError) as failure:
            self.service.Submit(self.metadata(rows[1], 'receipt-request'), b'', '')
        self.assertEqual(failure.exception.status, 409)

    def test_catalog_orders_final_exports_and_rejects_changed_version(self):
        first = self.write_stereo()
        self.write_stereo('recording-2026-10-02T220000_stereo.wav')
        self.write_stereo('not-an-export.wav')
        self.write_stereo('.partial_stereo.wav')
        rows = self.refresh()['recordings']
        self.assertEqual([row['date'] for row in rows], ['2026/10/02', '2026/10/01'])
        self.assertEqual(rows[1]['duration_seconds'], 5)
        old = rows[1]
        self.write_stereo(seconds=6)
        self.refresh()
        with self.assertRaises(PublishingError):
            self.service.Submit(self.metadata(old), b'', '')
        row = self.service.Snapshot()['recordings'][1]
        self.assertEqual(row['id'], old['id'])
        self.assertNotEqual(row['version'], old['version'])

    def test_numbered_sgrec_date_and_float_rf64_duration(self):
        fixture = Path(__file__).resolve().parents[2] / 'private/test/fixtures/streaming-recording/golden.sgrec'
        (self.root / 'recordings/recording-7.sgrec').write_bytes(fixture.read_bytes())
        path = self.write_stereo('recording-7_stereo.wav')
        from sgrec import Reader
        with fixture.open('rb') as source:
            expected = datetime.fromisoformat(Reader(source).m_header['recorded_at_utc'].replace('Z', '+00:00')).astimezone().strftime('%Y/%m/%d')
        fmt = struct.pack('<HHIIHH', 3, 2, 100, 800, 8, 32)
        data = b'\0' * 4000
        header = b'RF64' + struct.pack('<I', 0xffffffff) + b'WAVEds64' + struct.pack('<IQQQI', 28, 4072, 4000, 500, 0)
        path.write_bytes(header + b'fmt ' + struct.pack('<I', 16) + fmt + b'data' + struct.pack('<I', 0xffffffff) + data)
        row = self.refresh()['recordings'][0]
        self.assertEqual((row['date'], row['duration_seconds']), (expected, 5))
        os.utime(path, (1, 1))
        self.assertEqual(self.refresh()['recordings'][0]['date'], expected)

    def test_atomic_slots_replay_conflict_and_custom_artwork(self):
        self.write_stereo()
        self.write_stereo('recording-2026-10-01T220000_stereo.wav')
        rows = self.refresh()['recordings']
        self.api.gate.clear()
        one = self.service.Submit(self.metadata(rows[0]), b'', '')
        two = self.service.Submit(self.metadata(rows[1], 'request-2'), b'', '')
        self.assertEqual((one['title'], two['title']), ('Smart Grid Nightly 2026/10/01', 'Smart Grid Nightly 2026/10/01 #2'))
        self.assertEqual(self.service.Submit(self.metadata(rows[0]), b'', '')['job_id'], one['job_id'])
        with self.assertRaises(PublishingError) as failure:
            self.service.Submit(self.metadata(rows[0], title='Changed'), b'', '')
        self.assertEqual(failure.exception.status, 409)
        self.api.gate.set()
        self.wait_for(lambda: all(row['state'] == 'uploaded' for row in self.service.Snapshot()['recordings']))
        self.write_stereo('recording-2026-10-01T230000_stereo.wav')
        row = self.refresh()['recordings'][0]
        self.assertEqual(row['suggested_title'], 'Smart Grid Nightly 2026/10/01 #3')
        self.service.Submit(self.metadata(row, 'request-3', '  Event Name  '), b'\x89PNG\r\n\x1a\nimage', 'image/png')
        self.wait_for(lambda: self.service.Snapshot()['recordings'][0]['state'] == 'uploaded')
        self.assertEqual(self.api.uploads[-1][1:], ('Event Name', b'\x89PNG\r\n\x1a\nimage'))

    def test_missing_credentials_list_failure_and_cached_failure(self):
        self.write_stereo()
        self.restart(api=False)
        snapshot = self.service.Snapshot()
        self.assertFalse(snapshot['account']['connected'])
        self.assertEqual(len(snapshot['recordings']), 1)
        with self.assertRaises(PublishingError):
            self.service.Submit(self.metadata(snapshot['recordings'][0]), b'', '')
        self.restart()
        self.api.error = RuntimeError('remote unavailable')
        snapshot = self.refresh()
        self.assertFalse(snapshot['catalog_ready'])
        self.assertIn('remote unavailable', snapshot['error'])
        with self.assertRaises(PublishingError):
            self.service.Submit(self.metadata(snapshot['recordings'][0]), b'', '')
        self.api.error = None
        self.refresh()
        self.api.error = RuntimeError('remote unavailable')
        self.assertTrue(self.refresh()['catalog_ready'])

    def test_definite_failures_release_slots_unknown_retains_and_requires_retry(self):
        self.write_stereo()
        row = self.refresh()['recordings'][0]
        self.api.upload_error = APIError('rejected', uncertain=False)
        failed = self.service.Submit(self.metadata(row), b'', '')
        self.wait_for(lambda: self.service.Snapshot()['recordings'][0]['state'] == 'failed')
        self.assertEqual(self.service.Snapshot()['recordings'][0]['suggested_title'], 'Smart Grid Nightly 2026/10/01')
        with self.assertRaises(PublishingError):
            self.service.Submit(self.metadata(row, 'new'), b'', '')
        self.api.upload_error = APIError('lost response', uncertain=True)
        unknown = self.service.Submit(self.metadata(row, 'new', retry=failed['job_id']), b'', '')
        self.wait_for(lambda: self.service.Snapshot()['recordings'][0]['state'] == 'unknown')
        self.restart()
        self.assertEqual(self.service.Snapshot()['recordings'][0]['state'], 'unknown')
        self.assertEqual(self.service.Snapshot()['recordings'][0]['suggested_title'], 'Smart Grid Nightly 2026/10/01 #2')
        self.api.upload_error = None
        job = self.service.Submit(self.metadata(row, 'retry', retry=unknown['job_id']), b'', '')
        self.assertEqual(job['title'], 'Smart Grid Nightly 2026/10/01 #2')

    def test_interrupted_uploads_become_unknown_and_queued_jobs_resume(self):
        self.write_stereo()
        self.write_stereo('recording-2026-10-01T220000_stereo.wav')
        rows = self.refresh()['recordings']
        self.service.Close()
        import sqlite3
        database = sqlite3.connect(self.root / '.soundcloud.sqlite3')
        for index, state in enumerate(('uploading', 'queued')):
            database.execute("""INSERT INTO jobs (job_id,request_id,identity,account,recording_id,version,date,title,slot,artwork,state,progress,error,track_id,permalink)
                VALUES (?,?,?,'account-one',?,?,'2026/10/01',?,?,'',?,0,'','','')""",
                ('restart-' + str(index), 'restart-request-' + str(index), '{}', rows[index]['id'], rows[index]['version'],
                 'Smart Grid Nightly 2026/10/01' + (' #2' if index else ''), index + 1, state))
        database.commit()
        database.close()
        self.service = PublishingService(self.root, self.api)
        self.wait_for(lambda: self.service.Snapshot()['recordings'][1]['state'] == 'uploaded')
        states = [r['state'] for r in self.service.Snapshot()['recordings']]
        self.assertEqual(states, ['unknown', 'uploaded'])
        self.assertEqual(len(self.api.uploads), 1)

    def test_custom_nightly_suffix_counts_before_remote_refresh(self):
        self.write_stereo()
        row = self.refresh()['recordings'][0]
        self.service.Submit(self.metadata(row, title='smart grid nightly 2026/10/01 #9'), b'', '')
        self.wait_for(lambda: self.service.Snapshot()['recordings'][0]['state'] == 'uploaded')
        self.assertEqual(self.service.Snapshot()['recordings'][0]['suggested_title'], 'Smart Grid Nightly 2026/10/01 #10')

    def test_concurrent_submissions_reserve_distinct_slots(self):
        self.write_stereo()
        self.write_stereo('recording-2026-10-01T220000_stereo.wav')
        rows = self.refresh()['recordings']
        self.api.gate.clear()
        results = []
        threads = [threading.Thread(target=lambda i=i: results.append(self.service.Submit(self.metadata(rows[i], 'parallel-' + str(i)), b'', ''))) for i in range(2)]
        for thread in threads:
            thread.start()
        for thread in threads:
            thread.join()
        self.assertEqual(sorted(r['title'] for r in results), ['Smart Grid Nightly 2026/10/01', 'Smart Grid Nightly 2026/10/01 #2'])

    def test_failed_account_switch_never_reuses_old_catalog(self):
        self.write_stereo()
        self.api.tracks = [{'id': '11', 'title': 'Smart Grid Nightly 2026/10/01', 'duration': 5000,
                            'permalink_url': 'https://soundcloud.com/test/one'}]
        self.refresh()
        self.api.account = {'id': 'new-account', 'username': 'New'}
        self.api.error = RuntimeError('list failed')
        snapshot = self.refresh()
        self.assertEqual(snapshot['account']['id'], 'new-account')
        self.assertFalse(snapshot['catalog_ready'])
        self.assertEqual(snapshot['tracks'], [])
        with self.assertRaises(PublishingError):
            self.service.Submit(self.metadata(snapshot['recordings'][0]), b'', '')

    def test_catalog_rejects_symlinks_and_header_only_truncated_audio(self):
        valid = self.write_stereo()
        (self.root / 'recordings/recording-2026-10-02T210000_stereo.wav').symlink_to(valid)
        truncated = self.write_stereo('recording-2026-10-03T210000_stereo.wav')
        truncated.write_bytes(truncated.read_bytes()[:44])
        rows = self.refresh()['recordings']
        self.assertEqual(len(rows), 1)
        self.assertEqual(rows[0]['date'], '2026/10/01')
        valid.unlink()
        valid.symlink_to(truncated)
        with self.assertRaises(PublishingError):
            self.service.Submit(self.metadata(rows[0]), b'', '')

    def test_startup_skips_cloud_only_audio_without_reading_it(self):
        local = self.write_stereo('recording-7_stereo.wav')
        original = local.with_name('recording-7.sgrec')
        original.write_bytes(b'cloud placeholder')
        cloud = self.write_stereo()
        cloud_files = {original, cloud}
        original_stat = Path.stat
        original_open = Path.open

        def file_stat(path, *args, **kwargs):
            result = original_stat(path, *args, **kwargs)
            if path in cloud_files:
                return SimpleNamespace(st_mode=result.st_mode, st_mtime=result.st_mtime,
                                       st_flags=0x40000000)
            return result

        def file_open(path, *args, **kwargs):
            self.assertNotIn(path, cloud_files, 'Catalog must not download cloud-only files')
            return original_open(path, *args, **kwargs)

        with patch.object(Path, 'stat', file_stat), patch.object(Path, 'open', file_open):
            self.restart(api=False)
            rows = self.service.Snapshot()['recordings']
            self.assertEqual([row['filename'] for row in rows], [local.name])
            expected = datetime.fromtimestamp(original_stat(original).st_mtime).strftime('%Y/%m/%d')
            self.assertEqual(rows[0]['date'], expected)

        self.service.m_catalog.Scan()
        self.assertEqual(len(self.service.Snapshot()['recordings']), 2)

    def test_invalid_id_types_are_rejected_at_service_boundary(self):
        self.write_stereo()
        self.refresh()
        with self.assertRaises(PublishingError):
            self.service.Submit({'recording_id': [], 'version': 'x', 'request_id': 'bad', 'title': None, 'retry_of': None}, b'', '')
        with self.assertRaises(PublishingError):
            self.service.Link({'recording_id': {}, 'version': 'x', 'track_id': []})

    def test_historical_links_require_account_catalog_and_unique_bidirectional_match(self):
        self.write_stereo()
        self.write_stereo('recording-2026-10-01T220000_stereo.wav')
        self.api.tracks = [{'id': '11', 'title': 'Smart Grid Nightly 2026/10/01', 'duration': 5000,
                            'permalink_url': 'https://soundcloud.com/test/one'}]
        rows = self.refresh()['recordings']
        self.assertEqual([r['candidate_id'] for r in rows], ['', ''])
        with self.assertRaises(PublishingError):
            self.service.Link({'recording_id': rows[0]['id'], 'version': rows[0]['version'], 'track_id': 'outsider'})
        self.service.Link({'recording_id': rows[0]['id'], 'version': rows[0]['version'], 'track_id': '11'})
        self.assertEqual(self.service.Snapshot()['recordings'][0]['state'], 'uploaded')
        self.restart()
        self.assertEqual(self.service.Snapshot()['recordings'][0]['state'], 'uploaded')
        self.api.account = {'id': 'other-account', 'username': 'Other'}
        self.refresh()
        self.assertEqual(self.service.Snapshot()['recordings'][0]['state'], 'ready')


if __name__ == '__main__':
    unittest.main()
