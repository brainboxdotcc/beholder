#!/bin/sh
convert "$1" -set colorspace Gray -separate -average -brightness-contrast 10x50 "$1.out.png" 2>/dev/null
tesseract "$1.out.png" "$1.out" --psm 6
cat "$1.out.txt"
rm "$1.out.txt" "$1.out.png" "$1"

