#!/bin/sh
# Render and open in a browser, so parameters can be judged by eye.
#
# Takes exactly the arguments ascii-art takes, minus --format/--output:
#   ./preview.sh images/bull.jpg --mode gray --levels 20,195 --width 300
#
# BG=#ffffff ./preview.sh ...   to preview against a light page instead.
set -e

BG=${BG:-#000000}
OUT=${OUT:-build/preview.html}
BIN=./ascii-art

[ -x "$BIN" ] || make

mkdir -p "$(dirname "$OUT")"

# The fragment is embeddable, so it carries no page background of its own.
# A preview needs one, or grey-on-white makes every render look wrong.
{
    printf '<!doctype html><html><head><meta charset="utf-8">'
    printf '<title>ascii-art preview</title></head>'
    printf '<body style="background:%s;margin:0;display:grid;place-items:center;min-height:100vh">' "$BG"
    "$BIN" "$@" --format html
    printf '</body></html>'
} > "$OUT"

echo "wrote $OUT"
[ -n "$NO_OPEN" ] && exit 0
command -v open >/dev/null && open "$OUT"
