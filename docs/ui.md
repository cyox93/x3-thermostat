# UI — 원형 LCD 화면 (LVGL)

> GC9A01A 240×240 원형 LCD에 올리는 보일러 조절 화면의 **펌웨어 쪽 사양**이다.
> 화면 모양 · 수치 · 동작의 원본은 `design/`이다 (`design/README.md`, 기준 소스 `design/src/boiler-app.jsx`).
> 이 문서는 그 디자인을 이 펌웨어의 하드웨어 · loop 구조에 어떻게 얹는지를 정한다.
> 디자인과 이 문서가 다르면 **입력 · 밝기 · 데이터 경로 · 저장은 이 문서**, 모양은 디자인을 따른다.

## 1. 범위

| 이번에 한다 | 하지 않는다 (다음) |
|---|---|
| LVGL 9.5 도입, GC9A01 포트, 테마 · 폰트 · 아이콘 | PLC 방 상태 표 · 조절기 역할 (`plc-protocol.md` §7) |
| 화면 전부: MAIN(켜짐 · 꺼짐 · 마스터), SETTINGS, 방 선택, 화면밝기, 화면시간, 누름 플래시 | on/target 변경의 PLC 송신 |
| UI 상태 머신 (`design/README.md` Interaction Table) | WiFi 신호 실제 값 (레벨은 3 고정) |
| 방 값을 받는 API, 콘솔로 채워 시험 | 전등 정책 (K3 등) |
| 설치 방의 on/target NVS 저장 · 복원 | 보일러 가동 비트(PLC `0x10`)를 배지에 쓰기 |
| PC 시뮬레이터 + golden 픽셀 비교 + 시나리오 재생 (sim · 실기) | |

다른 방의 값은 PLC 엿듣기가 구현되면 그쪽이 같은 API(§3.3)로 넣는다. UI는 값이 어디서 오는지 모른다.

## 2. 하드웨어에 맞춘 결정

### 2.1 입력

디자인은 "인코더 + 푸시 버튼 + (거실) ROOM 버튼"을 가정한다. 실제 X3는 다르다.

| 디자인 입력 | X3에서 | 근거 |
|---|---|---|
| 인코더 1 tick | 엔코더 1 detent (`sc_encoder` 콜백, dir ±1) | |
| 버튼 짧게 | **K1 CLICK** | 엔코더에 push가 없다 (`hardware.md` §3.4) |
| 버튼 길게 | **K1 LONG** — 누른 채 **3 s** | switchloop의 판정 시간 (K1~K4 공통). 디자인은 500 ms, 처음엔 1 s였으나 설정 진입이 너무 쉬워 3 s로 늘렸다 |
| ROOM | **K2 CLICK** — 설치 방이 거실일 때만 | 큰방에서는 K2가 전등 L3이다. 그 밖의 방에서 K2는 기능이 없고, 넘겨도 마스터가 아니라 `app_state`가 무시한다. sysloop은 UI가 알려 주는 설치 방(§3.3 room 콜백)으로 고른다 |
| — | K3 · K4 (큰방은 K2도) | 전등 L1 · L2 (· L3) 토글 — sysloop 정책, UI를 거치지 않는다 (`architecture.md` §5). 화면이 꺼져 있어도 바로 토글하고, **화면을 깨우지 않는다** (사용자 결정 2026-11-10). 화면시간도 다시 세지 않는다 |

### 2.2 밝기

LCD 백라이트는 디자인대로 사용자가 정하고, UI(`ui_router`)가 소유한다.

- 화면밝기 10~100 % (5 단위, 기본 70) → `sc_led_brightness(pct × 255 / 100)` (8-bit LEDC, 10 % = 25).
  선형으로 시작하고 실기에서 눈으로 보고 필요하면 곡선을 넣는다. 0 %는 없다 — LED Ring이 같은 선(IO14)이라
  화면이 켜져 있는 동안 Ring도 켜진다 (CLAUDE.md 하드웨어 함정 2).
- 화면밝기 편집 중에는 값이 바뀔 때마다 백라이트에 바로 반영한다.
- 화면시간 {10, 20, 30, 60, 120, 300 s, 항상} (기본 30, `항상` = 0 = 끄지 않음) — 마지막 입력 후 지나면 듀티 0
  (**LCD와 Ring 둘 다 끈다**). 깨우면 화면밝기 듀티로 돌아간다. 실기(2026-11-10)에서 듀티 1/255는 LCD가 꺼져 보이고
  Ring은 켜진 채였다 — "Ring만 켜 두기"가 가능하지만, 사용자가 Ring도 끄기로 정했다.
