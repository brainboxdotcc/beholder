#!/bin/sh
{ [ $(wget --spider "$2" 2>&1 | awk '/Length/ {print $2}') -lt 8388608 ] && wget -q -O "$1" "$2"; } || exit 1
convert "$1" -set colorspace Gray -separate -average -brightness-contrast 10x50 "$1.out.png" 2>/dev/null
tesseract "$1.out.png" "$1.out" --psm 6
cat "$1.out.txt" | grep "[a-zA-Z0-9\(\)_:,.]"
rm "$1.out.txt" "$1.out.png" "$1"

