"""Exercise the engine-independent native client against a real TLS receiver."""
import hashlib
from pathlib import Path
import ssl
import subprocess
import sys
import tempfile
import threading
import time
import unittest
import uuid

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts'))
from sync_receiver import create_server, SyncHandler


@unittest.skipUnless(sys.platform == 'darwin', 'Foundation client requires macOS')
class NativeClientTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.m_build = tempfile.TemporaryDirectory()
        build = Path(cls.m_build.name)
        cls.m_binary = build / 'client'
        subprocess.run(['clang++', '-std=c++17', '-fobjc-arc', '-Wno-deprecated-declarations',
                        '-framework', 'Foundation', '-framework', 'Security',
                        '-I', str(ROOT / 'JUCE/SmartGridOne/Source'),
                        str(ROOT / 'scripts/tests/sync_client_harness.mm'),
                        str(ROOT / 'JUCE/SmartGridOne/Source/SyncClient.mm'),
                        '-o', str(cls.m_binary)], check=True)
        cert, key = build / 'cert.pem', build / 'key.pem'
        subprocess.run(['openssl', 'req', '-x509', '-newkey', 'rsa:2048', '-nodes',
                        '-keyout', str(key), '-out', str(cert), '-days', '1',
                        '-subj', '/CN=SmartGrid Sync Test'], check=True, capture_output=True)
        cls.m_context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        cls.m_context.load_cert_chain(cert, key)
        cls.m_fingerprint = hashlib.sha256(ssl.PEM_cert_to_DER_cert(cert.read_text())).hexdigest()

    @classmethod
    def tearDownClass(cls):
        cls.m_build.cleanup()

    def setUp(self):
        self.m_directory = tempfile.TemporaryDirectory()
        self.m_ipad = Path(self.m_directory.name) / 'ipad'
        self.m_mac = Path(self.m_directory.name) / 'mac'
        for root in (self.m_ipad, self.m_mac):
            for folder in ('patches', 'recordings', 'logs'):
                (root / folder).mkdir(parents=True)
        self.m_server = create_server('127.0.0.1', 0, self.m_mac, 'test-code')
        self.m_server.socket = self.m_context.wrap_socket(self.m_server.socket, server_side=True, do_handshake_on_connect=False)
        self.m_thread = threading.Thread(target=self.m_server.serve_forever)
        self.m_thread.start()

    def tearDown(self):
        self.m_server.shutdown()
        self.m_server.server_close()
        self.m_thread.join()
        self.m_directory.cleanup()

    def RunClient(self, cancel=-1, fingerprint=None, mode="reopen"):
        return subprocess.run([str(self.m_binary), str(self.m_ipad), str(self.m_server.server_port),
                               fingerprint or self.m_fingerprint, 'test-code', str(cancel), mode],
                              text=True, capture_output=True, timeout=25)

    def Recording(self, name='take.sgrec'):
        body = (ROOT / 'private/test/fixtures/streaming-recording/golden.sgrec').read_bytes()
        path = self.m_ipad / 'recordings' / name
        path.write_bytes(body)
        return path, body

    def test_full_sync_and_repeat_preserves_patches_logs_and_extracts_stereo(self):
        source, original = self.Recording()
        (self.m_ipad / 'patches/from-ipad.json').write_bytes(b'{"ipad":1}')
        (self.m_mac / 'patches/from-mac.json').write_bytes(b'{"mac":1}')
        (self.m_ipad / 'patches/conflict.json').write_bytes(b'ipad version')
        (self.m_mac / 'patches/conflict.json').write_bytes(b'mac version')
        (self.m_ipad / 'logs/session.log').write_bytes(b'line one\n')
        result = self.RunClient()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn('Sync complete', result.stdout)
        self.assertFalse(source.exists())
        self.assertEqual((self.m_mac / 'recordings/take.sgrec').read_bytes(), original)
        self.assertTrue((self.m_mac / 'recordings/take_stereo.wav').exists())
        self.assertEqual((self.m_mac / 'patches/from-ipad.json').read_bytes(), b'{"ipad":1}')
        self.assertEqual((self.m_ipad / 'patches/from-mac.json').read_bytes(), b'{"mac":1}')
        self.assertEqual((self.m_ipad / 'patches/conflict.json').read_bytes(), b'ipad version')
        self.assertEqual((self.m_mac / 'patches/conflict.json').read_bytes(), b'mac version')
        self.assertEqual((self.m_mac / 'logs/session.log').read_bytes(), b'line one\n')
        self.assertTrue((self.m_ipad / 'logs/session.log').exists())
        result = self.RunClient()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_bonjour_lists_receiver_and_close_clears_it(self):
        advertiser = subprocess.Popen(['dns-sd', '-R', 'SmartGridTest-' + uuid.uuid4().hex[:8],
                                       '_sgsync._tcp.', 'local.', str(self.m_server.server_port),
                                       'version=1', 'id=test', 'fingerprint=' + self.m_fingerprint],
                                      stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        try:
            result = self.RunClient(mode='discovery')
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn('discovered', result.stdout)
        finally:
            advertiser.terminate()
            advertiser.wait(timeout=3)

    def test_background_cancels_stalled_upload_without_publishing(self):
        entered = threading.Event()
        release = threading.Event()
        class StalledUpload(SyncHandler):
            def do_PUT(self):
                self.rfile.read(4096)
                entered.set()
                release.wait(4)
        self.m_server.RequestHandlerClass = StalledUpload
        patch = self.m_ipad / 'patches/large.json'
        with patch.open('wb') as output:
            output.truncate(32 * 1024 * 1024)
        try:
            started = time.monotonic()
            result = self.RunClient(cancel=500, mode='background')
            self.assertTrue(entered.is_set())
            self.assertLess(time.monotonic() - started, 3)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertTrue(patch.exists())
            self.assertFalse((self.m_mac / 'patches/large.json').exists())
        finally:
            release.set()

    def test_close_during_patch_download_removes_partial_file(self):
        entered = threading.Event()
        release = threading.Event()
        class StalledDownload(SyncHandler):
            def do_GET(self):
                if not self.path.startswith('/v1/patch?'):
                    return super().do_GET()
                self.send_response(200)
                self.send_header('Content-Length', '8388608')
                self.end_headers()
                self.wfile.write(b'x' * 4096)
                entered.set()
                release.wait(4)
        self.m_server.RequestHandlerClass = StalledDownload
        (self.m_mac / 'patches/from-mac.json').write_bytes(b'hello')
        try:
            result = self.RunClient(cancel=500)
            self.assertTrue(entered.is_set())
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertEqual(list((self.m_ipad / 'patches').iterdir()), [])
        finally:
            release.set()

    def test_close_during_extraction_retains_original(self):
        entered = threading.Event()
        release = threading.Event()
        extract = self.m_server.Extract
        def delayed_extract(*args):
            entered.set()
            release.wait(4)
            extract(*args)
        self.m_server.Extract = delayed_extract
        source, original = self.Recording()
        try:
            result = self.RunClient(cancel=500)
            self.assertTrue(entered.is_set())
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertEqual(source.read_bytes(), original)
            self.assertEqual((self.m_mac / 'recordings/take.sgrec').read_bytes(), original)
        finally:
            release.set()

    def test_wrong_certificate_keeps_every_source(self):
        source, original = self.Recording()
        result = self.RunClient(fingerprint='0' * 64)
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertEqual(source.read_bytes(), original)
        self.assertFalse((self.m_mac / 'recordings/take.sgrec').exists())

    def test_unfinished_recording_is_skipped(self):
        source, original = self.Recording()
        source.write_bytes(original[:-16])
        result = self.RunClient()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn('unfinished recordings retained', result.stdout)
        self.assertEqual(source.read_bytes(), original[:-16])
        self.assertFalse((self.m_mac / 'recordings/take.sgrec').exists())

    def test_extraction_failure_does_not_delete_source(self):
        source, original = self.Recording()
        damaged = bytearray(original)
        damaged[-20] ^= 0xff
        source.write_bytes(damaged)
        result = self.RunClient()
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertEqual(source.read_bytes(), damaged)
        self.assertIn('extraction failed', result.stdout)

    def test_close_cancels_stalled_request_and_reopen_has_no_transfer(self):
        release = threading.Event()
        entered = threading.Event()
        calls = []
        class Stalled(SyncHandler):
            def do_GET(self):
                calls.append(self.path)
                entered.set()
                release.wait(4)
        self.m_server.RequestHandlerClass = Stalled
        source, original = self.Recording()
        start = time.monotonic()
        try:
            result = self.RunClient(cancel=300)
            self.assertLess(time.monotonic() - start, 3)
            self.assertTrue(entered.is_set())
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn('cancelled', result.stdout)
            self.assertEqual(source.read_bytes(), original)
            time.sleep(0.15)
            self.assertEqual(calls, ['/v1/manifest?'])
        finally:
            release.set()


if __name__ == '__main__':
    unittest.main()