- 꺼질 때 0.5 s, 깨울 때 0.2 s에 걸쳐 **스르륵** 바꾼다 (smoothstep, 눈에 보이는 밝기 = 듀티^(1/2.2) 기준).
  Ring은 on/off만 하므로 듀티가 0이 되는 끝에 꺼진다. 화면밝기 편집은 바로 반영한다.
- **화면이 꺼진 상태의 첫 입력(인코더 · K1 · 거실의 K2)은 화면만 켜고 동작하지 않는다.** 전등 키는 깨우지 않는다.
- 화면이 꺼질 때 모드와 편집 중인 값은 그대로 둔다 (MAIN으로 돌아가지 않는다).
- 조도 센서의 3단계(NORMAL / DIM / NIGHT, `architecture.md` §3.5)는 **버튼 LED 밝기에만** 쓴다.
  sysloop은 LCD 백라이트를 만지지 않고, NIGHT에서 화면을 끄는 동작도 없앤다.
- **구현 (2026-11-08):** `ui_router.c`가 편집 중 값마다 `sc_led_brightness()`를 부르고, 편집 화면을 나가면
  NVS `ui_bright`에 저장, 부팅 때 복원한다. 실기에서 100 %(PWM 255)와 IO14 상시 High의 밝기가 같았다 —
  패널의 최대 밝기다.
- **구현 (2026-11-10):** 화면시간도 `ui_router.c` — 입력마다 시각을 남기고 10 ms `_drain`이 지난 시간을 본다.
  편집 화면을 나가면 NVS `ui_stime`에 저장, 부팅 때 복원한다. 버스에서 온 값(다른 방 · 연소)은 화면을 깨우지 않는다.
  조도 되먹임 방지 콜백은 아직 없다.
- **되먹임 방지는 유지한다.** 켜진 화면은 센서를 밝힌다. UI가 화면 on/off를 콜백으로 알리고,
  sysloop은 화면이 켜져 있는 동안 단계를 올리지 않는다 (지금 "깨운 화면" 규칙과 같은 판단, 대상만 바뀐다).

### 2.3 방

디자인의 방 4개를 그대로 쓴다. **번호 순서는 거실 0 · 큰방 1 · 작은방 2 · 일하는방 3**이고,
시험 명령 · JSON에서는 방 이름(한글)을 쓴다. 설치 방이 거실이면 마스터다.
방 번호 → PLC 주소 대응은 PLC 쪽 설정이 정한다 (이번 범위 밖).

## 3. 구조

```
components/
  state/app_state.c     UI 상태의 유일한 원본 — 순수 C, ESP-IDF 의존 없음
  state/app_input.c     상호작용 표(design/README.md) — 입력 하나 → 상태 변경 + 결과(플래시 · 알림)
  settings/settings.c   NVS에 남길 값 (core/config) — device 전용, sim은 stub
  ui/
    sources.cmake       ESP 빌드와 sim이 함께 쓰는 소스 목록
    ui_input.c          입력 → app_input → 누름 플래시                    ┐ sim과 공유
    ui_test.c           시험 명령 해석 · 실행 (RESET · 상태 명령 · DUMP …)  │ (sources.cmake)
    ui_util.c           라벨 · 상자 만들기 도우미                           │
    ui_screens.c        모드 → 화면 고르기, 모든 refresh 호출               │
    screen_main.c screen_settings.c screen_edit_room.c screen_edit_value.c │
    widget_gauge.c widget_focus_list.c widget_flame.c widget_room_summary.c│
    widget_wifi.c widget_flash.c                                          ┘
    fonts/ icons/       lv_font_conv · 이미지 변환 결과 (커밋)
    ui_router.c         device 전용 — LVGL 포트, 큐 수신, 깨우기 · 화면시간 · 백라이트, NVS 저장, 콘솔 명령
  include/
    ui.h app_state.h settings.h
    theme.h             색 토큰 · 폰트 선언 (헤더만)
    ui_geometry.h       좌표 · 반지름 · 각도의 유일한 원본 (design/src/boiler-app.jsx와 쌍)
tools/
  fonts/build_fonts.sh  TTF → components/ui/fonts/font_*.c
  sim/                  SDL2 PC 시뮬레이터 (같은 ui · state 소스 + stubs/)
design/                 디자인 원본 + tools/ (golden 생성 · 비교 · 시나리오 재생)
```

