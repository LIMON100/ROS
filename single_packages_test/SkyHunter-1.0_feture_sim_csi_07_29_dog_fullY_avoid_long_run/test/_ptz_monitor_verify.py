#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""신규 PTZ.send() 폴링 방식 — 실측 검증."""
import sys, time, importlib.util

spec = importlib.util.spec_from_file_location('ptz_test', 'test/ptz_test.py')
m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m)

ptz = m.PTZ('COM8', 2400, 0x01, verbose=False)
if not ptz.open():
    sys.exit(1)

print('\n=== query_pan()/query_tilt() 실제 호출 — 10회 반복 ===\n')
times = []
for i in range(10):
    t0 = time.time()
    p = ptz.query_pan()
    t_p = (time.time() - t0) * 1000
    t1 = time.time()
    t = ptz.query_tilt()
    t_t = (time.time() - t1) * 1000
    total = (time.time() - t0) * 1000
    times.append(total)
    p_str = f'{p.degrees:7.2f}°' if p else '   N/A'
    t_str = f'{t.degrees:7.2f}°' if t else '   N/A'
    print(f'#{i+1:2}: Pan={p_str}  Tilt={t_str}    '
          f'(query_pan={t_p:.0f}ms, query_tilt={t_t:.0f}ms, total={total:.0f}ms)')

print(f'\n평균 cycle: {sum(times) / len(times):.0f} ms')
print(f'  → 0.5 s interval 로 모니터 가능 (이전 ~3 s 대비 ~{3000/(sum(times)/len(times)):.1f}× 빠름)')

ptz.close()
