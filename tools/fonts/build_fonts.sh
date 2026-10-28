#!/bin/bash
# TTF/OTF (tools/fonts/src, from fetch_fonts.sh) → components/ui/fonts/font_*.c
# Glyphs: only what the UI shows (design/README.md "Fonts", docs/ui.md §4.1).
# After changing this file, re-run cmake configure (sources.cmake GLOBs the outputs).
set -euo pipefail
cd "$(dirname "$0")"
SRC=src
OUT=../../components/ui/fonts
mkdir -p "$OUT"
KO=$(tr -d '\n' < symbols.txt)
# ASCII the Pretendard labels use: digits, '.', '°', '%', '·', '/', space, ON/OFF
PRET_LATIN="0123456789.°%·/ ONF"

conv() {   # conv <font file> <size> <name> <symbols> [fallback]
  local fb=()
  [ -n "${5:-}" ] && fb=(--lv-fallback "$5")
  lv_font_conv --font "$1" --size "$2" --bpp 4 --format lvgl --no-compress \
    --symbols "$4" --lv-font-name "$3" ${fb[@]+"${fb[@]}"} -o "$OUT/$3.c"
  echo "  $3"
}

echo "Pretendard"
for sz in 12 13 14 15 17 18 20; do
  conv "$SRC/Pretendard-Medium.otf" $sz font_pret500_$sz "$KO$PRET_LATIN"
done
for sz in 17 23 26; do
  conv "$SRC/Pretendard-Regular.otf" $sz font_pret400_$sz "$KO$PRET_LATIN"
done
for sz in 23 26 32; do
  conv "$SRC/Pretendard-SemiBold.otf" $sz font_pret600_$sz "$KO$PRET_LATIN"
done
conv "$SRC/Pretendard-Medium.otf" 64 font_pret500_64 "분초"

echo "Inter Tight 200"
conv "$SRC/InterTight-ExtraLight.ttf" 24 font_inter200_24 "%"
conv "$SRC/InterTight-ExtraLight.ttf" 28 font_inter200_28 "°"
conv "$SRC/InterTight-ExtraLight.ttf" 60 font_inter200_60 "OFN"
conv "$SRC/InterTight-ExtraLight.ttf" 64 font_inter200_64 "0123456789." font_pret500_64
conv "$SRC/InterTight-ExtraLight.ttf" 72 font_inter200_72 "0123456789"

echo "JetBrains Mono 400"
conv "$SRC/JetBrainsMono-Regular.ttf" 17 font_jb400_17 "0123456789%" font_pret400_17
conv "$SRC/JetBrainsMono-Regular.ttf" 9  font_jb400_9  "0123456789/OFN"
echo "fonts built ✓"
