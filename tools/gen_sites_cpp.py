#!/usr/bin/env python3
"""DEPRECATED — catalog is runtime JSON (SitesLoad.cpp).

Use `make validate-sites` and edit data/sites.json (schema v2).
This script remains only as a historical reference and exits with an error.
"""

from __future__ import annotations

import sys


def main() -> int:
	print(
		"error: Sites.gen.cpp codegen is retired on Beta.\n"
		"  Edit data/sites.json (schema v2) and run: make validate-sites\n"
		"  Runtime catalog: addons/<addon>/sites.json",
		file=sys.stderr,
	)
	return 1


if __name__ == "__main__":
	raise SystemExit(main())
