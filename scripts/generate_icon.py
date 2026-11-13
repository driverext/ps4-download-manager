#!/usr/bin/env python3
"""Produce a dependency-free 512x512 PNG required by OpenOrbis packaging."""
import pathlib
import struct
import sys
import zlib

def chunk(kind, payload):
    return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", zlib.crc32(kind + payload) & 0xffffffff)

def png():
    width = height = 512
    rows = []
    for y in range(height):
        row = bytearray(b"\x00")
        for x in range(width):
            glow = int(max(0, 80 - abs(x - y) // 4))
            row.extend((8, 20 + glow // 2, 48 + glow, 255))
        rows.append(row)
    return (b"\x89PNG\r\n\x1a\n"
            + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(b"".join(rows), 9))
            + chunk(b"IEND", b""))

if __name__ == "__main__":
    dest = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else "sce_sys/icon0.png")
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_bytes(png())
    print("Generated", dest)
