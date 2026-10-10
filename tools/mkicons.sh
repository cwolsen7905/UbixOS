#!/bin/sh
# mkicons.sh — render the SVG icon sources to the PNGs objGFX loads.
#
# Sources live in share/icons/src/<size>/*.svg (from the ubixos-v2 design,
# docs/design/mockups/ubixos-v2/); output goes to share/icons/<size>/*.png at
# exactly <size>x<size>, transparent background.  The PNGs are committed, so a
# normal build never needs an SVG renderer — rerun this only after editing or
# adding an SVG.  Needs rsvg-convert (macOS: brew install librsvg).
#
# Usage: tools/mkicons.sh          (run from the repo root)

set -e

SRC=share/icons/src
OUT=share/icons

command -v rsvg-convert >/dev/null 2>&1 || {
	echo "mkicons: rsvg-convert not found (brew install librsvg)" >&2
	exit 1
}
[ -d "$SRC" ] || { echo "mkicons: run from the repo root ($SRC missing)" >&2; exit 1; }

n=0
for dir in "$SRC"/*/; do
	size=$(basename "$dir")
	mkdir -p "$OUT/$size"
	for svg in "$dir"*.svg; do
		[ -f "$svg" ] || continue
		png="$OUT/$size/$(basename "$svg" .svg).png"
		rsvg-convert -w "$size" -h "$size" "$svg" -o "$png"
		n=$((n + 1))
	done
done
echo "mkicons: rendered $n icons into $OUT/<size>/"
