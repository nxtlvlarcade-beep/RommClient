"""Fetch official WHDLoad game installers. No RomM credentials leave the gateway."""
import html
import io
import re
import unicodedata
import urllib.parse
import urllib.request
import zipfile
from html.parser import HTMLParser

ROOT = 'https://www.whdload.de/'
MAX_INDEX = 3 * 1024 * 1024
MAX_INSTALLER = 12 * 1024 * 1024

class CatalogError(ValueError):
    pass

class Links(HTMLParser):
    def __init__(self):
        super().__init__()
        self.links = []
        self.href = None
        self.label = []
    def handle_starttag(self, tag, attrs):
        if tag == 'a':
            self.href = dict(attrs).get('href')
            self.label = []
    def handle_data(self, data):
        if self.href is not None:
            self.label.append(data)
    def handle_endtag(self, tag):
        if tag == 'a' and self.href:
            self.links.append((self.href, ' '.join(self.label)))
            self.href = None

def fetch(url, limit):
    parts = urllib.parse.urlsplit(url)
    if parts.scheme != 'https' or parts.hostname not in ('www.whdload.de', 'whdload.de') or parts.username or parts.password or parts.port:
        raise CatalogError('Untrusted WHDLoad URL')
    request = urllib.request.Request(url, headers={'User-Agent': 'RommClient-WHDLoad/1.0'})
    with urllib.request.urlopen(request, timeout=25) as response:
        final = urllib.parse.urlsplit(response.geturl())
        if final.scheme != 'https' or final.hostname not in ('www.whdload.de', 'whdload.de'):
            raise CatalogError('Installer redirected outside whdload.de')
        data = response.read(limit + 1)
    if len(data) > limit:
        raise CatalogError('WHDLoad download exceeds size limit')
    return data

def norm(s):
    s = unicodedata.normalize('NFKD', s)
    s = ''.join(c for c in s if not unicodedata.combining(c))
    s = re.sub(r'\([^)]*\)|\[[^]]*\]', ' ', s)
    s = re.sub(r'\b(adf|ipf|disk|disc|amiga|usa|europe|world|rev|version|v\d+)\b', ' ', s, flags=re.I)
    return re.sub(r'[^a-z0-9]', '', s.lower())

def candidate_titles(metadata):
    game = metadata.get('game')
    values = [metadata.get('name'), metadata.get('fs_name')]
    if isinstance(game, dict):
        values += [game.get('name'), game.get('title')]
    result = set()
    for v in values:
        if isinstance(v, str):
            v = re.sub(r'\.(adf|ipf|zip|lha|lzx)$', '', v, flags=re.I)
            key = norm(v)
            if len(key) >= 4:
                result.add(key)
    return result

def official_installer(metadata):
    titles = candidate_titles(metadata)
    if not titles:
        raise CatalogError('No usable ROM title for installer lookup')
    # Official single-page games index; no untrusted third-party mirrors.
    index = fetch(ROOT + 'download.html', MAX_INDEX).decode('latin-1', 'replace')
    parser = Links(); parser.feed(index)
    pages = []
    for href, label in parser.links:
        url = urllib.parse.urljoin(ROOT + 'download.html', html.unescape(href))
        path = urllib.parse.urlsplit(url).path
        if 'games' in path.lower() and path.lower().endswith('.html') and ('all' in (href + label).lower()):
            pages.append(url)
    # Official all-games index is linked from download.html; fail closed if layout changes.
    pages = list(dict.fromkeys(pages))
    if not pages:
        raise CatalogError('Official WHDLoad games index not found')
    matches = {}
    for page in pages[:4]:
        listing = Links(); listing.feed(fetch(page, MAX_INDEX).decode('latin-1', 'replace'))
        for href, label in listing.links:
            target = urllib.parse.urljoin(page, html.unescape(href))
            path = urllib.parse.urlsplit(target).path
            if not path.startswith('/games/') or not path.lower().endswith('.html'):
                continue
            key = norm(label) or norm(path.rsplit('/', 1)[-1][:-5])
            stem = norm(path.rsplit('/', 1)[-1][:-5])
            if key in titles or stem in titles:
                matches[target] = label
    if len(matches) != 1:
        raise CatalogError('No unique official installer match for ROM title (%d candidates)' % len(matches))
    page, title = next(iter(matches.items()))
    details = Links(); details.feed(fetch(page, MAX_INDEX).decode('latin-1', 'replace'))
    archives = []
    for href, label in details.links:
        url = urllib.parse.urljoin(page, html.unescape(href))
        path = urllib.parse.urlsplit(url).path
        if path.lower().endswith('.lha') and not re.search(r'[-_]\d{6,8}\.lha$', path, re.I):
            archives.append(url)
    archives = list(dict.fromkeys(archives))
    if len(archives) != 1:
        raise CatalogError('No unique current LHA installer on official game page')
    data = fetch(archives[0], MAX_INSTALLER)
    if len(data) < 32 or data[2:5] not in (b'-lh', b'-lz'):
        raise CatalogError('Downloaded file is not an LHA/LZH archive')
    out = io.BytesIO()
    with zipfile.ZipFile(out, 'w', compression=zipfile.ZIP_DEFLATED) as z:
        z.writestr('installer.lha', data)
        z.writestr('romm-installer.txt', 'Title: %s\nSource: %s\nInstaller: %s\n\nThis is an installer, NOT a playable game.\nUse the matching original disk and follow the installer instructions.\n' % (title, page, archives[0]))
    return out.getvalue()
