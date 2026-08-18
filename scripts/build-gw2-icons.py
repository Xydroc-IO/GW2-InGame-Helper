#!/usr/bin/env python3
"""Build gw2-helper-icons.igh — unique ArenaNet render PNGs (IGH1, raw members).

Reads names from an existing `gw2-helper-catalog.igh`.
Caches downloads under dist/icon-cache/. Not part of the daily catalog Action
(too many CDN GETs). Upload beside the catalog on tag gw2-helper-catalog.

  python3 scripts/build-gw2-icons.py -o dist
  python3 scripts/build-gw2-icons.py -o dist --limit 20
"""
from __future__ import annotations

import argparse
import hashlib
import sys
import time
import urllib.error
import urllib.request
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from catalog_manifest import write_manifest
from ighpack import read_igh, write_igh, valid_name

RENDER = "https://render.guildwars2.com/file/"
UA = "GW2-InGame-Helper-catalog/1.0 (https://github.com/Xydroc-IO/GW2-InGame-Helper)"
EXTRA = [
    "C32BE61FC55C962524624F643897ECF1A9C80462/156634.png",
    "0A97E13F29B3597A447EEC04A09BE5BD699A2250/156643.png",
    "5CCB361F44CCC7256132405D31E3A24DACCF440A/156632.png",
    "49B10316B424F4E20139EB5E51ADCF24A8724E9B/156640.png",
    "F9EC00E23F630D6DB20CDA985592EC010E2A5705/156641.png",
    "77B793123251931AFF9FCA24C07E0F704BC4DA49/156630.png",
    "E43730AD49A903C3A1B4F27E41DE04EA51A775EC/156636.png",
    "AE56F8670807B87CF6EEE3FC7E6CB9710959E004/156638.png",
    "7C9309BE7A2A48C6A9FBCC70CC1EBEBFD7593C05/961390.png",
]


def collect_keys(names_text: str) -> list[str]:
    keys = set(EXTRA)
    for line in names_text.splitlines():
        if not line or line[0] == "#":
            continue
        parts = line.split("\t")
        if len(parts) < 4:
            continue
        fourth = parts[3].strip()
        if "/" in fourth and fourth.endswith(".png") and valid_name(fourth):
            keys.add(fourth)
    return sorted(keys)


def load_names(out: Path) -> str:
    igh = out / "gw2-helper-catalog.igh"
    if igh.is_file() and igh.read_bytes()[:4] == b"IGH1":
        return read_igh(igh)["names-en.tsv"].decode("utf-8")
    raise SystemExit("need gw2-helper-catalog.igh (IGH1) in out-dir")


def fetch_one(key: str, cache: Path) -> tuple[str, bytes | None]:
    safe = key.replace("/", "_")
    dest = cache / safe
    if dest.is_file() and dest.stat().st_size > 32:
        return key, dest.read_bytes()
    req = urllib.request.Request(
        RENDER + key,
        headers={"User-Agent": UA, "Accept": "image/png,*/*"},
    )
    for attempt in range(4):
        try:
            with urllib.request.urlopen(req, timeout=30) as resp:
                body = resp.read()
            if len(body) < 32 or body[:4] != b"\x89PNG":
                return key, None
            dest.write_bytes(body)
            return key, body
        except (urllib.error.HTTPError, urllib.error.URLError, TimeoutError, OSError):
            time.sleep(0.4 * (attempt + 1))
    return key, None


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("-o", "--out-dir", default=".", type=Path)
    ap.add_argument("--limit", type=int, default=0)
    ap.add_argument("--jobs", type=int, default=8)
    args = ap.parse_args()
    out: Path = args.out_dir
    out.mkdir(parents=True, exist_ok=True)
    cache = out / "icon-cache"
    cache.mkdir(exist_ok=True)

    keys = collect_keys(load_names(out))
    if args.limit > 0:
        keys = keys[: args.limit]
    print(f"unique render icons {len(keys)}", file=sys.stderr)

    files: dict[str, bytes] = {}
    miss = 0
    done = 0
    with ThreadPoolExecutor(max_workers=max(1, args.jobs)) as pool:
        futs = [pool.submit(fetch_one, k, cache) for k in keys]
        for fut in as_completed(futs):
            key, body = fut.result()
            done += 1
            if body:
                files[key] = body
            else:
                miss += 1
            if done % 500 == 0 or done == len(keys):
                print(f"  {done}/{len(keys)} ok={len(files)} miss={miss}", file=sys.stderr)

    if len(files) < 8:
        print("too few icons fetched", file=sys.stderr)
        return 1
    digest = hashlib.sha256("\n".join(sorted(files)).encode("utf-8")).hexdigest()
    write_igh(out / "gw2-helper-icons.igh", files, compress=False)
    man = write_manifest(out / "gw2-helper-catalog.manifest", icons=digest)
    print(f"icons {len(files)} miss={miss} ver={digest[:12]} json={man.get('icons', '')[:12]}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
