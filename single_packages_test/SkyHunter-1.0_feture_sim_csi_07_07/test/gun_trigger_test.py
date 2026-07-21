#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RDS3225 트리거 서보 모터 단독 테스트 (sysfs PWM 직접 제어)

ROS 없이 /sys/class/pwm 만 사용해서 PWM 펄스를 내보낸다.
출력단에서 파형이 반전되는 보드에 맞춰 duty_cycle 은 반대로 계산한다.

기본값은 ros/src/skyautonet/combat_robot_system/gun_trigger/ 에서 가져왔다:
  - PERIOD_NS  = 20_000_000  (50Hz, gun_trigger.hpp:PERIOD_NS_)
  - chip/line  = 0 / 0       (gun_trigger.launch.xml)
  - us1        = 1410        (gun_trigger.launch.xml)
  - us2        = 1580        (gun_trigger.launch.xml)
  - hold_ms    = 500         (gun_trigger.launch.xml)
  - trigger_ms = 200         (gun_trigger.launch.xml)

펄스 발사 시퀀스도 gun_trigger.cpp::fire_worker_ 와 동일하다:
  pwm_us(us1) → sleep(hold_ms) → sleep(trigger_ms) → pwm_us(us2) → sleep(hold_ms) → stop

리눅스 타겟(예: RK3588)에서만 동작한다. /sys/class/pwm 쓰기에는
보통 root 권한이 필요하므로 sudo 로 실행하거나 scripts/trigger.sh 로
권한을 미리 부여해두자.

사용 예:
  sudo python3 gun_trigger_test.py                # 대화형 메뉴
  sudo python3 gun_trigger_test.py fire           # 기본값으로 1회 발사
  sudo python3 gun_trigger_test.py angle-cycle    # 180도 이동 후 0도 복귀
  sudo python3 gun_trigger_test.py loop --interval-ms 1500            # 무한 shot-release 반복
  sudo python3 gun_trigger_test.py loop --count 10 --interval-ms 800  # 10회 shot-release 반복
  sudo python3 gun_trigger_test.py set --us 1500  # 특정 펄스폭으로 고정 출력
  sudo python3 gun_trigger_test.py sweep --start 1000 --end 2000 --step 50
