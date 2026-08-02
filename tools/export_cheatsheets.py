#!/usr/bin/env python3
"""One-shot / recovery: export legacy CheatSheets_Data.cpp into data/cheatsheets/.

Day-to-day edits belong in data/cheatsheets/*.html and shared.css — rebuild packs the folder.
This script needs src/CheatSheets_Data.cpp (restore from git history if regenerating).
"""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src" / "CheatSheets_Data.cpp"
OUT = ROOT / "data" / "cheatsheets"

FOOTER = (
	"Built into GW2 In-Game Helper. Not affiliated with ArenaNet or NCSoft. "
	"Informational reference only — check current meta for your squad."
)

CHECK_SCRIPT = r"""
(function(){
  var boxes=document.querySelectorAll("ul.checks input[type=checkbox]");
  if(!boxes.length) return;
  var key="gw2helper.checks."+(document.title||location.pathname||"sheet");
  var saved={};
  try{saved=JSON.parse(localStorage.getItem(key)||"{}")||{};}catch(e){}
  function save(){
    var state={};
    for(var i=0;i<boxes.length;i++) if(boxes[i].checked) state[i]=1;
    try{localStorage.setItem(key,JSON.stringify(state));}catch(e){}
  }
  function syncLabel(box){
    var lab=box.closest ? box.closest("label") : box.parentNode;
    if(!lab) return;
    if(box.checked) lab.classList.add("done"); else lab.classList.remove("done");
  }
  for(var i=0;i<boxes.length;i++){
    (function(box,idx){
      box.checked=!!saved[idx];
      syncLabel(box);
      box.addEventListener("change", function(){ syncLabel(box); save(); });
    })(boxes[i], i);
  }
})();
""".strip()


def decode_cpp_string_literal(s: str) -> str:
	"""Decode a single "..." C++ string literal body (no surrounding quotes)."""
	out: list[str] = []
	i = 0
	while i < len(s):
		c = s[i]
		if c != "\\":
			out.append(c)
			i += 1
			continue
		i += 1
		if i >= len(s):
			break
		esc = s[i]
		i += 1
		mapping = {
			"n": "\n",
			"r": "\r",
			"t": "\t",
			'"': '"',
			"'": "'",
			"\\": "\\",
		}
		if esc in mapping:
			out.append(mapping[esc])
		elif esc == "x" and i + 1 < len(s):
			h = s[i : i + 2]
			if re.fullmatch(r"[0-9a-fA-F]{2}", h):
				out.append(chr(int(h, 16)))
				i += 2
			else:
				out.append(esc)
		else:
			out.append(esc)
	return "".join(out)


def parse_cpp_string_expr(src: str, start: int) -> tuple[str, int]:
	"""Parse concatenated C++ string / raw-string literals starting at start. Returns (text, end_index)."""
	i = start
	n = len(src)
	parts: list[str] = []
	while i < n:
		while i < n and src[i] in " \t\r\n":
			i += 1
		if i >= n:
			break
		# Raw string R"delim( ... )delim"
		if src.startswith('R"', i):
			i += 2
			d0 = i
			while i < n and src[i] != "(":
				i += 1
			delim = src[d0:i]
			if i >= n or src[i] != "(":
				raise ValueError(f"bad raw string at {d0}")
			i += 1
			body0 = i
			end_token = ")" + delim + '"'
			j = src.find(end_token, i)
			if j < 0:
				raise ValueError(f"unclosed raw string delim={delim!r}")
			parts.append(src[body0:j])
			i = j + len(end_token)
			continue
		if src[i] == '"':
			i += 1
			body0 = i
			while i < n:
				if src[i] == "\\":
					i += 2
					continue
				if src[i] == '"':
					parts.append(decode_cpp_string_literal(src[body0:i]))
					i += 1
					break
				i += 1
			else:
				raise ValueError("unclosed string")
			continue
		break
	return "".join(parts), i


def parse_build_html_args(call_src: str) -> tuple[str, str, str, str, str, str]:
	"""Parse BuildHtml(a,b,c,d,e,f) argument list (without 'BuildHtml')."""
	i = 0
	while i < len(call_src) and call_src[i] in " \t\r\n(":
		i += 1
	args: list[str] = []
	for _ in range(6):
		text, i = parse_cpp_string_expr(call_src, i)
		args.append(text)
		while i < len(call_src) and call_src[i] in " \t\r\n":
			i += 1
		if i < len(call_src) and call_src[i] == ",":
			i += 1
			continue
		break
	if len(args) != 6:
		raise ValueError(f"expected 6 BuildHtml args, got {len(args)}")
	return args[0], args[1], args[2], args[3], args[4], args[5]


