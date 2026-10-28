#!/bin/bash
# Downloads the design's fonts (all SIL OFL) into tools/fonts/src/ (gitignored)
# and instances the variable fonts at the weights the design uses.
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p src && cd src

PRET=v1.3.9
[ -f Pretendard-Medium.otf ] || {
  curl -fL -o pretendard.zip "https://github.com/orioncactus/pretendard/releases/download/${PRET}/Pretendard-${PRET#v}.zip"
  unzip -j -o pretendard.zip 'public/static/Pretendard-Regular.otf' 'public/static/Pretendard-Medium.otf' 'public/static/Pretendard-SemiBold.otf'
  rm pretendard.zip
}
[ -f InterTight-VF.ttf ] || curl -fL -o InterTight-VF.ttf "https://github.com/google/fonts/raw/main/ofl/intertight/InterTight%5Bwght%5D.ttf"
[ -f JetBrainsMono-VF.ttf ] || curl -fL -o JetBrainsMono-VF.ttf "https://github.com/google/fonts/raw/main/ofl/jetbrainsmono/JetBrainsMono%5Bwght%5D.ttf"

PY=${IDF_PYTHON_ENV_PATH:-$HOME/.espressif/tools/python/v6.0/venv}/bin/python
"$PY" - <<'PY'
from fontTools.ttLib import TTFont
from fontTools.varLib.instancer import instantiateVariableFont
instantiateVariableFont(TTFont('InterTight-VF.ttf'), {'wght': 200}).save('InterTight-ExtraLight.ttf')
instantiateVariableFont(TTFont('JetBrainsMono-VF.ttf'), {'wght': 400}).save('JetBrainsMono-Regular.ttf')
PY
ls -1
