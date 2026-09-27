# 하드웨어 — main-board에서 ESP32-S3가 쓰는 핀

> main-board rev A의 헤더 · FFC 배정 기준. 코드의 원본은 `components/include/board.h`다. **핀을 바꾸면 이 문서와 함께 바꾼다.**

## 1. 모듈

| 항목 | 값 |
|---|---|
| 모듈 | **ESP32-S3-WROOM-1-N16R8** |
| Flash | 16 MB, Quad (QIO 80 MHz) |
| PSRAM | 8 MB, **Octal** (80 MHz) — **GPIO 33~37을 점유** |
| 콘솔 · 플래시 | USB-C → 네이티브 USB (IO19 D− / IO20 D+) — **USB Serial/JTAG** |
| 보조 UART | 헤더 J3-1 TX(IO43) · J3-2 RX(IO44) — UART0, 기본 미사용 |
| 스위치 | BOOT = IO0, RESET = EN |
| 보드 LED | WS2812B-0807 ← IO21 |

### 1.1 쓰면 안 되는 핀

| GPIO | 이유 |
|---|---|
| **33 · 34 · 35 · 36 · 37** | Octal PSRAM. 건드리면 PSRAM이 깨진다 |
| 26 ~ 32 | 모듈 내부 Flash / PSRAM (모듈 핀에 나오지 않음) |
| 0 · 3 · 45 · 46 | 스트래핑. IO0은 BOOT 스위치에만 쓴다 |
| 19 · 20 | USB |

## 2. 핀맵

### 2.1 GPIO 순 전체 맵

모듈에 나온 GPIO 전부를 번호 순으로 적는다. 방향은 ESP32-S3 기준(→ 출력, ← 입력).
헤더와 CN1에 **같은 GPIO가 함께 나온 핀**은 같은 네트다 — X3에 연결한 상태에서 헤더로 다른 것을 구동하지 않는다.

| GPIO | 용도 | 방향 | 헤더 | CN1 (main FFC) | `board.h` | 비고 |
|---:|---|:--:|:--:|:--:|---|---|
| 0 | BOOT 스위치 | ← | | | `BOARD_BOOT_PIN` | 스트래핑 |
| 1 | 조도 센서 ALS | ← | J2-12 | | `BOARD_ALS_PIN` | ADC1_CH0 |
| 2 | — | | J2-11 | | | 범용, ADC1_CH1 |
| 3 | — | | | | | 스트래핑, 연결 안 함 |
| 4 | — | | J3-12 | | | 범용, ADC1_CH3 |
| 5 | — | | J3-11 | | | 범용, ADC1_CH4 |
| 6 | — | | J3-10 | | | 범용, ADC1_CH5 |
| 7 | — | | J3-9 | | | 범용, ADC1_CH6 |
| 8 | 불명 신호 (X3 CON1-1) | ← | J3-4 | 1 | `BOARD_UNKNOWN_PIN` | ADC1_CH7, R13 1 kΩ |
| 9 | LCD_DC | → | | 11 | `BOARD_LCD_PIN_DC` | |
| 10 | LCD_CS | → | | 10 | `BOARD_LCD_PIN_CS` | FSPI |
| 11 | LCD_MOSI | → | | 7 | `BOARD_LCD_PIN_MOSI` | FSPI, R8 33 Ω |
| 12 | LCD_SCK | → | | 8 | `BOARD_LCD_PIN_SCLK` | FSPI, R7 33 Ω |
| 13 | x3-switch TX (UART1 RX) | ← | J3-3 | 5 | `BOARD_X3SWITCH_RX_PIN` | 내부 풀업 |
| 14 | LCD 백라이트 + LED Ring EN | → | | 9 | `BOARD_BL_RING_PIN` | LEDC PWM |
| 15 | — | | J3-8 | | | 범용 (ADC2 — WiFi 중 ADC 불가) |
| 16 | — | | J3-7 | | | 범용 (ADC2) |
| 17 | LCD_RES | → | J3-6 | 12 | `BOARD_LCD_PIN_RST` | |
| 18 | x3-switch RX (UART1 TX) | → | J3-5 | 13 | `BOARD_X3SWITCH_TX_PIN` | |
| 19 | USB D− | | | | | USB-C, R3 22 Ω |
| 20 | USB D+ | | | | | USB-C, R4 22 Ω |
| 21 | RGB LED (WS2812B) | → | | | `BOARD_RGB_LED_PIN` | 드라이버 없음 |
| 26~34 | — | | | | | 모듈 내부 (Flash / PSRAM), 핀 없음 |
| 35 · 36 · 37 | — | | | | | **Octal PSRAM** — 연결 안 함 |
| 38 | 엔코더 A | ← | J2-6 | 2 | `BOARD_ENC_A_PIN` | PCNT, R9 1 kΩ |
| 39 | PLC `/SCTL` | → | J2-7 | | `BOARD_PLC_SCTL_PIN` | **오픈드레인 전용** |
| 40 | PLC `DOUT_BUF` | ← | J2-8 | | `BOARD_PLC_DOUT_PIN` | HIGH = 캐리어 |
| 41 | I2C SDA (AHT25) | ↔ | J2-9 | | `BOARD_I2C_SDA_PIN` | 풀업 R14 4.7 kΩ |
| 42 | I2C SCL (AHT25) | → | J2-10 | | `BOARD_I2C_SCL_PIN` | 풀업 R15 4.7 kΩ |
| 43 | UART0 TX | → | J3-1 | | | 기본 미사용 |
| 44 | UART0 RX | ← | J3-2 | | | 기본 미사용 |
| 45 · 46 | — | | | | | 스트래핑, 연결 안 함 |
| 47 | LED Ring 색 | → | J2-4 | 4 | `BOARD_RING_COLOR_PIN` | LOW = red, HIGH = white |
| 48 | 엔코더 B | ← | J2-5 | 3 | `BOARD_ENC_B_PIN` | PCNT, R10 1 kΩ |

