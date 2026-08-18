#!/usr/bin/env python3
"""Build the GW2 Helper Catalog pack from api.guildwars2.com (no API key).

Writes (GitHub pre-release tag `gw2-helper-catalog`, title GW2 Helper Catalog;
that tag also hosts cef-runtime-150-windows64.zip — do not delete that zip):
  gw2-helper-catalog.manifest  catalog / icons / cef stamps (cheap freshness check)
  gw2-helper-catalog.igh   IGH1: catalog.ver, names-en.tsv, recipes.tsv (gzip members)
  gw2-helper-icons.igh     IGH1: unique render PNGs (see build-gw2-icons.py)

Row kinds: i item, c currency, s skin, n mini, m materials, d dye, f finisher,
o outfit, g glider, u mail carrier, v novelty, t title, a achievement,
y legendary armory (4th column = max_count).

Icon path is the suffix after https://render.guildwars2.com/file/
(signature/file_id.png). Empty if ArenaNet has no render icon.

Upload (not a shipping DLL tag):

  gh release create gw2-helper-catalog --prerelease --title "GW2 Helper Catalog" \\
    --notes-file docs/CATALOG_RELEASE.md \\
    gw2-helper-catalog.manifest gw2-helper-catalog.igh

  gh release upload gw2-helper-catalog gw2-helper-catalog.manifest \\
    gw2-helper-catalog.igh --clobber
"""
from __future__ import annotations

import argparse
import json
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from catalog_manifest import write_manifest
from ighpack import write_igh as pack_igh_file

API = "https://api.guildwars2.com"
UA = "GW2-InGame-Helper-catalog/1.0 (https://github.com/Xydroc-IO/GW2-InGame-Helper)"
BATCH = 200
SLEEP_SEC = 0.08
RENDER_PREFIX = "https://render.guildwars2.com/file/"


def get(path: str) -> object:
    req = urllib.request.Request(
        API + path,
        headers={"User-Agent": UA, "Accept": "application/json", "Accept-Language": "en"},
    )
    for attempt in range(5):
        try:
            with urllib.request.urlopen(req, timeout=60) as resp:
                return json.loads(resp.read().decode("utf-8"))
        except urllib.error.HTTPError as e:
            if e.code == 429 or e.code >= 500:
                time.sleep(1.5 * (attempt + 1))
                continue
            raise
        except OSError:
            time.sleep(1.0 * (attempt + 1))
    raise RuntimeError(f"GET {path} failed after retries")


def clean_name(s: str) -> str:
    return s.replace("\t", " ").replace("\r", " ").replace("\n", " ").strip()


def pack_icon(url: object) -> str:
    if not isinstance(url, str) or not url.startswith(RENDER_PREFIX):
        return ""
    rest = url[len(RENDER_PREFIX) :]
    if not rest or "\t" in rest or "\n" in rest or "\r" in rest:
        return ""
    return rest


def fetch_paged(ids_path: str, detail_prefix: str) -> list[tuple[int, str, str]]:
    ids = get(ids_path)
    if not isinstance(ids, list):
        raise RuntimeError(f"{ids_path} is not a list")
    int_ids = [int(x) for x in ids]
    rows: list[tuple[int, str, str]] = []
    for i in range(0, len(int_ids), BATCH):
        chunk = int_ids[i : i + BATCH]
        q = ",".join(str(x) for x in chunk)
        data = get(f"{detail_prefix}?ids={q}&lang=en")
        if isinstance(data, dict):
            data = [data]
        if not isinstance(data, list):
            continue
        for obj in data:
            if not isinstance(obj, dict):
                continue
            try:
                iid = int(obj.get("id") or 0)
            except (TypeError, ValueError):
                continue
            name = clean_name(str(obj.get("name") or ""))
            icon = pack_icon(obj.get("icon"))
            if iid > 0 and name:
                rows.append((iid, name, icon))
        time.sleep(SLEEP_SEC)
        done = min(i + BATCH, len(int_ids))
        print(f"  {detail_prefix} {done}/{len(int_ids)}", file=sys.stderr)
    return rows


