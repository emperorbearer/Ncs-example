# Thing Plus Matter — CoAP over Thread 전등 스위치

SparkFun Thing Plus Matter(MGM240P) 보드의 GPIO에 65mm 오락실 버튼을 연결하고,
버튼이 눌리면 **CoAP over Thread**로 신호를 보내 **Matter 전등**을 켜고 끄는 예제입니다.

## 동작 구조

```
[Thing Plus Matter + 65mm 버튼]          [OTBR 호스트 (라즈베리파이 등)]        [Matter 전등]
  버튼 눌림 (PB00, 인터럽트)
  CoAP NON PUT /matter-light  ──Thread──>  coap_matter_bridge.py
  payload: "toggle"                          └─ chip-tool onoff toggle ──Matter──> 전등 토글
```

> **왜 브리지가 필요한가?**
> Matter over Thread 기기는 UDP 위에서 Matter 전용 보안 세션(CASE/PASE) 프로토콜을 쓰기
> 때문에 일반 CoAP 메시지로 직접 제어할 수 없습니다. 그래서 버튼 보드는 표준 CoAP를
> Thread 메시로 전송하고, Border Router 호스트에서 돌아가는 작은 브리지가 이를 받아
> Matter 컨트롤러(`chip-tool`)로 전등을 제어합니다.

## 준비물

- SparkFun Thing Plus Matter — MGM240P (DEV-20270)
- 65mm 오락실 버튼 (마이크로스위치 내장형)
- OpenThread Border Router(OTBR)가 동작 중인 호스트 (예: 라즈베리파이 + RCP 동글)
- 같은 OTBR로 커미셔닝된 Matter over Thread 전등 + `chip-tool`

## 하드웨어 배선

| 오락실 버튼 단자 | 보드 |
|---|---|
| 마이크로스위치 **COM** | **GND** |
| 마이크로스위치 **NO** (Normally Open) | **PB00** |

- 펌웨어가 PB00에 **내부 풀업**을 켜고 **active-low**(누르면 LOW)로 읽으므로 외부 저항이 필요 없습니다.
- 버튼에 LED가 내장된 경우: 12V용 LED 모듈은 보드에 직접 연결하지 마세요. 5V형은 USB의
  VUSB, LED만 따로 3.3V로 구동 가능한 경우에만 3V3 핀을 사용하세요.
- 다른 핀을 쓰려면 `boards/sparkfun_thing_plus_matter_mgm240p.overlay`에서
  `gpios = <&gpiob 0 ...>`의 포트/핀만 바꾸면 됩니다.
  (참고: Silicon Labs Arduino 코어 기준 PB00은 `A4`로 매핑되는 핀입니다.
  보드 실크 라벨 ↔ PB00 위치는 SparkFun 후크업 가이드의 그래픽 데이터시트를 확인하세요.)

## 빌드

이 예제는 보드/라디오 드라이버가 업스트림에 있는 **vanilla Zephyr(4.2 이상)** 로 빌드합니다.
(NCS의 west 매니페스트에는 Silicon Labs HAL이 포함되지 않아 NCS로는 빌드할 수 없습니다.)

```bash
# Zephyr 워크스페이스에서 (최초 1회) Silabs 라디오 바이너리 블롭 다운로드
west blobs fetch silabs

# 빌드 & 플래시 (온보드 J-Link 사용)
west build -b sparkfun_thing_plus_matter_mgm240p thingplus-coap-light-switch/ --pristine
west flash
```

> 보드의 J-Link 펌웨어가 오래됐다면 Simplicity Studio에서 한 번 업데이트해야 할 수 있습니다.

## 1) Thread 네트워크 조인

OTBR 호스트에서 현재 네트워크의 dataset을 가져옵니다.

```bash
sudo ot-ctl dataset active -x
# 예: 0e080000000000010000...
```

보드 시리얼 콘솔(115200 baud)에서:

```
ot dataset set active 0e080000000000010000...   # 위에서 복사한 hex
ot ifconfig up
ot thread start
ot state          # child 또는 router가 되면 조인 완료
```

dataset은 플래시에 저장되므로 재부팅하면 자동으로 다시 조인합니다.

## 2) Matter 전등 커미셔닝 (이미 했다면 생략)

OTBR 호스트에서 chip-tool로 전등을 Thread 네트워크에 커미셔닝합니다.

```bash
chip-tool pairing ble-thread 1 hex:$(sudo ot-ctl dataset active -x | head -n1) 20202021 3840
#                            ^ node-id                                          ^ PIN   ^ discriminator
chip-tool onoff toggle 1 1   # 동작 확인
```

## 3) 브리지 실행 (OTBR 호스트)

```bash
cd thingplus-coap-light-switch/bridge
sudo ./coap_matter_bridge.py --node-id 1 --endpoint 1 --interface wpan0 \
    --chip-tool /path/to/chip-tool
```

표준 라이브러리만 사용하므로 pip 설치가 필요 없습니다.
버튼 보드가 기본값(멀티캐스트 `ff03::1`)으로 보내는 CoAP를 받기 위해 `wpan0`에서
멀티캐스트 그룹에 가입합니다.

## 4) 테스트

버튼을 누르면:

- 보드의 파란 LED(PA08)가 짧게 깜빡이고 시리얼에 로그가 출력됩니다.
  ```
  <inf> coap_switch: CoAP PUT coap://[ff03::1]/matter-light "toggle" 전송
  ```
- 브리지 로그:
  ```
  INFO 수신: PUT /matter-light "toggle" from [fd11:22:...]
  INFO 실행: chip-tool onoff toggle 1 1
  INFO Matter 전등 toggle 완료
  ```
- 전등이 토글됩니다.

보드 없이 브리지만 먼저 테스트하려면 OTBR 호스트에서:

```bash
sudo ot-ctl coap start
sudo ot-ctl coap put ff03::1 matter-light non toggle
```

## 설정 변경

`west build -t menuconfig` 또는 `prj.conf`에 추가:

| Kconfig | 기본값 | 설명 |
|---|---|---|
| `CONFIG_LIGHT_SWITCH_PEER_ADDR` | `"ff03::1"` | CoAP 목적지. 멀티캐스트 대신 브리지 호스트 `wpan0`의 mesh-local 주소(유니캐스트)를 넣을 수 있음 |
| `CONFIG_LIGHT_SWITCH_COAP_URI` | `"matter-light"` | CoAP 리소스 경로 (브리지의 `--uri`와 일치해야 함) |
| `CONFIG_LIGHT_SWITCH_DEBOUNCE_MS` | `250` | 버튼 디바운스/전송 속도 제한 (ms) |

페이로드는 `toggle` 외에 `on` / `off`도 브리지가 지원하므로, 버튼을 2개 달아
켜기/끄기를 분리하는 식으로 확장할 수 있습니다.