전원 핀: J2-1 5V · J2-2 GND · J2-3 3V3, CN1-15 · 16 +5V, CN1-6 · 14 GND.
신호별 상세(부가 소자, 연결 대상)는 아래 §2.2 · §2.3.


### 2.2 main FFC → x3_uart_board → 패널 / x3-switch

| 신호 | GPIO | 방향 | 부가 소자 | 비고 |
|---|---:|:--:|---|---|
| LCD_MOSI | **11** | → | main-board R8 33 Ω | FSPI IOMUX 핀 |
| LCD_SCK | **12** | → | main-board R7 33 Ω | 〃 |
| LCD_CS | **10** | → | | 〃 |
| LCD_DC | **9** | → | | HIGH = data |
| LCD_RES | **17** | → | x3_uart_board 10 kΩ 풀다운 | |
| LCD_BL + RING_EN | **14** | → | x3_uart_board 100 kΩ 풀다운 | LEDC PWM. 공용 선 → [§3.1](#31-lcd-백라이트와-led-ring은-한-선이다) |
| RING_COLOR | **47** | → | | LOW = red, HIGH = white |
| ENC_A | **38** | ← | R9 1 kΩ, 풀업 자리 R11 (DNP) | PCNT |
| ENC_B | **48** | ← | R10 1 kΩ, 풀업 자리 R12 (DNP) | PCNT |
| 불명 (X3 CON1-1) | **8** | ← | R13 1 kΩ, C6 (DNP) | ADC1. 용도 확인 전까지 입력으로만 |
| x3-switch RX (ESP TX) | **18** | → | x3_uart_board 330 Ω | UART1. 회로도 넷 이름 `MM32_RX` |
| x3-switch TX (ESP RX) | **13** | ← | x3_uart_board 330 Ω | UART1. 내부 풀업 사용. 회로도 넷 이름 `MM32_TX` |

main-board CN1 핀 번호는 FFC 도체 번호(= x3_uart_board CN2 핀)와 뒤집혀 있다: `CN1 = 17 − CN2`.

### 2.3 헤더

| 헤더 | 신호 | GPIO | 연결 | 비고 |
|---|---|---:|---|---|
| J2-1 · 2 | 5V · GND | | 외부 12V→5V 레귤레이터, X3 J1-10 · 11 | |
| J2-3 | 3V3 | | AHT25 · ALS VCC | |
| J2-7 | PLC `/SCTL` | **39** | PLC 모듈 J4-2 | **오픈드레인 전용** → [§3.3](#33-plc-모듈) |
| J2-8 | PLC `DOUT_BUF` | **40** | PLC 모듈 J4-3 | HIGH = 캐리어 검출 |
| J2-9 | I2C SDA | **41** | AHT25 | 풀업 R14 4.7 kΩ (X3에서만 실장) |
| J2-10 | I2C SCL | **42** | AHT25 | 풀업 R15 4.7 kΩ (〃) |
| J2-12 | ALS 출력 | **1** | ALS-PT19-315C | ADC1_CH0. 부하 저항은 보드 밖 → [§3.6](#36-조도-센서-als-pt19-315c) |

X3에 쓰지 않는 범용 핀: IO2 · 4 · 5 · 6 · 7 · 15 · 16, TX · RX(IO43 · 44).
디버그용 신호(로직 분석기 트리거 등)는 여기서 가져다 쓴다.

## 3. 신호별 전기적 동작

### 3.1 LCD 백라이트와 LED Ring은 한 선이다

X3 패널의 CON1-9 한 선이 **LCD 백라이트와 LED Ring enable을 함께** 구동한다.

- PWM 듀티 → LCD 밝기. LED Ring은 on/off로만 반응한다 (밝기 조절 안 됨).
- 듀티 0 → **LCD와 Ring 둘 다 꺼진다.** Ring만 끄고 화면을 켜 두는 조합은 없다.
- Ring 색은 IO47로 고른다. Ring이 켜져 있을 때만 의미가 있다.

현재 드라이버(`device/led.c`)는 LEDC 5 kHz · 8 bit, `led_brightness 0~255`.
UI 설계 시 "화면은 켜고 Ring만 끔"이 불가능하다는 점을 전제로 한다.

### 3.2 LCD (GC9A01A)

| 항목 | 값 |
|---|---|
| 버스 | SPI2 (FSPI), mode 0, 쓰기 전용 (MISO 없음) |
| 클럭 | 현재 **40 MHz**. main FFC 길이가 정해진 뒤 파형 보고 조정 (R7 · R8 33 Ω 포함) |
| 픽셀 | RGB565, BGR 순서, 반전(INVON) 필요 |
| 리셋 | IO17 하드웨어 리셋 후 120 ms 대기 |

X3 원본 경로의 120 Ω 댐핑(R32 · R37)은 CON1을 비우므로 경로에서 빠진다.
main-board의 33 Ω가 그 역할을 대신한다.

### 3.3 PLC 모듈

- `/SCTL`(IO39): 모듈 안에 **10 kΩ → +5V 풀업**이 있다. 3.3 V 푸시풀로 구동하면
  U4의 `VIH = 3.5 V`에 못 미치고 풀업을 통해 5 V가 역류한다.
  → `GPIO_MODE_OUTPUT_OD`로만 설정하고 **LOW만 구동, HIGH는 개방**한다. LOW = 송신.
- `DOUT_BUF`(IO40): 모듈 R23 = 20 kΩ로 3.3 V 레벨. **HIGH = 캐리어 검출**.
- J4-1(+5V)은 연결하지 않는다.

### 3.4 엔코더

- 회전 전용, push 없음. PCNT 두 채널로 4배 체배, 글리치 필터 1 µs.
- 한 칸(detent)마다 PCNT limit(= watch point)에 닿아 **ISR 콜백**이 한 번 불린다 — 폴링 없음.
  한 칸 = 카운트 2로 가정했다(`ENCODER_COUNTS_PER_DETENT`, 시제품의 폴링 코드 기준).
  **main-board에서 한 칸당 카운트와 방향 부호를 다시 확인한다.** `encoder_read`는 누적 카운트.
- 패널 쪽에 풀업이 있는지 아직 모른다 → R11 · R12(DNP) 실장 여부는 첫 보드에서 판단.
  그 전까지는 ESP32 내부 풀업으로 시험한다.

### 3.5 AHT25

- ESP32 내부 풀업(~45 kΩ)으로는 케이블 용량을 못 버틴다 → 보드의 4.7 kΩ(R14 · R15) 실장.
- 드라이버 `device/temp_humid.c` — API는 칩을 드러내지 않는다: `sc_temp_humid_measure_trigger()` →
  `SC_TEMP_HUMID_MEASURE_MS`(80 ms) 뒤 `sc_temp_humid_get(&temp, &humi)` (변환 중이면 `ESP_ERR_NOT_FINISHED`).
  I2C 100 kHz, 주소 0x38. 측정 명령 `0xAC 0x33 0x00` → 80 ms 뒤 7바이트,
  CRC-8(0x31) 확인. 전원 인가 후 상태가 `0x18`이 아니면 0x1B · 0x1C · 0x1E 레지스터를 제조사 절차로
  재초기화한다. 센서가 없어도 부팅은 되도록 첫 측정 때 초기화한다. 콘솔 `temp_get`.
- 매립 박스 안은 레귤레이터 · 릴레이 발열로 실온과 다르다. **배치는 미결.**

### 3.6 조도 센서 (ALS-PT19-315C)

방 밝기에 맞춰 LCD 백라이트와 버튼 LED 밝기를 조절하려고 쓴다 — 주목적은 **잘 때 유닛이 방을 밝히지 않게** 하는 것이다 → [architecture.md](architecture.md) §3.5.

```
3V3 (J2-3) ── VCC ┐
                  ALS-PT19-315C
IO1 (J2-12) ◀─────┤ OUT ──┬── R_L ── GND (J2-2)
                          └── C (선택, 노이즈)
```

- 포토트랜지스터라 **출력이 전류**다. 부하 저항 R_L에 걸린 전압을 ADC로 읽는다.
  main-board에는 이 저항 자리가 없으므로 **센서 쪽에 둔다**.
- **ADC1 핀이어야 한다.** ADC2(GPIO11~20)는 WiFi가 켜져 있으면 읽을 수 없다.
  헤더에 나온 ADC1 핀 중 IO1(J2-12)을 골랐다 — J2-2 GND · J2-3 3V3와 같은 열이다.

**lux 환산** — 데이터시트(Everlight DLS-0000039 Rev.8, 25 ℃)

| 항목 | 값 | 조건 |
|---|---|---|
| 광전류 IPH1 | min 5 / typ 15 µA | 100 lx, 백색 형광등 6500K (양산 시험은 백색 LED) |
| 광전류 IPH2 | min 50 / typ **150 µA** | 1000 lx, 같은 광원 → **typ 0.15 µA/lux**, 선형 |
| 광전류 IPH3 | min 140 / typ 520 µA | 1000 lx, 백열등(CIE A 2856K) — IPH2의 약 3.5배 |
| VCE(sat) | max 0.4 V | 3.3 V 전원에서 출력 상한 ≈ 2.9 V |

`lux = V / (R_L × 0.15 µA/lux)`. 개체 편차(min이 typ의 1/3)와 광원에 따른 차이가 있어
**대략적인 lux**다. 밝기 단계를 나누는 용도로는 충분하므로 개체별 보정은 하지 않는다.

**R_L 선택** (typ 감도, 상한 2.85 V 기준 — 감도가 min인 개체면 상한이 3배 올라간다)

| R_L | 감도 | 측정 상한 | 특징 |
|---:|---:|---:|---|
| 10 kΩ | 1.5 mV/lux | 약 1900 lux | 창가 낮까지 포화 없음. 밤(수 lux)은 ADC 잡음 수준이라 거칠다 |
| 33 kΩ | 5 mV/lux | 약 580 lux | 실내 조명 범위를 넓게 씀. 밝은 낮은 포화 |
| **100 kΩ** ✔ | 15 mV/lux | 약 190 lux | 어두운 쪽 분해능 최고. 조명만 켜도 포화 — **채택** |

**100 kΩ로 정했다.** 목적이 밤에 화면 · LED를 어둡게 하는 것이라 필요한 구분은 0~60 lux이고
(architecture.md §3.5), 3 lux 경계가 약 45 mV로 ADC 잡음보다 충분히 크다. 방 조명을 켜면
포화되지만 그건 "밝음"이라는 뜻이면 된다. 암전류(최대 0.1 µA → 10 mV ≈ 0.7 lux)는 3 lux
경계보다 작다.

값을 바꾸면 `board.h`의 `BOARD_ALS_LOAD_OHM`도 바꾼다. 상한을 넘으면 상한 값으로 고정된다.

- 드라이버(`device/als.c`)는 1 ms 간격으로 10회 읽어 평균한다 — 한 주기 10 ms(100 Hz) ·
  8.3 ms(120 Hz)를 덮어야 형광등 · LED 조명의 깜빡임이 평균된다. 연달아 읽으면 1 ms 안에
  끝나 한순간만 보게 된다. eFuse ADC 보정으로 mV를 구한다.
- API: `sc_als_read_mv()`, `sc_als_read_lux(&lux)`. 콘솔 `als_read`(mV · 대략 lux · 포화 여부).
- **센서가 유닛 자신의 빛을 보면 안 된다.** LCD · LED Ring · 버튼 LED 빛이 들어오면
  "밝다 → 더 밝게"로 되먹임이 생긴다. 백라이트와 Ring은 한 선이라 측정 순간만 끄는 것도
  깜빡임 없이는 불가능하다 → **배치와 차광으로 막는다** (미결).

## 4. 전원 주의

- **USB-C만으로는 X3 본체를 구동하지 않는다** (릴레이 코일 + X3 LDO, USB 500 mA 한도).
  USB만 꽂고 개발할 때는 패널(LCD · Ring)까지만 동작한다고 본다.
- X3 후면 PCB의 AC-DC는 **비절연 벅**이라 그 GND가 AC와 결합되어 있다. **AC-DC를 끊지 않은 X3에
  main-board를 연결하지 않는다.** 연결하면 AC 기준 전위가 main-board → PLC 모듈 → PLC 2선을 타고
  다른 방 유닛과 보일러 제어기까지 번지고, 후면 AC-DC와 외부 5V가 같은 5V 네트를 양쪽에서 구동한다.
  - 최소: AC 중성선(ACN)을 결선하지 않는다 — 벅의 리턴 경로가 없어 동작하지 않는다.
  - 확실: 정류부(브리지) 입력을 물리적으로 분리한다.
  - 확인: 개조 후 AC 인가 상태에서 X3 J1-11(GND) ↔ 접지 간 전위를 잰다.
- 오실로스코프 · 디버거를 물릴 때도 같다.