def fetch_recipes() -> list[tuple[int, int, int, int, str, str]]:
    ids = get("/v2/recipes")
    if not isinstance(ids, list):
        raise RuntimeError("/v2/recipes is not a list")
    int_ids = [int(x) for x in ids]
    rows: list[tuple[int, int, int, int, str, str]] = []
    for i in range(0, len(int_ids), BATCH):
        chunk = int_ids[i : i + BATCH]
        q = ",".join(str(x) for x in chunk)
        data = get(f"/v2/recipes?ids={q}")
        if isinstance(data, dict):
            data = [data]
        if not isinstance(data, list):
            continue
        for obj in data:
            if not isinstance(obj, dict):
                continue
            try:
                rid = int(obj.get("id") or 0)
                out_id = int(obj.get("output_item_id") or 0)
                out_cnt = int(obj.get("output_item_count") or 0)
                rating = int(obj.get("min_rating") or 0)
            except (TypeError, ValueError):
                continue
            if out_cnt <= 0:
                out_cnt = 1
            discs_raw = obj.get("disciplines") or []
            discs: list[str] = []
            if isinstance(discs_raw, list):
                for d in discs_raw:
                    s = clean_name(str(d)).replace("|", " ")
                    if s:
                        discs.append(s)
            ings_raw = obj.get("ingredients") or []
            parts: list[str] = []
            if isinstance(ings_raw, list):
                for ing in ings_raw:
                    if not isinstance(ing, dict):
                        continue
                    try:
                        iid = int(ing.get("item_id") or 0)
                        cnt = int(ing.get("count") or 0)
                    except (TypeError, ValueError):
                        continue
                    if iid > 0 and cnt > 0:
                        parts.append(f"{iid}:{cnt}")
            if rid > 0 and out_id > 0:
                rows.append((rid, out_id, out_cnt, rating, "|".join(discs), ",".join(parts)))
        time.sleep(SLEEP_SEC)
        done = min(i + BATCH, len(int_ids))
        print(f"  /v2/recipes {done}/{len(int_ids)}", file=sys.stderr)
    return rows


def fetch_armory(item_names: dict[int, str]) -> list[tuple[int, str, str]]:
    data = get("/v2/legendaryarmory?ids=all")
    if not isinstance(data, list):
        return []
    rows: list[tuple[int, str, str]] = []
    for obj in data:
        if not isinstance(obj, dict):
            continue
        try:
            iid = int(obj.get("id") or 0)
            mx = int(obj.get("max_count") or 1)
        except (TypeError, ValueError):
            continue
        if iid <= 0:
            continue
        if mx <= 0:
            mx = 1
        name = clean_name(item_names.get(iid) or f"Item {iid}")
        rows.append((iid, name, str(mx)))
    print(f"  /v2/legendaryarmory {len(rows)}", file=sys.stderr)
    return rows


def write_igh(path: Path, build_id: str, names: str, recipes: str) -> None:
    pack_igh_file(
        path,
        {
            "catalog.ver": (build_id + "\n").encode("utf-8"),
            "names-en.tsv": names.encode("utf-8"),
            "recipes.tsv": recipes.encode("utf-8"),
        },
        compress=True,
    )


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("-o", "--out-dir", default=".", type=Path)
    ap.add_argument(
        "--inherit-manifest",
        type=Path,
        default=None,
        help="Keep icons/cef fields from an existing gw2-helper-catalog.manifest",
    )
    args = ap.parse_args()
    out: Path = args.out_dir
    out.mkdir(parents=True, exist_ok=True)

    build = get("/v2/build")
    build_id = str(build.get("id") if isinstance(build, dict) else build)
    print(f"build {build_id}", file=sys.stderr)

    NAME_PACKS = [
        ("c", "/v2/currencies"),
        ("i", "/v2/items"),
        ("s", "/v2/skins"),
        ("n", "/v2/minis"),
        ("m", "/v2/materials"),
        ("d", "/v2/colors"),
        ("f", "/v2/finishers"),
        ("o", "/v2/outfits"),
        ("g", "/v2/gliders"),
        ("u", "/v2/mailcarriers"),
        ("v", "/v2/novelties"),
        ("t", "/v2/titles"),
        ("a", "/v2/achievements"),
    ]

    by_kind: dict[str, list[tuple[int, str, str]]] = {}
    for kind, path in NAME_PACKS:
        print(f"{path}…", file=sys.stderr)
        by_kind[kind] = fetch_paged(path, path)

    item_names = {iid: name for iid, name, _icon in by_kind.get("i", [])}
    print("legendaryarmory…", file=sys.stderr)
    by_kind["y"] = fetch_armory(item_names)

    print("recipes…", file=sys.stderr)
    recipes = fetch_recipes()

    name_lines = ["# gw2-helper-catalog 1\n", f"# build {build_id}\n"]
    for kind, _path in NAME_PACKS + [("y", "/v2/legendaryarmory")]:
        for iid, name, extra in sorted(by_kind.get(kind, [])):
            name_lines.append(f"{kind}\t{iid}\t{name}\t{extra}\n")
    rec_lines = ["# gw2-recipes 1\n", f"# build {build_id}\n"]
    for row in sorted(recipes):
        rec_lines.append(f"r\t{row[0]}\t{row[1]}\t{row[2]}\t{row[3]}\t{row[4]}\t{row[5]}\n")

    names_text = "".join(name_lines)
    rec_text = "".join(rec_lines)
    write_igh(out / "gw2-helper-catalog.igh", build_id, names_text, rec_text)
    man = write_manifest(
        out / "gw2-helper-catalog.manifest",
        inherit=args.inherit_manifest,
        catalog=build_id,
    )
    print(f"wrote {out / 'gw2-helper-catalog.manifest'} catalog={man.get('catalog', '')}")
    bits = " ".join(f"{k}={len(by_kind.get(k, []))}" for k, _ in NAME_PACKS + [("y", "")])
    print(f"names {bits}; recipes={len(recipes)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
