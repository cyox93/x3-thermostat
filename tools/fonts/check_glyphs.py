"""Checks that every UI string has its glyphs in the font that draws it
(fallback fonts included). Run after changing a string or build_fonts.sh.
usage: python3 tools/fonts/check_glyphs.py"""
import os
import re
import sys

ROOMS = '거실 큰방 일하는방 노는방'
USES = {  # font → strings drawn with it (docs/ui.md §4, design/README.md)
    'font_pret500_12': '보일러 거큰일노',
    'font_pret500_13': '돌려 조절 · 눌러 확인',
    'font_pret500_14': '버튼을 눌러 켜기',
    'font_pret500_15': '희망온도',
    'font_pret500_17': '돌려 선택 · 눌러 확인',
    'font_pret500_18': '설정 방 선택',
    'font_pret500_20': ROOMS + ' 화면밝기 화면시간',
    'font_pret400_17': '현재 -0123456789.° %',
    'font_pret400_23': '방선택 화면밝기 화면시간 이전',
    'font_pret600_23': '방선택 화면밝기 화면시간 이전',
    'font_pret400_26': ROOMS,
    'font_pret600_26': ROOMS,
    'font_pret600_32': 'ON OFF 설정 이전 확인 ' + ROOMS,
    'font_inter200_24': '%',
    'font_inter200_28': '°',
    'font_inter200_60': 'OFF',
    'font_inter200_64': '0123456789. 초분',
    'font_inter200_72': '0123456789',
    'font_jb400_17': '0123456789% 초분 ' + ROOMS,
    'font_jb400_9': '0123/ OFF',
}
DIR = os.path.join(os.path.dirname(__file__), '../../components/ui/fonts')

def glyphs(name, seen=()):
    src = open(os.path.join(DIR, name + '.c'), encoding='utf-8').read()
    have = {int(h, 16) for h in re.findall(r'/\* U\+([0-9A-F]+) ', src)}
    fb = re.search(r'\.fallback = &(\w+)', src)
    if fb and fb.group(1) not in seen:
        have |= glyphs(fb.group(1), seen + (name,))
    return have

bad = 0
for font, text in USES.items():
    missing = sorted({c for c in text if c != ' ' and ord(c) not in glyphs(font)})
    if missing:
        bad += 1
        print(f'{font}: missing {"".join(missing)}')
print('glyphs ok' if not bad else f'{bad} fonts miss glyphs')
sys.exit(1 if bad else 0)
