#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""ptz_test.py 모니터링 동작 진단."""
import sys, time, importlib.util

spec = importlib.util.spec_from_file_location('ptz_test', 'test/ptz_test.py')
m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m)

ptz = m.PTZ('COM8', 2400, 0x01, verbose=False)
if not ptz.open():
    sys.exit(1)

print(f'\n=== 모니터링 진단 — 10회 query 반복, 각 단계 시간 측정 ===\n')

for i in range(10):
    t0 = time.time()
    # query_pan 분해
    pkt = m.build_packet(0x01, 0x00, m.OP.QUERY_PAN)
    ptz.ser.reset_input_buffer()
    t_reset = time.time()
    ptz.ser.write(pkt); ptz.ser.flush()
    t_write = time.time()
    time.sleep(m.RESPONSE_WAIT)  # 0.6s
    t_sleep = time.time()
    in_waiting = ptz.ser.in_waiting
    rx = ptz.ser.read(32)
    t_read = time.time()
    clean = m.strip_echo(rx, pkt)
    resp = m.parse_response(clean)
    total = (t_read - t0) * 1000
    print(f'#{i+1:2}: total={total:5.0f}ms  '
          f'(write+sleep={(t_sleep-t_write)*1000:.0f}, read={(t_read-t_sleep)*1000:.0f}, '
          f'in_waiting before read={in_waiting})')
    print(f'    RX raw ({len(rx)} bytes): {" ".join(f"{b:02X}" for b in rx) if rx else "(none)"}')
    if resp:
        print(f'    응답 OK: opcode=0x{resp.opcode:02X} pos=0x{resp.value16:04X} ({resp.degrees:.2f}°)')
    else:
        print(f'    응답 PARSE 실패')
    time.sleep(0.1)

print('\n=== 추가: read(14) 로 시도 (정확한 길이 지정) ===\n')
for i in range(5):
    t0 = time.time()
    pkt = m.build_packet(0x01, 0x00, m.OP.QUERY_PAN)
    ptz.ser.reset_input_buffer()
    ptz.ser.write(pkt); ptz.ser.flush()
    time.sleep(0.5)
    rx = ptz.ser.read(14)
    total = (time.time() - t0) * 1000
    resp = m.parse_response(m.strip_echo(rx, pkt))
    pos = f'0x{resp.value16:04X} {resp.degrees:.2f}°' if resp else 'FAIL'
    print(f'#{i+1}: total={total:5.0f}ms  rx_len={len(rx)}  {pos}')
    time.sleep(0.1)

ptz.close()
