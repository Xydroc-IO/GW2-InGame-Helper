#!/usr/bin/env python3
"""Validate Browse catalog integrity in data/sites.json (schema v2).

Checks:
  - Unique non-empty site ids
  - Required string fields present per entry
  - Categories stay contiguous (no interleaving)
  - browseSections present for known categories
  - browsePath entries are non-empty strings when present

Exit 0 on success; non-zero with diagnostics on failure.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SITES_JSON = ROOT / "data" / "sites.json"

ALLOWED_KEYS = {
	"id",
	"category",
	"label",
	"title",
	"homeUrl",
	"searchUrlPrefix",
	"searchUrlSuffix",
	"browsePath",
}


def main() -> int:
	json_path = Path(sys.argv[1]) if len(sys.argv) > 1 else SITES_JSON
	payload = json.loads(json_path.read_text(encoding="utf-8"))
	sites = payload.get("sites") or []
	errors: list[str] = []

	if payload.get("version") != 2:
		errors.append(f"unsupported version: {payload.get('version')!r} (want 2)")
	if payload.get("count") != len(sites):
		errors.append(f"count {payload.get('count')} != len(sites) {len(sites)}")

	sections = payload.get("browseSections")
	if not isinstance(sections, dict) or not sections:
		errors.append("browseSections must be a non-empty object")

	seen_ids: dict[str, int] = {}
	categories_order: list[str] = []
	last_cat: str | None = None
	entries: list[tuple[str, str]] = []

	for i, s in enumerate(sites):
		if not isinstance(s, dict):
			errors.append(f"sites[{i}] not an object")
			continue
		for key in s:
			if key not in ALLOWED_KEYS:
				errors.append(f"sites[{i}] unknown key {key!r}")
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
		bp = s.get("browsePath")
		if bp is not None:
			if not isinstance(bp, list) or not bp:
				errors.append(f"{site_id}: browsePath must be a non-empty array when set")
			else:
				for j, part in enumerate(bp):
					if not isinstance(part, str) or not part:
						errors.append(f"{site_id}: browsePath[{j}] empty")
		if last_cat is None or category != last_cat:
			if category in categories_order:
				errors.append(
					f"category {category!r} reappears after other categories "
					f"(sites must stay contiguous by category; id={site_id})"
				)
			else:
				categories_order.append(category)
			last_cat = category

	if isinstance(sections, dict):
		for cat in categories_order:
			if cat not in sections:
				# Categories without section chrome are allowed (flat list).
				continue
			sec_list = sections[cat]
			if not isinstance(sec_list, list) or not all(
				isinstance(x, str) and x for x in sec_list
			):
				errors.append(f"browseSections[{cat!r}] must be a string array")

	if not entries:
		errors.append("no site entries in JSON")

	if errors:
		print(f"{len(errors)} validation error(s) in {json_path}:", file=sys.stderr)
		for e in errors[:50]:
			print(f"  - {e}", file=sys.stderr)
		if len(errors) > 50:
			print(f"  … and {len(errors) - 50} more", file=sys.stderr)
		return 1

	print(
		f"ok: {len(entries)} sites, {len(categories_order)} categories "
		f"(schema v2, {json_path})"
	)
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
