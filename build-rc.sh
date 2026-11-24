#!/bin/sh
# GFOpenPrint's release candidate in the kitchen (stove_build script build-rc.sh): the programs from
# build-amiga.sh and build-browser.sh (build-amiga.sh builds the viewer too), then the Installer package
# (tools/make_package.py) as GFOpenPrint-<version>-AmigaOS3.lha with its SHA-256, the release notes and
# BUILD-INFO.txt, in OUT.
#
#   candidate/VERSION              one line: the version (as 0.4-rc1)
#   candidate/RELEASE-NOTES.txt    beside the .lha and in the drawer
#
# The package's icons need AmiCradle's scripts/icons and GFOpenUp's tools: AMICRADLE and GFOPENUP name
# checkouts; else the checkouts beside this one; else they are fetched (depth 1) from the kitchen's git
# mirror (AMIGACHROME_GIT_MIRROR, default http://127.0.0.1:8418/), as other GF builds fetch DalsinAI's
# repositories with no login.
# MIT, Copyright (c) 2026 AmiCradle Team.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
OUT=${OUT:-"$ROOT/build/rc"}
VERSION=$(head -n 1 "$ROOT/candidate/VERSION" | tr -d '[:space:]')
case "$VERSION" in
    [0-9]*.[0-9]*) ;;
    *) echo "build-rc.sh: candidate/VERSION does not begin with a version (as 0.4-rc1)" >&2; exit 1 ;;
esac
case "$VERSION" in
    *[!0-9A-Za-z.-]*) echo "build-rc.sh: the version may hold letters, digits, '.' and '-' only" >&2; exit 1 ;;
esac
[ -f "$ROOT/candidate/RELEASE-NOTES.txt" ] || { echo "build-rc.sh: no candidate/RELEASE-NOTES.txt" >&2; exit 1; }
command -v lha >/dev/null 2>&1 || { echo "build-rc.sh: no lha on this machine" >&2; exit 1; }   # before the builds
# the compiler, as build-amiga.sh picks it, for BUILD-INFO.txt
if [ -n "${AMIGA_CC:-}" ]; then RC_CC=$AMIGA_CC
elif [ -n "${STOVE:-}" ] && [ -x "$STOVE/prefix/bin/m68k-amigaos-gcc" ]; then RC_CC="$STOVE/prefix/bin/m68k-amigaos-gcc"
else RC_CC=m68k-amigaos-gcc; fi
mkdir -p "$OUT"
BUILD="$ROOT/build/amigaos3"                    # where make_package.py reads the programs
rm -rf "$BUILD"
OAP_OUT="$BUILD" sh "$ROOT/build-amiga.sh"
OAP_OUT="$BUILD" sh "$ROOT/build-browser.sh"

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
MIRROR=${AMIGACHROME_GIT_MIRROR:-http://127.0.0.1:8418/}
fetch() {   # fetch REPO PATH...: a depth-1 checkout of those paths of DalsinAI/REPO, into WORK/REPO
    r=$1; shift
    if GIT_TERMINAL_PROMPT=0 git clone -q --depth 1 --filter=blob:none --sparse "${MIRROR}DalsinAI/$r" "$WORK/$r" >&2; then
        git -C "$WORK/$r" sparse-checkout set "$@" >&2
    else                                        # a mirror with no partial clone: the whole of it, depth 1
        rm -rf "$WORK/$r"
        GIT_TERMINAL_PROMPT=0 git clone -q --depth 1 "${MIRROR}DalsinAI/$r" "$WORK/$r" >&2
    fi
    echo "$WORK/$r"
}
if [ -z "${AMICRADLE:-}" ]; then
    if [ -d "$ROOT/../amicradle/scripts/icons" ]; then AMICRADLE="$ROOT/../amicradle"; else AMICRADLE=$(fetch amicradle scripts/icons); fi
fi
if [ -z "${GFOPENUP:-}" ]; then
    if [ -d "$ROOT/../gfopenup/tools" ]; then GFOPENUP="$ROOT/../gfopenup"; else GFOPENUP=$(fetch gfopenup tools); fi
fi
export AMICRADLE GFOPENUP

NAME="GFOpenPrint-$VERSION-AmigaOS3"
P="$WORK/package"
python3 "$ROOT/tools/make_package.py" --version "$VERSION" --out "$P"
cp "$ROOT/candidate/RELEASE-NOTES.txt" "$P/GFOpenPrint/RELEASE-NOTES.txt"
{
    echo "GFOpenPrint $VERSION"
    echo "commit: $(git -C "$ROOT" rev-parse HEAD 2>/dev/null || echo unknown)"
    echo "compiler: $("$RC_CC" --version 2>/dev/null | head -n 1)"
    echo "icons: amicradle $(git -C "$AMICRADLE" rev-parse HEAD 2>/dev/null || echo unknown), gfopenup $(git -C "$GFOPENUP" rev-parse HEAD 2>/dev/null || echo unknown)"
    echo "built: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
} > "$P/GFOpenPrint/BUILD-INFO.txt"
(cd "$P/GFOpenPrint" && find . -type f ! -name SHA256SUMS | LC_ALL=C sort | xargs -d '\n' sha256sum > SHA256SUMS)
rm -f "$OUT/$NAME.lha"
(cd "$P" && lha -aq "$OUT/$NAME.lha" GFOpenPrint GFOpenPrint.info >/dev/null)
(cd "$OUT" && sha256sum "$NAME.lha" > "$NAME.lha.sha256")
cp "$ROOT/candidate/RELEASE-NOTES.txt" "$OUT/RELEASE-NOTES.txt"
cp "$P/GFOpenPrint/BUILD-INFO.txt" "$OUT/BUILD-INFO.txt"
cat "$OUT/BUILD-INFO.txt"
echo "build-rc.sh: $OUT/$NAME.lha ($(wc -c < "$OUT/$NAME.lha") bytes, $(find "$P/GFOpenPrint" -type f | wc -l | tr -d ' ') files)"
