#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""PTZ /dev/ttyUSB0 신호 진단 — 송수신 모두 확인."""
import serial
import time

PORT = '/dev/ttyUSB0'
BAUD = 9600

ser = serial.Serial(PORT, BAUD, timeout=0.6,
                    bytesize=serial.EIGHTBITS,
                    parity=serial.PARITY_NONE,
                    stopbits=serial.STOPBITS_ONE)
time.sleep(0.2)
ser.reset_input_buffer()


def tx_rx(label, pkt, wait_sec=0.5, read_size=16):
    ser.reset_input_buffer()
    ser.write(pkt)
    ser.flush()
    time.sleep(wait_sec)
    rx = ser.read(read_size)
    tx_str = ' '.join(f'{b:02X}' for b in pkt)
    rx_str = ' '.join(f'{b:02X}' for b in rx) if rx else '(아무 응답 없음)'
    print(f'  TX [{label:24s}]: {tx_str}')
    print(f'  RX                              : {rx_str}  '
          f'(in_waiting={ser.in_waiting}, read_len={len(rx)})')
    return rx


print(f'=== 포트: {PORT}, baud={BAUD}, 8N1 ===')
print()

print('[1] Pan / Tilt position inquiry — 500ms 대기')
tx_rx('Pan inquiry (addr 0x01)',
      bytes([0xFF, 0x01, 0x00, 0x51, 0x00, 0x00, 0x52]))
tx_rx('Tilt inquiry (addr 0x01)',
      bytes([0xFF, 0x01, 0x00, 0x53, 0x00, 0x00, 0x54]))
print()

print('[2] 주소 0x01~0x05 sweep (Pan inquiry)')
for addr in range(1, 6):
    chk = (addr + 0x00 + 0x51 + 0x00 + 0x00) & 0xFF
    pkt = bytes([0xFF, addr, 0x00, 0x51, 0x00, 0x00, chk])
    tx_rx(f'Pan inq addr=0x{addr:02X}', pkt, wait_sec=0.35)
print()

print('[3] 1.0초 idle listen — 비요청 데이터 흐름 확인')
ser.reset_input_buffer()
time.sleep(1.0)
rx = ser.read(64)
print(f'  idle RX (1s) : {len(rx)} bytes, in_waiting={ser.in_waiting}')
print(f'  bytes        : ' +
      (' '.join(f'{b:02X}' for b in rx) if rx else '(idle 무신호)'))
print()

print('[4] 보드율 19200 으로 재시도 (Pan inquiry)')
ser.close()
ser = serial.Serial(PORT, 19200, timeout=0.6,
                    bytesize=serial.EIGHTBITS,
                    parity=serial.PARITY_NONE,
                    stopbits=serial.STOPBITS_ONE)
time.sleep(0.2)
ser.reset_input_buffer()
tx_rx('Pan inq @ 19200',
      bytes([0xFF, 0x01, 0x00, 0x51, 0x00, 0x00, 0x52]))
print()

print('[5] Pan 제어 명령 송신 (실제 회전 시도 → 1초 후 정지)')
print('     속도 0x20, 각도 0x1000')
tx_rx('Pan ctrl 0x20 / 0x1000',
      bytes([0xFF, 0x01, 0x20, 0x4B, 0x10, 0x00, 0x7B]),
      wait_sec=0.3)
time.sleep(0.8)
print('  → Pan / Tilt 정지 명령 (FF 01 00 00 00 00 01)')
tx_rx('Stop', bytes([0xFF, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01]),
      wait_sec=0.3)

ser.close()
print()
print('=== 진단 완료 ===')
