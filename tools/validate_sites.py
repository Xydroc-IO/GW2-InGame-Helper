#!/usr/bin/env python3
"""Validate SiteDef registry integrity in src/Sites.cpp.

Checks:
  - Unique non-empty site ids
  - Required string fields present per entry
  - Categories stay contiguous (no interleaving)
  - Known Browse section ids referenced in UI.cpp exist in the registry

Exit 0 on success; non-zero with diagnostics on failure.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SITES_CPP = ROOT / "src" / "Sites.cpp"
UI_CPP = ROOT / "src" / "UI.cpp"

# SiteDef fields: id, category, label, title, homeUrl, searchPrefix, searchSuffix
ENTRY_RE = re.compile(
    r'\{\s*"([^"]*)"\s*,\s*"([^"]*)"\s*,\s*"([^"]*)"\s*,\s*"([^"]*)"\s*,\s*"([^"]*)"\s*,',
    re.MULTILINE,
)
STRCMP_ID_RE = re.compile(r'std::strcmp\(id,\s*"([^"]+)"\)')


def main() -> int:
    sites_text = SITES_CPP.read_text(encoding="utf-8")
    ui_text = UI_CPP.read_text(encoding="utf-8")

    errors: list[str] = []
    seen_ids: dict[str, int] = {}
    categories_order: list[str] = []
    last_cat: str | None = None
    entries: list[tuple[str, str, str, str, str]] = []

    for m in ENTRY_RE.finditer(sites_text):
        site_id, category, label, title, home = m.groups()
        entries.append((site_id, category, label, title, home))
        if not site_id:
            errors.append("empty site id")
        if site_id in seen_ids:
            errors.append(f"duplicate site id: {site_id!r}")
        else:
            seen_ids[site_id] = len(entries)
        for field_name, value in (
            ("category", category),
            ("label", label),
            ("title", title),
            ("homeUrl", home),
        ):
            if not value:
                errors.append(f"{site_id}: empty {field_name}")
        if last_cat is None or category != last_cat:
            if category in categories_order:
                errors.append(
                    f"category {category!r} reappears after other categories "
                    f"(sites must stay contiguous by category; id={site_id})"
                )
            else:
                categories_order.append(category)
            last_cat = category

    if not entries:
        errors.append("no SiteDef entries parsed from Sites.cpp")

    # Every id compared in BrowseSection must exist in the registry.
    for mid in STRCMP_ID_RE.findall(ui_text):
        if mid not in seen_ids:
            errors.append(f"UI.cpp BrowseSection references unknown id: {mid!r}")

    if errors:
        print(f"validate_sites: FAIL ({len(errors)} issue(s))", file=sys.stderr)
        for err in errors:
            print(f"  - {err}", file=sys.stderr)
        return 1

    print(
        f"validate_sites: OK — {len(entries)} sites, "
        f"{len(categories_order)} categories, unique ids"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
