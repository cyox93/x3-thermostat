"""Rewrite an lv_font_conv output's line_height/base_line with the font's
hhea metrics, as a browser does for CSS `line-height: normal`.

lv_font_conv measures only the glyphs it converted, so a subset font gets a
much smaller line than the design's HTML (Pretendard 20: 18 px vs 24 px) and
every stacked label lands a few pixels off.

usage: css_metrics.py <font file> <size px> <generated .c>
"""
import re
import sys

from fontTools.ttLib import TTFont

font, size, out = sys.argv[1], int(sys.argv[2]), sys.argv[3]
t = TTFont(font)
upm = t['head'].unitsPerEm
hhea = t['hhea']
# Chrome rounds ascent and descent separately
asc = round(hhea.ascent * size / upm)
desc = round(-hhea.descent * size / upm)

src = open(out, encoding='utf-8').read()
src = re.sub(r'\.line_height = -?\d+,', f'.line_height = {asc + desc},', src, count=1)
src = re.sub(r'\.base_line = -?\d+,', f'.base_line = {desc},', src, count=1)
open(out, 'w', encoding='utf-8').write(src)