`ui_router.c`에는 하드웨어와 스레드에 묶인 것만 둔다. 입력 해석, 시험 명령처럼 **판단이 들어가는 코드는
공유 파일에 둬서 sim이 실기와 같은 코드를 시험하게 한다.**

### 3.1 규칙

- **화면 하나 = 파일 하나** (`screen_*.c`). 화면은 자기 배치를 갖고 `screen_*_refresh()`로
  app_state를 그린다. refresh는 자기가 활성 화면이 아니면 아무것도 하지 않는다.
- **위젯은 그리기만 한다** (`widget_*.c`). app_state를 구독하지 않고, 입력을 받지 않는다.
- **좌표는 `ui_geometry.h`에만**, 색 · 폰트는 `theme.h`에만 둔다. 화면에 숫자를 직접 쓰지 않는다.
- `app_state` mutator는 스스로 가드한다 (예: MAIN이 아니거나 off면 target 변경은 무시,
  마스터가 아니면 ROOM 무시). 바뀌면 단 하나의 observer(`ui_router`, sim에서는 stub)에 알린다.
- 온도는 `int16_t` ×10 (`225` = 22.5°C) — 그리기 경로에 부동소수를 두지 않는다.

### 3.2 상태의 주인

| 값 | 주인 | 저장 |
|---|---|---|
| mode · 선택 위치 · active_room | `app_state` | 저장 안 함 |
| installed_room · brightness · screen_time | `app_state` (실행 중 원본) | settings → NVS, 편집 화면에서 확인할 때 |
| 방별 on · target | `app_state` — **PLC가 생기기 전까지** | 설치 방만 NVS (§3.4) |
| 방별 current · humidity · burning | 밖에서 들어온 값을 `app_state`가 보관 | 저장 안 함 |
| WiFi 레벨 | `app_state`, 실행 중에는 3 고정 | 저장 안 함 |

settings는 부팅 때 `app_state`에 값을 한 번 넣고, 그 뒤로는 `app_state`가 원본이다.
`ui_router`가 확인 시점에 `settings_set_*()` + `settings_commit()`을 부른다. 보일러 배지는 디자인대로
"`on && burning`인 방이 하나라도 있는가"에서 계산한다.

### 3.3 스레드와 API

`esp_lvgl_port`가 띄우는 LVGL 태스크가 UI의 유일한 스레드다. `app_state`와 LVGL 객체는 이
태스크 안에서만 바뀐다. 다른 loop의 "자기 eventloop 스레드에서만 상태를 바꾼다"는 규칙과 같은 뜻이다.

- 밖에서 부르는 `ui.h` 함수는 메시지를 **값으로 복사해 FreeRTOS 큐에 넣고 바로 돌아온다** (post에 해당,
  큐가 차면 버리고 경고). LVGL 태스크의 10 ms `lv_timer`가 큐를 비우며 처리한다.
  `lv_async_call`은 내부에서 `lv_timer`를 만들기 때문에 다른 스레드에서 lock 없이 부르면 안전하지 않다.
  그래서 쓰지 않는다. 메시지는 고정 크기 구조체라 따로 해제할 것이 없다.
- UI가 밖에 알리는 콜백은 LVGL 태스크에서 불린다. 받는 쪽은 자기 loop로 post만 한다.

```c
/* components/include/ui.h */
typedef enum {
    UI_IN_ENC_CW, UI_IN_ENC_CCW,
    UI_IN_SHORT, UI_IN_LONG, UI_IN_ROOM,
} ui_input_t;

#define UI_TEMP_NONE INT16_MIN
#define UI_HUM_NONE  (-1)

esp_err_t ui_init(void);
void ui_input(ui_input_t in);

/* 이 유닛의 센서 — UI가 그 순간의 설치 방에 넣는다 */
void ui_local_climate_update(int16_t temp_x10, int8_t humidity);
/* 다른 방 (콘솔 · 나중에 PLC) */
void ui_room_climate_update(int room, int16_t temp_x10, int8_t humidity);
void ui_room_burn_update(int room, bool burning);
/* 밖에서 바뀐 on/target (나중에 PLC 원격 설정). 그 방에 보내지 않은 변경이 있으면 무시한다 */
void ui_room_heat_update(int room, bool on, int16_t target_x10);

/* LVGL 태스크에서 불린다 */
typedef void (*ui_heat_callback_t)(int room, bool on, int16_t target_x10, void *arg);
typedef void (*ui_screen_callback_t)(bool on, void *arg);
void ui_register_heat_callback(ui_heat_callback_t cb, void *arg);
void ui_register_screen_callback(ui_screen_callback_t cb, void *arg);
```


