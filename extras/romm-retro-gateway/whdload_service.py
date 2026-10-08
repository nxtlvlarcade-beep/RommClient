
#!/usr/bin/env python3
"""
RomM WHDLoad package preparation service.

Endpoint:
    GET /whdload/<rom_id>

Environment:
    ROMM_URL
    ROMM_TOKEN
    WHDLOAD_MAX_BYTES (optional, default 256 MiB)

Supported input:
    ZIP and TAR archives containing exactly one WHDLoad .slave
    and the required game files.

Output:
    ZIP containing the game files and romm-launch.txt.

Important:
    ADF/IPF images are not automatically converted to WHDLoad.
    A compatible installer and a real installation process are
    required for that.
"""

import hashlib
import io
import json
import os
import pathlib
import re
import tarfile
import tempfile
import urllib.error
import urllib.parse
import urllib.request
import zipfile

from whdload_catalog import official_installer, CatalogError

from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer


UPSTREAM = os.environ["ROMM_URL"].rstrip("/")
TOKEN = os.environ["ROMM_TOKEN"]

LIMIT = int(
    os.getenv("WHDLOAD_MAX_BYTES", str(256 * 1024 * 1024))
)

MAX_FILES = 4000
MAX_NAME = 240


class PackageError(ValueError):
    pass


def get(url):
    request = urllib.request.Request(
        url,
        headers={
            "Authorization": "Bearer " + TOKEN,
            "Accept": "*/*",
        },
    )

    with urllib.request.urlopen(request, timeout=90) as response:
        content_length = response.headers.get("Content-Length")

        if content_length:
            if int(content_length) > LIMIT:
                raise PackageError(
                    "ROM exceeds configured size limit"
                )

        data = response.read(LIMIT + 1)

        if len(data) > LIMIT:
            raise PackageError(
                "ROM exceeds configured size limit"
            )

        return data


def safe_relative(name):
    if not isinstance(name, str):
        return False

    if not name or len(name) > MAX_NAME:
        return False

    if "\\" in name or ":" in name or "\x00" in name:
        return False

    path = pathlib.PurePosixPath(name)

    if path.is_absolute():
        return False

    if any(part in ("", ".", "..") for part in name.split("/")):
        return False

    return bool(
        re.fullmatch(
            r"[A-Za-z0-9 _./()\[\]-]+",
            name,
        )
    )


def validate_hash(data, metadata):
    """
    Verify hashes when RomM provides a recognizable hash value.

    Unknown metadata structures are ignored rather than guessed.
    """

    hash_fields = (
        ("md5_hash", "md5"),
        ("sha1_hash", "sha1"),
        ("sha256_hash", "sha256"),
    )

    for field, algorithm in hash_fields:
        expected = metadata.get(field)

        if not isinstance(expected, str) or not expected:
            continue

        expected = expected.strip().lower()

        if not re.fullmatch(r"[0-9a-f]+", expected):
            continue

        digest = hashlib.new(algorithm, data).hexdigest()

        if len(expected) != len(digest):
            continue

        if digest != expected:
            raise PackageError(
                "ROM hash mismatch: " + algorithm
            )


def inspect_zip(archive):
    files = []
    total = 0

    with zipfile.ZipFile(archive, "r") as z:
        infos = z.infolist()

        if len(infos) > MAX_FILES:
            raise PackageError(
                "Archive contains too many entries"
            )

        seen = set()

        for info in infos:
            name = info.filename

            if info.is_dir():
                name = name.rstrip("/")

                if name and not safe_relative(name):
                    raise PackageError(
                        "Unsafe directory name"
                    )

                continue

            if not safe_relative(name):
                raise PackageError(
                    "Unsafe archive filename"
                )

            # Reject Unix symlinks and special file types.
            mode = (info.external_attr >> 16) & 0o170000

            if mode not in (0, 0o100000):
                raise PackageError(
                    "Archive contains a link or special file"
                )

            key = name.lower()

            if key in seen:
                raise PackageError(
                    "Duplicate archive filename"
                )

            seen.add(key)

            if key == "romm-launch.txt":
                raise PackageError(
                    "Reserved manifest filename"
                )

            total += info.file_size

            if total > LIMIT:
                raise PackageError(
                    "Expanded archive exceeds size limit"
                )

            if info.file_size > LIMIT:
                raise PackageError(
                    "Archive entry exceeds size limit"
                )

            files.append(info)

        slaves = [
            info.filename
            for info in files
            if info.filename.lower().endswith(".slave")
        ]

        if len(slaves) != 1:
            raise PackageError(
                "No unique WHDLoad .slave found; "
                "this ROM needs a compatible WHDLoad installer"
            )

        result = []

        for info in files:
            with z.open(info, "r") as source:
                data = source.read(info.file_size + 1)

            if len(data) != info.file_size:
                raise PackageError(
                    "Archive entry size mismatch"
                )

            result.append((info.filename, data))

    return slaves[0], result


