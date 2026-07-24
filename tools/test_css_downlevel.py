#!/usr/bin/env python3
"""Sanity-check CSS downlevel rules that BootJs / CssCompat must preserve."""
from __future__ import annotations

import re
import sys


def parse_color(s: str):
    s = s.strip()
    m = re.match(r"^#([0-9a-fA-F]{3,8})$", s)
    if not m:
        return None
    h = m.group(1)
    if len(h) == 3:
        h = h[0] * 2 + h[1] * 2 + h[2] * 2
    a = 1.0
    if len(h) == 8:
        a = int(h[6:8], 16) / 255.0
        h = h[:6]
    return int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16), a


def rewrite_mix(css: str) -> str:
    def repl(full: str) -> str:
        m = re.match(
            r"color-mix\(\s*(?:in\s+[\w-]+\s*,\s*)?(.+?)\s+(\d+(?:\.\d+)?)%\s*,\s*transparent\s*\)",
            full,
        )
        if not m:
            return full
        col, pct = m.group(1).strip(), float(m.group(2))
        c = parse_color(col)
        if not c:
            return full
        r, g, b, a = c
        return f"rgba({r},{g},{b},{a * (pct / 100.0)})"

    return re.sub(r"color-mix\((?:[^()]|\([^)]*\))*\)", lambda m: repl(m.group(0)), css)


def strip_in(css: str) -> str:
    return re.sub(
        r"(linear|radial|conic)-gradient\(\s*in\s+(?:oklab|oklch|srgb|hsl|lab|xyz)\s*,?",
        r"\1-gradient(",
        css,
    )


def downlevel(css: str) -> str:
    css = rewrite_mix(css)
    css = strip_in(css)
    return css


def main() -> int:
    gemini_var = "--mat-ripple-color:color-mix(in srgb,#1f1f1f 10%,transparent)"
    out = downlevel(gemini_var)
    if "color-mix(" in out:
        print("FAIL: color-mix left intact:", out)
        return 1
    if "rgba(31,31,31,0.1)" not in out:
        print("FAIL: unexpected rewrite:", out)
        return 1

    # Optional-in form (if "in srgb" was stripped): still rewrite.
    stripped_mix = "color-mix(#1f1f1f 10%,transparent)"
    fixed_buggy = rewrite_mix(stripped_mix)
    if "color-mix(" in fixed_buggy:
        print("FAIL: optional-in rewrite should still fix stripped mix:", fixed_buggy)
        return 1

    grad = "background:linear-gradient(in oklab,#fff,#000)"
    gout = strip_in(grad)
    if "in oklab" in gout:
        print("FAIL: gradient strip:", gout)
        return 1
    if "linear-gradient(#fff,#000)" not in gout:
        print("FAIL: gradient expected flatten:", gout)
        return 1

    # strip_in must not touch color-mix
    keep = strip_in(gemini_var)
    if keep != gemini_var:
        print("FAIL: strip_in mutated color-mix:", keep)
        return 1

    p3 = "box-shadow:0 0 0 1px color(display-p3 0 0 0/6%)"
    # mimic rewriteDisplayP3
    def rewrite_p3(css: str) -> str:
        def repl(m):
            body = m.group(1).strip()
            rgb_part, alpha_part = body, ""
            if "/" in body:
                rgb_part, alpha_part = body.rsplit("/", 1)
                rgb_part, alpha_part = rgb_part.strip(), alpha_part.strip()
            nums = rgb_part.split()
            if len(nums) < 3:
                return m.group(0)
            r = round(max(0, min(1, float(nums[0]))) * 255)
            g = round(max(0, min(1, float(nums[1]))) * 255)
            b = round(max(0, min(1, float(nums[2]))) * 255)
            a = 1.0
            if alpha_part:
                if alpha_part.endswith("%"):
                    a = float(alpha_part[:-1]) / 100.0
                else:
                    a = float(alpha_part)
            return f"rgba({r},{g},{b},{a})"

        return re.sub(r"color\(display-p3\s+([^)]*)\)", repl, css)

    p3o = rewrite_p3(p3)
    if "color(display" in p3o or "rgba(0,0,0,0.06)" not in p3o:
        print("FAIL: display-p3:", p3o)
        return 1

    print("test_css_downlevel: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
