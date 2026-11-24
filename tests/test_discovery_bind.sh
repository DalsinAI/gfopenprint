#!/bin/sh
# Copyright (c) 2026 AmiCradle Team. SPDX-License-Identifier: MIT
# BL-134: on GFOS's host a UDP bind to 0.0.0.0 stays on 127.0.0.1 unless the
# network is in LAN address mode, and a query from there finds no printer.
# The search must send its mDNS queries from an unbound socket.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
[ "$(uname -s)" = Linux ] || { echo 'SKIP: the GFOS bind policy is faked with Linux LD_PRELOAD'; exit 0; }
OUT="$ROOT/build/bindpolicy"
mkdir -p "$OUT"
rm -f "$OUT/queries.log"
CC=${HOST_CC:-cc}
"$CC" -shared -fPIC "$ROOT/tests/host/gfos_bind.c" -o "$OUT/gfos_bind.so" -ldl
"$CC" -I"$ROOT/include" -O2 -Wall -Wextra -Werror \
    "$ROOT"/src/discovery/*.c "$ROOT/src/core/ipp.c" "$ROOT/src/core/job.c" "$ROOT"/src/net/*.c "$ROOT/src/tls/tls.c" "$ROOT/src/tls/trust.c" \
    -o "$OUT/discover"
GFOS_BIND_LOG="$OUT/queries.log" LD_PRELOAD="$OUT/gfos_bind.so" "$OUT/discover" "$OUT/result.tsv"
[ -s "$OUT/queries.log" ] || { echo 'FAIL: no mDNS query was sent'; exit 1; }
if grep -q 'from 127\.' "$OUT/queries.log"; then
    echo 'FAIL: mDNS queries left from 127.0.0.1, where no printer can answer'
    exit 1
fi
printf 'PASS: %s mDNS queries, none from 127.0.0.1\n' "$(wc -l < "$OUT/queries.log" | tr -d ' ')"
