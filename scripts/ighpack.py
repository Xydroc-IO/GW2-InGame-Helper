#!/usr/bin/env python3
"""IGH1 pack format — custom catalog/icon container (not zip).

Little-endian:
  4s   magic  b'IGH1'
  u32  version (1)
  u32  flags (0)
  u32  entry count
  u32  index bytes
  index:
    u16 name_len, name utf-8, u8 packed (1=gzip), u32 uncomp, u32 stored, u64 offset
  payloads at offset (from start of file)
"""
from __future__ import annotations

import gzip
import struct
from pathlib import Path

MAGIC = b"IGH1"
VERSION = 1
HEADER_SIZE = 20


def valid_name(name: str) -> bool:
    if not name or len(name) > 200 or ".." in name or "\\" in name or name[0] == "/":
        return False
    for c in name:
        ok = (
            ("a" <= c <= "z")
            or ("A" <= c <= "Z")
            or ("0" <= c <= "9")
            or c in "/._-"
        )
        if not ok:
            return False
    return True


def write_igh(path: Path, files: dict[str, bytes], *, compress: bool) -> None:
    items: list[tuple[bytes, bytes, bytes, int]] = []
    for name in sorted(files):
        if not valid_name(name):
            raise ValueError(f"bad IGH name {name!r}")
        raw = files[name]
        stored = gzip.compress(raw, compresslevel=9) if compress else raw
        items.append((name.encode("utf-8"), raw, stored, 1 if compress else 0))

    index = bytearray()
    idx_size = 0
    for name_b, _raw, _stored, _pk in items:
        idx_size += 2 + len(name_b) + 1 + 4 + 4 + 8
    cur = HEADER_SIZE + idx_size
    blobs = bytearray()
    for name_b, raw, stored, packed in items:
        index += struct.pack("<H", len(name_b))
        index += name_b
        index += struct.pack("<B", packed)
        index += struct.pack("<I", len(raw))
        index += struct.pack("<I", len(stored))
        index += struct.pack("<Q", cur)
        blobs += stored
        cur += len(stored)

    header = MAGIC + struct.pack("<IIII", VERSION, 0, len(items), len(index))
    path = Path(path)
    path.write_bytes(header + index + blobs)
    print(f"wrote {path} ({path.stat().st_size} bytes, {len(items)} entries)")


def read_igh(path: Path) -> dict[str, bytes]:
    data = Path(path).read_bytes()
    if len(data) < HEADER_SIZE or data[:4] != MAGIC:
        raise ValueError("not IGH1")
    version, _flags, count, index_bytes = struct.unpack_from("<IIII", data, 4)
    if version != VERSION or count > 100000:
        raise ValueError("bad IGH1 header")
    p = HEADER_SIZE
    end = HEADER_SIZE + index_bytes
    out: dict[str, bytes] = {}
    for _ in range(count):
        if p + 2 > end:
            raise ValueError("truncated index")
        (nlen,) = struct.unpack_from("<H", data, p)
        p += 2
        name = data[p : p + nlen].decode("utf-8")
        p += nlen
        packed = data[p]
        p += 1
        uncomp, stored, off = struct.unpack_from("<IIQ", data, p)
        p += 16
        blob = data[off : off + stored]
        if packed:
            blob = gzip.decompress(blob)
        if len(blob) != uncomp:
            raise ValueError(f"size mismatch {name}")
        out[name] = blob
    return out
