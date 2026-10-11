#!/usr/bin/env python3
"""One-time Mac setup: python3 scripts/soundcloud_connect.py.

Register a SoundCloud app with an exact loopback redirect such as
http://127.0.0.1:47659/callback, then enter that same URL when prompted.
Credentials and rotating authorization tokens are stored only in Keychain.
"""

import base64
import ctypes
import getpass
import hashlib
import hmac
import json
import secrets
import sys
import time
import webbrowser
from http.server import BaseHTTPRequestHandler, HTTPServer
from urllib.parse import parse_qs, urlencode, urlsplit

from soundcloud_api import HTTPTransport, SoundCloudAPI, x_token_url


class KeychainStorage:
    def __init__(self):
        if sys.platform != 'darwin':
            raise RuntimeError('SoundCloud connection requires macOS Keychain')
        self.m_security = ctypes.CDLL('/System/Library/Frameworks/Security.framework/Security')
        self.m_service = b'com.theallelectricsmartgrid.soundcloud'
        self.m_account = b'publisher'
        self.m_security.SecKeychainFindGenericPassword.argtypes = [ctypes.c_void_p, ctypes.c_uint32, ctypes.c_char_p, ctypes.c_uint32, ctypes.c_char_p, ctypes.POINTER(ctypes.c_uint32), ctypes.POINTER(ctypes.c_void_p), ctypes.POINTER(ctypes.c_void_p)]
        self.m_security.SecKeychainAddGenericPassword.argtypes = [ctypes.c_void_p, ctypes.c_uint32, ctypes.c_char_p, ctypes.c_uint32, ctypes.c_char_p, ctypes.c_uint32, ctypes.c_char_p, ctypes.POINTER(ctypes.c_void_p)]
        self.m_security.SecKeychainItemModifyAttributesAndData.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_uint32, ctypes.c_char_p]
        self.m_security.SecKeychainItemFreeContent.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
        self.m_core = ctypes.CDLL('/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation')
        self.m_core.CFRelease.argtypes = [ctypes.c_void_p]

    def Find(self):
        length, content, item = ctypes.c_uint32(), ctypes.c_void_p(), ctypes.c_void_p()
        status = self.m_security.SecKeychainFindGenericPassword(None, len(self.m_service), self.m_service,
                    len(self.m_account), self.m_account, ctypes.byref(length), ctypes.byref(content), ctypes.byref(item))
        if status == -25300:
            return None, None
        if status:
            raise RuntimeError(f'Keychain access failed ({status})')
        try:
            value = ctypes.string_at(content, length.value)
        finally:
            self.m_security.SecKeychainItemFreeContent(None, content)
        return value, item

    def Load(self):
        value, item = self.Find()
        try:
            return json.loads(value) if value else None
        finally:
            if item:
                self.m_core.CFRelease(item)

    def Save(self, value):
        payload = json.dumps(value, separators=(',', ':')).encode()
        _, item = self.Find()
        try:
            if item:
                status = self.m_security.SecKeychainItemModifyAttributesAndData(item, None, len(payload), payload)
            else:
                status = self.m_security.SecKeychainAddGenericPassword(None, len(self.m_service), self.m_service,
                             len(self.m_account), self.m_account, len(payload), payload, None)
            if status:
                raise RuntimeError(f'Keychain save failed ({status})')
        finally:
            if item:
                self.m_core.CFRelease(item)


