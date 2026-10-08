#!/usr/bin/env python3
"""Bounded, per-platform RomM listing cache; compatible with /api/roms pages."""
import json
import os
import threading
import time
import urllib.parse
import urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

UPSTREAM = os.environ['ROMM_URL'].rstrip('/')
TOKEN = os.environ['ROMM_TOKEN']
TTL = max(60, int(os.getenv('ROMM_INDEX_TTL', '900')))
PAGE = 250
MAX_ROMS = int(os.getenv('ROMM_INDEX_MAX_ROMS', '100000'))
_cache = {}
_lock = threading.Lock()


def upstream_page(platform, offset):
    query = urllib.parse.urlencode({'platform_ids': platform, 'limit': PAGE, 'offset': offset,
                                    'with_char_index': 'false', 'with_filter_values': 'false',
                                    'with_rom_id_index': 'false'})
    req = urllib.request.Request(f'{UPSTREAM}/api/roms?{query}',
                                 headers={'Authorization': 'Bearer ' + TOKEN, 'Accept': 'application/json'})
    with urllib.request.urlopen(req, timeout=45) as response:
        return json.load(response)


def platform_games(platform):
    now = time.monotonic()
    with _lock:
        entry = _cache.get(platform)
        if entry and entry[0] > now:
            return entry[1]
        games = []
        while True:
            payload = upstream_page(platform, len(games))
            batch = payload if isinstance(payload, list) else payload.get('items', payload.get('roms', []))
            if not isinstance(batch, list):
                raise ValueError('Unexpected RomM listing format')
            if not batch:
                break
            if len(games) + len(batch) > MAX_ROMS:
                raise ValueError('Platform exceeds ROMM_INDEX_MAX_ROMS')
            games.extend(batch)
            total = len(payload) if isinstance(payload, list) else int(payload.get('total', len(games)))
            if len(games) >= total:
                break
        _cache[platform] = (time.monotonic() + TTL, games)
        return games


class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        path = urllib.parse.urlsplit(self.path)
        if path.path not in ('/api/roms', '/api/roms/index-search'):
            self.send_error(404)
            return
        args = urllib.parse.parse_qs(path.query)
        try:
            platform = int(args.get('platform_ids', ['0'])[0])
            limit = min(500, max(1, int(args.get('limit', ['100'])[0])))
            offset = max(0, int(args.get('offset', ['0'])[0]))
            if platform <= 0:
                raise ValueError('platform_ids is required')
            games = platform_games(platform)
            if path.path == '/api/roms/index-search':
                term = args.get('q', [''])[0].casefold()
                letter = args.get('letter', [''])[0].upper()
                if letter and (len(letter) != 1 or not ('A' <= letter <= 'Z')):
                    raise ValueError('Invalid letter')
                games = [g for g in games if
                         (not term or term in str(g.get('name', '')).casefold() or
                          term in str(g.get('fs_name', '')).casefold()) and
                         (not letter or str(g.get('name', '')).upper().startswith(letter))]
            # Retain the RomM paginated response shape used by libromm.
            data = json.dumps({'items': games[offset:offset + limit], 'total': len(games)},
                              ensure_ascii=True, separators=(',', ':')).encode('utf-8')
        except (ValueError, KeyError, TimeoutError, OSError) as exc:
            self.send_error(502, str(exc)[:150])
            return
        self.send_response(200)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Content-Length', str(len(data)))
        self.end_headers()
        self.wfile.write(data)


if __name__ == '__main__':
    ThreadingHTTPServer(('127.0.0.1', 8091), Handler).serve_forever()