**지금 실기에 있는 것 (2026-11-10):** `ui_input()`(큐 16개, 10 ms `lv_timer`가 비운다),
`ui_local_climate_update` · `ui_room_climate_update` · `ui_room_burn_update` · `ui_room_heat_update`,
`ui_register_room_callback(room, heat_on)`(키 배정 · K1 LED), `ui_register_heat_callback(room, on, target)`
(노브가 300 ms 조용해지면 방마다 한 번, 시작할 때 방마다 한 번 — 버스에서 온 변경은 되알리지 않는다).
설치 방 · 화면밝기 · 설치 방의 켜짐/목표 온도는 NVS(`ui_room` · `ui_bright` · `ui_heat`)에 남는다.
보일러 버스와의 연결은 `plc-protocol.md` §7.2.

### 3.4 흐름

```
encoder (ISR) ───────post──▶ sysloop ─ui_input()──────────────┐
K1 · K2 · K3 (switchloop) ─post─▶ sysloop ─ui_input()─────────┤
AHT25 10 s ────────────────▶ sysloop ─ui_local_climate_update()┤  FreeRTOS 큐
콘솔 ui room … ─────────────────────── ui_room_*_update() ───────┘
                                                              ▼
                                              LVGL 태스크 (10 ms lv_timer)
                                                ├─ ui_router: 화면 꺼짐? → 켜기만 하고 끝 (깨우기는 여기만)
                                                ├─ ui_input.c: app_state_*() → observer → screen_*_refresh()
                                                ├─ ui_router: 화면시간 타이머 재시작
                                                ├─ on/target 바뀜 → 300 ms 조용해지면
                                                │     heat 콜백 ─post─▶ sysloop (지금은 로그만)
                                                │     설치 방이면 NVS 저장
                                                └─ 화면 on/off → screen 콜백 ─post─▶ sysloop (조도 되먹임 방지)
```

- 설치 방을 바꾸면 이후 센서 값은 새 방에 들어간다. 전 방은 마지막 값을 그대로 보여 준다
  (다른 출처가 덮어쓸 때까지).
- 부팅 시 설치 방의 on/target은 NVS 값, 없으면 디자인 기본값(on, 22.0)이다. 다른 방은 off · 22.0 ·
  값 없음(`UI_TEMP_NONE`)에서 시작한다. 값이 없는 방의 현재온도 줄과 마커는 그리지 않는다.
- **시험 모드**(`ui test` 첫 명령 이후, 재부팅까지)에서는
  - `ui_*_update()`로 들어오는 바깥 값을 버린다 — 10 s AHT25 값이 시나리오 중간에 끼어들지 않게.
  - **NVS에 쓰지 않는다** — `settings_commit()`과 on/target 저장을 건너뛴다. 시나리오가 설치 방 ·
    밝기 · 목표 온도를 바꾸므로, 그대로 저장하면 시험 뒤 유닛이 시험 값으로 부팅한다.
  - **화면을 켜 둔다** — 화면시간 타이머를 멈추고 `RESET`이 화면을 켠다. 꺼진 화면의 첫 입력은 깨우기만
    하므로, 꺼지면 다음 단계의 `DUMP`가 `expect`와 어긋난다.

### 3.5 콘솔 명령

UI가 소유한 출력은 UI의 명령으로만 바꾼다 (`architecture.md` §3.4). 하나의 `ui` 명령 아래에 둔다.

| 명령 | 동작 |
|---|---|
| `ui` | 모드 · 설치 방 · 밝기 · 화면시간 · 화면 on/off · 방별 값 출력 |
| `ui bright <10-100>` | 화면밝기 설정 (저장하지 않음, 편집 화면과 같은 효과) |
| `ui screen on\|off` | 화면 켜기 · 끄기 |
| `ui room <방이름> temp <°C>\|- hum <%>\|-` | 그 방의 온습도 (`-` = 값 없음) |
| `ui room <방이름> burn 0\|1` | 연소 신호 |
| `ui room <방이름> heat 0\|1 <°C>` | 밖에서 바뀐 on/target (PLC 원격 설정 흉내) |
| `ui test <명령…>` | §6.2 시험 명령 (`CONFIG_SC_UI_TEST` 빌드만) |

