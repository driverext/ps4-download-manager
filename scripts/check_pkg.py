#!/usr/bin/env python3
"""Check the PS4 PKG header before installing via GoldHEN (not full validation)."""
import argparse
import hashlib
from pathlib import Path
import re
import struct
import sys

MAGIC = b"\x7fCNT"
CONTENT_ID = re.compile(r"^[A-Z0-9]{6}-[A-Z0-9]{9}_00-[A-Z0-9]{16}$")

def inspect(path: Path, expected_id=None):
    size = path.stat().st_size
    errors = []
    with path.open("rb") as f:
        h = f.read(0x440)
    if len(h) < 0x440:
        return [f"Truncated package header: {len(h)} bytes, need at least 0x440"]
    if h[:4] != MAGIC:
        return [f"Not a PS4 PKG: magic {h[:4].hex()}, expected 7f434e54"]
    raw = h[0x40:0x64]
    try:
        cid = raw.rstrip(b"\0").decode("ascii")
    except UnicodeDecodeError:
        cid = ""
    if not CONTENT_ID.fullmatch(cid):
        errors.append(f"Invalid Content ID at header offset 0x40: {raw!r}")
    if expected_id and cid != expected_id:
        errors.append(f"Content ID mismatch: got {cid!r}, expected {expected_id!r}")
    entries, = struct.unpack_from(">I", h, 0x10)
    offset, = struct.unpack_from(">I", h, 0x18)
    if not 0 < entries < 100000:
        errors.append(f"Suspicious entry count: {entries}")
    if not 0 < offset < size:
        errors.append(f"Entry table offset 0x{offset:x} outside file")
    declared, = struct.unpack_from(">Q", h, 0x430)
    if declared and declared != size:
        errors.append(f"Truncated/mismatched package size: header {declared}, file {size}")
    if not errors:
        print(f"Header checks passed: Content ID {cid}; size {size} bytes")
        print("NOT proof of installability: also validate param.sfo, eboot.bin and GP4.")
    return errors

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pkg", type=Path)
    parser.add_argument("--expected-id")
    parser.add_argument("--sha256", action="store_true")
    args = parser.parse_args()
    if not args.pkg.is_file():
        parser.error("PKG does not exist")
    try:
        errors = inspect(args.pkg, args.expected_id)
        if args.sha256:
            digest = hashlib.sha256()
            with args.pkg.open("rb") as f:
                for chunk in iter(lambda: f.read(1024 * 1024), b""):
                    digest.update(chunk)
            print("SHA-256:", digest.hexdigest())
    except OSError as exc:
        print(f"File error: {exc}", file=sys.stderr)
        return 2
    for error in errors:
        print("FAIL:", error, file=sys.stderr)
    return 1 if errors else 0

if __name__ == "__main__":
    sys.exit(main())
