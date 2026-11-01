#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
B="$ROOT/build/amigaos3"
TOOL=${AMIGA_TOOL_PREFIX:-m68k-amigaos}
NM="$TOOL-nm"
OD="$TOOL-objdump"

cd "$ROOT"
make test
./build-amiga.sh >"$ROOT/build/verify-amiga-build.log" 2>&1

if grep -Eq 'warning:|error:' "$ROOT/build/verify-amiga-build.log"; then
    echo "Amiga build emitted diagnostics:" >&2
    grep -nE 'warning:|error:' "$ROOT/build/verify-amiga-build.log" >&2
    exit 1
fi

for f in GFOpenPrint oapspool.device oapspooltest oapstatustest oapprinttest GFOpenPrint.driver; do
    test -s "$B/$f"
    file "$B/$f" | grep -q 'AmigaOS loadseg'
done
test -s "$B/GFOpenPrint.info"
file "$B/GFOpenPrint.info" | grep -q 'Amiga Workbench project icon'

"$NM" -n "$B/GFOpenPrint.driver" | grep -q '^00000000 T _oap_printer_tag$'
"$NM" -n "$B/GFOpenPrint.driver" | grep -q '^00000008 T _oap_ped$'
test -z "$("$NM" -u "$B/GFOpenPrint.driver")"
test -z "$("$NM" -u "$B/oapspool.device")"
"$OD" -s -j .text "$B/GFOpenPrint.driver" | grep -q '0000 70004e75 00230000'

if command -v pdfinfo >/dev/null 2>&1; then
    pdfinfo "$ROOT/build/oap-firstlight.pdf" | grep -q 'PDF version:     1.4'
    pdfinfo "$ROOT/build/oap-firstlight.pdf" | grep -q 'Pages:           1'
fi

echo "GFOpenPrint verification: PASS"
sha256sum "$B/GFOpenPrint" "$B/oapspool.device" "$B/oapspooltest" \
  "$B/oapstatustest" "$B/oapprinttest" "$B/GFOpenPrint.driver" "$B/GFOpenPrint.info"
