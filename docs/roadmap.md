# 로드맵 — 단계와 진행 상태

> **이 펌웨어의 작업 순서**를 다룬다. 상태: ✅ 완료 · 🔶 진행 중 · ⬜ 미착수.

## 현재 위치 (2026-09-27)

| 항목 | 상태 |
|---|---|
| LCD · LED Ring · 엔코더 드라이버 (임시 배선으로 개별 동작 확인) | ✅ |
| ESP32-S3-WROOM-1-N16R8로 타깃 전환 (16 MB · Octal PSRAM · 파티션 · USB Serial/JTAG) | ✅ 빌드 확인 |
| 핀을 `board.h`로 모으고 main-board 설계 핀으로 교체 | ✅ 빌드 확인, **실기 미확인** |
| 조도 센서 ALS-PT19-315C 드라이버 (`als_read`, 대략 lux, R_L 100 kΩ) | ✅ 빌드 확인, 센서 미연결 |
| 온습도 드라이버 `temp_humid` (AHT25, `temp_get`) | ✅ 실기 확인 (개발 보드, 2026-09-29) |
| x3-switch 드라이버 + switchloop (AT 클라이언트, 상태 동기화, CLICK/LONG) | ✅ 빌드 확인, **실기 미확인** |
| loop 구조 (`main/` mainloop · switchloop · sysloop, eventloop 기반) | ✅ 빌드 확인 |
| 밝기 3단계 (NORMAL / DIM / NIGHT) — 임시 사양 | ✅ 빌드 확인 |
| PLC 파형 규칙 실측 (MC200 송신: 셀 1017 µs, 온타임 PWM, 프레임 = 8비트) | ✅ `docs/plc-protocol.md` |
| PLC 드라이버 (RMT RX/TX) + plcloop | ✅ 실기 수신 · echo 확인 |
| PLC 메시지 해독 — 종류 · 방향 · 주소 · 상태 비트 · 항목 · 모드 · 등록 / 알림 / 원격 설정 순서 (LT200SPD · DT-200 실측) | ✅ `docs/plc-protocol.md` §5 — 일부 항목 미상 |
| PLC 수신 개선 (idle 5 ms, 1024 심볼 버퍼, 그룹 100 ms) | ✅ 실기 메시지 오류 0 |
| main-board rev A · x3_uart_board rev A | 2026-09-15 발주, 수령 대기 |
| x3-switch 펌웨어 AT 규격 0.1.0 | ✅ 구현 · 실측 (K4 리셋 문제 제외) |

## Phase A — main-board bring-up

보드가 도착하면 가장 먼저 한다.

**보드 단품 (전원 넣기 전)**

- [ ] 5V 레일 ↔ GND, 3V3 ↔ GND 절연
- [ ] 헤더 24핀 ↔ 모듈 핀 도통, CN1 핀 ↔ GPIO 도통 (직렬 저항 값 포함) — [hardware.md](hardware.md) §2
- [ ] 외부 5V(J2-1)와 USB-C를 함께 연결했을 때 USB 쪽으로 역류하지 않는지 (D1)
- [ ] main FFC 방향 — main-board CN1-15 · 16과 x3_uart_board CN2-1 · 2(+5V) 도통
- [ ] X3 후면 AC-DC 차단 확인 — [hardware.md](hardware.md) §4

**펌웨어**

- [ ] USB-C만 연결: 3V3, 부팅, USB Serial/JTAG 콘솔, BOOT · RESET
- [ ] PSRAM 8 MB 인식 (`heap_caps_get_total_size(MALLOC_CAP_SPIRAM)`)
- [ ] 보드 RGB LED (IO21, RMT/`led_strip`) — 드라이버 추가
- [ ] 패널을 main FFC로 연결: LCD 초기화 · 색 채우기
- [ ] LED Ring / 백라이트 PWM · 색 전환
- [ ] 엔코더 — 한 칸당 카운트(`ENCODER_COUNTS_PER_DETENT` = 2 가정) · **방향 부호** 확인, 패널 쪽 풀업 유무 → R11 · R12 실장 결정
- [ ] SPI 클럭 · 직렬 저항 — FFC 길이 확정 후 파형으로
- [ ] 불명 신호 IO8 관찰 (ADC 값 로그)
- [x] 온습도(AHT25): `temp_get` 응답 · 값 확인 (개발 보드: 27.0°C / 46.5%RH, sysloop 10 s 주기 측정도 `sysinfo`로 확인)
- [ ] 온습도(AHT25): main-board에서 R14 · R15 실장 상태로 다시 확인
- [ ] 센서 없이 보드만 켤 때는 `bright normal` — 안 하면 20 s 뒤 NIGHT로 화면이 꺼진다
- [ ] 조도 센서 (R_L 100 kΩ): 소등 · 스탠드 · 천장 조명에서 `als_read` 값 기록 → 경계값(3 / 8 / 30 / 60 lux) 확인

## Phase B — x3-switch 링크

코드는 있다 (`components/x3switch/`, `main/switchloop.c`). 실기 확인이 남았다.

- [ ] UART1 트랜잭션 · 100 ms 타임아웃, `x3switch`로 버전 · 상태 조회
- [ ] `+KEY` → CLICK / LONG(1 s) 판정, sysloop 로그 확인
- [ ] 부팅 시 `AT+STATUS?` 채택 — ESP32만 리셋해도 전등이 유지되는지
- [ ] `+READY` 시 상태 재주입 — `x3switch reset` 후 릴레이 · LED가 돌아오는지
- [ ] `relay` · `btn_led` · `btn_led_en` · `btn_led_bri` 콘솔 명령
- [ ] 보드를 뽑았다 꽂기 — 2 s 재시도, 경고 로그가 한 번만 나오는지
- [ ] 연타 · 통보 끼어들기 시험 (x3-switch 쪽 실측과 같은 조건)

