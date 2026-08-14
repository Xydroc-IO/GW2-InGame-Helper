#!/usr/bin/env python3
"""Pack data/cheatsheets/ into build/embed/cheatsheets.zip for DLL embedding."""

from __future__ import annotations

import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "data" / "cheatsheets"
OUT = ROOT / "build" / "embed" / "cheatsheets.zip"


def main() -> int:
	if not (SRC / "manifest.json").is_file():
		print(f"missing {SRC / 'manifest.json'} — run tools/export_cheatsheets.py", file=sys.stderr)
		return 1
	OUT.parent.mkdir(parents=True, exist_ok=True)
	files = sorted(p for p in SRC.iterdir() if p.is_file())
	with zipfile.ZipFile(OUT, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as zf:
		for p in files:
			zf.write(p, arcname=p.name)
	print(f"packed {len(files)} files → {OUT} ({OUT.stat().st_size} bytes)")
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
