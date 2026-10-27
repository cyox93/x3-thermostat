# Visual & Behavior Verification — x3 thermostat

HTML 시뮬레이터와 LVGL 구현을 비교하는 방법은 두 가지입니다.

1. **화면 비교 (pixel diff):** 같은 상태를 240×240 PNG로 만들어 픽셀 단위로 비교합니다.
2. **동작 비교 (scenario replay):** 같은 입력 순서를 HTML(Chrome)과 펌웨어에 똑같이 넣고, 상태값과 화면을 비교합니다.

```
tools/
  package.json          npm scripts
  capture-golden.mjs    golden.html → golden/*.png  (Headless Chrome, DPR 1)
  compare.mjs           golden/*.png vs out/lvgl/*.png → out/diff/*.png + PASS/FAIL
  scenarios.json        입력 시나리오 + 기대 상태 (HTML과 펌웨어가 같은 파일 사용)
  replay-html.mjs       scenarios.json → Chrome에서 index.html?test=1 재생
golden.html             기준 상태 29개를 240×240 원본 크기로 렌더 (?state=<id>)
```

## 0. 준비
```bash
cd tools && npm i
npm run serve        # 프로젝트 루트를 http://localhost:8080 으로 서빙 (file:// 에서는 JSX 로드 불가)
```
`golden.html`을 파라미터 없이 열면 전체 갤러리가 보입니다. `?state=main-on`처럼 지정하면 한 장만 (0,0)에 240×240으로 렌더합니다.

## 1. 기준(golden) 이미지 만들기
```bash
npm run golden       # → tools/golden/<id>.png  + golden/states.json
```
- 애니메이션은 모두 끕니다. 불꽃은 정지 프레임, 목록은 이동이 끝난 위치로 찍힙니다.
- 밝기 설정은 이미지에 반영하지 않습니다. 실제 기기에서는 백라이트로 조절하므로 프레임버퍼 값이 같아야 합니다.
- `golden/states.json`에 각 이미지의 입력값(방, on, target, burning, summary, wifi, 습도 생략 여부 등)이 들어 있습니다. LVGL 쪽에서 같은 상태를 만들 때 이 파일을 그대로 읽으면 됩니다.

## 2. LVGL 쪽 프레임 만들기
권장 방식은 **LVGL PC 시뮬레이터(SDL)** 빌드입니다. 같은 UI 코드를 PC에서 돌리면 빠르게 반복할 수 있습니다.
- 디스플레이 240×240, 색 깊이는 실제 기기와 같게 (보통 RGB565)
- `ui_apply_state(json)` 함수: `states.json`의 항목 하나를 UI 상태에 그대로 넣고 화면을 다시 그립니다.
- `lv_snapshot_take(lv_scr_act(), LV_COLOR_FORMAT_RGB888)` → PNG로 저장 (`out/lvgl/<id>.png`)
- 애니메이션은 끄거나 끝난 상태로 바로 이동시킵니다 (`lv_anim_del_all()` 후 최종값 설정).

실제 기기로도 확인하려면 아래 **4번 시리얼 테스트 프로토콜**의 `SNAP` 명령으로 프레임버퍼를 받아 PNG로 저장합니다.

## 3. 비교
```bash
npm run diff                                    # 기본: threshold 0.1, 원 안쪽 2% 초과 시 FAIL
node compare.mjs golden out/lvgl out/diff --threshold 0.15 --max 3
```
- 원 밖(반지름 120 바깥)은 비교하지 않습니다.
- `out/diff/<id>.png`에서 빨간 픽셀이 다른 곳입니다.

### 완전히 똑같을 수는 없습니다
- **글자:** Chrome(FreeType, 서브픽셀 위치)과 `lv_font_conv` 비트맵(4bpp, 정수 위치)은 안티앨리어싱이 다릅니다. 글자 가장자리에서 1px 정도 차이는 정상입니다.
- **원호·선:** 브라우저 SVG와 LVGL draw의 안티앨리어싱이 다릅니다.
- **색:** RGB565로 바꾸면 색이 약간 달라집니다. golden을 RGB565로 한 번 양자화한 뒤 비교하면 더 공정합니다.
- 그래서 판단 기준은 **"위치·크기·색이 맞는가"** 입니다. 처음엔 `--max 2~3%`로 시작해서 diff 이미지를 보고 조정하세요. 큰 덩어리로 빨갛게 나오면 위치나 크기가 틀린 것이고, 글자 테두리만 빨갛다면 정상입니다.

## 4. 동작 비교 — 같은 시나리오를 양쪽에 재생

### HTML (Chrome)
```bash
npm run replay       # 시나리오마다 상태 검증 + out/html/<scenario>-<snap>.png
```
`index.html?test=1`은 누름 플래시와 밝기 흐림을 끈 상태로 열립니다. `window.__thermo`로 조작합니다:
- `dispatch('enc+' | 'enc-' | 'short' | 'long' | 'room' | {burn:[room, bool]})`
- `getState()` → `{ mode, installedRoom, activeRoom, isMaster, settingsSel, roomEditSel, brightness, screenTime, rooms:{ 거실:{on,target,current,humidity,burning}, ... } }`

### 펌웨어 — 시리얼 테스트 프로토콜 (구현 필요)
테스트 빌드(`CONFIG_UI_TEST=y`)에서 UART로 한 줄씩 명령을 받습니다. 하드웨어 입력과 **같은 상태 머신 함수**를 호출해야 의미가 있습니다.

| 명령 | 동작 | 응답 |
|---|---|---|
| `RESET` | `scenarios.json`의 `initial` 상태로 초기화 | `OK` |
| `ENC+` / `ENC-` | 인코더 1 tick | `OK` |
| `SHORT` / `LONG` / `ROOM` | 버튼 이벤트 | `OK` |
| `BURN <room_idx> <0|1>` | 연소 신호 주입 | `OK` |
| `DUMP` | 현재 상태를 위 `getState()`와 **같은 키 이름**의 JSON 한 줄로 | `{...}` |
| `SNAP` | `lv_snapshot` → RGB565 240×240 raw를 base64로 | `SNAP <base64>` |
| `ANIM 0` | 애니메이션 끄기 (테스트 중 권장) | `OK` |

PC 쪽에 `replay-serial.mjs`(Claude Code가 작성)를 두고, 같은 `scenarios.json`을 재생하면서 `DUMP` 결과를 `expect`와 비교하고, `SNAP` 결과는 `out/lvgl-replay/<scenario>-<snap>.png`로 저장합니다. 그 다음 아래처럼 비교합니다:
```bash
node compare.mjs out/html out/lvgl-replay out/diff-replay
```
이렇게 하면 같은 입력에 대해 **상태값**과 **화면**이 HTML과 펌웨어에서 둘 다 일치하는지 확인할 수 있습니다.

## 5. Claude Code에 줄 작업 지시 예시
> `design_handoff_x3_thermostat/README.md`대로 LVGL UI를 구현해 줘. 먼저 LVGL PC 시뮬레이터 타깃을 만들고, `tools/golden/states.json`의 모든 상태를 렌더해서 `out/lvgl/`에 PNG로 저장해. 그다음 `npm run diff`가 전부 PASS할 때까지 수정해. 마지막으로 `TESTING.md` 4번의 시리얼 테스트 프로토콜과 `replay-serial.mjs`를 구현해서 `scenarios.json`이 HTML과 펌웨어 양쪽에서 통과하는지 확인해.
