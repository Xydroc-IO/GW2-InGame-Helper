#!/usr/bin/env python3
"""Validate SiteDef catalog integrity in data/sites.json.

Checks:
  - Unique non-empty site ids
  - Required string fields present per entry
  - Categories stay contiguous (no interleaving)
  - Known Browse section ids referenced in UI.cpp exist in the registry

Exit 0 on success; non-zero with diagnostics on failure.
"""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SITES_JSON = ROOT / "data" / "sites.json"
UI_CPP = ROOT / "src" / "UI.cpp"
# Also accept Browse helpers after UI split
UI_BROWSE = ROOT / "src" / "UI_Browse.cpp"

STRCMP_ID_RE = re.compile(r'std::strcmp\(id,\s*"([^"]+)"\)')


def main() -> int:
	json_path = Path(sys.argv[1]) if len(sys.argv) > 1 else SITES_JSON
	payload = json.loads(json_path.read_text(encoding="utf-8"))
	sites = payload.get("sites") or []
	errors: list[str] = []

	if payload.get("version") != 1:
		errors.append(f"unsupported version: {payload.get('version')!r}")
	if payload.get("count") != len(sites):
		errors.append(f"count {payload.get('count')} != len(sites) {len(sites)}")

	seen_ids: dict[str, int] = {}
	categories_order: list[str] = []
	last_cat: str | None = None
	entries: list[tuple[str, str]] = []

	for i, s in enumerate(sites):
		if not isinstance(s, dict):
			errors.append(f"sites[{i}] not an object")
			continue
		site_id = s.get("id") or ""
		category = s.get("category") or ""
		entries.append((site_id, category))
		if not site_id:
			errors.append("empty site id")
		if site_id in seen_ids:
			errors.append(f"duplicate site id: {site_id!r}")
		else:
			seen_ids[site_id] = len(entries)
		for field_name in ("category", "label", "title", "homeUrl"):
			if not s.get(field_name):
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
		errors.append("no SiteDef entries in JSON")

	ui_texts: list[str] = []
	for p in (UI_CPP, UI_BROWSE):
		if p.is_file():
			ui_texts.append(p.read_text(encoding="utf-8"))
	ui_blob = "\n".join(ui_texts)
	for mid in STRCMP_ID_RE.findall(ui_blob):
		if mid not in seen_ids:
			errors.append(f"UI BrowseSection references unknown id: {mid!r}")

	for site_id, _category in entries:
		if site_id.startswith("wiki_relic_") and site_id.startswith("wiki_lrelic_"):
			errors.append(f"ambiguous relic id prefix: {site_id!r}")

	if errors:
		print(f"{len(errors)} validation error(s) in {json_path}:", file=sys.stderr)
		for e in errors[:50]:
			print(f"  - {e}", file=sys.stderr)
		if len(errors) > 50:
			print(f"  … and {len(errors) - 50} more", file=sys.stderr)
		return 1

	print(
		f"ok: {len(entries)} sites, {len(categories_order)} categories "
		f"({json_path})"
	)
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
