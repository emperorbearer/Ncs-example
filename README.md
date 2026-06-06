# micro:bit v2 NCS 예제 모음

각 폴더는 독립적인 nRF Connect SDK 애플리케이션입니다.  
빌드 아티팩트(`build/`)는 각 프로젝트 폴더 안에 생성되므로 예제끼리 영향을 주지 않습니다.

| 폴더 | 설명 |
|------|------|
| `broadcaster/` | BLE Periodic Advertising 송신기 — 카운터를 1초마다 PA로 브로드캐스트 |
| `receiver/` | BLE Periodic Advertising 수신기 — PA 동기화 후 수신 카운터를 LED에 표시 |
| `microbit-binary-clock/` | BLE 시간 동기화 Binary Clock — 1초 단위 이진 시계, 버튼 A/B 기능 포함 |

---

## microbit-binary-clock

### 기능

- **Binary Clock**: 5×5 LED에 시/분/초를 이진수로 1초마다 표시
- **BLE**: `MBit-Clock`으로 광고 → 핸드폰에서 시간을 3바이트로 전송해 동기화
- **버튼 A**: 현재 시간 저장 (시리얼 출력)
- **버튼 B**: 하트 패턴 3초 표시

### LED 배치

```
row 0: ■ ■ ■ ■ ■   시(Hour)       — 5비트  (0~23)
row 1: · · ■ ■ ■   분 십의 자리   — 3비트  (0~5)
row 2: · ■ ■ ■ ■   분 일의 자리   — 4비트  (0~9)
row 3: · · ■ ■ ■   초 십의 자리   — 3비트  (0~5)
row 4: · ■ ■ ■ ■   초 일의 자리   — 4비트  (0~9)
MSB → 왼쪽, LSB → 오른쪽
```

예) **15:23:47**
```
row 0 (15 = 01111): · * * * *
row 1 ( 2 =   010): · · · * ·
row 2 ( 3 =  0011): · · · * *
row 3 ( 4 =   100): · · * · ·
row 4 ( 7 =  0111): · · * * *
```

### BLE 시간 설정 (nRF Connect 앱)

1. `MBit-Clock` 검색 후 연결
2. Custom Service `AB120001-...` 선택
3. Time Characteristic `AB120002-...` 에 **3바이트** Write:
   ```
   [시(0x00~0x17), 분(0x00~0x3B), 초(0x00~0x3B)]
   예) 15:30:00 → 0F 1E 00
   ```

### 빌드 & 플래시

```bash
west build -b bbc_microbit_v2 microbit-binary-clock/ --pristine
west flash
```

---

## BLE Periodic Advertising 예제 (broadcaster / receiver)

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

각 프로젝트는 독립 폴더로, 빌드 시 `build/` 가 해당 폴더 안에만 생성됩니다.

```
.
├── broadcaster/                  # PA 송신기
│   ├── CMakeLists.txt
│   ├── prj.conf
│   └── src/main.c
├── receiver/                     # PA 수신기
│   ├── CMakeLists.txt
│   ├── prj.conf
│   └── src/main.c
└── microbit-binary-clock/        # BLE Binary Clock
    ├── CMakeLists.txt
    ├── prj.conf
    └── src/main.c
```

> 새로운 예제를 추가할 때는 루트에 새 폴더를 만들고,
> 그 안에 `CMakeLists.txt`, `prj.conf`, `src/main.c` 를 작성하면 됩니다.
> 기존 예제에는 아무런 영향을 주지 않습니다.

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
