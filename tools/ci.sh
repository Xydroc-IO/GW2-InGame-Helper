#!/usr/bin/env bash
# Local / Actions CI gate. Exit non-zero on any failure.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

echo "==> CI: validate-sites"
make validate-sites

echo "==> CI: check-sites (Sites.gen.cpp matches data/sites.json)"
make check-sites

echo "==> CI: CSS downlevel unit test"
python3 tools/test_css_downlevel.py

echo "==> CI: parse golden fixtures (LogManager + .trl)"
make test-parse

echo "==> CI: MinGW smoke build (DLL + embedded helper)"
make -j"$(nproc)" all

echo "==> CI: OK"
