#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
# The compiler and where it builds, as build-amiga.sh: AMIGA_CC, else the kitchen's STOVE, else PATH;
# OAP_OUT, else the kitchen's OUT (so stove_build lists OAPDiscover and OAPPrinters), else build/amigaos3
if [ -n "${AMIGA_CC:-}" ]; then CC=$AMIGA_CC
elif [ -n "${STOVE:-}" ] && [ -x "$STOVE/prefix/bin/m68k-amigaos-gcc" ]; then CC="$STOVE/prefix/bin/m68k-amigaos-gcc"
else CC=$(command -v m68k-amigaos-gcc || echo m68k-amigaos-gcc); fi
OUT=${OAP_OUT:-${OUT:-"$ROOT/build/amigaos3"}}
echo "compiler: $CC ($("$CC" --version 2>/dev/null | head -1))"
mkdir -p "$OUT"
# Strong library-base definitions prevent libnix autolib from opening window.library.
# ReAction classes and bsdsocket.library are opened explicitly by our code.
FLAGS="-m68040 -specs=$ROOT/specs/m68k-soft-m68040.specs -fno-common -O2 -fno-delete-null-pointer-checks -Wall -Wextra -Werror -noixemul"
. "$ROOT/scripts/tls-flags.sh"
NET="$ROOT/src/net/conn.c $ROOT/src/net/ipp_client.c $ROOT/src/net/http_auth.c $ROOT/src/net/mdns.c"
"$CC" $FLAGS -I"$ROOT/include" $TLS_FLAGS "$ROOT"/src/discovery/*.c "$ROOT/src/core/ipp.c" $NET $TLS_SOURCES "$ROOT/src/amiga/oap_stack.c" -lamiga -o "$OUT/OAPDiscover"
# Printers and Queue (the 3 Oct 2026 redesign): the browser and the queue in one ReAction window.
"$CC" $FLAGS -I"$ROOT/include" "$ROOT/src/ui/oap_printers_window.c" "$ROOT/src/ui/oap_gt.c" "$ROOT/src/ui/oap_printers.c" "$ROOT/src/amiga/oap_stack.c" "$ROOT/src/amiga/selection_events.c" "$ROOT/src/amiga/printer_preferences.c" "$ROOT/src/discovery/protocol.c" "$ROOT/src/core/pdf_demo.c" "$ROOT/src/core/ipp.c" "$ROOT/src/tls/tls.c" "$ROOT/src/tls/trust.c" -lamiga -o "$OUT/OAPPrinters"
python3 "$ROOT/tools/make_oap_app_icon.py" "$OUT/OAPPrinters.info"
file "$OUT/OAPDiscover" "$OUT/OAPPrinters"

# Isolated native IPC regression; never notifies normal application windows.
"$CC" $FLAGS -DOAP_SELECTION_PREFIX='"OAP.TestSelection."' -I"$ROOT/include" \
 "$ROOT/tests/oap_selection_smoke.c" "$ROOT/src/amiga/selection_events.c" "$ROOT/src/amiga/printer_preferences.c" \
 -o "$OUT/OAPSelectionSmoke"
