#!/usr/bin/env python3
"""Golden checks for Tekkit .trl binary layout (mirrors TekkitParse::ParseTrl)."""

from __future__ import annotations

import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "build" / "fixtures" / "sample.trl"


def build_trl(map_id: int, points: list[tuple[float, float, float]]) -> bytes:
	buf = bytearray()
	buf += struct.pack("<II", 1, map_id)  # version, mapId
	for x, y, z in points:
		buf += struct.pack("<fff", x, y, z)
	return bytes(buf)


def parse_trl(data: bytes) -> tuple[int, list[tuple[float, float, float]]] | None:
	if len(data) < 20:
		return None
	_ver, mid = struct.unpack_from("<II", data, 0)
	if mid == 0 or mid > 100000:
		return None
	rem = len(data) - 8
	rem -= rem % 12
	count = rem // 12
	if count < 2:
		return None
	pts = []
	for i in range(count):
		x, y, z = struct.unpack_from("<fff", data, 8 + i * 12)
		pts.append((x, y, z))
	return mid, pts


def main() -> int:
	pts = [(1.0, 2.0, 3.0), (4.0, 5.0, 6.0), (7.0, 8.0, 9.0)]
	raw = build_trl(50, pts)
	OUT.parent.mkdir(parents=True, exist_ok=True)
	OUT.write_bytes(raw)

	parsed = parse_trl(raw)
	if not parsed:
		print("FAIL: parse_trl returned None", file=sys.stderr)
		return 1
	mid, got = parsed
	if mid != 50:
		print(f"FAIL: mapId {mid}", file=sys.stderr)
		return 1
	if len(got) != 3 or abs(got[0][0] - 1.0) > 1e-5 or abs(got[2][2] - 9.0) > 1e-5:
		print(f"FAIL: points {got}", file=sys.stderr)
		return 1

	# Reject garbage map id
	bad = build_trl(0, pts)
	if parse_trl(bad) is not None:
		print("FAIL: expected reject mapId 0", file=sys.stderr)
		return 1

	print("test_trl_parse: OK")
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
