#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""COM8 2400 bps 확정 테스트 — 카메라 응답 파싱 + 회전 검증."""
import serial
import time

PORT = 'COM8'
BAUD = 2400
ADDR = 0x01


def chk(b1, b2, b3, b4, b5):
    return (b1 + b2 + b3 + b4 + b5) & 0xFF


def parse_response(rx, tx_pkt):
    """TX 에코 제거 후 카메라 응답 추출."""
    if not rx:
        return None
    # TX 에코 (앞 7바이트) 제거
    if len(rx) >= 7 and rx[:7] == tx_pkt:
        rx = rx[7:]
    if len(rx) >= 7 and rx[0] == 0xFF:
        return rx[:7]
    return rx if rx else None


def query(ser, label, opcode):
    pkt = bytes([0xFF, ADDR, 0x00, opcode, 0x00, 0x00,
                 chk(ADDR, 0x00, opcode, 0x00, 0x00)])
    ser.reset_input_buffer()
    ser.write(pkt); ser.flush()
    time.sleep(0.6)
    rx = ser.read(32)
    resp = parse_response(rx, pkt)
    print(f'  {label}:')
    print(f'    TX     : {" ".join(f"{b:02X}" for b in pkt)}')
    print(f'    RX raw : {" ".join(f"{b:02X}" for b in rx) if rx else "(no rx)"}')
    if resp and len(resp) >= 7:
        position = (resp[4] << 8) | resp[5]
        print(f'    응답   : opcode=0x{resp[3]:02X}, position=0x{position:04X} ({position} = {position / 100:.2f}°)')
        return position
    print(f'    응답   : (없음)')
    return None


def control(ser, label, opcode, speed, angle):
    pkt = bytes([0xFF, ADDR, speed, opcode,
                 (angle >> 8) & 0xFF, angle & 0xFF,
                 chk(ADDR, speed, opcode, (angle >> 8) & 0xFF, angle & 0xFF)])
    ser.reset_input_buffer()
    ser.write(pkt); ser.flush()
    time.sleep(0.4)
    rx = ser.read(32)
    print(f'  {label}:')
    print(f'    TX     : {" ".join(f"{b:02X}" for b in pkt)}')
    print(f'    RX raw : {" ".join(f"{b:02X}" for b in rx) if rx else "(no rx)"}')


ser = serial.Serial(PORT, BAUD, timeout=0.8,
                    bytesize=serial.EIGHTBITS,
                    parity=serial.PARITY_NONE,
                    stopbits=serial.STOPBITS_ONE)
time.sleep(0.2)
ser.reset_input_buffer()

print(f'=== {PORT} @ {BAUD} bps — Pelco-D, addr 0x{ADDR:02X} 확정 테스트 ===\n')

print('[1] 초기 위치 조회')
pan0 = query(ser, 'Pan position inquiry  (0x51)', 0x51)
tilt0 = query(ser, 'Tilt position inquiry (0x53)', 0x53)
print()

print('[2] Pan 우회전 — 속도 0x20, 절대각도 0x0500 명령')
control(ser, 'Pan ctrl (0x4B)', 0x4B, 0x20, 0x0500)
print('  → 1.5 초 회전 후 정지')
time.sleep(1.5)
control(ser, 'Stop (0x00)', 0x00, 0x00, 0x0000)
time.sleep(0.5)
print()

print('[3] 회전 후 위치 재조회')
pan1 = query(ser, 'Pan position inquiry  (0x51)', 0x51)
tilt1 = query(ser, 'Tilt position inquiry (0x53)', 0x53)
print()

print('[4] Tilt 상승 — 속도 0x20, 절대각도 0x0300')
control(ser, 'Tilt ctrl (0x4D)', 0x4D, 0x20, 0x0300)
time.sleep(1.5)
control(ser, 'Stop', 0x00, 0x00, 0x0000)
time.sleep(0.5)
print()

print('[5] 최종 위치')
pan2 = query(ser, 'Pan position inquiry  (0x51)', 0x51)
tilt2 = query(ser, 'Tilt position inquiry (0x53)', 0x53)

print()
print('=== 변화량 요약 ===')
def fmt(v): return f'0x{v:04X} ({v / 100:.2f}°)' if v is not None else '(N/A)'
print(f'  Pan  :  {fmt(pan0)} → {fmt(pan1)} → {fmt(pan2)}')
print(f'  Tilt :  {fmt(tilt0)} → {fmt(tilt1)} → {fmt(tilt2)}')

ser.close()
print('\n=== 완료 ===')
