#!/usr/bin/env bash
# Build a trimmed CEF Stable 150 Windows x64 runtime zip for GW2-InGame-Helper.
# Downloads the official minimal binary from cef-builds.spotifycdn.com, flattens
# Release/ + Resources/ into one folder, zips it, and prints SHA256 for CefRuntime.h.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT_DIR="${ROOT}/build/cef-runtime"
CEF_VER="${CEF_VER:-150.0.14+g7c1aa68+chromium-150.0.7871.129}"
CEF_FILE="cef_binary_${CEF_VER}_windows64_minimal.tar.bz2"
URL="https://cef-builds.spotifycdn.com/${CEF_FILE}"
ZIP_NAME="cef-runtime-150-windows64.zip"

mkdir -p "${OUT_DIR}/dl" "${OUT_DIR}/stage"
cd "${OUT_DIR}/dl"

if [[ ! -f "${CEF_FILE}" ]]; then
	echo "Downloading ${URL}"
	curl -L --fail -o "${CEF_FILE}" "${URL}"
fi

EXTRACT="${OUT_DIR}/extract"
rm -rf "${EXTRACT}"
mkdir -p "${EXTRACT}"
echo "Extracting ${CEF_FILE}…"
tar -xjf "${CEF_FILE}" -C "${EXTRACT}" --strip-components=1

STAGE="${OUT_DIR}/runtime"
rm -rf "${STAGE}"
mkdir -p "${STAGE}"

echo "Flattening Release/ + Resources/ → runtime/"
# DLLs / bins from Release (skip import lib)
shopt -s nullglob
for f in "${EXTRACT}/Release/"*; do
	base="$(basename "$f")"
	case "$base" in
		*.lib) continue ;;
		bootstrap.exe|bootstrapc.exe) continue ;; # helper EXE is our subprocess
	esac
	cp -a "$f" "${STAGE}/"
done
# Resources (paks, icudtl, locales) sit next to libcef.dll
cp -a "${EXTRACT}/Resources/"* "${STAGE}/"

# Drop debug-ish extras if present
rm -f "${STAGE}/"*.pdb 2>/dev/null || true

if [[ ! -f "${STAGE}/libcef.dll" ]]; then
	echo "error: libcef.dll missing after flatten" >&2
	exit 1
fi
if [[ ! -d "${STAGE}/locales" ]]; then
	echo "error: locales/ missing after flatten" >&2
	exit 1
fi

rm -f "${OUT_DIR}/${ZIP_NAME}"
(
	cd "${STAGE}"
	zip -qr "${OUT_DIR}/${ZIP_NAME}" .
)

HASH="$(sha256sum "${OUT_DIR}/${ZIP_NAME}" | awk '{print $1}')"
SIZE="$(wc -c < "${OUT_DIR}/${ZIP_NAME}")"
cat > "${OUT_DIR}/SHA256SUMS" <<EOF
${HASH}  ${ZIP_NAME}
EOF

echo
echo "Wrote ${OUT_DIR}/${ZIP_NAME} (${SIZE} bytes)"
echo "SHA256 ${HASH}"
echo
echo "Upload to a GitHub Release, then set in src/CefRuntime.h:"
echo "  kDownloadUrl = \"https://github.com/<org>/<repo>/releases/download/<tag>/${ZIP_NAME}\""
echo "  kSha256Hex   = \"${HASH}\""
echo "  kStamp       = \"150.0.14\"  (already set)"
