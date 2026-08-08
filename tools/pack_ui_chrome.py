#!/usr/bin/env python3
"""Pack data/ui-chrome/ into build/ui_chrome.zip for DLL embedding."""

from __future__ import annotations

import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "data" / "ui-chrome"
OUT = ROOT / "build" / "ui_chrome.zip"

REQUIRED = (
	"manifest.txt",
	"155985.png",
	"155981.png",
	"156022.png",
	"156008.png",
	"156009.png",
	"156010.png",
	"button-exit.png",
	"button-exit-active.png",
	"crest-hero.png",
	"panel-wash.png",
	"title-bar.png",
	"panel-edge.png",
	"ink-edge.png",
	"card-fill.png",
	"card-fill-dark.png",
	"card-border.png",
	"hero-plate.png",
	"btn-frame.png",
	"btn-frame-hover.png",
	"btn-plate.png",
	"divider-gold.png",
	"header-ornament.png",
	"plaque-corner.png",
)


def main() -> int:
	for name in REQUIRED:
		if not (SRC / name).is_file():
			print(f"missing {SRC / name}", file=sys.stderr)
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