class ConnectionFlow:
    def __init__(self, client_id, client_secret, redirect_uri, storage, transport=None):
        parsed = urlsplit(redirect_uri)
        if parsed.scheme != 'http' or parsed.hostname != '127.0.0.1' or not parsed.port or parsed.query or parsed.fragment or parsed.username or parsed.password:
            raise ValueError('Use the registered http://127.0.0.1:PORT/callback URL')
        self.m_client_id = client_id
        self.m_client_secret = client_secret
        self.m_redirect_uri = redirect_uri
        self.m_storage = storage
        self.m_transport = transport or HTTPTransport()
        self.m_state = secrets.token_urlsafe(32)
        self.m_verifier = secrets.token_urlsafe(64)
        self.m_complete = False

    def AuthorizationURL(self):
        challenge = base64.urlsafe_b64encode(hashlib.sha256(self.m_verifier.encode()).digest()).rstrip(b'=').decode()
        return 'https://secure.soundcloud.com/authorize?' + urlencode({
            'client_id': self.m_client_id, 'redirect_uri': self.m_redirect_uri,
            'response_type': 'code', 'code_challenge': challenge,
            'code_challenge_method': 'S256', 'state': self.m_state})

    def Complete(self, callback):
        parsed = urlsplit(callback)
        query = parse_qs(parsed.query, keep_blank_values=True)
        state = query.get('state', [])
        code = query.get('code', [])
        if self.m_complete or parsed.path != urlsplit(self.m_redirect_uri).path or len(state) != 1 or not hmac.compare_digest(state[0], self.m_state) or len(code) != 1 or not code[0]:
            raise ValueError('Invalid SoundCloud callback state or code')
        values = {'grant_type': 'authorization_code', 'client_id': self.m_client_id,
                  'client_secret': self.m_client_secret, 'redirect_uri': self.m_redirect_uri,
                  'code_verifier': self.m_verifier, 'code': code[0]}
        status, tokens = self.m_transport.Request('POST', x_token_url,
                           {'Content-Type': 'application/x-www-form-urlencoded', 'Accept': 'application/json'},
                           [urlencode(values).encode()])
        if status != 200 or not tokens.get('access_token') or not tokens.get('refresh_token'):
            raise RuntimeError('SoundCloud token exchange failed')
        tokens = dict(tokens)
        tokens.update({'client_id': self.m_client_id, 'client_secret': self.m_client_secret,
                       'expires_at': time.time() + float(tokens['expires_in'])})
        status, account = self.m_transport.Request('GET', 'https://api.soundcloud.com/me',
                           {'Authorization': 'OAuth ' + tokens['access_token'], 'Accept': 'application/json'}, None)
        if status != 200 or not (account.get('urn') or account.get('id')):
            raise RuntimeError('SoundCloud account verification failed')
        tokens['account'] = {'id': str(account.get('urn') or account['id']), 'name': account.get('username', '')}
        self.m_storage.Save(tokens)
        self.m_complete = True
        return tokens['account']


def LoadSavedAPI():
    storage = KeychainStorage()
    credentials = storage.Load()
    return SoundCloudAPI(storage, credentials=credentials) if credentials else None


def main():
    client_id = getpass.getpass('SoundCloud app client ID: ').strip()
    client_secret = getpass.getpass('SoundCloud app client secret: ').strip()
    redirect = input('Registered loopback callback URL [http://127.0.0.1:47659/callback]: ').strip() or 'http://127.0.0.1:47659/callback'
    flow = ConnectionFlow(client_id, client_secret, redirect, KeychainStorage())

    class Callback(BaseHTTPRequestHandler):
        def log_message(self, format_string, *args):
            pass

        def do_GET(self):
            try:
                flow.Complete(self.path)
                status, body = 200, b'SoundCloud connected. You can close this window.'
            except ValueError:
                status, body = 400, b'Invalid authorization callback.'
            except Exception:
                status, body = 502, b'Connection failed. Run Mac setup again.'
            self.send_response(status)
            self.send_header('Content-Type', 'text/plain')
            self.send_header('Content-Length', str(len(body)))
            self.end_headers()
            self.wfile.write(body)

    parsed = urlsplit(redirect)
    with HTTPServer(('127.0.0.1', parsed.port), Callback) as server:
        server.timeout = 1
        webbrowser.open(flow.AuthorizationURL())
        deadline = time.monotonic() + 300
        while not flow.m_complete and time.monotonic() < deadline:
            server.handle_request()
    if not flow.m_complete:
        raise RuntimeError('SoundCloud authorization timed out')
    print('SoundCloud account connected in Mac Keychain.')


if __name__ == '__main__':
    main()
