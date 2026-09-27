# 문서

X3 Thermostat의 **main-board(ESP32-S3-WROOM-1-N16R8) 펌웨어** 문서. 이 펌웨어가
구현해야 하는 범위와, 그에 필요한 하드웨어 사실을 둔다.

| 문서 | 내용 | 상태 |
|---|---|---|
| [architecture.md](architecture.md) | 시스템 안에서 ESP32-S3의 역할, 책임 분할, 소프트웨어 구조 | ✅ 초안 |
| [hardware.md](hardware.md) | main-board 핀맵, 모듈 제약, 패널 신호의 전기적 동작 | ✅ 초안 |
| [roadmap.md](roadmap.md) | 단계별 작업 목록과 진행 상태, 미결 사항 | ✅ 초안 |

**함께 보는 문서**

| 문서 | 위치 |
|---|---|
| ESP32-S3 ↔ x3-switch AT 명령 규격 | x3-switch 저장소의 `docs/at-protocol.md` — **x3-switch 저장소가 원본**이다. 이 저장소는 그 규격의 클라이언트다 |
| x3-switch 보드의 핀과 실측 함정 (K4 리셋 등) | x3-switch 저장소의 `docs/hardware.md` |

> 빌드 · 플래시 · 콘솔 사용법은 저장소 루트의 `README.md`에 있다.
