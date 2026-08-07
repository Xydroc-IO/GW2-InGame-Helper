#!/usr/bin/env python3
"""Verify shipping version + extract stamps match source SSOT.

Source of truth (code):
  AddonVersion.h, kHelperStamp, kHomePageVersion, kSitesStamp, CheatSheets kPackStamp,
  LivePanels kPanelVer, RaidFood kRaidFoodVersion, UiChrome kPackStamp, CefRuntime::kStamp

Docs that must agree (when present):
  README.md, docs/RELEASE_NOTES.md (header + current What's new stamps),
  docs/ARCHITECTURE.md, docs/DOCUMENTATION.md, docs/COMPLIANCE.md,
  docs/WHITEPAPER.md (appendix constants), docs/description.html

Exit 0 on success, 1 on drift. Used by `make check-stamps` / `make ci`.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8", errors="replace")


def first_group(pattern: str, text: str, flags: int = 0) -> str | None:
    m = re.search(pattern, text, flags)
    return m.group(1) if m else None


def require(cond: bool, msg: str, errors: list[str]) -> None:
    if not cond:
        errors.append(msg)


def parse_addon_version(h: str) -> str:
    maj = first_group(r"#define\s+ADDON_VERSION_MAJOR\s+(\d+)", h)
    minor = first_group(r"#define\s+ADDON_VERSION_MINOR\s+(\d+)", h)
    build = first_group(r"#define\s+ADDON_VERSION_BUILD\s+(\d+)", h)
    rev = first_group(r"#define\s+ADDON_VERSION_REVISION\s+(\d+)", h)
    if not all([maj, minor, build, rev]):
        raise SystemExit("FATAL: could not parse AddonVersion.h")
    return f"{maj}.{minor}.{build}.{rev}"


def parse_source_stamps() -> dict[str, str]:
    helper = read(ROOT / "src/browser/WikiBrowserHelper.cpp")
    home = read(ROOT / "src/browse/HomePage.cpp")
    sites = read(ROOT / "src/browse/sites/SitesLoadParse.cpp")
    cheats = read(ROOT / "src/browse/CheatSheets.cpp")
    live = read(ROOT / "src/browse/livepanels/LivePanelsInternal.h")
    raid = read(ROOT / "src/browse/RaidFood.cpp")
    chrome = read(ROOT / "src/app/UiChrome.cpp")
    cef = read(ROOT / "src/browser/CefRuntime.h")

    stamps = {
        "helper": first_group(r'kHelperStamp\s*=\s*"([^"]+)"', helper),
        "home": first_group(r'kHomePageVersion\s*=\s*"([^"]+)"', home),
        "sites": first_group(r'kSitesStamp\s*=\s*"([^"]+)"', sites),
        "cheatsheets": first_group(r'kPackStamp\s*=\s*"(c[^"]+)"', cheats),
        "live": first_group(r'kPanelVer\s*=\s*"([^"]+)"', live),
        "raid_food": first_group(r'kRaidFoodVersion\s*=\s*"([^"]+)"', raid),
        "ui_chrome": first_group(r'kPackStamp\s*=\s*"(uc[^"]+)"', chrome),
        "cef": first_group(r'kStamp\s*=\s*"([^"]+)"', cef),
    }
    missing = [k for k, v in stamps.items() if not v]
    if missing:
        raise SystemExit(f"FATAL: missing source stamps: {', '.join(missing)}")
    return stamps  # type: ignore[return-value]


def current_release_stamps(notes: str) -> dict[str, str] | None:
    """Parse the first 'Stamps:' bullet under the newest What's new section."""
    m = re.search(
        r"## What’s new in [0-9.]+.*?-\s+\*\*Stamps:\*\*\s*"
        r"Helper `([^`]+)`\s*·\s*homepage `([^`]+)`\s*·\s*sites `([^`]+)`\s*·\s*cheatsheets `([^`]+)`"
        r"(?:\s*·\s*live panel `([^`]+)`)?(?:\s*·\s*raid food `([^`]+)`)?(?:\s*·\s*ui-chrome `([^`]+)`)?",
        notes,
        re.S,
    )
    if not m:
        # ASCII apostrophe variant
        m = re.search(
            r"## What's new in [0-9.]+.*?-\s+\*\*Stamps:\*\*\s*"
            r"Helper `([^`]+)`\s*·\s*homepage `([^`]+)`\s*·\s*sites `([^`]+)`\s*·\s*cheatsheets `([^`]+)`"
            r"(?:\s*·\s*live panel `([^`]+)`)?(?:\s*·\s*raid food `([^`]+)`)?(?:\s*·\s*ui-chrome `([^`]+)`)?",
            notes,
            re.S,
        )
    if not m:
        return None
    return {
        "helper": m.group(1),
        "home": m.group(2),
        "sites": m.group(3),
        "cheatsheets": m.group(4),
        "live": m.group(5) or "",
        "raid_food": m.group(6) or "",
        "ui_chrome": m.group(7) or "",
    }