## 4. 화면

모양 · 수치는 `design/README.md` "Screens"를 그대로 따른다. 구현 선택만 적는다.

| 항목 | 구현 |
|---|---|
| 화면 전환 | 화면 객체 4개 (MAIN · SETTINGS · EDIT_ROOM · EDIT_VALUE) + `lv_screen_load`. 화면밝기 · 화면시간은 EDIT_VALUE 하나가 모드로 나눈다 |
| 원호 게이지 | 트랙 · 값 `lv_arc` (노브 숨김), 디자인 각 −130°~+130° → LVGL `bg_angles(140, 40)` |
| 현재온도 마커 | `lv_line` (두께 4, 둥근 끝, 반지름 91→109로 원호를 가로지름), 점은 `ui_geometry.h` 값으로 미리 계산. 눈금은 디자인 v2에서 없앴다 — 반지름 ~110 바깥은 X3 창 베젤에 가려진다 |
| 슬롯 목록 (설정 · 방 선택) | `widget_focus_list` — 거리 0/1/2+ → 크기 1.0/0.74/0.55, 투명도 1.0/0.45/0.18, 300 ms ease-out |
| 불꽃 | `design/tools/build-icons.mjs`가 Chrome으로 SVG를 그려 C 배열로 만든다. 크기는 정수로 올림: 14×18 · 10×13 · 9×11. 컬러(14 · 10)는 ARGB8888 한 장, 단색(9, 방 요약)은 A8 + `image_recolor`. 방 이름 앞 · 배지 불꽃은 자리를 늘 차지하고, 연소 중일 때만 보인다(투명도로 감춘다 — `HIDDEN`은 flex에서 빠져 이름이 움직인다). 애니메이션은 `lv_image_set_scale_x/y`로 근사 |
| 방 요약 줄 | 칸마다 불꽃 자리를 항상 잡고 아이콘만 숨긴다 (글자가 움직이지 않게) |
| 누름 플래시 | `lv_layer_top()` 오버레이 하나. 그라데이션 대신 단색 반투명 원, 세리프 이탤릭 대신 Pretendard 600 32 px. 투명도 0→1 (150 ms) → 0 (450 ms) |
| MAIN 값 자리 | 높이 66 고정 컨테이너에 희망온도 줄과 `OFF`를 두고 `HIDDEN`만 바꾼다 — ON/OFF 전환에 다른 요소가 움직이지 않는다 |
| WiFi 상태바 | golden과 같이 아이콘 + `3/3` (레벨 0이면 `OFF`). 아이콘은 레벨별 ARGB8888 4장 (호마다 색 · 투명도가 달라 recolor로는 안 된다). 레벨은 `app_state` 값 |

### 4.1 폰트

`tools/fonts/build_fonts.sh`로 4bpp, **쓰는 글자만** 변환한다. 한글 목록은 `design/README.md`
"Fonts"의 글자 목록이고, 크기 조합은 그 표에 아래를 더한 것이다.

| 폰트 | 크기 | 쓰는 곳 |
|---|---|---|
| (README 표 전부) | | |
| Pretendard 600 | 32 | 누름 플래시 (ON · OFF · 설정 · 이전 · 확인 · 방 이름) |
| Pretendard 500 | 64 | 화면시간 값의 `분` · `초` |
| JetBrains Mono 400 | 17 | 설정 목록 값 (`70%` · `30초`의 숫자 부분) — 한글은 Pretendard 17 |
| JetBrains Mono 400 | 9 | WiFi `3/3` · `OFF` |

400과 600을 모두 넣는다 — 앱 파티션(6 MB)에 여유가 있다. 글자를 더하면 해당 폰트를 다시 만들고
`cmake` configure를 다시 돌린다 (`sources.cmake`의 `file(GLOB)`은 configure 때만 평가된다).

## 5. LVGL · 디스플레이 설정

- **`lvgl/lvgl ^9.5.0` + `espressif/esp_lvgl_port ^2.7.0`** (managed component). `esp_lvgl_port.h`가
  knob · button 헤더를 끌어오므로 `espressif/knob` · `espressif/button`도 의존성에 넣는다 (쓰지는 않는다).
