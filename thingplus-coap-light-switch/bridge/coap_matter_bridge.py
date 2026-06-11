#!/usr/bin/env python3
"""CoAP over Thread → Matter(chip-tool) 브리지.

OTBR(OpenThread Border Router) 호스트에서 실행한다.
Thread 메시의 버튼 보드가 보내는 CoAP PUT /matter-light (payload: on/off/toggle)을
수신해 chip-tool로 Matter 전등의 OnOff 클러스터를 제어한다.

외부 라이브러리 의존성 없음 (표준 라이브러리만 사용).

사용 예:
    sudo ./coap_matter_bridge.py --node-id 1 --endpoint 1 --interface wpan0
"""

import argparse
import logging
import socket
import struct
import subprocess
import sys

COAP_PORT = 5683
COAP_TYPE_CON = 0
COAP_TYPE_NON = 1
COAP_TYPE_ACK = 2
COAP_CODE_PUT = 0x03          # 0.03 PUT
COAP_CODE_CHANGED = 0x44      # 2.04 Changed
COAP_CODE_BAD_REQUEST = 0x80  # 4.00 Bad Request
OPT_URI_PATH = 11
PAYLOAD_MARKER = 0xFF

log = logging.getLogger("coap-matter-bridge")


def parse_coap(data):
    """CoAP 메시지에서 (type, code, msg_id, token, uri_path, payload)를 추출한다."""
    if len(data) < 4:
        raise ValueError("too short")

    ver_type_tkl, code, msg_id = struct.unpack("!BBH", data[:4])
    version = ver_type_tkl >> 6
    msg_type = (ver_type_tkl >> 4) & 0x3
    tkl = ver_type_tkl & 0xF
    if version != 1 or tkl > 8:
        raise ValueError("bad version/token")

    pos = 4
    token = data[pos:pos + tkl]
    pos += tkl

    uri_path = []
    option_number = 0
    while pos < len(data):
        byte = data[pos]
        if byte == PAYLOAD_MARKER:
            pos += 1
            break
        delta = byte >> 4
        length = byte & 0xF
        pos += 1
        if delta == 13:
            delta = data[pos] + 13
            pos += 1
        elif delta == 14:
            delta = struct.unpack("!H", data[pos:pos + 2])[0] + 269
            pos += 2
        elif delta == 15:
            raise ValueError("reserved option delta")
        if length == 13:
            length = data[pos] + 13
            pos += 1
        elif length == 14:
            length = struct.unpack("!H", data[pos:pos + 2])[0] + 269
            pos += 2
        elif length == 15:
            raise ValueError("reserved option length")
        option_number += delta
        value = data[pos:pos + length]
        pos += length
        if option_number == OPT_URI_PATH:
            uri_path.append(value.decode("utf-8", "replace"))

    payload = data[pos:]
    return msg_type, code, msg_id, token, "/".join(uri_path), payload


def build_ack(msg_id, token, code):
    header = struct.pack("!BBH", (1 << 6) | (COAP_TYPE_ACK << 4) | len(token),
                         code, msg_id)
    return header + token


def run_chip_tool(args, command):
    cmd = [args.chip_tool, "onoff", command, str(args.node_id), str(args.endpoint)]
    log.info("실행: %s", " ".join(cmd))
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
    except FileNotFoundError:
        log.error("chip-tool을 찾을 수 없음: %s (--chip-tool 옵션 확인)", args.chip_tool)
        return False
    except subprocess.TimeoutExpired:
        log.error("chip-tool 타임아웃")
        return False
    if result.returncode != 0:
        log.error("chip-tool 실패 (rc=%d): %s", result.returncode,
                  result.stderr.strip()[-500:])
        return False
    log.info("Matter 전등 %s 완료", command)
    return True


def open_socket(args):
    sock = socket.socket(socket.AF_INET6, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(("::", args.port))

    # 버튼 보드가 멀티캐스트(ff03::1)로 보내는 경우를 위해 그룹 가입
    if_index = socket.if_nametoindex(args.interface)
    for group in ("ff03::1", "ff02::1"):
        mreq = socket.inet_pton(socket.AF_INET6, group) + struct.pack("!I", if_index)
        try:
            sock.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_JOIN_GROUP, mreq)
            log.info("멀티캐스트 그룹 %s 가입 (%s)", group, args.interface)
        except OSError as exc:
            log.warning("%s 그룹 가입 실패: %s", group, exc)
    return sock


def serve(args):
    sock = open_socket(args)
    log.info("CoAP 서버 대기 중: [::]:%d, 리소스 /%s, Matter node %d endpoint %d",
             args.port, args.uri, args.node_id, args.endpoint)

    while True:
        data, addr = sock.recvfrom(1500)
        try:
            msg_type, code, msg_id, token, uri, payload = parse_coap(data)
        except (ValueError, IndexError, struct.error) as exc:
            log.debug("CoAP 파싱 실패 (%s): %s", addr[0], exc)
            continue

        if code != COAP_CODE_PUT or uri != args.uri:
            log.debug("무시: code=0x%02x uri=%s from %s", code, uri, addr[0])
            continue

        command = payload.decode("utf-8", "replace").strip().lower() or "toggle"
        log.info("수신: PUT /%s \"%s\" from [%s]", uri, command, addr[0])

        ok = command in ("on", "off", "toggle") and run_chip_tool(args, command)

        if msg_type == COAP_TYPE_CON:
            resp = build_ack(msg_id, token,
                             COAP_CODE_CHANGED if ok else COAP_CODE_BAD_REQUEST)
            sock.sendto(resp, addr)


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--chip-tool", default="chip-tool",
                        help="chip-tool 실행 파일 경로 (기본: PATH에서 검색)")
    parser.add_argument("--node-id", type=int, required=True,
                        help="커미셔닝된 Matter 전등의 node id")
    parser.add_argument("--endpoint", type=int, default=1,
                        help="OnOff 클러스터 endpoint (기본: 1)")
    parser.add_argument("--interface", default="wpan0",
                        help="Thread 네트워크 인터페이스 (기본: wpan0)")
    parser.add_argument("--port", type=int, default=COAP_PORT,
                        help="CoAP 수신 포트 (기본: 5683)")
    parser.add_argument("--uri", default="matter-light",
                        help="수신할 CoAP 리소스 경로 (기본: matter-light)")
    parser.add_argument("-v", "--verbose", action="store_true")
    args = parser.parse_args()

    logging.basicConfig(level=logging.DEBUG if args.verbose else logging.INFO,
                        format="%(asctime)s %(levelname)s %(message)s")
    try:
        serve(args)
    except KeyboardInterrupt:
        return 0


if __name__ == "__main__":
    sys.exit(main())
