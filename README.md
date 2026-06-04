# micro:bit v2 BLE Periodic Advertising 예제

마이크로비트 v2 2대로 구성하는 BLE Periodic Broadcasting 예제입니다.

- **broadcaster**: 1초마다 카운터를 1씩 증가시켜 BLE Periodic Advertising으로 송신
- **receiver**: Periodic Advertising에 동기화하여 수신된 카운터를 LED 매트릭스에 표시

## 동작 원리

```
[broadcaster]                        [receiver]
  카운터 증가 (0, 1, 2, ...)
  PA 데이터 업데이트
  Extended Adv + Periodic Adv 송신 ──────────> Extended Adv 스캔
                                               PA 동기화 생성
                                    <──────── PA 수신 (카운터 값)
                                               LED 매트릭스에 숫자 표시
```

### BLE Periodic Advertising 이란?

BLE 5.0의 Periodic Advertising(PA)은 브로드캐스터와 옵저버가 **정확한 타이밍으로 동기화**되어
데이터를 주고받는 방식입니다.

1. **Extended Advertising**: 브로드캐스터가 존재를 알리고 PA 동기화 정보(SID, 인터벌 등)를 포함
2. **PA Sync 생성**: 수신기가 Extended Adv를 발견하면 PA 동기화를 생성
3. **Periodic Advertising**: 동기화 후 설정된 인터벌마다 데이터 수신

## 빌드 및 플래시

### 요구 사항

- nRF Connect SDK (NCS) 2.x 이상
- Zephyr 3.x
- 마이크로비트 v2 × 2대

### broadcaster 빌드

```bash
west build -b bbc_microbit_v2 broadcaster/ --pristine
west flash
```

### receiver 빌드 (다른 마이크로비트 v2에 플래시)

```bash
west build -b bbc_microbit_v2 receiver/ --pristine
west flash
```

## 프로젝트 구조

```
.
├── broadcaster/
│   ├── CMakeLists.txt
│   ├── prj.conf
│   └── src/
│       └── main.c        # 송신기: PA 브로드캐스터
└── receiver/
    ├── CMakeLists.txt
    ├── prj.conf
    └── src/
        └── main.c        # 수신기: PA 동기화 후 숫자 표시
```

## LED 매트릭스 표시

| 장치         | 표시 내용                   |
|--------------|-----------------------------|
| broadcaster  | 현재 송신 중인 카운터 값    |
| receiver     | 수신된 카운터 값            |
| receiver     | `SCAN` - 스캔/재스캔 중     |
| receiver     | `SYNC` - 동기화 완료 메시지 |
| receiver     | `LOST` - 동기화 끊어짐      |

## 시리얼 로그 (115200 baud)

broadcaster:
```
=== BLE Periodic Advertising Broadcaster 시작 ===
Bluetooth 초기화 완료
BLE Periodic Broadcasting 시작! 1초마다 카운터 증가
송신 카운터: 0
송신 카운터: 1
송신 카운터: 2
...
```

receiver:
```
=== BLE Periodic Advertising Receiver 시작 ===
Bluetooth 초기화 완료
PA 브로드캐스터 스캔 중...
PA 브로드캐스터 발견: XX:XX:XX:XX:XX:XX (SID: 0, interval: 48 * 1.25ms)
PA 동기화 생성 중...
Periodic Adv 동기화 완료! 브로드캐스터: XX:XX:XX:XX:XX:XX
수신 카운터: 5  (RSSI: -42 dBm)
수신 카운터: 6  (RSSI: -41 dBm)
...
```

## 데이터 포맷

Periodic Advertising 페이로드:

```
AD Type: 0xFF (Manufacturer Specific Data)
 ├─ Company ID : 0xFFFF (테스트용)
 └─ Counter    : uint32_t (little-endian, 4바이트)
```