- `device.h`에 `sc_lcd_get_handles(esp_lcd_panel_io_handle_t *io, esp_lcd_panel_handle_t *panel)`를 더해
  기존 `gc9a01.c`의 핸들을 `lvgl_port_add_disp`에 넘긴다. RGB565, `flags.swap_bytes`,
  버퍼는 PSRAM에 1/10 화면 × 2.
- **PSRAM에서 코드 · rodata를 실행하지 않는다** (`SPIRAM_XIP_FROM_PSRAM` 계열 끔).
  LVGL 스타일이 PSRAM에 놓인 rodata를 읽을 때 `LoadProhibited`가 나는 알려진 문제가 있다
  (lvgl#3742). `LV_USE_CLIB_MALLOC`, `LV_USE_ASSERT_STYLE`를 켠다.
- LVGL 9.5의 `LV_SCALE_NONE`은 256이다 (1024 아님) — 위젯 크기가 4배로 보이면 이것부터 본다.
- indev는 쓰지 않는다. 입력은 `ui_input()`으로 상태 머신에 바로 들어간다 — 짧게 · 길게 판정이
  §2.1 표와 정확히 맞아야 하기 때문이다.

## 6. 검증

### 6.1 PC 시뮬레이터 (`tools/sim/`)

같은 `components/ui`(router 제외) · `components/state` 소스를 macOS SDL2로 빌드한다
(`brew install sdl2 cmake ninja`). LVGL 소스는 `managed_components/lvgl__lvgl`을 쓰므로 ESP 빌드를
한 번 먼저 해야 한다. `ui_router.c` 대신 `tools/sim/main.c`가 observer를 걸고 입력을 넣는다.
**화면시간 · 깨우기 · 백라이트는 sim에서 검증되지 않는다.** 색 깊이는 실기와 같은 RGB565
(`LV_COLOR_FORMAT_DEFAULT` RGB565)로 둬서 SNAP을 그대로 비교할 수 있게 한다. 창에는 원 바깥을 가리는
마스크가 있지만 `lv_layer_sys`에 있어 SNAP에는 들어가지 않는다.

| sim 입력 | 뜻 |
|---|---|
| 마우스 휠 · ←/→ | 인코더 ± |
| Space | K1 — 3 s 미만에 떼면 짧게, 3 s가 지나면 길게 (누른 채 발생) |
| Enter | K1 짧게 |
| R | K2 = ROOM |
| TCP 9999 | 아래 시험 명령 (한 연결 = 한 명령 = 한 응답) |

### 6.2 시험 명령

해석과 실행은 `ui_test.c` 하나가 한다 — sim(TCP 9999)과 실기(콘솔 `ui test <명령>`, `CONFIG_SC_UI_TEST`로
켜는 빌드 전용)는 줄을 넘겨주는 통로만 다르다. 하드웨어 입력과 **같은 `ui_input.c` 경로**를 부른다.

- **실행은 항상 UI 스레드에서 한다.** 실기의 콘솔 핸들러는 콘솔 태스크에서 돌므로, 명령 줄을 UI 큐에
  넣고 세마포어로 기다린다. LVGL 태스크가 `ui_test` 실행 · 응답 출력(SNAP의 긴 줄 포함)까지 마치고
  세마포어를 놓는다. sim에서는 SDL · LVGL 메인 루프가 TCP 줄을 받아 그 자리에서 실행한다.
- 명령은 모두 공백으로 나뉜 짧은 줄이다 — 콘솔의 인자 8개 · 256자 제한 안에 들어가므로 실기와 sim이
  같은 명령을 받는다. JSON은 받지 않는다.

| 명령 | 동작 | 응답 | 실기 |
|---|---|---|---|
| `RESET` | `scenarios.json`의 `initial`과 같은 상태 (`ui_test.c`에 고정) | `OK` | ✅ |
| `ENC+` / `ENC-` | 인코더 1 tick | `OK` | ✅ |
| `SHORT` / `LONG` / `ROOM` | 버튼 | `OK` | ✅ |
| `BURN <방이름> <0\|1>` | 연소 신호 (`scenarios.json`의 `{"burn":["큰방",true]}`) | `OK` | ✅ |
| `MODE <mode>` | 모드를 바로 바꾼다 (`DUMP`의 mode 문자열) | `OK` | ✅ |
| `INSTALL <방>` / `ACTIVE <방>` | 설치 방 · 활성 방 | `OK` | ✅ |
| `ROOM <방> <on> <target×10> <cur×10\|-> <hum\|-> <burn>` | 방 하나의 값 전부 (`-` = 값 없음) | `OK` | ✅ |
| `SEL <n>` | 현재 목록 모드의 선택 (설정 0~3, 방 선택 0~3) | `OK` | ✅ |
| `BRIGHT <10-100>` / `STIME <초>` / `WIFI <0-3>` | 화면밝기 · 화면시간 · WiFi 레벨 | `OK` | ✅ |
| `DUMP` | 상태 JSON 한 줄 (아래 형식) | `{...}` | ✅ |
| `SNAP` | 화면 240×240 RGB565 raw, base64 한 줄 (약 150 KB). 실기는 출력 동안 로그를 멈춘다 | `SNAP <base64>` | ✅ |
| `ANIM 0\|1` | 애니메이션 · **누름 플래시** 끄기 · 켜기 (HTML `?test=1`과 같게) | `OK` | ✅ |

`DUMP` 형식 — 시뮬레이터 `getState()`와 같다. 방은 이름으로, 온도는 소수로 쓴다.

```json
{"mode":"main","installedRoom":"거실","activeRoom":"거실","isMaster":true,
 "settingsSel":0,"roomEditSel":"거실","brightness":70,"screenTime":30,
 "rooms":{"거실":{"on":true,"target":22.0,"current":21.4,"humidity":45,"burning":true}, …}}
```

- `mode`: `main` · `settings` · `edit-room` · `edit-brightness` · `edit-screenTime`
- 값이 없는 `current` · `humidity`는 `null`
- JSON은 손으로 쓴다 (작고 고정된 형식)

### 6.3 golden 항목 → 시험 명령 (`render-sim.mjs`)

`npm run golden`이 쓰는 `states.json`은 `design/src/golden-states.jsx`의 `GOLDEN` 배열이다.
온습도는 항목에 없고 그 파일의 상수 `TEMPS` · `HUMS`다. `render-sim.mjs`가 항목마다 `RESET` 뒤
§6.2의 상태 명령으로 이렇게 옮긴다.

| `kind` | 상태 |
|---|---|
| `main` | mode MAIN. `summary`가 있으면 설치 방 = 거실(마스터), 없으면 설치 방 = `room`. 활성 방 = `room`. 그 방의 on · target · burning은 항목 값, current · humidity는 `TEMPS` · `HUMS` (`noHum`이면 없음). `summary`가 있으면 각 방의 on · burning을 그 값으로. `wifi` → WiFi 레벨 (없으면 3) |
| `settings` | mode SETTINGS, 선택 = `sel`, 설치 방 = `settings.roomName`, brightness · screenTime = `settings` 값 |
| `room` | mode EDIT_ROOM, 선택 = `sel` |
| `bright` | mode EDIT_BRIGHTNESS, 값 = `value` |
| `time` | mode EDIT_SCREENTIME, 값 = `value` (초) |

항목마다 `RESET`부터 하므로 앞 항목의 값이 다음 항목에 남지 않는다. 항목에 없는 방은 `RESET` 값
그대로다. `TEMPS` · `HUMS`는 `render-sim.mjs`와 `ui_test.c`(RESET)에 같은 값으로 둔다.
애니메이션은 끈 상태로 찍는다.

### 6.4 비교 절차

`design/TESTING.md`를 따른다. 스크립트는 모두 `design/tools/`에 둔다.

1. `npm run golden` → `golden/*.png` 29장 + `golden/states.json`
2. `render-sim.mjs`: sim에 항목마다 `ANIM 0` · `RESET` · 상태 명령 · `SNAP` → `out/lvgl/<id>.png` → `npm run diff`
   (원 안쪽 3.5% 초과면 FAIL — 아래)
3. `scenarios.json`을 HTML(`npm run replay`) · sim(`npm run replay:sim`) · 실기(`replay-serial.mjs`)에 재생해
   `DUMP`를 `expect`와 비교하고, 화면은 HTML 결과와 비교한다 (`npm run diff:replay`)

판단 기준은 "위치 · 크기 · 색이 맞는가"다. 글자 테두리만 빨가면 정상, 덩어리로 빨가면 틀렸다.

**한도 3.5%의 근거 (2026-10-28, sim).** 29장 중 16장은 2% 아래, 13장(편집 화면 · 마스터 MAIN ·
방 선택 1장)이 2.1~3.11%였다. 13장 모두 요소 위치는 1~3 px 안이고 빨간 곳은 글자 획을 따라서만 나온다 —
Chrome이 Inter Tight · Pretendard 획을 더 굵게 그린다. 글리프 알파에 감마(0.75 · 0.6)를 줘도 13장이
그대로 2%를 넘어 농도가 아니라 획 폭 차이로 본다. 한도를 넘는 프레임이 생기면 diff 이미지부터 본다.

**디자인 v2 (2026-11-08, sim).** 밝은 토큰, Inter Tight 300, 두꺼운 원호(7 · 8), 눈금 삭제, 방 이름 앞
불꽃, 고정 높이 값 자리로 바꾼 뒤 golden을 다시 만들었다. 29장 모두 통과(최대 2.71%, 화면시간),
`scenarios.json`은 HTML · sim 모두 통과, 단계 화면 19장 통과.

**맞추면서 알게 된 것.** `lv_font_conv`는 변환한 글리프로만 줄 높이를 잡아 CSS `line-height: normal`보다
낮다 → `tools/fonts/css_metrics.py`가 hhea 값으로 고쳐 쓴다. CSS `letter-spacing`은 마지막 글자 뒤에도
붙는다 → 양수 자간 라벨은 오른쪽 패딩으로 그 칸을 둔다. 그 밖의 1 px 보정은 `ui_geometry.h`에 이유와 함께 있다.
`design/tools/golden/` · `out/` · `node_modules/`는 `.gitignore`에 넣는다.

### 6.5 실기에서만 보는 것

- 화면시간이 지나면 백라이트 · Ring이 꺼지고, 첫 입력은 깨우기만 하는지
- 화면밝기 10 %와 100 %가 눈으로 구분되는지
- K1 길게(3 s)가 SETTINGS로 들어가고 뗄 때 짧게가 따로 나오지 않는지
- 엔코더 연속 회전에 화면이 밀리지 않는지, heat 콜백이 300 ms 뒤 한 번만 오는지
- 설치 방의 on/target이 재부팅 뒤 돌아오는지
- 어두운 방에서 화면을 켜도 버튼 LED 단계가 올라가지 않는지 (§2.2 되먹임 방지)

## 7. 기존 코드 · 문서 정리

| 대상 | 바꿀 것 |
|---|---|
| `main/sysloop.c` | 밝기 표의 `lcd` · `screen_sleep`, `_SCREEN_WAKE_MS` · `SYSLOOP_SCREEN_TIMER_ID` · `_screen_touch` · `_screen_apply` 삭제. 키 · 엔코더 · AHT25를 `ui_*`로 넘기고, heat · screen 콜백을 받는다. `bright` 명령 도움말에서 "display" 삭제 |
| `main/main.h` | `sc_sysinfo_t.screen_on`은 UI가 알려 준 값으로 (sysinfo 표시용) |
| `components/device/led.c` | `led_on` · `led_brightness` 콘솔 명령 삭제 — 백라이트는 UI가 소유한다 (출력은 소유자의 명령으로만). 대신 `ui bright` · `ui screen` (§3.5) |
| `components/device/gc9a01.c` | `lcd_color` · `lcd_id`는 bring-up용으로 남기고, LVGL이 다음 갱신에서 덮는다고 도움말에 적는다 |
| `components/core/console.c` | 그대로 (시험 명령은 모두 짧은 줄이라 줄 길이를 늘리지 않는다) |
| `docs/architecture.md` §3 · §3.5, CLAUDE.md 하드웨어 함정 7, `README.md` 명령 목록, `docs/roadmap.md` `uiloop.c` 항목 | 위 변경에 맞춘다 |

## 8. 순서

| 단계 | 내용 | 끝난 기준 |
|---|---|---|
| 1 | LVGL · 포트 · theme · 폰트 · `ui_geometry.h`, MAIN 화면, sim + TCP 시험 명령 (`RESET` · 상태 명령 · `ANIM` · `SNAP`) · `render-sim.mjs` | MAIN 계열 golden PASS (sim), 실기에 MAIN 표시 |
| 2 | SETTINGS · EDIT 화면, 플래시, 마스터 요소, `app_input.c` 상태 머신 전부, 나머지 시험 명령 · `DUMP` · `replay-sim.mjs` | golden 29장 PASS, `scenarios.json` sim 통과 |
| 3 | 실기 `ui test` · `ui` 콘솔 명령, `replay-serial.mjs`, sysloop 연결, NVS, 화면시간 · 백라이트, §7 정리 | `scenarios.json` 실기 통과, §6.5 실기 확인 |