def build_html(title: str, eyebrow: str, heading: str, tagline: str, toc: str, body: str) -> str:
	parts = [
		"<!DOCTYPE html>",
		'<html lang="en">',
		"<head>",
		'<meta charset="utf-8"/>',
		'<meta name="viewport" content="width=device-width, initial-scale=1"/>',
		f"<title>{title}</title>",
		'<link rel="stylesheet" href="shared.css"/>',
		"</head>",
		"<body>",
		'<div class="wrap">',
		'<header class="hero">',
		f'<p class="eyebrow">{eyebrow}</p>',
		f"<h1>{heading}</h1>",
		f'<p class="tagline">{tagline}</p>',
		"</header>",
	]
	if toc.strip():
		parts.append('<nav class="toc" aria-label="Sections">')
		parts.append(toc.rstrip())
		parts.append("</nav>")
	parts.append(body.strip("\n"))
	parts.append(f"<footer>{FOOTER}</footer>")
	parts.append("</div>")
	parts.append("<script>")
	parts.append(CHECK_SCRIPT)
	parts.append("</script>")
	parts.append("</body>")
	parts.append("</html>")
	parts.append("")
	return "\n".join(parts)


def extract_css(src: str) -> str:
	m = re.search(r'static const char\*\s+kSharedCss\s*=\s*R"CSS\((.*?)\)CSS"', src, re.S)
	if not m:
		raise SystemExit("kSharedCss not found")
	return m.group(1).strip("\n") + "\n"


def extract_pages_meta(src: str) -> list[dict]:
	block = re.search(r"static const PageSpec kPages\[\]\s*=\s*\{(.*?)\n\t\};", src, re.S)
	if not block:
		raise SystemExit("kPages not found")
	body = block.group(1)
	# {{"id", "about:", "stem", "ver", "label", "title"}, HtmlFn},
	pat = re.compile(
		r'\{\{"([^"]+)",\s*"([^"]+)",\s*"([^"]+)",\s*"([^"]+)",\s*\n'
		r'\s*"([^"]*)",\s*"([^"]*)"\},\s*\n'
		r"\s*(\w+)\}",
		re.S,
	)
	rows = []
	for m in pat.finditer(body):
		rows.append(
			{
				"id": m.group(1),
				"about": m.group(2),
				"fileStem": m.group(3),
				"version": m.group(4),
				"browseLabel": m.group(5),
				"browseTitle": m.group(6),
				"fn": m.group(7),
			}
		)
	if not rows:
		raise SystemExit("no kPages entries parsed")
	return rows


def extract_html_fn(src: str, fn: str) -> str:
	m = re.search(
		rf"std::string\s+{re.escape(fn)}\s*\(\)\s*\{{.*?return\s+BuildHtml\s*\(",
		src,
		re.S,
	)
	if not m:
		raise SystemExit(f"{fn}: BuildHtml call not found")
	# Find matching close for BuildHtml( ... );
	i = m.end() - 1  # points at '('
	depth = 0
	j = i
	while j < len(src):
		c = src[j]
		if c == "(":
			depth += 1
		elif c == ")":
			depth -= 1
			if depth == 0:
				j += 1
				break
		elif c == '"':
			# skip ordinary string
			j += 1
			while j < len(src):
				if src[j] == "\\":
					j += 2
					continue
				if src[j] == '"':
					j += 1
					break
				j += 1
			continue
		elif src.startswith('R"', j):
			# skip raw string
			k = j + 2
			while k < len(src) and src[k] != "(":
				k += 1
			delim = src[j + 2 : k]
			end = ")" + delim + '"'
			k = src.find(end, k + 1)
			if k < 0:
				raise SystemExit(f"{fn}: unclosed raw string")
			j = k + len(end)
			continue
		j += 1
	else:
		raise SystemExit(f"{fn}: unclosed BuildHtml(")
	args_src = src[i:j]  # includes outer parens
	title, eyebrow, heading, tagline, toc, body = parse_build_html_args(args_src)
	return build_html(title, eyebrow, heading, tagline, toc, body)


def main() -> int:
	if not SRC.is_file():
		print(f"missing {SRC}", file=sys.stderr)
		return 1
	src = SRC.read_text(encoding="utf-8")
	OUT.mkdir(parents=True, exist_ok=True)
	for old in OUT.glob("*"):
		if old.is_file():
			old.unlink()

	css = extract_css(src)
	(OUT / "shared.css").write_text(css, encoding="utf-8")

	pages = extract_pages_meta(src)
	manifest_sheets = []
	for p in pages:
		html = extract_html_fn(src, p["fn"])
		fname = f"{p['fileStem']}.html"
		(OUT / fname).write_text(html, encoding="utf-8")
		manifest_sheets.append(
			{
				"id": p["id"],
				"about": p["about"],
				"file": fname,
				"version": p["version"],
				"browseLabel": p["browseLabel"],
				"browseTitle": p["browseTitle"],
			}
		)
		print(f"  wrote {fname} ({len(html)} bytes)")

	manifest = {"schema": 1, "sheets": manifest_sheets}
	(OUT / "manifest.json").write_text(
		json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8"
	)
	print(f"ok: {len(manifest_sheets)} sheets → {OUT}")
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
