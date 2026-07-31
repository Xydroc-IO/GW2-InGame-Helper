#!/usr/bin/env python3
"""Export SiteDef registry from src/Sites.cpp to Companion-App sites.json."""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SITES_CPP = ROOT / "src" / "Sites.cpp"
OUT = ROOT / "Companion-App" / "core" / "data" / "src" / "main" / "assets" / "sites.json"

# Full SiteDef: id, category, label, title, homeUrl, searchPrefix|nullptr, searchSuffix|nullptr
ENTRY_RE = re.compile(
    r'\{\s*"([^"]*)"\s*,\s*"([^"]*)"\s*,\s*"([^"]*)"\s*,\s*"([^"]*)"\s*,\s*"([^"]*)"\s*,\s*'
    r'(nullptr|"([^"]*)")\s*,\s*(nullptr|"([^"]*)")\s*,?\s*\}',
    re.MULTILINE,
)


def main() -> int:
    text = SITES_CPP.read_text(encoding="utf-8")
    sites: list[dict[str, str | None]] = []
    for m in ENTRY_RE.finditer(text):
        site_id, category, label, title, home = m.group(1, 2, 3, 4, 5)
        search_prefix = m.group(7)  # None when nullptr
        search_suffix = m.group(9)
        sites.append(
            {
                "id": site_id,
                "category": category,
                "label": label,
                "title": title,
                "homeUrl": home,
                "searchUrlPrefix": search_prefix,
                "searchUrlSuffix": search_suffix,
            }
        )

    if not sites:
        print("error: no sites parsed", file=sys.stderr)
        return 1

    OUT.parent.mkdir(parents=True, exist_ok=True)
    payload = {
        "version": 1,
        "source": "src/Sites.cpp",
        "count": len(sites),
        "sites": sites,
    }
    OUT.write_text(json.dumps(payload, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"wrote {len(sites)} sites → {OUT.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
