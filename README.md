# x3-thermostat — ESP32-S3 main-board firmware

X3 Thermostat의 **main-board rev A (ESP32-S3-WROOM-1-N16R8)** 펌웨어.
Tuya X3 노브 스위치를 **온도조절기(거실 DT-200 · 각방 LT200SPD 대체) 겸 전등 스위치**로 바꾸는 프로젝트에서
두뇌 역할을 한다 — LCD · 엔코더 · LED Ring 구동, AHT25 측정, 조도에 따른 화면 · LED 밝기 조절, PLC로 보일러(MC200) 제어,
WiFi/MQTT로 전등 연동, 그리고 X3 안의 x3-switch 보드(별도 저장소 `x3-switch`)에 AT 명령으로 버튼 · LED · 릴레이를 맡긴다.

## 문서

| 문서 | 내용 |
|---|---|
| [docs/architecture.md](docs/architecture.md) | 역할 분담, 소프트웨어 구조, 이벤트 흐름, x3-switch 링크 |
| [docs/hardware.md](docs/hardware.md) | main-board 핀맵, 모듈 제약, 신호별 전기적 동작 |
| [docs/roadmap.md](docs/roadmap.md) | 단계별 작업과 진행 상태 |

## 환경

| | |
|---|---|
| ESP-IDF | **v6.0** (`~/project/esp/v6.0/esp-idf`) |
| 타깃 | `esp32s3` — Flash 16 MB (QIO), Octal PSRAM 8 MB |
| 파티션 | `partitions.csv` — nvs · otadata · phy · ota_0 6 MB · ota_1 6 MB · storage ~3.9 MB |
| 콘솔 | USB-C (USB Serial/JTAG) |
| 추가 컴포넌트 | `$IDF_PATH/examples/system/console/advanced/components` (`cmd_system`, `cmd_nvs`) |

## 빌드 · 플래시

```sh
source ~/.espressif/tools/activate_idf_v6.0.sh   # 또는 . $IDF_PATH/export.sh

idf.py build
idf.py -p /dev/cu.usbmodem* flash monitor
```

`sdkconfig`는 `sdkconfig.defaults`에서 생성된다. 설정을 바꿀 때는 `sdkconfig.defaults`를
고치고 `sdkconfig`를 지운 뒤 다시 빌드한다 (`idf.py menuconfig`로 바꾼 것은 커밋 전에
defaults로 옮긴다).

## 콘솔 명령

| 명령 | 동작 |
|---|---|
| `lcd_color <r> <g> <b>` | LCD 전체를 한 색으로 |
| `led_on <on\|off>` | LED Ring + 백라이트 on/off (마지막 밝기 복원) |
| `led_brightness <0-255>` | 백라이트 PWM 듀티 (Ring은 on/off로만 반응) |
| `led_color <red\|white>` | LED Ring 색 |
| `encoder_read` · `encoder_clear` | 엔코더 카운트 |
| `als_read` | 조도 — mV, 대략 lux, 포화 여부 |
| `temp_get` | 온도 · 습도 |
| `gpio_mode` · `gpio_get` · `gpio_set` | 임의 GPIO 확인 · 설정 |
| `relay [<1-4> <on\|off>]` | 릴레이 상태 / 설정 (switchloop) |
| `btn_led [<1-4> <white\|orange\|blink> [ms]]` | 버튼 LED 상태 / 설정 (switchloop) |
| `btn_led_en <on\|off>` · `btn_led_bri <0-100>` | 버튼 LED 전체 on/off · 밝기 (switchloop) |
| `x3switch [reset]` | x3-switch 버전 · 보드가 보고하는 전체 상태 / 소프트 리셋 |
| `bright [auto\|night\|dim\|normal]` | 밝기 단계 조회 / 강제 (센서 없는 책상에서는 `normal`) |
| `plc` | PLC 마지막 그룹 · 수신 / 오류 / 송신 카운트 (plcloop) |
| `plc_mon <on\|off>` | PLC 프레임마다 출력 |
| `plc_send <hex>...` | 바이트마다 PLC 프레임 송신, 예 `plc_send 83 01 04` (plcloop). 실제 버스에 나간다 |
| `plc_mc200 [on\|off [<주소>]]` | 벤치 빌드(`sdkconfig.bench`)에만 있다: MC200 대신 조절기 하나를 폴링하고 답한다 (`docs/plc-protocol.md` §6.4) |
| `sysinfo` | sysloop이 가진 온습도 · 조도 · x3-switch 상태 |
| `get_ver` | 펌웨어 버전 |
| `help` | 전체 목록 (시스템 명령 포함) |

## 안전

> ⚠️ X3 후면 PCB의 AC-DC는 **비절연**이다. AC-DC를 끊지 않은 X3에 main-board · PC ·
> 계측기를 연결하지 않는다. 이유와 차단 방법은 [docs/hardware.md](docs/hardware.md) §4.
