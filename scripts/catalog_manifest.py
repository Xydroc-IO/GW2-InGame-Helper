#!/usr/bin/env python3
"""Merge/write gw2-helper-catalog.manifest (one cheap freshness file for the pre-release).

JSON object, `.manifest` name. Keys (all strings):
  catalog  ArenaNet /v2/build id
  icons    SHA-256 of packed icon keys
  cef      CefRuntime::kStamp (e.g. 150.0.14)
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

KEYS = ("catalog", "icons", "cef")
FILENAME = "gw2-helper-catalog.manifest"


def read_manifest(path: Path | None) -> dict[str, str]:
    if path is None or not path.is_file():
        return {}
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError, UnicodeError):
        return {}
    if not isinstance(data, dict):
        return {}
    out: dict[str, str] = {}
    for key in KEYS:
        raw = data.get(key)
        if raw is None:
            continue
        text = str(raw).strip()
        if text:
            out[key] = text
    return out


def write_manifest(
    path: Path,
    *,
    inherit: Path | None = None,
    catalog: str | None = None,
    icons: str | None = None,
    cef: str | None = None,
) -> dict[str, str]:
    data = read_manifest(inherit)
    data.update(read_manifest(path))
    for key, value in (("catalog", catalog), ("icons", icons), ("cef", cef)):
        if value:
            text = str(value).strip()
            if text:
                data[key] = text
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return data


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("-o", "--out", required=True, type=Path)
    ap.add_argument("--inherit", type=Path, default=None)
    ap.add_argument("--catalog")
    ap.add_argument("--icons")
    ap.add_argument("--cef")
    args = ap.parse_args()
    data = write_manifest(
        args.out,
        inherit=args.inherit,
        catalog=args.catalog,
        icons=args.icons,
        cef=args.cef,
    )
    bits = " ".join(f"{k}={data.get(k, '')}" for k in KEYS if data.get(k))
    print(f"wrote {args.out} {bits}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
