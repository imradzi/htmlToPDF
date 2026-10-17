#!/usr/bin/env bash
# Smoke-test the PlutoBook PDF pipeline (Linux build).
#
# Usage: BUILD_DIR=/path/to/build-ux ./htmlToPDF/scripts/verify_pdf.sh
#
# Requires: pdfinfo + pdftotext (poppler-utils).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HTML2PDF_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
PROJECT_DIR="$(cd "${HTML2PDF_DIR}/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${PROJECT_DIR}/build-ux}"
JOBS="${JOBS:-4}"

for tool in pdfinfo pdftotext; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "ERROR: $tool not found (sudo apt install poppler-utils)" >&2
        exit 1
    fi
done

echo "=== Building PDF tools ==="
cmake --build "$BUILD_DIR" --target handlebars_pdf report_probe -j "$JOBS"

echo ""
echo "=== Sample templates ==="
pushd "${BUILD_DIR}/htmlToPDF" >/dev/null
./handlebars_pdf
for pdf in invoice.pdf letter.pdf report.pdf; do
    pages=$(pdfinfo "$pdf" | awk '/^Pages/{print $2}')
    if [ -z "$pages" ] || [ "$pages" -lt 1 ]; then
        echo "ERROR: $pdf has no pages" >&2
        exit 1
    fi
    printf '  %-14s %s page(s)\n' "$pdf" "$pages"
done
popd >/dev/null

echo ""
echo "=== Report builder (running headers/footers, multi-section) ==="
"${BUILD_DIR}/htmlToPDF/report_probe"
pdftotext -layout /tmp/report_probe.pdf - | grep -q "Page 1 of"
pdftotext -layout /tmp/report_probe.pdf - | grep -q "Page 2 of"

echo ""
echo "PDF verification passed."