def inspect_tar(archive):
    files = []
    total = 0

    with tarfile.open(archive, "r:*") as tar:
        members = tar.getmembers()

        if len(members) > MAX_FILES:
            raise PackageError(
                "Archive contains too many entries"
            )

        seen = set()

        for member in members:
            name = member.name

            if member.isdir():
                if not safe_relative(name.rstrip("/")):
                    raise PackageError(
                        "Unsafe directory name"
                    )
                continue

            if not member.isfile():
                raise PackageError(
                    "Archive contains a link or special file"
                )

            if not safe_relative(name):
                raise PackageError(
                    "Unsafe archive filename"
                )

            key = name.lower()

            if key in seen:
                raise PackageError(
                    "Duplicate archive filename"
                )

            seen.add(key)

            if key == "romm-launch.txt":
                raise PackageError(
                    "Reserved manifest filename"
                )

            total += member.size

            if total > LIMIT:
                raise PackageError(
                    "Expanded archive exceeds size limit"
                )

            files.append(member)

        slaves = [
            member.name
            for member in files
            if member.name.lower().endswith(".slave")
        ]

        if len(slaves) != 1:
            raise PackageError(
                "No unique WHDLoad .slave found; "
                "this ROM needs a compatible WHDLoad installer"
            )

        result = []

        for member in files:
            source = tar.extractfile(member)

            if source is None:
                raise PackageError(
                    "Cannot read archive entry"
                )

            with source:
                data = source.read(member.size + 1)

            if len(data) != member.size:
                raise PackageError(
                    "Archive entry size mismatch"
                )

            result.append((member.name, data))

    return slaves[0], result


def inspect_archive(data):
    archive = io.BytesIO(data)

    if zipfile.is_zipfile(archive):
        archive.seek(0)
        return inspect_zip(archive)

    archive.seek(0)

    try:
        return inspect_tar(archive)
    except (tarfile.TarError, EOFError) as exc:
        raise PackageError(
            "Unsupported archive format; "
            "ADF/IPF conversion requires a WHDLoad installer"
        ) from exc


def make_package(slave, files):
    output = io.BytesIO()

    with zipfile.ZipFile(
        output,
        "w",
        compression=zipfile.ZIP_DEFLATED,
        compresslevel=3,
        allowZip64=True,
    ) as z:
        for name, data in files:
            z.writestr(name, data)

        z.writestr(
            "romm-launch.txt",
            slave + "\n",
        )

    result = output.getvalue()

    if len(result) > LIMIT:
        raise PackageError(
            "Prepared WHDLoad package exceeds size limit"
        )

    return result


def prepare(rom_id):
    metadata_url = (
        f"{UPSTREAM}/api/roms/{rom_id}"
    )

    metadata = json.loads(get(metadata_url))

    if not isinstance(metadata, dict):
        raise PackageError(
            "Invalid RomM metadata"
        )

    filename = metadata.get("fs_name")

    if not isinstance(filename, str) or not filename:
        raise PackageError(
            "ROM filename unavailable"
        )

    if "/" in filename or "\\" in filename:
        raise PackageError(
            "Invalid ROM filename"
        )

    encoded_name = urllib.parse.quote(
        filename,
        safe="",
    )

    content_url = (
        f"{UPSTREAM}/api/roms/"
        f"{rom_id}/content/{encoded_name}"
    )

    source = get(content_url)
    validate_hash(source, metadata)
    try:
        slave, files = inspect_archive(source)
    except PackageError:
        # No pre-installed slave: retrieve the official game-specific installer.
        # Never pretend that an installer archive contains playable game data.
        return official_installer(metadata)
    return make_package(slave, files)


class Handler(BaseHTTPRequestHandler):

    def do_GET(self):
        authorization = self.headers.get(
            "Authorization",
            "",
        )

        if authorization != "Bearer " + TOKEN:
            self.send_error(
                403,
                "Forbidden",
            )
            return

        match = re.fullmatch(
            r"/whdload/([1-9][0-9]*)",
            self.path,
        )

        if not match:
            self.send_error(404)
            return

        rom_id = int(match.group(1))

        try:
            data = prepare(rom_id)

        except (
            PackageError,
            CatalogError,
            ValueError,
            KeyError,
            urllib.error.HTTPError,
            urllib.error.URLError,
            TimeoutError,
            OSError,
            zipfile.BadZipFile,
            tarfile.TarError,
        ) as exc:
            self.send_error(
                422,
                str(exc)[:180],
            )
            return

        self.send_response(200)

        self.send_header(
            "Content-Type",
            "application/zip",
        )

        self.send_header(
            "Content-Disposition",
            'attachment; filename="romm-'
            + str(rom_id)
            + '-whdload.zip"',
        )

        self.send_header(
            "Content-Length",
            str(len(data)),
        )

        self.end_headers()

        try:
            self.wfile.write(data)
        except (
            BrokenPipeError,
            ConnectionResetError,
        ):
            pass


if __name__ == "__main__":
    ThreadingHTTPServer(
        ("127.0.0.1", 8090),
        Handler,
    ).serve_forever()
