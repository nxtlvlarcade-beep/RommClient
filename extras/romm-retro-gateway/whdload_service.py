#!/usr/bin/env python3
"""Authenticated, bounded WHDLoad package preparation service.
Only existing .slave-based packages are supported; no ADF conversion.
"""
import io
import json
import os
import pathlib
import re
import resource
import subprocess
import tempfile
import urllib.error
import urllib.parse
import urllib.request
import zipfile
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

UPSTREAM = os.environ['ROMM_URL'].rstrip('/')
TOKEN = os.environ['ROMM_TOKEN']
LIMIT = int(os.getenv('WHDLOAD_MAX_BYTES', str(256 * 1024 * 1024)))
MAX_FILES = 4000


def get(url):
    req = urllib.request.Request(url, headers={'Authorization': 'Bearer ' + TOKEN})
    with urllib.request.urlopen(req, timeout=90) as response:
        if int(response.headers.get('Content-Length', '0')) > LIMIT:
            raise ValueError('ROM exceeds configured limit')
        data = response.read(LIMIT + 1)
        if len(data) > LIMIT:
            raise ValueError('ROM exceeds configured limit')
        return data


def safe_relative(name):
    path = pathlib.PurePosixPath(name.replace('\\', '/'))
    return (not path.is_absolute() and not any(p in ('', '.', '..') for p in path.parts)
            and ':' not in name and '\x00' not in name and len(name) <= 240
            and bool(re.fullmatch(r"[A-Za-z0-9 _./()\[\]-]+", name)))


def prepare(rom_id):
    meta = json.loads(get(f'{UPSTREAM}/api/roms/{rom_id}'))
    filename = meta.get('fs_name') or ''
    if not filename or not isinstance(filename, str):
        raise ValueError('ROM filename unavailable')
    source = get(f'{UPSTREAM}/api/roms/{rom_id}/content/{urllib.parse.quote(filename, safe="")}')
    with tempfile.TemporaryDirectory(prefix='romm-whd-') as tmp:
        archive = pathlib.Path(tmp) / ('source' + pathlib.Path(filename).suffix.lower())
        archive.write_bytes(source)
        target = pathlib.Path(tmp) / 'unpacked'
        target.mkdir()
        # Reject unsafe archive entries BEFORE extraction.
        listing = subprocess.run(['bsdtar', '-tf', str(archive)], capture_output=True, text=True, timeout=30)
        if listing.returncode:
            raise ValueError('Unsupported or corrupt archive')
        names = listing.stdout.splitlines()
        if not names or len(names) > MAX_FILES or not all(safe_relative(n.rstrip('/')) for n in names):
            raise ValueError('Unsafe archive paths or too many files')
        def limit_filesize():
            resource.setrlimit(resource.RLIMIT_FSIZE, (LIMIT, LIMIT))
        extraction = subprocess.run(['bsdtar', '-xf', str(archive), '-C', str(target)],
                                    capture_output=True, timeout=90, preexec_fn=limit_filesize)
        if extraction.returncode:
            raise ValueError('Archive extraction failed')
        files = []
        total = 0
        for root, dirs, names in os.walk(target, followlinks=False):
            for d in dirs:
                if (pathlib.Path(root) / d).is_symlink():
                    raise ValueError('Links are not allowed')
            for name in names:
                p = pathlib.Path(root) / name
                if not p.is_file() or p.is_symlink():
                    raise ValueError('Non-regular archive entry')
                rel = p.relative_to(target).as_posix()
                if not safe_relative(rel):
                    raise ValueError('Unsafe filename')
                total += p.stat().st_size
                if total > LIMIT or len(files) >= MAX_FILES:
                    raise ValueError('Expanded archive too large')
                files.append((rel, p))
        if any(rel.lower() == 'romm-launch.txt' for rel, _ in files):
            raise ValueError('Reserved manifest filename')
        slaves = [rel for rel, _ in files if rel.lower().endswith('.slave')]
        if len(slaves) != 1:
            raise ValueError('Exactly one WHDLoad .slave is required')
        # The Amiga client reads this relative path after extraction.
        out = io.BytesIO()
        with zipfile.ZipFile(out, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=3) as z:
            for rel, path in files:
                z.write(path, rel)
            z.writestr('romm-launch.txt', slaves[0] + '\n')
        if out.tell() > LIMIT:
            raise ValueError('Prepared archive too large')
        return out.getvalue()


class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.headers.get('Authorization', '') != 'Bearer ' + TOKEN:
            self.send_error(403)
            return
        match = re.fullmatch(r'/whdload/([1-9][0-9]*)', self.path)
        if not match:
            self.send_error(404)
            return
        try:
            data = prepare(int(match.group(1)))
        except (ValueError, urllib.error.HTTPError, subprocess.TimeoutExpired, OSError) as exc:
            self.send_error(422, str(exc)[:180])
            return
        self.send_response(200)
        self.send_header('Content-Type', 'application/zip')
        self.send_header('Content-Disposition', f'attachment; filename="romm-{match.group(1)}-whdload.zip"')
        self.send_header('Content-Length', str(len(data)))
        self.end_headers()
        self.wfile.write(data)


if __name__ == '__main__':
    ThreadingHTTPServer(('127.0.0.1', 8090), Handler).serve_forever()
