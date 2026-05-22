#!/bin/bash
# Converts all PPM files in a directory to PNG for easy viewing.
# Usage: bash convert_outputs.sh <directory>
# Requires ImageMagick (winget install ImageMagick.ImageMagick)

DIR="${1:-.}"
count=0
for f in "$DIR"/*.ppm; do
    [ -f "$f" ] || continue
    out="${f%.ppm}.png"
    magick "$f" "$out" && echo "  $f -> $out"
    count=$((count + 1))
done
echo "Converted $count file(s)."
