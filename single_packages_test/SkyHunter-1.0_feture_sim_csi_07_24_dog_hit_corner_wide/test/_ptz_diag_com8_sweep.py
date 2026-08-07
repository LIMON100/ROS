#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""COM8 종합 PTZ 진단 — 전체 baud sweep + Pelco-D/P 프로토콜 + 주소 sweep + 회전 테스트.

각 baud 에서:
  (1) Pelco-D Pan inquiry (addr 0x01)         -> RX 감시
  (2) Pelco-P Pan inquiry (addr 0x01)         -> RX 감시
  (3) 800ms idle listen                       -> 비요청 데이터 유무
  (4) Pan ctrl 미세 회전 -> 0.8s -> Stop       -> 사용자가 물리 회전 관찰

전 단계가 끝나면 9600 표준 가정으로 주소 0x01~0x10 sweep (참고용 — 실제 SP5145 는 2400 사용).
"""
import serial
import time

PORT = 'COM8'
BAUDS = [2400, 4800, 9600, 19200, 38400, 57600, 115200]
ADDR = 0x01


def pelco_d_checksum(b1, b2, b3, b4, b5):
    return (b1 + b2 + b3 + b4 + b5) & 0xFF


def pelco_p_checksum(b0, b1, b2, b3, b4, b5):
    """Pelco-P XOR of bytes 0..5 (start..data2)."""
    v = b0
    for x in (b1, b2, b3, b4, b5):
        v ^= x
    return v & 0xFF


def open_port(baud):
    return serial.Serial(PORT, baud, timeout=0.6,
                        bytesize=serial.EIGHTBITS,
                        parity=serial.PARITY_NONE,
                        stopbits=serial.STOPBITS_ONE)


def tx_rx(ser, label, pkt, wait_sec, read_size=16):
    ser.reset_input_buffer()
    ser.write(pkt); ser.flush()
    time.sleep(wait_sec)
    rx = ser.read(read_size)
    tx_str = ' '.join(f'{b:02X}' for b in pkt)
    rx_str = ' '.join(f'{b:02X}' for b in rx) if rx else '(no rx)'
    marker = ' *** RX! ***' if rx else ''
    print(f'    {label:26s} TX: {tx_str}')
    print(f'    {"":26s} RX: {rx_str}{marker}')
    return rx


def baud_round(baud, *, do_motion=True):
    """단일 baud 에서 inquiry + idle listen + (옵션) motion."""
    print(f'\n--- baud={baud} ---')
    try:
        ser = open_port(baud)
    except serial.SerialException as e:
        print(f'    open failed: {e}')
        return None
    time.sleep(0.15)
    ser.reset_input_buffer()
    any_rx = False

    # (1) Pelco-D Pan inquiry
    pkt_d = bytes([0xFF, ADDR, 0x00, 0x51, 0x00, 0x00,
                   pelco_d_checksum(ADDR, 0x00, 0x51, 0x00, 0x00)])
    rx = tx_rx(ser, 'Pelco-D Pan inq', pkt_d, wait_sec=0.7)
    if rx:
        any_rx = True

    # (2) Pelco-P Pan inquiry (start=0xA0, addr-1 typical)
    p_addr = (ADDR - 1) & 0xFF
    p_chk = pelco_p_checksum(0xA0, p_addr, 0x00, 0x51, 0x00, 0x00)
    pkt_p = bytes([0xA0, p_addr, 0x00, 0x51, 0x00, 0x00, 0xAF, p_chk])
    rx = tx_rx(ser, 'Pelco-P Pan inq', pkt_p, wait_sec=0.7)
    if rx:
        any_rx = True

    # (3) Idle listen
    ser.reset_input_buffer()
    time.sleep(0.8)
    rx_idle = ser.read(64)
    if rx_idle:
        print(f'    {"idle listen 0.8s":26s} RX: '
              f'{" ".join(f"{b:02X}" for b in rx_idle)} *** RX! ***')
        any_rx = True
    else:
        print(f'    {"idle listen 0.8s":26s} (no data)')

    # (4) Motion (Pan small, then stop) — 사용자가 물리 관찰
    if do_motion:
        # 작은 각도로 Pan 살짝 우회전
        mv = bytes([0xFF, ADDR, 0x20, 0x4B, 0x05, 0x00,
                    pelco_d_checksum(ADDR, 0x20, 0x4B, 0x05, 0x00)])
        tx_rx(ser, '>>> Pan move (관찰)', mv, wait_sec=0.4)
        time.sleep(0.8)
        st = bytes([0xFF, ADDR, 0x00, 0x00, 0x00, 0x00,
                    pelco_d_checksum(ADDR, 0x00, 0x00, 0x00, 0x00)])
        tx_rx(ser, 'Stop', st, wait_sec=0.3)

    ser.close()
    return any_rx


print(f'=== {PORT} 종합 sweep — 7 baud × (Pelco-D + Pelco-P + idle + motion) ===')
print('  ※ Pan move 명령 송신 시 카메라가 잠깐 회전하는지 직접 관찰해주세요.')
print('  ※ 회전한 baud 가 정답입니다 (RX 응답이 없어도).')

hits = []
for b in BAUDS:
    rx = baud_round(b, do_motion=True)
    if rx:
        hits.append(b)

# 9600 (표준) 주소 sweep
print(f'\n\n=== 주소 sweep @ 9600 (addr 0x01 ~ 0x10) ===')
ser = open_port(9600)
time.sleep(0.15)
ser.reset_input_buffer()
for addr in range(1, 17):
    pkt = bytes([0xFF, addr, 0x00, 0x51, 0x00, 0x00,
                 pelco_d_checksum(addr, 0x00, 0x51, 0x00, 0x00)])
    ser.reset_input_buffer()
    ser.write(pkt); ser.flush()
    time.sleep(0.35)
    rx = ser.read(16)
    rx_str = ' '.join(f'{b:02X}' for b in rx) if rx else '(no rx)'
    marker = ' *** RX! ***' if rx else ''
    print(f'    addr=0x{addr:02X}  TX: {" ".join(f"{b:02X}" for b in pkt)}    RX: {rx_str}{marker}')
ser.close()

print('\n=== 완료 ===')
print(f'inquiry 응답이 온 baud: {hits if hits else "(없음)"}')
print('회전 관찰 결과는 직접 알려주세요.')
