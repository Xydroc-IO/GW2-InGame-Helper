#!/usr/bin/env python3
"""Build the GW2 Helper Catalog pack from api.guildwars2.com (no API key).

Writes (GitHub pre-release tag `gw2-helper-catalog`, title GW2 Helper Catalog;
that tag also hosts cef-runtime-150-windows64.zip — do not delete that zip):
  gw2-helper-catalog.manifest  catalog / icons / cef stamps (cheap freshness check)
  gw2-helper-catalog.igh   IGH1: catalog.ver, names-en.tsv, recipes.tsv (raw members, ~8MB)
  gw2-helper-achievements.igh  IGH1: ach.ver, groups.tsv, categories.tsv, defs.tsv
  gw2-helper-icons.igh     IGH1: unique render PNGs (see build-gw2-icons.py)

Row kinds: i item, c currency, s skin, n mini, m materials, d dye, f finisher,
o outfit, g glider, u mail carrier, v novelty, t title, a achievement,
y legendary armory (4th column = max_count).

Achievement defs TSV: d, id, points, flags, name, requirement, description,
  locked_text, bits, tiers. flags h=Hidden r=Repeatable; bits kind,id,text
  joined by | (t/i/s/n/a/o); tiers count:points joined by comma.

Icon path is the suffix after https://render.guildwars2.com/file/
(signature/file_id.png). Empty if ArenaNet has no render icon.
Dye rows (`d`) store cloth RGB as `r,g,b` in that column instead.

Upload (not a shipping DLL tag):

  gh release create gw2-helper-catalog --prerelease --title "GW2 Helper Catalog" \\
    --notes-file docs/CATALOG_RELEASE.md \\
    gw2-helper-catalog.manifest gw2-helper-catalog.igh gw2-helper-achievements.igh

  gh release upload gw2-helper-catalog gw2-helper-catalog.manifest \\
    gw2-helper-catalog.igh gw2-helper-achievements.igh --clobber
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


def clamp_byte(n: int) -> int:
    return max(0, min(255, int(n)))


def dye_rgb_extra(obj: dict) -> str:
    """Cloth swatch first; leather/metal; last resort base_rgb (often a shared red)."""
    for key in ("cloth", "leather", "metal"):
        mat = obj.get(key)
        if not isinstance(mat, dict):
            continue
        rgb = mat.get("rgb")
        if isinstance(rgb, list) and len(rgb) >= 3:
            try:
                r, g, b = clamp_byte(rgb[0]), clamp_byte(rgb[1]), clamp_byte(rgb[2])
                return f"{r},{g},{b}"
            except (TypeError, ValueError):
                continue
    base = obj.get("base_rgb")
    if isinstance(base, list) and len(base) >= 3:
        try:
            r, g, b = clamp_byte(base[0]), clamp_byte(base[1]), clamp_byte(base[2])
            return f"{r},{g},{b}"
        except (TypeError, ValueError):
            pass
    return ""


def fetch_paged(ids_path: str, detail_prefix: str, extra_fn=None) -> list[tuple[int, str, str]]:
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
            extra = extra_fn(obj) if extra_fn else pack_icon(obj.get("icon"))
            if iid > 0 and name:
                rows.append((iid, name, extra))
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


def pack_cell(s: object) -> str:
    return clean_name(str(s or "")).replace("|", " ")


def pack_bit_text(s: object) -> str:
    return pack_cell(s).replace(",", " ")


def int_list(obj: dict, key: str) -> list[int]:
    raw = obj.get(key) or []
    out: list[int] = []
    if not isinstance(raw, list):
        return out
    for x in raw:
        try:
            n = int(x)
        except (TypeError, ValueError):
            continue
        if n > 0:
            out.append(n)
    return out


def ach_flags(obj: dict) -> str:
    raw = obj.get("flags") or []
    bits = ""
    if isinstance(raw, list):
        if "Hidden" in raw:
            bits += "h"
        if "Repeatable" in raw:
            bits += "r"
    return bits


def ach_bits(obj: dict) -> str:
    raw = obj.get("bits") or []
    if not isinstance(raw, list):
        return ""
    kind_map = {
        "Item": "i",
        "Skin": "s",
        "Minipet": "n",
        "Achievement": "a",
        "Text": "t",
    }
    parts: list[str] = []
    for bit in raw:
        if not isinstance(bit, dict):
            continue
        ty = str(bit.get("type") or "")
        kind = kind_map.get(ty, "o" if ty else "t")
        try:
            tid = int(bit.get("id") or 0)
        except (TypeError, ValueError):
            tid = 0
        if tid < 0:
            tid = 0
        parts.append(f"{kind},{tid},{pack_bit_text(bit.get('text'))}")
    return "|".join(parts)


def ach_tiers(obj: dict) -> tuple[int, str]:
    raw = obj.get("tiers") or []
    parts: list[str] = []
    total = 0
    if isinstance(raw, list):
        for tier in raw:
            if not isinstance(tier, dict):
                continue
            try:
                cnt = int(tier.get("count") or 0)
                pts = int(tier.get("points") or 0)
            except (TypeError, ValueError):
                continue
            if cnt <= 0:
                continue
            if pts < 0:
                pts = 0
            total += pts
            parts.append(f"{cnt}:{pts}")
    try:
        cap = int(obj.get("point_cap") or 0)
    except (TypeError, ValueError):
        cap = 0
    if cap > 0:
        total = cap
    return total, ",".join(parts)


def fetch_achievements() -> tuple[list[tuple[int, str, str]], list[tuple]]:
    """Name rows for names-en.tsv plus full defs for the achievements pack."""
    ids = get("/v2/achievements")
    if not isinstance(ids, list):
        raise RuntimeError("/v2/achievements is not a list")
    int_ids = [int(x) for x in ids]
    names: list[tuple[int, str, str]] = []
    defs: list[tuple] = []
    for i in range(0, len(int_ids), BATCH):
        chunk = int_ids[i : i + BATCH]
        q = ",".join(str(x) for x in chunk)
        data = get(f"/v2/achievements?ids={q}&lang=en")
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
            if iid <= 0:
                continue
            name = pack_cell(obj.get("name"))
            icon = pack_icon(obj.get("icon"))
            if name:
                names.append((iid, name, icon))
            points, tiers = ach_tiers(obj)
            defs.append(
                (
                    iid,
                    points,
                    ach_flags(obj),
                    name,
                    pack_cell(obj.get("requirement")),
                    pack_cell(obj.get("description")),
                    pack_cell(obj.get("locked_text")),
                    ach_bits(obj),
                    tiers,
                )
            )
        time.sleep(SLEEP_SEC)
        done = min(i + BATCH, len(int_ids))
        print(f"  /v2/achievements {done}/{len(int_ids)}", file=sys.stderr)
    return names, defs


def fetch_guid_paged(
    ids_path: str, detail_prefix: str
) -> list[dict]:
    ids = get(ids_path)
    if not isinstance(ids, list):
        raise RuntimeError(f"{ids_path} is not a list")
    guids = [str(x).strip() for x in ids if str(x).strip()]
    rows: list[dict] = []
    guid_batch = 40
    for i in range(0, len(guids), guid_batch):
        chunk = guids[i : i + guid_batch]
        q = ",".join(chunk)
        data = get(f"{detail_prefix}?ids={q}&lang=en")
        if isinstance(data, dict):
            data = [data]
        if not isinstance(data, list):
            continue
        for obj in data:
            if isinstance(obj, dict):
                rows.append(obj)
        time.sleep(SLEEP_SEC)
        done = min(i + guid_batch, len(guids))
        print(f"  {detail_prefix} {done}/{len(guids)}", file=sys.stderr)
    return rows


def fetch_ach_groups() -> list[tuple[str, int, str, list[int]]]:
    rows: list[tuple[str, int, str, list[int]]] = []
    for obj in fetch_guid_paged("/v2/achievements/groups", "/v2/achievements/groups"):
        gid = pack_cell(obj.get("id")).replace(" ", "")
        name = pack_cell(obj.get("name"))
        if not gid or not name:
            continue
        try:
            order = int(obj.get("order") or 0)
        except (TypeError, ValueError):
            order = 0
        if order < 0:
            order = 0
        rows.append((gid, order, name, int_list(obj, "categories")))
    return rows


def fetch_ach_categories() -> list[tuple[int, int, str, list[int]]]:
    rows: list[tuple[int, int, str, list[int]]] = []
    ids = get("/v2/achievements/categories")
    if not isinstance(ids, list):
        raise RuntimeError("/v2/achievements/categories is not a list")
    int_ids = [int(x) for x in ids]
    for i in range(0, len(int_ids), BATCH):
        chunk = int_ids[i : i + BATCH]
        q = ",".join(str(x) for x in chunk)
        data = get(f"/v2/achievements/categories?ids={q}&lang=en")
        if isinstance(data, dict):
            data = [data]
        if not isinstance(data, list):
            continue
        for obj in data:
            if not isinstance(obj, dict):
                continue
            try:
                cid = int(obj.get("id") or 0)
                order = int(obj.get("order") or 0)
            except (TypeError, ValueError):
                continue
            if cid <= 0:
                continue
            if order < 0:
                order = 0
            rows.append(
                (cid, order, pack_cell(obj.get("name")), int_list(obj, "achievements"))
            )
        time.sleep(SLEEP_SEC)
        done = min(i + BATCH, len(int_ids))
        print(f"  /v2/achievements/categories {done}/{len(int_ids)}", file=sys.stderr)
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
        # Raw members. Gzip inside IGH1 plus GitHub Content-Encoding gzip made
        # Wine/WinHTTP treat the file as junk (~2MB compressed vs ~8MB raw).
        compress=False,
    )


def write_ach_igh(
    path: Path,
    build_id: str,
    groups: str,
    categories: str,
    defs: str,
) -> None:
    pack_igh_file(
        path,
        {
            "ach.ver": (build_id + "\n").encode("utf-8"),
            "groups.tsv": groups.encode("utf-8"),
            "categories.tsv": categories.encode("utf-8"),
            "defs.tsv": defs.encode("utf-8"),
        },
        compress=False,
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
    ach_defs: list[tuple] = []
    for kind, path in NAME_PACKS:
        print(f"{path}…", file=sys.stderr)
        if kind == "a":
            by_kind[kind], ach_defs = fetch_achievements()
        else:
            by_kind[kind] = fetch_paged(
                path, path, dye_rgb_extra if kind == "d" else None
            )

    item_names = {iid: name for iid, name, _icon in by_kind.get("i", [])}
    print("legendaryarmory…", file=sys.stderr)
    by_kind["y"] = fetch_armory(item_names)

    print("recipes…", file=sys.stderr)
    recipes = fetch_recipes()

    print("achievement groups…", file=sys.stderr)
    ach_groups = fetch_ach_groups()
    print("achievement categories…", file=sys.stderr)
    ach_cats = fetch_ach_categories()

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

    g_lines = ["# gw2-ach-groups 1\n", f"# build {build_id}\n"]
    for gid, order, name, cats in sorted(ach_groups, key=lambda r: (r[1], r[0])):
        g_lines.append(f"g\t{gid}\t{order}\t{name}\t{','.join(str(x) for x in cats)}\n")
    c_lines = ["# gw2-ach-categories 1\n", f"# build {build_id}\n"]
    for cid, order, name, aids in sorted(ach_cats):
        c_lines.append(
            f"c\t{cid}\t{order}\t{name}\t{','.join(str(x) for x in aids)}\n"
        )
    d_lines = ["# gw2-ach-defs 1\n", f"# build {build_id}\n"]
    for row in sorted(ach_defs):
        d_lines.append(
            f"d\t{row[0]}\t{row[1]}\t{row[2]}\t{row[3]}\t{row[4]}\t{row[5]}\t{row[6]}\t{row[7]}\t{row[8]}\n"
        )
    write_ach_igh(
        out / "gw2-helper-achievements.igh",
        build_id,
        "".join(g_lines),
        "".join(c_lines),
        "".join(d_lines),
    )

    man = write_manifest(
        out / "gw2-helper-catalog.manifest",
        inherit=args.inherit_manifest,
        catalog=build_id,
    )
    print(f"wrote {out / 'gw2-helper-catalog.manifest'} catalog={man.get('catalog', '')}")
    bits = " ".join(f"{k}={len(by_kind.get(k, []))}" for k, _ in NAME_PACKS + [("y", "")])
    print(
        f"names {bits}; recipes={len(recipes)}; "
        f"ach-groups={len(ach_groups)} ach-cats={len(ach_cats)} ach-defs={len(ach_defs)}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
