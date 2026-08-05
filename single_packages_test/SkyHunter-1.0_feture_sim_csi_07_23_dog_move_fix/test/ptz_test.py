#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
SP5145 (Pelco-D) PTZ 테스트 도구 — 검증된 설정 기본값.

대상 카메라 (실측 검증, 2026-06-01):
  모델     : SP5145
  S/N      : 822603901741
  포트     : COM8 (Windows) / /dev/ttyUSB0 (Linux)
  Baud     : 2400 bps   ← 9600 아님 주의 (카메라 default 가 2400)
  프로토콜  : Pelco-D
  주소     : 0x01
  형식     : 8 data bits, no parity, 1 stop bit
  scale    : raw count / 100 = 도 (degrees)

RS-485 USB 어댑터 특성:
  - 모든 TX 가 RX 로 에코됨 (half-duplex 자동 echo)
  - 카메라 실제 응답은 TX 에코 뒤에 이어붙어 들어옴
  - 본 도구는 echo 자동 제거 후 응답만 파싱
"""
import os
import sys
import threading
import time
from dataclasses import dataclass
from typing import Optional

# Windows cmd.exe / PowerShell VT 처리 활성화 (clear-to-EOL '\x1b[K' 등)
if sys.platform == 'win32':
    os.system('')

try:
    import msvcrt  # Windows 전용 — 잔여 키 버퍼 비우기
except ImportError:
    msvcrt = None

try:
    import serial
except ImportError:
    print('pyserial 미설치. 다음 명령으로 설치:\n  pip install pyserial')
    sys.exit(1)


# ============================================================
# 설정 (필요 시 상단에서 변경)
# ============================================================
DEFAULT_PORT = 'COM8'
DEFAULT_BAUD = 2400
DEFAULT_ADDR = 0x01
DEFAULT_PAN_SPEED = 0x20    # 0x00 ~ 0x3F (0x40 = turbo)
DEFAULT_TILT_SPEED = 0x20
RESPONSE_WAIT = 0.3         # inquiry → 응답 대기 (2400bps 기준, 실측 ~80ms)
COMMAND_WAIT = 0.15         # 일반 명령 후 처리 여유
STALE_THRESHOLD_S = 0.5     # 각도 stale 판정 (poll cycle ~270ms 기준 ≈ 2 cycles)


# ============================================================
# Pelco-D 패킷 처리
# ============================================================
def pelco_d_checksum(b1: int, b2: int, b3: int, b4: int, b5: int) -> int:
    return (b1 + b2 + b3 + b4 + b5) & 0xFF


def build_packet(addr: int, b2: int, opcode: int,
                 data1: int = 0x00, data2: int = 0x00) -> bytes:
    """7-byte Pelco-D 패킷 생성 (start + addr + b2 + opcode + d1 + d2 + chk)."""
    return bytes([0xFF, addr & 0xFF, b2 & 0xFF, opcode & 0xFF,
                  data1 & 0xFF, data2 & 0xFF,
                  pelco_d_checksum(addr, b2, opcode, data1, data2)])


def fmt_packet(pkt: bytes) -> str:
    return ' '.join(f'{b:02X}' for b in pkt)


@dataclass
class PelcoResponse:
    opcode: int
    data1: int
    data2: int
    raw: bytes

    @property
    def value16(self) -> int:
        return (self.data1 << 8) | self.data2

    @property
    def degrees(self) -> float:
        return self.value16 / 100.0


def strip_echo(rx: bytes, tx: bytes) -> bytes:
    """RS-485 어댑터의 TX 에코를 제거하고 카메라 응답만 반환."""
    if rx[:len(tx)] == tx:
        return rx[len(tx):]
    return rx


def parse_response(rx: bytes) -> Optional[PelcoResponse]:
    """첫 번째 유효한 Pelco-D 응답 패킷을 추출."""
    if len(rx) < 7:
        return None
    # 0xFF 시작 위치 탐색
    for i in range(len(rx) - 6):
        if rx[i] != 0xFF:
            continue
        chunk = rx[i:i + 7]
        if len(chunk) < 7:
            return None
        if pelco_d_checksum(chunk[1], chunk[2], chunk[3],
                            chunk[4], chunk[5]) == chunk[6]:
            return PelcoResponse(opcode=chunk[3],
                                 data1=chunk[4], data2=chunk[5],
                                 raw=chunk)
    return None


# ============================================================
# Pelco-D 명령 상수
# ============================================================
class OP:
    # 위치 조회 (Inquiry)
    QUERY_PAN = 0x51
    QUERY_TILT = 0x53
    RESP_PAN = 0x59
    RESP_TILT = 0x5B

    # 절대 각도 제어
    PAN_POS = 0x4B
    TILT_POS = 0x4D

    # 방향 제어 (b2 = 0x00, b3 = 방향 비트)
    DIR_UP = 0x08
    DIR_DOWN = 0x10
    DIR_LEFT = 0x04
    DIR_RIGHT = 0x02
    DIR_STOP = 0x00

    # 프리셋
    CALL_PRESET = 0x07
    PRESET_HOME = 223


# ============================================================
# PTZ 컨트롤러
# ============================================================
class PTZ:
    def __init__(self, port: str = DEFAULT_PORT,
                 baud: int = DEFAULT_BAUD,
                 addr: int = DEFAULT_ADDR,
                 verbose: bool = True):
        self.port = port
        self.baud = baud
        self.addr = addr
        self.verbose = verbose
        self.ser: Optional[serial.Serial] = None
        # 시리얼 IO 직렬화 — 멀티 스레드 동시 송수신 방지
        # (pynput 리스너 스레드 + 백그라운드 각도 폴링 스레드 등)
        self._io_lock = threading.Lock()

    def open(self) -> bool:
        try:
            self.ser = serial.Serial(
                port=self.port, baudrate=self.baud,
                bytesize=serial.EIGHTBITS,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                timeout=RESPONSE_WAIT + 0.2)
            time.sleep(0.15)
            self.ser.reset_input_buffer()
            print(f'[연결] {self.port} @ {self.baud} bps  addr=0x{self.addr:02X}  Pelco-D')
            return True
        except serial.SerialException as e:
            print(f'[연결 실패] {e}')
            return False

    def close(self):
        if self.ser and self.ser.is_open:
            self.ser.close()
            print('[해제] 시리얼 닫음')

    def send(self, pkt: bytes, *, wait: float = COMMAND_WAIT,
             expect_bytes: int = 14, poll_ms: int = 15) -> bytes:
        """패킷 전송 + 응답 읽기 + echo 제거 결과 반환.

        3단계 구조 — 가운데 poll-wait 동안은 lock 을 풀어 다른 스레드
        (예: 키보드 listener 의 stop_fast/move_dir_fast) 가 즉시 끼어들
        수 있도록 함. _query 에서 opcode 검증으로 contamination 시 None 처리.

        Phase 1 (lock): reset_input_buffer + write + flush
        Phase 2 (no lock): in_waiting 도착 폴링 — 다른 스레드 write 가능
        Phase 3 (lock): read
        """
        # Phase 1 — 짧게 lock 잡고 write
        with self._io_lock:
            if not self.ser or not self.ser.is_open:
                raise RuntimeError('시리얼 미연결')
            self.ser.reset_input_buffer()
            self.ser.write(pkt); self.ser.flush()
        # Phase 2 — lock 해제 상태로 응답 도착 폴링.
        # 임계값 (expect_bytes) 도달 후 짧은 settle 시간을 더해, 다른 스레드의
        # 끼어든 TX echo 로 threshold 가 일찍 충족되었을 때 뒤늦은 카메라
        # 응답도 포착함 (35ms = 7바이트 @ 2400bps ≈ 29ms + slack).
        deadline = time.time() + wait
        settle_until: Optional[float] = None
        while time.time() < deadline:
            if settle_until is not None and time.time() >= settle_until:
                break
            try:
                if settle_until is None and \
                        self.ser.in_waiting >= expect_bytes:
                    settle_until = time.time() + 0.035
            except (serial.SerialException, OSError):
                break
            time.sleep(poll_ms / 1000.0)
        # Phase 3 — 다시 lock 잡고 read
        with self._io_lock:
            if not self.ser or not self.ser.is_open:
                return bytes()
            rx = self.ser.read(self.ser.in_waiting)
        clean = strip_echo(rx, pkt)
        if self.verbose:
            print(f'  TX  : {fmt_packet(pkt)}')
            if rx == pkt:
                print(f'  RX  : (TX echo only — 카메라 응답 없음)')
            elif clean:
                print(f'  echo: {fmt_packet(pkt)}')
                print(f'  RESP: {fmt_packet(clean)}')
            else:
                print(f'  RX  : (no data)')
        return clean

    # ---------- High-level helpers ----------
    def query_pan(self) -> Optional[PelcoResponse]:
        return self._query(OP.QUERY_PAN, OP.RESP_PAN)

    def query_tilt(self) -> Optional[PelcoResponse]:
        return self._query(OP.QUERY_TILT, OP.RESP_TILT)

    def _query(self, op_send: int, op_expect: int) -> Optional[PelcoResponse]:
        pkt = build_packet(self.addr, 0x00, op_send)
        clean = self.send(pkt, wait=RESPONSE_WAIT, expect_bytes=14)
        resp = parse_response(clean)
        # opcode 검증 — Phase 2 중 다른 스레드 TX 가 끼어들어 응답이 오염될 수
        # 있으므로 예상 opcode (Pan: 0x59, Tilt: 0x5B) 가 아니면 None 반환.
        # 다음 폴 사이클에서 정상 응답 복구 → stale 마커 한 사이클만 뜸.
        if resp and resp.opcode == op_expect:
            return resp
        return None

    def pan_to(self, angle: int, speed: int = DEFAULT_PAN_SPEED):
        pkt = build_packet(self.addr, speed, OP.PAN_POS,
                           (angle >> 8) & 0xFF, angle & 0xFF)
        self.send(pkt)

    def tilt_to(self, angle: int, speed: int = DEFAULT_TILT_SPEED):
        pkt = build_packet(self.addr, speed, OP.TILT_POS,
                           (angle >> 8) & 0xFF, angle & 0xFF)
        self.send(pkt)

    def move_dir(self, direction: int,
                 pan_speed: int = DEFAULT_PAN_SPEED,
                 tilt_speed: int = DEFAULT_TILT_SPEED):
        pkt = build_packet(self.addr, 0x00, direction,
                           pan_speed, tilt_speed)
        self.send(pkt)

    def stop(self):
        self.send(build_packet(self.addr, 0x00, 0x00))

    def goto_preset(self, preset_id: int = OP.PRESET_HOME,
                    speed: int = 0x10):
        pkt = build_packet(self.addr, speed, OP.CALL_PRESET,
                           0x00, preset_id)
        self.send(pkt)

    # ---------- Fire-and-forget (응답 안 읽음, 인터랙티브 모드용) ----------
    def _write_only(self, pkt: bytes):
        with self._io_lock:
            if not self.ser or not self.ser.is_open:
                raise RuntimeError('시리얼 미연결')
            try:
                self.ser.write(pkt); self.ser.flush()
            except serial.SerialException:
                pass

    def move_dir_fast(self, direction: int, pan_speed: int, tilt_speed: int):
        self._write_only(build_packet(self.addr, 0x00, direction,
                                      pan_speed, tilt_speed))

    def stop_fast(self):
        self._write_only(build_packet(self.addr, 0x00, 0x00))


# ============================================================
# UI 헬퍼
# ============================================================
def prompt_hex(prompt: str, default: int, lo: int = 0, hi: int = 0xFF) -> int:
    s = input(f'{prompt} (16진수, 기본 0x{default:02X}): ').strip()
    if not s:
        return default
    try:
        v = int(s, 16)
        if lo <= v <= hi:
            return v
        print(f'  범위 0x{lo:02X}~0x{hi:02X} 벗어남 — 기본값 사용')
    except ValueError:
        print('  잘못된 입력 — 기본값 사용')
    return default


def prompt_int(prompt: str, default: int, lo: int = 0, hi: int = 0xFFFF) -> int:
    s = input(f'{prompt} (기본 {default}): ').strip()
    if not s:
        return default
    try:
        v = int(s, 0)  # 0 / 0x / 0b 자동
        if lo <= v <= hi:
            return v
        print(f'  범위 {lo}~{hi} 벗어남 — 기본값 사용')
    except ValueError:
        print('  잘못된 입력 — 기본값 사용')
    return default


def print_pos(label: str, resp: Optional[PelcoResponse]):
    if not resp:
        print(f'  {label}: (응답 없음)')
        return
    print(f'  {label}: opcode=0x{resp.opcode:02X}  raw=0x{resp.value16:04X}  '
          f'({resp.value16} = {resp.degrees:.2f}°)')


# ============================================================
# 메뉴
# ============================================================
MENU = """
=== Pelco-D PTZ 테스트 ===
  1. Pan/Tilt 위치 조회 (inquiry)
  2. Pan 절대 각도 제어
  3. Tilt 절대 각도 제어
  4. 방향 제어 (키보드 화살표 — hold to move, release to stop)
  5. 원점 복귀 (Call Preset 223)
  6. 위치 실시간 모니터링 (N초)
  7. 사용자 정의 패킷 송신 (raw HEX)
  8. 진단 모드 (baud sweep)
  9. 설정 변경 (port/baud/addr)
  0. 종료
