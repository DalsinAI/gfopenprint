#!/bin/sh
# Copyright (c) 2026 AmiCradle Team. SPDX-License-Identifier: MIT
# build-rc.sh's checks before it builds: candidate/VERSION must be a plain version and the release notes
# must be there. Run on a copy of the scripts, with a stand-in compiler, so nothing is built or fetched.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
mkdir -p "$T/r/candidate"
cp "$ROOT/build-rc.sh" "$T/r/"
printf '#!/bin/sh\necho "built $*"\n' > "$T/r/build-amiga.sh"
cp "$T/r/build-amiga.sh" "$T/r/build-browser.sh"
run() { OUT="$T/out" sh "$T/r/build-rc.sh" > "$T/log" 2>&1; }
echo 'rc1' > "$T/r/candidate/VERSION"
echo notes > "$T/r/candidate/RELEASE-NOTES.txt"
if run; then echo 'FAIL: a version with no number was taken'; exit 1; fi
grep -q 'does not begin with a version' "$T/log"
echo '0.4-rc1;rm' > "$T/r/candidate/VERSION"
if run; then echo 'FAIL: a version with ; was taken'; exit 1; fi
grep -q "letters, digits" "$T/log"
echo '0.4-rc1' > "$T/r/candidate/VERSION"
rm "$T/r/candidate/RELEASE-NOTES.txt"
if run; then echo 'FAIL: no release notes was taken'; exit 1; fi
grep -q 'no candidate/RELEASE-NOTES.txt' "$T/log"
v=$(head -n 1 "$ROOT/candidate/VERSION")
case "$v" in [0-9]*.[0-9]*-rc[0-9]*|[0-9]*.[0-9]*) ;; *) echo "FAIL: candidate/VERSION is $v"; exit 1 ;; esac
[ -s "$ROOT/candidate/RELEASE-NOTES.txt" ]
grep -q "GFOpenPrint $v" "$ROOT/candidate/RELEASE-NOTES.txt" || { echo 'FAIL: the release notes do not name the version'; exit 1; }
printf 'PASS: build-rc.sh refuses a bad version or no notes; candidate %s has its notes\n' "$v"
