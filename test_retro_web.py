import importlib.util
import unittest
from unittest.mock import patch
from http.server import ThreadingHTTPServer
from threading import Thread
import urllib.request
import os
os.environ.setdefault('ROMM_URL', 'http://example.invalid')
os.environ.setdefault('ROMM_TOKEN', 'test')
spec = importlib.util.spec_from_file_location('retro_web', os.path.join(os.path.dirname(__file__), 'extras/romm-retro-gateway/retro_web.py'))
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)

class TestWeb(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.server = ThreadingHTTPServer(('127.0.0.1', 0), m.Handler)
        cls.thread = Thread(target=cls.server.serve_forever, daemon=True)
        cls.thread.start()
        cls.url = 'http://127.0.0.1:%d' % cls.server.server_port
    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown(); cls.server.server_close()
    def test_platforms_and_head(self):
        with patch.object(m, 'api', return_value=[{'id': 5, 'name': 'Amiga & Friends'}]):
            with urllib.request.urlopen(self.url + '/') as r:
                page = r.read().decode('latin1')
                self.assertIn('Amiga &amp; Friends', page)
                self.assertIn('/games?p=5', page)
            req = urllib.request.Request(self.url + '/', method='HEAD')
            with urllib.request.urlopen(req) as r:
                self.assertEqual(r.status, 200)
                self.assertEqual(r.read(), b'')
    def test_detail(self):
        with patch.object(m, 'api', return_value={'id': 2, 'name': 'Test', 'fs_name': 'Test.zip', 'path_cover_small': '/assets/test.jpg', 'platform_id': 5}):
            with urllib.request.urlopen(self.url + '/game?id=2') as r:
                page = r.read().decode('latin1')
                self.assertIn('/cover?id=2', page)
                self.assertIn('/download?id=2', page)
    def test_invalid_id(self):
        try: urllib.request.urlopen(self.url + '/game?id=abc')
        except urllib.error.HTTPError as e: self.assertEqual(e.code, 400)
        else: self.fail('expected 400')

if __name__ == '__main__': unittest.main()
