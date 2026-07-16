#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""PTZ COM8 4800 bps 진단 (Windows 로컬)."""
import serial
import time

PORT = 'COM8'
BAUD = 4800

ser = serial.Serial(PORT, BAUD, timeout=0.8,
                    bytesize=serial.EIGHTBITS,
                    parity=serial.PARITY_NONE,
                    stopbits=serial.STOPBITS_ONE)
time.sleep(0.2)
ser.reset_input_buffer()


def tx_rx(label, pkt, wait_sec=0.8, read_size=16):
    ser.reset_input_buffer()
    ser.write(pkt)
    ser.flush()
    time.sleep(wait_sec)
    rx = ser.read(read_size)
    tx_str = ' '.join(f'{b:02X}' for b in pkt)
    rx_str = ' '.join(f'{b:02X}' for b in rx) if rx else '(no response)'
    print(f'  TX [{label:24s}]: {tx_str}')
    print(f'  RX                              : {rx_str}  '
          f'(in_waiting={ser.in_waiting}, read_len={len(rx)})')
    return rx


print(f'=== Port: {PORT}, baud={BAUD}, 8N1 ===')
print()

print('[1] Pan / Tilt inquiry (addr 0x01) -- 800ms wait')
tx_rx('Pan inquiry (addr 0x01)',
      bytes([0xFF, 0x01, 0x00, 0x51, 0x00, 0x00, 0x52]))
tx_rx('Tilt inquiry (addr 0x01)',
      bytes([0xFF, 0x01, 0x00, 0x53, 0x00, 0x00, 0x54]))
print()

print('[2] Address sweep 0x01-0x05 (Pan inquiry)')
for addr in range(1, 6):
    chk = (addr + 0x00 + 0x51 + 0x00 + 0x00) & 0xFF
    pkt = bytes([0xFF, addr, 0x00, 0x51, 0x00, 0x00, chk])
    tx_rx(f'Pan inq addr=0x{addr:02X}', pkt, wait_sec=0.5)
print()

print('[3] 1.5s idle listen')
ser.reset_input_buffer()
time.sleep(1.5)
rx = ser.read(64)
print(f'  idle RX (1.5s): {len(rx)} bytes, in_waiting={ser.in_waiting}')
print(f'  bytes        : ' +
      (' '.join(f'{b:02X}' for b in rx) if rx else '(no data)'))
print()

print('[4] Pan control (speed 0x20, angle 0x1000) -> wait 0.8s -> Stop')
tx_rx('Pan ctrl 0x20 / 0x1000',
      bytes([0xFF, 0x01, 0x20, 0x4B, 0x10, 0x00, 0x7B]),
      wait_sec=0.5)
time.sleep(0.8)
print('  -> Stop')
tx_rx('Stop', bytes([0xFF, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01]),
      wait_sec=0.4)

ser.close()
print()
print('=== Done ===')