## Phase C — 로컬 온도조절기 (네트워크 없이)

- [x] AHT25 10 s · 조도 1 s 주기 측정 — sysloop 타이머 (`sysinfo`로 확인)
- [ ] sysloop 정책 — 목표 온도, 난방 ON/OFF, 키 → 전등 배정, 엔코더 → 목표 온도
- [ ] 방 설정(NVS): 전등 수, 버튼 배정, 방 이름
- [ ] `main/uiloop.c` — LVGL 도입, 현재/목표 온도 · 난방 상태 화면, 엔코더로 목표 온도 조절
- [ ] LED Ring = 난방 동작 표시, 버튼 LED = 채널 상태
- [ ] 목표 온도 · 모드 NVS 저장/복원
- [x] **밝기 3단계** (NORMAL / DIM / NIGHT) — 히스테리시스 · 머무름 시간 · 밤에는 화면 끄고 만지면 10 s 켜짐
- [ ] 밤 실사용 확인 — 잘 때 방이 밝지 않은지, 깨운 화면이 되먹임을 일으키지 않는지
- [ ] 밤에 노브 첫 칸은 화면만 깨우기, 단계 전환 페이드
- [ ] **밝기 사양 재정리** — 실제 구현 시 단계 · 경계값 · 밤 화면 동작을 다시 정한다

## Phase D — PLC (프로젝트 임계 경로)

- [x] 파형 규칙 실측 — 셀 · 심볼 · 프레임 · 그룹 (`docs/plc-protocol.md`)
- [x] 드라이버 `components/plc/` (RMT RX 해독 · RMT TX 오픈드레인) + `main/plcloop.c` (그룹 조립, `plc` · `plc_mon` · `plc_send`)
- [x] **실기 수신**: MC200 폴링이 체크섬 오류 없이 잡힘
- [x] 메시지 틀 · 비트 순서 · 폴링 방 번호 해독
- [x] 자기 송신 echo 확인 (`plc_send` → 자기 수신기에서 그대로 해독, 송신 끝난 뒤 해독되는 것까지 echo로 판정)
- [x] **송신 검증 (벤치)**: MC200 없이 ESP가 MC200 대신 → LT200SPD 등록 · 폴링 · `A5` 원격 설정 · `C5`/`B3` 확인 (`docs/plc-protocol.md` §6.2). 12 V 전원에 470 µH 직렬 필요 (§6.3)
- [x] LT200SPD(방 2 · 방 3) · DT-200을 물리고 조작별 기록 → 메시지 해독 (`docs/plc-protocol.md` §5)
- [ ] 남은 미상 항목 (`plc-protocol.md` §8) — OFF 모드, `CA`의 미상 값, MC200 항목 등
- [ ] 재부팅 · 플래시 뒤 등록이 풀리는 원인 확인 (재부팅 중 IO39 스코프)
- [ ] **방 조절기 역할**: 빈 주소(예: 주소 6)로 등록 → 폴링 응답 · 알림 · 원격 설정, MC200이 받아 주는지 확인
- [ ] 난방 요구 송신 → 보일러 가동 비트 · 밸브 동작 확인
- [ ] 원본 LT200SPD 제거 후 단독 제어 (DT-200과 공존)
- [ ] **거실 조절기 역할**: 주소 1, 방 상태 표(엿듣기), `F5` 원격 설정, 온수 설정 → DT-200 제거

> 해독하지 못하면 온도 제어가 불가능하고 전등 스위치만 남는다.

## Phase E — 네트워크

- [ ] `main/netloop.c` — WiFi (자격 증명은 NVS, 콘솔로 기록), 재연결
- [ ] MQTT — homebridge-mqttthing 규칙에 맞는 전등 set/state 토픽
- [ ] 온도 · 습도 · 난방 상태 publish
- [ ] WiFi 단절 중에도 로컬 조작이 되는지 시험

## Phase F — 마무리

- [ ] OTA (`ota_0` / `ota_1` 파티션은 이미 잡혀 있다)
- [ ] 워치독 · 오류 복구 (x3-switch 무응답, 센서 실패 시 난방 안전 동작)
- [ ] 3대 설치 · 방 설정 기록

## 미결 사항 (펌웨어 관련)

| # | 항목 | 영향 |
|---|---|---|
| 1 | PLC 프로토콜 — 메시지는 해독, 일부 항목 미상 · 조절기는 우리 송신을 받음(벤치), MC200이 받는지 미시험 (`docs/plc-protocol.md` §8) | Phase D |
| 2 | 큰방 버튼 배정 (전등 3개, K3 · K4 외 한 개) | sysloop 방 설정 |
| 3 | K4 → x3-switch 리셋 (하드웨어 수리 필요) | K4를 정책에서 제외 중 |
| 4 | AHT25 배치 (발열) | 온도 보정 필요 여부 |
| 5 | 불명 신호 IO8 | 무시해도 되는지 |
| 6 | 조도 센서 배치 · 차광 — 방을 보면서 유닛 자신의 빛(LCD · Ring · 버튼 LED)은 안 보는 자리 | 자동 밝기의 되먹임 |
| 7 | 조도 센서 부하 저항 — 100 kΩ 채택, 실제 방에서 값 확인 필요 | 밤 경계(3 lux ≈ 45 mV) 분해능 |
| 8 | 난방 판단 방식 (히스테리시스 폭, 최소 ON/OFF 시간) | MC200 쪽 동작 확인 후 결정 |