"""

import argparse
import os
import signal
import sys
import time
from pathlib import Path


# gun_trigger.hpp / launch xml 에서 가져온 기본값
DEFAULT_CHIP = 0
DEFAULT_LINE = 0
DEFAULT_US1 = 1410
DEFAULT_US2 = 1580
DEFAULT_HOLD_MS = 500
DEFAULT_TRIGGER_MS = 200
DEFAULT_MIN_US = 1000
DEFAULT_MAX_US = 2000
DEFAULT_MIN_DEG = 0.0
DEFAULT_MAX_DEG = 180.0
INVERT_OUTPUT_DUTY = True
PERIOD_NS = 20_000_000  # 50Hz, gun_trigger.hpp:PERIOD_NS_


class SysfsPwm:
    """sysfs PWM 으로 트리거 서보 신호를 직접 출력한다."""

    def __init__(self, chip: int, line: int, period_ns: int = PERIOD_NS, verbose: bool = True):
        if chip < 0 or line < 0:
            raise ValueError(f"invalid chip/line: {chip}/{line}")
        self.chip = chip
        self.line = line
        self.period_ns = period_ns
        self.verbose = verbose
        self.base_path = Path(f"/sys/class/pwm/pwmchip{chip}")
        self.pwm_path = self.base_path / f"pwm{line}"

    # -------- sysfs helpers --------
    def _write(self, rel: str, val) -> None:
        target = self.pwm_path / rel if rel else self.pwm_path
        try:
            with open(target, "w") as f:
                f.write(str(val))
        except OSError as e:
            raise OSError(e.errno, f"{e.strerror}: {target}") from e

    def _log(self, msg: str) -> None:
        if self.verbose:
            print(f"[pwm] {msg}")

    def _idle_duty_ns(self) -> int:
        return self.period_ns - 1 if INVERT_OUTPUT_DUTY else 0

    def _write_idle_low(self) -> None:
        self._write("duty_cycle", self._idle_duty_ns())
        self._write("enable", 1)

    # -------- lifecycle --------
    def open(self) -> None:
        if not self.base_path.exists():
            raise FileNotFoundError(f"PWM chip path not found: {self.base_path}")

        if not self.pwm_path.exists():
            self._log(f"export pwm{self.line} on pwmchip{self.chip}")
            with open(self.base_path / "export", "w") as f:
                f.write(str(self.line))
            time.sleep(0.5)  # sysfs 노드 생성 대기 (gun_trigger.cpp 와 동일)

        # period 설정은 enable=0 상태에서 진행
        try:
            self._write("enable", 0)
        except OSError:
            pass

        self._write("period", self.period_ns)
        self._write_idle_low()
        self._log(
            f"ready: pwmchip{self.chip}/pwm{self.line}, "
            f"period={self.period_ns}ns, invert_output_duty={INVERT_OUTPUT_DUTY}, idle"
        )

    def close(self) -> None:
        if not self.pwm_path.exists():
            return
        try:
            self._write_idle_low()
            self._log("stopped (idle LOW, enable=1)")
        except OSError as e:
            print(f"[pwm] close 중 오류 (무시): {e}", file=sys.stderr)

    # -------- output --------
    def pulse_us(self, us: int) -> None:
        """목표 HIGH 펄스폭(us)을 실제 반전 출력에 맞게 duty_cycle 로 변환한다."""
        if us < 0:
            us = 0
        target_high_ns = us * 1000
        if target_high_ns >= self.period_ns:
            target_high_ns = self.period_ns - 1

        if INVERT_OUTPUT_DUTY:
            duty_ns = self.period_ns - target_high_ns
            if duty_ns >= self.period_ns:
                duty_ns = self.period_ns - 1
        else:
            duty_ns = target_high_ns

        self._write("duty_cycle", duty_ns)
        self._write("enable", 1)
        self._log(f"pulse us={us} (target_high_ns={target_high_ns}, duty_ns={duty_ns})")

    def stop(self) -> None:
        """반전 출력 보정 시 실제 출력이 LOW가 되도록 idle duty를 유지한다."""
        try:
            self._write_idle_low()
        except OSError:
            pass


def fire_sequence(pwm: SysfsPwm, us1: int, us2: int, hold_ms: int, trigger_ms: int) -> None:
    """gun_trigger.cpp::fire_worker_ 와 동일한 1회 발사 시퀀스."""
    print(f"[fire] us1={us1}, us2={us2}, hold_ms={hold_ms}, trigger_ms={trigger_ms}")
    try:
        pwm.pulse_us(us1)
        time.sleep(hold_ms / 1000.0)

        if trigger_ms > 0:
            time.sleep(trigger_ms / 1000.0)

        if us2 > 0:
            pwm.pulse_us(us2)
            time.sleep(hold_ms / 1000.0)
    finally:
        pwm.stop()
        print("[fire] done")


def angle_to_us(deg: float, min_deg: float, max_deg: float,
                min_us: int, max_us: int) -> int:
    """각도를 서보 PWM 펄스폭(us)으로 선형 변환한다."""
    if max_deg <= min_deg:
        raise ValueError("max_deg must be greater than min_deg")
    if deg < min_deg or deg > max_deg:
        raise ValueError(f"deg must be in range [{min_deg}, {max_deg}]")

    ratio = (deg - min_deg) / (max_deg - min_deg)
    return int(round(min_us + ratio * (max_us - min_us)))


def angle_cycle(pwm: SysfsPwm, first_deg: float, second_deg: float,
                min_deg: float, max_deg: float, min_us: int, max_us: int,
                hold_ms: int, gap_ms: int) -> None:
    """first_deg 위치로 이동한 뒤 second_deg 위치로 복귀한다."""
    first_us = angle_to_us(first_deg, min_deg, max_deg, min_us, max_us)
    second_us = angle_to_us(second_deg, min_deg, max_deg, min_us, max_us)
    print(f"[angle-cycle] {first_deg:g}deg({first_us}us) -> "
          f"{second_deg:g}deg({second_us}us), hold_ms={hold_ms}, gap_ms={gap_ms}")
    try:
        pwm.pulse_us(first_us)
        time.sleep(hold_ms / 1000.0)

        if gap_ms > 0:
            time.sleep(gap_ms / 1000.0)

        pwm.pulse_us(second_us)
        time.sleep(hold_ms / 1000.0)
    finally:
        pwm.stop()
        print("[angle-cycle] done")


def continuous_fire(pwm: SysfsPwm, us1: int, us2: int, hold_ms: int,
                    trigger_ms: int, interval_ms: int, count: int) -> None:
    """발사 시퀀스를 interval_ms 간격으로 반복.

    count=0 이면 Ctrl+C 까지 무한 반복.
    """
    label = "무한" if count <= 0 else f"{count}회"
    print(f"[loop] 연속 발사 {label}, interval={interval_ms}ms "
          f"(us1={us1}, us2={us2}, hold_ms={hold_ms}, trigger_ms={trigger_ms})")
    print("[loop] 중단: Ctrl+C")
    shots = 0
    try:
        while count <= 0 or shots < count:
            shots += 1
            print(f"[loop] shot #{shots}")
            fire_sequence(pwm, us1, us2, hold_ms, trigger_ms)
            if count > 0 and shots >= count:
                break
            if interval_ms > 0:
                time.sleep(interval_ms / 1000.0)
    finally:
        pwm.stop()
        print(f"[loop] 종료 (총 {shots}회 발사)")


def continuous_angle_cycle(pwm: SysfsPwm, shot_deg: float, release_deg: float,
                           min_deg: float, max_deg: float, min_us: int, max_us: int,
                           hold_ms: int, gap_ms: int, interval_ms: int,
                           count: int) -> None:
    """shot_deg -> release_deg 시퀀스를 interval_ms 간격으로 반복."""
    shot_us = angle_to_us(shot_deg, min_deg, max_deg, min_us, max_us)
    release_us = angle_to_us(release_deg, min_deg, max_deg, min_us, max_us)
    label = "무한" if count <= 0 else f"{count}회"
    print(f"[loop] shot-release {label}, interval={interval_ms}ms "
          f"(shot={shot_deg:g}deg/{shot_us}us, "
          f"release={release_deg:g}deg/{release_us}us, "
          f"hold_ms={hold_ms}, gap_ms={gap_ms})")
    print("[loop] 중단: Ctrl+C")
    shots = 0
    try:
        while count <= 0 or shots < count:
            shots += 1
            print(f"[loop] shot #{shots}")
            fire_sequence(pwm, shot_us, release_us, hold_ms, gap_ms)
            if count > 0 and shots >= count:
                break
            if interval_ms > 0:
                time.sleep(interval_ms / 1000.0)
    finally:
        pwm.stop()
        print(f"[loop] 종료 (총 {shots}회 shot-release)")


def sweep(pwm: SysfsPwm, start_us: int, end_us: int, step_us: int, dwell_ms: int) -> None:
    """기계 가동 범위/원점 찾기용. start_us → end_us 까지 step 단위로 훑는다."""
    if step_us == 0:
        raise ValueError("step must not be 0")
    direction = 1 if end_us >= start_us else -1
    step = abs(step_us) * direction
    print(f"[sweep] {start_us} → {end_us} step={step} dwell={dwell_ms}ms")
    us = start_us
    try:
        while (direction > 0 and us <= end_us) or (direction < 0 and us >= end_us):
            pwm.pulse_us(us)
            time.sleep(dwell_ms / 1000.0)
            us += step
    finally:
        pwm.stop()
        print("[sweep] done")


def set_hold(pwm: SysfsPwm, us: int, duration_s: float) -> None:
    """특정 펄스폭을 일정 시간 동안 유지 (영점/토크 확인용)."""
    print(f"[set] us={us} 유지 {duration_s:.2f}s (Ctrl+C 로 중단)")
    try:
        pwm.pulse_us(us)
        time.sleep(duration_s)
    finally:
        pwm.stop()
        print("[set] done")


def interactive(pwm: SysfsPwm, defaults: dict) -> None:
    print("\n=== RDS3225 트리거 단독 테스트 ===")
    print(f"PWM 채널: pwmchip{pwm.chip}/pwm{pwm.line}  (period {pwm.period_ns} ns / 50Hz)")
    print(f"기본 발사 파라미터: us1={defaults['us1']}, us2={defaults['us2']}, "
          f"hold_ms={defaults['hold_ms']}, trigger_ms={defaults['trigger_ms']}")
    while True:
        print("\n--- 메뉴 ---")
        print("1. 기본값으로 1회 발사 (launch xml 그대로)")
        print("2. 파라미터 직접 입력해서 1회 발사")
        print("3. 연속 발사 (간격/횟수 지정, Ctrl+C 로 중단)")
        print("4. 단일 펄스폭 유지 (영점/토크 확인)")
        print("5. 스윕 테스트 (가동 범위 확인)")
        print("6. 각도 왕복 (기본: 180도 이동 후 0도 복귀)")
        print("7. 비상 정지 (idle LOW 유지)")
        print("0. 종료")
        choice = input("선택: ").strip()

        if choice == "0":
            break
        elif choice == "1":
            fire_sequence(pwm,
                          defaults["us1"], defaults["us2"],
                          defaults["hold_ms"], defaults["trigger_ms"])
        elif choice == "2":
            try:
                us1 = int(input(f"us1 [{defaults['us1']}]: ") or defaults["us1"])
                us2_raw = input(f"us2 (<=0 이면 생략) [{defaults['us2']}]: ").strip()
                us2 = int(us2_raw) if us2_raw else defaults["us2"]
                hold_ms = int(input(f"hold_ms [{defaults['hold_ms']}]: ") or defaults["hold_ms"])
                trigger_ms = int(input(f"trigger_ms [{defaults['trigger_ms']}]: ")
                                 or defaults["trigger_ms"])
            except ValueError:
                print("잘못된 입력입니다.")
                continue
            fire_sequence(pwm, us1, us2, hold_ms, trigger_ms)
        elif choice == "3":
            try:
                shot_deg = float(input("shot deg [180]: ").strip() or "180")
                release_deg = float(input("release deg [0]: ").strip() or "0")
                min_us = int(input("0도 pulse us [1000]: ").strip() or "1000")
                max_us = int(input("180도 pulse us [2000]: ").strip() or "2000")
                hold_ms = int(input(f"hold_ms [{defaults['hold_ms']}]: ") or defaults["hold_ms"])
                gap_ms = int(input(f"shot-release 사이 대기 ms [{defaults['trigger_ms']}]: ")
                             or defaults["trigger_ms"])
                interval_ms = int(input("발사 사이 간격 ms [1000]: ").strip() or "1000")
                count = int(input("총 발사 횟수 (0=무한) [0]: ").strip() or "0")
            except ValueError:
                print("잘못된 입력입니다.")
                continue
            try:
                continuous_angle_cycle(pwm, shot_deg, release_deg,
                                       DEFAULT_MIN_DEG, DEFAULT_MAX_DEG,
                                       min_us, max_us, hold_ms, gap_ms,
                                       interval_ms, count)
            except ValueError as e:
                print(f"잘못된 각도 설정입니다: {e}")
        elif choice == "4":
            try:
                us = int(input("us (예: 1500): ").strip())
                dur = float(input("유지 시간 초 [1.0]: ").strip() or "1.0")
            except ValueError:
                print("잘못된 입력입니다.")
                continue
            set_hold(pwm, us, dur)
        elif choice == "5":
            try:
                start_us = int(input("start us [1000]: ").strip() or "1000")
                end_us = int(input("end us [2000]: ").strip() or "2000")
                step_us = int(input("step us [50]: ").strip() or "50")
                dwell_ms = int(input("dwell ms [300]: ").strip() or "300")
            except ValueError:
                print("잘못된 입력입니다.")
                continue
            sweep(pwm, start_us, end_us, step_us, dwell_ms)
        elif choice == "6":
            try:
                first_deg = float(input("first deg [180]: ").strip() or "180")
                second_deg = float(input("second deg [0]: ").strip() or "0")
                min_us = int(input("0도 pulse us [1000]: ").strip() or "1000")
                max_us = int(input("180도 pulse us [2000]: ").strip() or "2000")
                hold_ms = int(input("각 위치 유지 ms [1000]: ").strip() or "1000")
                gap_ms = int(input("중간 대기 ms [0]: ").strip() or "0")
            except ValueError:
                print("잘못된 입력입니다.")
                continue
            try:
                angle_cycle(pwm, first_deg, second_deg,
                            DEFAULT_MIN_DEG, DEFAULT_MAX_DEG,
                            min_us, max_us, hold_ms, gap_ms)
            except ValueError as e:
                print(f"잘못된 각도 설정입니다: {e}")
        elif choice == "7":
            pwm.stop()
            print("정지됨.")
        else:
            print("잘못된 선택입니다.")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="RDS3225 트리거 서보 sysfs PWM 단독 테스트",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument("--chip", type=int, default=DEFAULT_CHIP,
                        help="pwmchipN 의 N")
    parser.add_argument("--line", type=int, default=DEFAULT_LINE,
                        help="pwm<N> 의 N")

    sub = parser.add_subparsers(dest="cmd")

    p_fire = sub.add_parser("fire", help="gun_trigger 와 동일한 1회 발사")
    p_fire.add_argument("--us1", type=int, default=DEFAULT_US1)
    p_fire.add_argument("--us2", type=int, default=DEFAULT_US2,
                        help="<=0 이면 두 번째 펄스 생략")
    p_fire.add_argument("--hold-ms", type=int, default=DEFAULT_HOLD_MS)
    p_fire.add_argument("--trigger-ms", type=int, default=DEFAULT_TRIGGER_MS)

    p_loop = sub.add_parser("loop", help="연속 shot-release (기본: 180도 이동 후 0도 복귀)")
    p_loop.add_argument("--shot-deg", type=float, default=180.0,
                        help="shot 위치 각도")
    p_loop.add_argument("--release-deg", type=float, default=0.0,
                        help="release 위치 각도")
    p_loop.add_argument("--min-deg", type=float, default=DEFAULT_MIN_DEG,
                        help="min-us 에 대응하는 각도")
    p_loop.add_argument("--max-deg", type=float, default=DEFAULT_MAX_DEG,
                        help="max-us 에 대응하는 각도")
    p_loop.add_argument("--min-us", type=int, default=DEFAULT_MIN_US,
                        help="min-deg 에 대응하는 펄스폭(us)")
    p_loop.add_argument("--max-us", type=int, default=DEFAULT_MAX_US,
                        help="max-deg 에 대응하는 펄스폭(us)")
    p_loop.add_argument("--us1", type=int, default=None,
                        help="직접 펄스폭 모드: 첫 번째 펄스(us)")
    p_loop.add_argument("--us2", type=int, default=None,
                        help="직접 펄스폭 모드: 두 번째 펄스(us), <=0 이면 생략")
    p_loop.add_argument("--hold-ms", type=int, default=DEFAULT_HOLD_MS)
    p_loop.add_argument("--trigger-ms", type=int, default=DEFAULT_TRIGGER_MS)
    p_loop.add_argument("--interval-ms", type=int, default=1000,
                        help="발사 사이 간격(ms)")
    p_loop.add_argument("--count", type=int, default=0,
                        help="총 발사 횟수 (0 이면 무한)")

    p_set = sub.add_parser("set", help="특정 펄스폭 유지")
    p_set.add_argument("--us", type=int, required=True)
    p_set.add_argument("--duration", type=float, default=1.0,
                       help="유지 시간(초)")

    p_sweep = sub.add_parser("sweep", help="펄스폭 스윕 (가동 범위 확인)")
    p_sweep.add_argument("--start", type=int, default=1000)
    p_sweep.add_argument("--end", type=int, default=2000)
    p_sweep.add_argument("--step", type=int, default=50)
    p_sweep.add_argument("--dwell-ms", type=int, default=300)

    p_angle = sub.add_parser("angle-cycle",
                             help="각도 기준 2점 이동 (기본: 180도 이동 후 0도 복귀)")
    p_angle.add_argument("--first-deg", type=float, default=180.0,
                         help="먼저 이동할 각도")
    p_angle.add_argument("--second-deg", type=float, default=0.0,
                         help="다음에 이동할 각도")
    p_angle.add_argument("--min-deg", type=float, default=DEFAULT_MIN_DEG,
                         help="min-us 에 대응하는 각도")
    p_angle.add_argument("--max-deg", type=float, default=DEFAULT_MAX_DEG,
                         help="max-us 에 대응하는 각도")
    p_angle.add_argument("--min-us", type=int, default=DEFAULT_MIN_US,
                         help="min-deg 에 대응하는 펄스폭(us)")
    p_angle.add_argument("--max-us", type=int, default=DEFAULT_MAX_US,
                         help="max-deg 에 대응하는 펄스폭(us)")
    p_angle.add_argument("--hold-ms", type=int, default=1000,
                         help="각 위치 유지 시간(ms)")
    p_angle.add_argument("--gap-ms", type=int, default=0,
                         help="두 위치 사이 추가 대기(ms)")

    sub.add_parser("stop", help="idle LOW 를 쓰고 종료 (비상 정지)")

    return parser


def main() -> int:
    args = build_parser().parse_args()

    if sys.platform != "linux":
        print("이 스크립트는 /sys/class/pwm 을 쓰는 리눅스 타겟 전용입니다.",
              file=sys.stderr)
        return 2

    if os.geteuid() != 0:
        print("[warn] sysfs PWM 쓰기에는 보통 root 권한이 필요합니다. "
              "scripts/trigger.sh 로 권한을 미리 부여했다면 그대로 진행해도 됩니다.",
              file=sys.stderr)

    pwm = SysfsPwm(chip=args.chip, line=args.line)

    def _sigint(_signum, _frame):
        print("\n[ctrl-c] 정지하고 종료합니다.")
        pwm.stop()
        sys.exit(130)

    signal.signal(signal.SIGINT, _sigint)
    signal.signal(signal.SIGTERM, _sigint)

    try:
        pwm.open()
    except (FileNotFoundError, PermissionError, OSError) as e:
        print(f"PWM 초기화 실패: {e}", file=sys.stderr)
        return 1

    defaults = {
        "us1": DEFAULT_US1,
        "us2": DEFAULT_US2,
        "hold_ms": DEFAULT_HOLD_MS,
        "trigger_ms": DEFAULT_TRIGGER_MS,
    }

    try:
        if args.cmd is None:
            interactive(pwm, defaults)
        elif args.cmd == "fire":
            fire_sequence(pwm, args.us1, args.us2, args.hold_ms, args.trigger_ms)
        elif args.cmd == "loop":
            if args.us1 is not None or args.us2 is not None:
                us1 = args.us1 if args.us1 is not None else DEFAULT_MAX_US
                us2 = args.us2 if args.us2 is not None else DEFAULT_MIN_US
                continuous_fire(pwm, us1, us2, args.hold_ms, args.trigger_ms,
                                args.interval_ms, args.count)
            else:
                try:
                    continuous_angle_cycle(pwm, args.shot_deg, args.release_deg,
                                           args.min_deg, args.max_deg,
                                           args.min_us, args.max_us,
                                           args.hold_ms, args.trigger_ms,
                                           args.interval_ms, args.count)
                except ValueError as e:
                    print(f"각도 설정 오류: {e}", file=sys.stderr)
                    return 2
        elif args.cmd == "set":
            set_hold(pwm, args.us, args.duration)
        elif args.cmd == "sweep":
            sweep(pwm, args.start, args.end, args.step, args.dwell_ms)
        elif args.cmd == "angle-cycle":
            try:
                angle_cycle(pwm, args.first_deg, args.second_deg,
                            args.min_deg, args.max_deg,
                            args.min_us, args.max_us,
                            args.hold_ms, args.gap_ms)
            except ValueError as e:
                print(f"각도 설정 오류: {e}", file=sys.stderr)
                return 2
        elif args.cmd == "stop":
            pwm.stop()
        else:
            print(f"알 수 없는 명령: {args.cmd}", file=sys.stderr)
            return 2
    finally:
        pwm.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