def main() -> int:
    errors: list[str] = []
    version = parse_addon_version(read(ROOT / "src/app/AddonVersion.h"))
    stamps = parse_source_stamps()

    print(f"SSOT version: {version}")
    print(
        "SSOT stamps: "
        f"helper={stamps['helper']} home={stamps['home']} sites={stamps['sites']} "
        f"cheatsheets={stamps['cheatsheets']} live={stamps['live']} "
        f"raid_food={stamps['raid_food']} ui_chrome={stamps['ui_chrome']} cef={stamps['cef']}"
    )

    # README
    readme = read(ROOT / "README.md")
    require(
        f"**Version:** `{version}`" in readme or f"**Version:** {version}" in readme,
        f"README.md version must be {version}",
        errors,
    )

    # RELEASE_NOTES header + current stamps
    notes_path = ROOT / "docs/RELEASE_NOTES.md"
    if notes_path.is_file():
        notes = read(notes_path)
        require(
            notes.startswith(f"# GW2 In-Game Helper v{version}")
            or f"# GW2 In-Game Helper v{version}\n" in notes[:80],
            f"RELEASE_NOTES.md title must be v{version}",
            errors,
        )
        rel = current_release_stamps(notes)
        require(rel is not None, "RELEASE_NOTES.md: could not parse current **Stamps:** line", errors)
        if rel:
            for key in ("helper", "home", "sites", "cheatsheets", "live", "raid_food", "ui_chrome"):
                if not rel.get(key):
                    errors.append(f"RELEASE_NOTES.md current stamps missing `{key}`")
                    continue
                require(
                    rel[key] == stamps[key],
                    f"RELEASE_NOTES.md stamp `{key}`={rel[key]!r} != source {stamps[key]!r}",
                    errors,
                )
    else:
        errors.append("docs/RELEASE_NOTES.md missing")

    # ARCHITECTURE header table
    arch_path = ROOT / "docs/ARCHITECTURE.md"
    if arch_path.is_file():
        arch = read(arch_path)
        require(
            f"| Addon revision (shipping) | `{version}` |" in arch
            or f"| Addon revision (shipping) | {version} |" in arch,
            f"ARCHITECTURE.md Addon revision must be {version}",
            errors,
        )
        expect_row = (
            f"| Helper / home / sites / cheatsheets stamps | "
            f"`{stamps['helper']}` / `{stamps['home']}` / `{stamps['sites']}` / `{stamps['cheatsheets']}` |"
        )
        require(
            expect_row in arch,
            "ARCHITECTURE.md helper/home/sites/cheatsheets stamp row must match source",
            errors,
        )
        require(
            f"| Live panel stamp | `{stamps['live']}` |" in arch,
            f"ARCHITECTURE.md Live panel stamp must be `{stamps['live']}`",
            errors,
        )
        require(
            f"| Raid food stamp | `{stamps['raid_food']}` |" in arch,
            f"ARCHITECTURE.md Raid food stamp must be `{stamps['raid_food']}`",
            errors,
        )
        require(
            f"| ui-chrome stamp | `{stamps['ui_chrome']}` |" in arch,
            f"ARCHITECTURE.md ui-chrome stamp must be `{stamps['ui_chrome']}`",
            errors,
        )
    # else: local-only; skip hard fail if absent (gitignored clones)

    # DOCUMENTATION shipping revision
    doc_path = ROOT / "docs/DOCUMENTATION.md"
    if doc_path.is_file():
        doc = read(doc_path)
        require(
            f"**Shipping revision:** **{version}**" in doc or f"Shipping revision:** **{version}**" in doc,
            f"DOCUMENTATION.md shipping revision must be {version}",
            errors,
        )

    # COMPLIANCE policy snapshot (published)
    comp_path = ROOT / "docs/COMPLIANCE.md"
    if comp_path.is_file():
        comp = read(comp_path)
        snap = first_group(r"Current policy snapshot:\s*\*\*v([0-9.]+)\*\*", comp)
        require(
            snap == version,
            f"COMPLIANCE.md policy snapshot is v{snap!r}, expected v{version}",
            errors,
        )

    # WHITEPAPER appendix constants (when present)
    wp_path = ROOT / "docs/WHITEPAPER.md"
    if wp_path.is_file():
        wp = read(wp_path)
        require(
            f"| Revision described | {version} |" in wp
            or f"| Revision described | `{version}` |" in wp,
            f"WHITEPAPER.md Revision described must be {version}",
            errors,
        )
        # Prefer exact stamp row if present
        if "Helper / home / sites / cheatsheets stamps" in wp:
            expect = (
                f"| Helper / home / sites / cheatsheets stamps | "
                f"{stamps['helper']} / {stamps['home']} / {stamps['sites']} / {stamps['cheatsheets']} |"
            )
            expect_q = (
                f"| Helper / home / sites / cheatsheets stamps | "
                f"`{stamps['helper']}` / `{stamps['home']}` / `{stamps['sites']}` / `{stamps['cheatsheets']}` |"
            )
            require(
                expect in wp or expect_q in wp,
                "WHITEPAPER.md appendix stamp row must match source helper/home/sites/cheatsheets",
                errors,
            )
        if "ui-chrome stamp" in wp:
            require(
                f"| ui-chrome stamp | {stamps['ui_chrome']} |" in wp
                or f"| ui-chrome stamp | `{stamps['ui_chrome']}` |" in wp,
                f"WHITEPAPER.md ui-chrome stamp must be {stamps['ui_chrome']}",
                errors,
            )
        if "Live panel stamp" in wp:
            require(
                f"| Live panel stamp | {stamps['live']} |" in wp
                or f"| Live panel stamp | `{stamps['live']}` |" in wp,
                f"WHITEPAPER.md Live panel stamp must be {stamps['live']}",
                errors,
            )
        if "Raid food stamp" in wp:
            require(
                f"| Raid food stamp | {stamps['raid_food']} |" in wp
                or f"| Raid food stamp | `{stamps['raid_food']}` |" in wp,
                f"WHITEPAPER.md Raid food stamp must be {stamps['raid_food']}",
                errors,
            )

    # description.html
    desc_path = ROOT / "docs/description.html"
    if desc_path.is_file():
        desc = read(desc_path)
        require(
            f"Version</strong> {version}" in desc or f"v{version}" in desc[:500] or version in desc,
            f"description.html must mention version {version}",
            errors,
        )
        # Current What's new stamp quartet
        require(
            f"<code>{stamps['helper']}</code>" in desc and f"<code>{stamps['cheatsheets']}</code>" in desc,
            "description.html current stamps must include helper + cheatsheets from source",
            errors,
        )

    if errors:
        print("\nSTAMP DRIFT — fix before shipping:")
        for e in errors:
            print(f"  - {e}")
        print(
            "\nBump asset stamps in source when those assets change; always sync "
            "RELEASE_NOTES / ARCHITECTURE / COMPLIANCE / WHITEOBR / description.html. "
            "See docs/DOCUMENTATION.md § Version stamp checklist."
        )
        return 1

    print("check-stamps: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