"""

DIR_MENU = {
    '1': ('Up',        OP.DIR_UP),
    '2': ('Down',      OP.DIR_DOWN),
    '3': ('Left',      OP.DIR_LEFT),
    '4': ('Right',     OP.DIR_RIGHT),
    '5': ('Up-Left',   OP.DIR_UP | OP.DIR_LEFT),
    '6': ('Up-Right',  OP.DIR_UP | OP.DIR_RIGHT),
    '7': ('Down-Left', OP.DIR_DOWN | OP.DIR_LEFT),
    '8': ('Down-Right', OP.DIR_DOWN | OP.DIR_RIGHT),
    '0': ('Stop',      OP.DIR_STOP),
}


def menu_inquiry(ptz: PTZ):
    print('\n[위치 조회]')
    print_pos('Pan ', ptz.query_pan())
    print_pos('Tilt', ptz.query_tilt())


def menu_pan_abs(ptz: PTZ):
    print('\n[Pan 절대 각도 제어]')
    angle = prompt_int('각도 (raw count, 0~0xFFFF 또는 십진수)',
                       0x0500, 0, 0xFFFF)
    speed = prompt_hex('속도', DEFAULT_PAN_SPEED, 0, 0x40)
    ptz.pan_to(angle, speed)


def menu_tilt_abs(ptz: PTZ):
    print('\n[Tilt 절대 각도 제어]')
    angle = prompt_int('각도 (raw count)', 0x0300, 0, 0xFFFF)
    speed = prompt_hex('속도', DEFAULT_TILT_SPEED, 0, 0x40)
    ptz.tilt_to(angle, speed)


def menu_direction(ptz: PTZ):
    """키보드 화살표 hold-to-move / release-to-stop 모드.

    pynput 미설치 시 자동으로 숫자 키 기반 레거시 모드로 대체.
    """
    try:
        from pynput import keyboard as kbd
    except ImportError:
        print('  pynput 미설치 — 숫자 키 모드로 진행 (설치: pip install pynput)')
        return _menu_direction_legacy(ptz)

    print('\n[방향 제어 — 키보드 화살표]')
    print('  ↑ ↓ ← →   : Pan / Tilt (조합으로 대각선)')
    print('  + / -    : 속도 증감 (4 단위)')
    print('  ESC / q  : 종료 (자동 정지)')
    print('  ※ 이 창이 포커스를 갖지 않아도 키 이벤트가 잡힙니다.')

    pan_speed = prompt_hex('Pan 속도', DEFAULT_PAN_SPEED, 0, 0x40)
    tilt_speed = prompt_hex('Tilt 속도', DEFAULT_TILT_SPEED, 0, 0x3F)

    KEY_TO_DIR = {
        kbd.Key.up:    OP.DIR_UP,
        kbd.Key.down:  OP.DIR_DOWN,
        kbd.Key.left:  OP.DIR_LEFT,
        kbd.Key.right: OP.DIR_RIGHT,
    }
    DIR_NAMES = [('Up', OP.DIR_UP), ('Down', OP.DIR_DOWN),
                 ('Left', OP.DIR_LEFT), ('Right', OP.DIR_RIGHT)]
    BUMP_DEBOUNCE_S = 0.18  # +/- 속도 조정 edge-trigger
    EOL = '\x1b[K'          # ANSI clear-to-EOL — Windows VT enabled at module load

    pressed: set = set()
    state = {'pan_speed': pan_speed, 'tilt_speed': tilt_speed,
             'last_dir': 0,
             'pan_deg': None,  'tilt_deg': None,
             'pan_age': None,  'tilt_age': None,
             'plus_ts': 0.0,   'minus_ts': 0.0}  # +/- 디바운스 타임스탬프
    display_lock = threading.Lock()
    stop_event = threading.Event()

    def cur_dir() -> int:
        """현재 눌린 키 → Pelco-D direction 바이트.

        - tuple(pressed) 로 snapshot 후 iterate (set 동시 변이 RuntimeError 회피)
        - 반대 방향 동시 입력은 상쇄 (Up+Down → 0, Left+Right → 0)
        """
        d = 0
        for k in tuple(pressed):
            d |= KEY_TO_DIR.get(k, 0)
        if (d & OP.DIR_UP) and (d & OP.DIR_DOWN):
            d &= ~(OP.DIR_UP | OP.DIR_DOWN)
        if (d & OP.DIR_LEFT) and (d & OP.DIR_RIGHT):
            d &= ~(OP.DIR_LEFT | OP.DIR_RIGHT)
        return d

    def _fmt_angle(deg, age):
        if deg is None:
            return '   ...° '
        if age is not None and (time.time() - age) > STALE_THRESHOLD_S:
            return f'{deg:7.2f}°?'
        return f'{deg:7.2f}° '

    def status():
        """현재 commanded 방향(state['last_dir']) + 속도 + 각도 redraw.

        last_dir 는 실제로 카메라에 보낸 방향 — pressed 와 분리해서 표시 일관성 유지.
        """
        with display_lock:
            d = state['last_dir']
            names = [n for n, v in DIR_NAMES if d & v]
            s = '+'.join(names) if names else 'STOP'
            pan_s = _fmt_angle(state['pan_deg'], state['pan_age'])
            tilt_s = _fmt_angle(state['tilt_deg'], state['tilt_age'])
            print(f'\r   [{s:14s}]  ps=0x{state["pan_speed"]:02X}  '
                  f'ts=0x{state["tilt_speed"]:02X}    '
                  f'Pan={pan_s}  Tilt={tilt_s}{EOL}',
                  end='', flush=True)

    def apply():
        d = cur_dir()
        if d == state['last_dir']:
            return
        try:
            if d == 0:
                ptz.stop_fast()
            else:
                ptz.move_dir_fast(d, state['pan_speed'],
                                  state['tilt_speed'])
            state['last_dir'] = d
        except (serial.SerialException, RuntimeError, OSError) as e:
            print(f'\n  [!] PTZ 송신 실패: {e}{EOL}')
        status()

    def _bump_speed(delta: int, ts_key: str):
        """+/- 디바운스 — autorepeat 폭주 방지."""
        now = time.time()
        if now - state[ts_key] < BUMP_DEBOUNCE_S:
            return
        state[ts_key] = now
        new_ps = max(0x01, min(0x3F, state['pan_speed'] + delta))
        new_ts = max(0x01, min(0x3F, state['tilt_speed'] + delta))
        state['pan_speed'] = new_ps
        state['tilt_speed'] = new_ts
        if state['last_dir']:
            try:
                ptz.move_dir_fast(state['last_dir'], new_ps, new_ts)
            except (serial.SerialException, RuntimeError, OSError):
                pass
        status()

    def on_press(key):
        try:
            if key == kbd.Key.esc:
                return False
            if hasattr(key, 'char') and key.char in ('q', 'Q'):
                return False
            if key in KEY_TO_DIR:
                if key not in pressed:
                    pressed.add(key)
                    apply()
                return
            if hasattr(key, 'char') and key.char:
                if key.char in ('+', '='):
                    _bump_speed(+4, 'plus_ts')
                elif key.char in ('-', '_'):
                    _bump_speed(-4, 'minus_ts')
        except Exception as e:
            print(f'\n  [!] 키 처리 예외: {e}{EOL}')
            return False  # listener 깨끗하게 종료

    def on_release(key):
        try:
            if key in pressed:
                pressed.discard(key)
                apply()
        except Exception as e:
            print(f'\n  [!] 키 해제 예외: {e}{EOL}')

    def angle_poller():
        """백그라운드 — pan/tilt 위치 주기 조회.

        - pressed != ∅ (사용자 조작 중) 이면 query 보류 → control 우선
        - sleep 후 query 또는 sleep 만 (둘 다 stop_event.wait 로 interruptible)
        - status() 는 사이클당 1회 (Tilt 직후) — 깜빡임 최소화
        """
        EXPECTED_ERRORS = (serial.SerialException, RuntimeError, OSError)
        consecutive_fail = 0
        while not stop_event.is_set():
            # send() 가 phase-2 (poll-wait) 동안 lock 을 풀므로 hold 중에도
            # 폴링 가능. listener write 와 충돌하면 _query 가 None 반환 →
            # 한 사이클만 stale '?' 표시되고 다음 사이클에 정상 복귀.
            updated = False
            try:
                p = ptz.query_pan()
                if p:
                    state['pan_deg'] = p.degrees
                    state['pan_age'] = time.time()
                    updated = True
                    consecutive_fail = 0
            except EXPECTED_ERRORS:
                consecutive_fail += 1
            if stop_event.is_set():
                break
            if stop_event.wait(0.04):
                break
            try:
                t = ptz.query_tilt()
                if t:
                    state['tilt_deg'] = t.degrees
                    state['tilt_age'] = time.time()
                    updated = True
                    consecutive_fail = 0
            except EXPECTED_ERRORS:
                consecutive_fail += 1
            if updated:
                status()
            # 통신 두절 의심 시 안내 1회 (3 cycles ≈ 1초)
            if consecutive_fail == 3:
                print(f'\n  [!] 시리얼 응답 없음 — 카메라/배선 확인{EOL}')
            if stop_event.wait(0.1):
                break

    save_verbose = ptz.verbose
    ptz.verbose = False

    # 초기 동기 query — 첫 redraw 가 "..." 가 아니라 실제 값으로 보이게
    try:
        p0 = ptz.query_pan()
        if p0:
            state['pan_deg'] = p0.degrees
            state['pan_age'] = time.time()
        t0 = ptz.query_tilt()
        if t0:
            state['tilt_deg'] = t0.degrees
            state['tilt_age'] = time.time()
    except (serial.SerialException, RuntimeError, OSError):
        pass

    poller_thread = threading.Thread(
        target=angle_poller, name='ptz-angle-poll', daemon=True)
    poller_thread.start()

    status()
    try:
        with kbd.Listener(on_press=on_press, on_release=on_release) as l:
            l.join()
    finally:
        # 1) 폴러 정지 신호 → join (최대 1.5s)
        stop_event.set()
        poller_thread.join(timeout=1.5)
        if poller_thread.is_alive():
            print(f'\n  [!] 폴링 스레드 정지 실패 — 시리얼 상태 의심{EOL}')
        # 2) 카메라 안전 정지 (2회 송신)
        try:
            ptz.stop_fast()
            time.sleep(0.05)
            ptz.stop_fast()
        except Exception as e:
            print(f'\n  [!] 안전 정지 실패: {e}{EOL}')
        # 3) 최종 상태 — pressed 비우고 redraw → "[STOP]" 일관 표시
        pressed.clear()
        state['last_dir'] = 0
        status()
        ptz.verbose = save_verbose
        # 4) stdin 잔여 키 비우기 (q/ESC 가 다음 메뉴 input() 으로 새는 것 차단)
        if msvcrt is not None:
            try:
                while msvcrt.kbhit():
                    msvcrt.getch()
            except Exception:
                pass
        print()
        print('  → Stop 송신 / 메인 메뉴 복귀')


def _menu_direction_legacy(ptz: PTZ):
    """pynput 미설치 fallback — 숫자 키 1~8 + 0 정지."""
    print('\n[방향 제어 — 레거시 숫자 키]')
    pan_speed = prompt_hex('Pan 속도', DEFAULT_PAN_SPEED, 0, 0x40)
    tilt_speed = prompt_hex('Tilt 속도', DEFAULT_TILT_SPEED, 0, 0x3F)
    while True:
        print()
        for k, (name, _) in DIR_MENU.items():
            print(f'  {k}. {name}')
        print('  m. 메인 메뉴로 (정지 후)')
        c = input('선택: ').strip().lower()
        if c == 'm':
            ptz.stop()
            return
        if c in DIR_MENU:
            name, val = DIR_MENU[c]
            print(f'  → {name}')
            if val == OP.DIR_STOP:
                ptz.stop()
            else:
                ptz.move_dir(val, pan_speed, tilt_speed)
        else:
            print('  잘못된 선택')


def menu_home(ptz: PTZ):
    print('\n[원점 복귀]')
    preset = prompt_int('Preset ID', OP.PRESET_HOME, 1, 255)
    ptz.goto_preset(preset)


def menu_monitor(ptz: PTZ):
    print('\n[위치 모니터링] Ctrl+C 로 중단')
    duration = prompt_int('지속 시간(초)', 10, 1, 600)
    interval = float(input('갱신 간격(초, 기본 0.4): ').strip() or '0.4')
    end = time.time() + duration
    ptz.verbose = False  # 화면 깔끔하게
    try:
        while time.time() < end:
            p = ptz.query_pan()
            t = ptz.query_tilt()
            ps = f'{p.degrees:7.2f}° (0x{p.value16:04X})' if p else '   N/A   '
            ts = f'{t.degrees:7.2f}° (0x{t.value16:04X})' if t else '   N/A   '
            print(f'  Pan={ps}    Tilt={ts}')
            time.sleep(interval)
    except KeyboardInterrupt:
        print('  (중단)')
    finally:
        ptz.verbose = True


def menu_raw(ptz: PTZ):
    print('\n[사용자 정의 패킷] 6 바이트 HEX 입력 (체크섬 자동)')
    print('  예: FF 01 00 51 00 00')
    s = input('패킷: ').strip()
    parts = s.split()
    if len(parts) != 6:
        print('  6 바이트 입력 필요'); return
    try:
        vals = [int(p, 16) for p in parts]
    except ValueError:
        print('  HEX 파싱 실패'); return
    if not all(0 <= v <= 0xFF for v in vals):
        print('  범위 (0~FF) 벗어남'); return
    pkt = bytes(vals + [pelco_d_checksum(*vals[1:6])])
    ptz.send(pkt, wait=RESPONSE_WAIT, expect_bytes=14)


def menu_diagnose(ptz: PTZ):
    """현재 어드레스 / baud 와 무관하게 다양한 baud 로 inquiry 시도."""
    print('\n[진단 모드 — baud sweep, addr 0x01 고정]')
    save_port = ptz.port
    bauds = [2400, 4800, 9600, 19200, 38400, 57600, 115200]
    ptz.close()
    results = []
    for b in bauds:
        try:
            ser = serial.Serial(save_port, b, timeout=0.8,
                                bytesize=serial.EIGHTBITS,
                                parity=serial.PARITY_NONE,
                                stopbits=serial.STOPBITS_ONE)
            time.sleep(0.1); ser.reset_input_buffer()
            pkt = build_packet(0x01, 0x00, OP.QUERY_PAN)
            ser.write(pkt); ser.flush()
            time.sleep(0.7)
            rx = ser.read(32)
            clean = strip_echo(rx, pkt)
            resp = parse_response(clean)
            mark = '★ 응답!' if resp else '(echo only)' if rx else '(no rx)'
            results.append((b, mark, resp))
            print(f'  {b:>6} bps : {mark}')
            ser.close()
        except serial.SerialException as e:
            print(f'  {b:>6} bps : open 실패 ({e})')
        time.sleep(0.1)
    print()
    hits = [b for b, m, r in results if r]
    if hits:
        print(f'  → 카메라가 응답한 baud: {hits}')
    else:
        print('  → 응답한 baud 없음 (전원/배선/주소 확인)')
    # 원상 복귀 — 재연결 실패 시 명시적으로 안내
    if not ptz.open():
        print('  [!] 재연결 실패 — 메뉴 9 에서 포트/baud 확인 필요')


def menu_settings(ptz: PTZ):
    print(f'\n[현재 설정]  port={ptz.port}  baud={ptz.baud}  '
          f'addr=0x{ptz.addr:02X}')
    new_port = input(f'포트 ({ptz.port}): ').strip() or ptz.port
    new_baud = input(f'baud ({ptz.baud}): ').strip()
    new_baud_v = int(new_baud) if new_baud else ptz.baud
    new_addr = input(f'주소 (0x{ptz.addr:02X}): ').strip()
    new_addr_v = int(new_addr, 16) if new_addr else ptz.addr
    ptz.close()
    ptz.port = new_port; ptz.baud = new_baud_v; ptz.addr = new_addr_v
    if not ptz.open():
        print('  [!] 새 설정으로 연결 실패 — 메뉴 9 로 재진입해 확인하세요')


# ============================================================
# 메인
# ============================================================
def main():
    print(__doc__)
    print(f'기본 포트: {DEFAULT_PORT}   기본 baud: {DEFAULT_BAUD}   '
          f'기본 addr: 0x{DEFAULT_ADDR:02X}')
    port = input(f'시리얼 포트 (기본 {DEFAULT_PORT}): ').strip() or DEFAULT_PORT
    baud_s = input(f'Baud rate (기본 {DEFAULT_BAUD}): ').strip()
    baud = int(baud_s) if baud_s else DEFAULT_BAUD
    addr_s = input(f'주소 (16진수, 기본 0x{DEFAULT_ADDR:02X}): ').strip()
    addr = int(addr_s, 16) if addr_s else DEFAULT_ADDR

    ptz = PTZ(port=port, baud=baud, addr=addr)
    if not ptz.open():
        return

    handlers = {
        '1': menu_inquiry,
        '2': menu_pan_abs,
        '3': menu_tilt_abs,
        '4': menu_direction,
        '5': menu_home,
        '6': menu_monitor,
        '7': menu_raw,
        '8': menu_diagnose,
        '9': menu_settings,
    }
    try:
        while True:
            print(MENU)
            c = input('선택: ').strip()
            if c == '0':
                break
            h = handlers.get(c)
            if h:
                try:
                    h(ptz)
                except Exception as e:
                    print(f'  [오류] {e}')
            else:
                print('  잘못된 선택')
    except KeyboardInterrupt:
        print('\n[중단]')
    finally:
        try:
            ptz.stop()
        except Exception:
            pass
        ptz.close()


if __name__ == '__main__':
    main()
