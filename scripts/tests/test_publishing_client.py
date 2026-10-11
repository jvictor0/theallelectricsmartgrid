"""Native control client over real pinned TLS; only SoundCloud is a fixture."""
import hashlib
from pathlib import Path
import ssl
import socket
import subprocess
import sys
import tempfile
import threading
import time
import unittest
import wave

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts'))
sys.path.insert(0, str(ROOT / 'scripts/tests'))
from sync_receiver import create_server, SyncHandler
from soundcloud_publisher import PublishingService
from soundcloud_api import APIError
from test_soundcloud_publishing import ExternalAPI


@unittest.skipUnless(sys.platform == 'darwin', 'Foundation requires macOS')
class PublishingClientTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.build = tempfile.TemporaryDirectory()
        root = Path(cls.build.name)
        cls.binary = root / 'publisher'
        subprocess.run(['clang++', '-std=c++17', '-fobjc-arc', '-Wno-deprecated-declarations',
                        '-framework', 'Foundation', '-framework', 'Security',
                        '-I', str(ROOT / 'JUCE/SmartGridOne/Source'),
                        str(ROOT / 'scripts/tests/publishing_client_harness.mm'),
                        str(ROOT / 'JUCE/SmartGridOne/Source/PublishingClient.mm'),
                        str(ROOT / 'JUCE/SmartGridOne/Source/SyncClient.mm'),
                        '-o', str(cls.binary)], check=True)
        cert, key = root / 'cert.pem', root / 'key.pem'
        subprocess.run(['openssl', 'req', '-x509', '-newkey', 'rsa:2048', '-nodes',
                        '-keyout', str(key), '-out', str(cert), '-days', '1',
                        '-subj', '/CN=Publishing Test'], check=True, capture_output=True)
        cls.context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        cls.context.load_cert_chain(cert, key)
        cls.fingerprint = hashlib.sha256(ssl.PEM_cert_to_DER_cert(cert.read_text())).hexdigest()

    @classmethod
    def tearDownClass(cls):
        cls.build.cleanup()

    def run_scenario(self, mode):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            ipad, mac = root / 'ipad', root / 'mac'
            for directory in (ipad, mac):
                for kind in ('recordings', 'patches', 'logs'):
                    (directory / kind).mkdir(parents=True)
            for date in ('01', '02'):
                with wave.open(str(mac / 'recordings' / f'recording-2026-10-{date}T210000_stereo.wav'), 'wb') as wav:
                    wav.setnchannels(2)
                    wav.setsampwidth(2)
                    wav.setframerate(100)
                    wav.writeframes(b'\0' * 2000)
            artwork = root / 'cover.png'
            artwork.write_bytes(b'\x89PNG\r\n\x1a\nfixture-image')
            api = ExternalAPI()
            api.tracks = [{'id': 'historic-track', 'title': 'Historic', 'permalink_url': 'https://soundcloud.com/test/historic', 'duration': 5000}]
            original_upload = api.Upload
            def delayed_upload(*args):
                time.sleep(0.7)
                return original_upload(*args)
            api.Upload = delayed_upload
            service = PublishingService(mac, api)
            if mode == 'retry-lost':
                service.Refresh()
                deadline = time.monotonic() + 3
                while service.Snapshot()['refreshing'] and time.monotonic() < deadline:
                    time.sleep(0.01)
                row = service.Snapshot()['recordings'][0]
                api.upload_error = APIError('previous rejected upload')
                service.Submit({'recording_id': row['id'], 'version': row['version'], 'request_id': 'previous',
                                'title': 'Previous title', 'retry_of': None}, b'', '')
                while service.Snapshot()['recordings'][0]['state'] != 'failed' and time.monotonic() < deadline:
                    time.sleep(0.01)
                api.upload_error = None
                api.uploads.clear()
            calls = []
            accepted = []
            dropped = []
            class Handler(SyncHandler):
                def do_GET(self):
                    calls.append((time.monotonic(), self.path))
                    if mode == 'old' and self.path == '/v1/publishing':
                        return self._json(404, {'error': 'not found'})
                    if mode == 'concurrent' and self.path.startswith('/v1/manifest'):
                        time.sleep(3)
                    super().do_GET()
                def _json(self, status, value):
                    if mode == 'lost' and self.path == '/v1/publishing/uploads' and status == 202:
                        self.close_connection = True
                        self.connection.shutdown(socket.SHUT_RDWR)
                        self.connection.close()
                        return
                    return super()._json(status, value)
                def do_POST(self):
                    calls.append((time.monotonic(), self.path))
                    if mode == 'old':
                        return self._json(404, {'error': 'not found'})
                    if mode in ('retry-lost', 'two-recordings') and self.path == '/v1/publishing/uploads' and not dropped:
                        self.rfile.read(int(self.headers['Content-Length']))
                        dropped.append(True)
                        self.close_connection = True
                        self.connection.shutdown(socket.SHUT_RDWR)
                        self.connection.close()
                        return
                    super().do_POST()
                    if self.path == '/v1/publishing/uploads':
                        accepted.append(time.monotonic())
            server = create_server('127.0.0.1', 0, mac, 'test-code', service)
            server.RequestHandlerClass = Handler
            server.socket = self.context.wrap_socket(server.socket, server_side=True, do_handshake_on_connect=False)
            thread = threading.Thread(target=server.serve_forever)
            thread.start()
            try:
                fingerprint = '0' * 64 if mode == 'wrong' else self.fingerprint
                result = subprocess.run([str(self.binary), str(ipad), str(server.server_port), fingerprint, mode, str(artwork)],
                                        capture_output=True, text=True, timeout=25)
                ended = time.monotonic()
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                if mode in ('old', 'wrong', 'large-metadata', 'close-race'):
                    self.assertEqual(api.uploads, [])
                    self.assertFalse([path for _, path in calls if path == '/v1/publishing/uploads'],
                                     'unbounded request was sent to receiver')
                elif mode == 'two-recordings':
                    self.assertEqual(len(api.uploads), 2)
                    self.assertEqual(api.uploads[0][1:], ('Custom title', artwork.read_bytes()))
                    self.assertEqual(api.uploads[1][1:], ('Second current title', artwork.read_bytes() + b'B'))
                    self.assertEqual(len(accepted), 2)
                else:
                    self.assertEqual(len(api.uploads), 1)
                    self.assertEqual(api.uploads[0][1:], ('Smart Grid Nightly 2026/10/02', b'') if mode == 'default' else ('Custom title', artwork.read_bytes()))
                    self.assertEqual(len(accepted), 1)
                    self.assertGreater(ended - calls[-1][0], 2.1, 'control requests continued after Close')
                    if mode in ('close', 'background'):
                        self.assertFalse([path for timestamp, path in calls
                                          if accepted[0] + 0.3 < timestamp < accepted[0] + 2.2],
                                         'control requests continued while inactive')
                    receipts = service.Snapshot()['recordings']
                    self.assertEqual(receipts[1]['permalink'], 'https://soundcloud.com/test/historic')
                count = len(calls)
                time.sleep(0.2)
                self.assertEqual(len(calls), count, 'requests continued after Close')
            finally:
                server.shutdown()
                server.server_close()
                thread.join()
                service.Close()

    def test_publishing_while_bulk_sync_is_stalled_and_cancelled(self):
        self.run_scenario('concurrent')

    def test_close_after_acceptance_recovers_receipt_without_resubmit(self):
        self.run_scenario('close')

    def test_background_after_acceptance_recovers_receipt_without_resubmit(self):
        self.run_scenario('background')

    def test_lost_acceptance_response_recovers_receipt_without_resubmit(self):
        self.run_scenario('lost')

    def test_retry_keeps_request_identity_when_old_failed_job_is_still_visible(self):
        self.run_scenario('retry-lost')

    def test_default_title_and_no_artwork_use_receiver_allocation(self):
        self.run_scenario('default')

    def test_unbounded_metadata_is_rejected_before_network_transfer(self):
        self.run_scenario('large-metadata')

    def test_close_interleaves_with_task_creation_without_invalidated_session_exception(self):
        self.run_scenario('close-race')

    def test_second_recording_cannot_overwrite_uncertain_first_request(self):
        self.run_scenario('two-recordings')

    def test_wrong_fingerprint_rejects_receiver(self):
        self.run_scenario('wrong')

    def test_older_receiver_is_unavailable(self):
        self.run_scenario('old')


if __name__ == '__main__':
    unittest.main()
