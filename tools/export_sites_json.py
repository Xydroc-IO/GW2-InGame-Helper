#!/usr/bin/env python3
"""Sync data/sites.json → Companion-App assets."""

from __future__ import annotations

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "data" / "sites.json"
OUT = ROOT / "Companion-App" / "core" / "data" / "src" / "main" / "assets" / "sites.json"


def main() -> int:
	if not SRC.is_file():
		print(f"error: missing {SRC}", file=sys.stderr)
		return 1
	OUT.parent.mkdir(parents=True, exist_ok=True)
	payload = json.loads(SRC.read_text(encoding="utf-8"))
	payload["source"] = "data/sites.json"
	OUT.write_text(json.dumps(payload, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
	print(f"synced {payload.get('count')} sites → {OUT.relative_to(ROOT)}")
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
