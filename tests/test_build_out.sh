#!/bin/sh
# Copyright (c) 2026 AmiCradle Team. SPDX-License-Identifier: MIT
# build-browser.sh builds where the kitchen's OUT says, with the STOVE's compiler, as build-amiga.sh does,
# so stove_build hands back OAPDiscover and OAPPrinters. A stand-in compiler only notes its -o.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
mkdir -p "$T/stove/prefix/bin" "$T/out"
cat > "$T/stove/prefix/bin/m68k-amigaos-gcc" <<'CC'
#!/bin/sh
[ "${1:-}" = --version ] && { echo "stand-in gcc"; exit 0; }
while [ $# -gt 0 ]; do [ "$1" = -o ] && { : > "$2"; echo "$2" >> "$STANDIN_LOG"; }; shift; done
CC
chmod +x "$T/stove/prefix/bin/m68k-amigaos-gcc"
cat > "$T/file" <<'F'
#!/bin/sh
exit 0
F
chmod +x "$T/file"
STANDIN_LOG="$T/log" STOVE="$T/stove" OUT="$T/out" PATH="$T:$PATH" sh "$ROOT/build-browser.sh" > "$T/run.txt"
for p in OAPDiscover OAPPrinters; do
    [ -e "$T/out/$p" ] || { echo "FAIL: $p not built in OUT"; cat "$T/log"; exit 1; }
done
grep -q 'compiler: .*stove/prefix/bin/m68k-amigaos-gcc' "$T/run.txt" || { echo 'FAIL: the STOVE compiler was not used'; exit 1; }
printf 'PASS: build-browser.sh builds OAPDiscover and OAPPrinters in OUT with the STOVE compiler\n'
