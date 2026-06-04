#!/usr/bin/env python3
# Copyright 2026 SkyAutoNet Inc.
#
# Proprietary and confidential. Unauthorized copying, distribution, or use
# of this file, via any medium, is strictly prohibited.

"""
lora_model.py — LoRa link simulation model for SkyHunter comm stack.

Pure Python — no ROS dependency.  Fully unit-testable in isolation.

Responsibilities:
  - Duty cycle enforcement (1% rolling window)
  - Packet loss simulation
  - E-stop retry logic (3 attempts)
  - Per-robot heartbeat sequence counters
  - Uptime and packet statistics
"""

from dataclasses import dataclass
from random import random, uniform, randint
from typing import Dict


# ── LoRa RF constants ─────────────────────────────────────────────────────────

# Approximate airtimes at SF10, 125 kHz BW, coding rate 4/5
HEARTBEAT_AIRTIME_MS = 370.0   # ~20 byte payload
ESTOP_AIRTIME_MS     = 100.0   # ~8 byte payload (smaller, faster)

# Duty cycle rolling window
DUTY_CYCLE_WINDOW_S  = 100.0   # seconds


# ── Config dataclass ──────────────────────────────────────────────────────────

@dataclass
class LoraModelConfig:
    num_robots:          int   = 8
    heartbeat_interval_s: float = 5.0
    base_delay_ms:       float = 350.0
    delay_jitter_ms:     float = 150.0
    base_loss_rate:      float = 0.07     # 7% per packet
    duty_cycle_limit:    float = 0.01     # 1% max TX
    spreading_factor:    int   = 10
    max_payload_bytes:   int   = 222


# ── Heartbeat data (plain, no ROS types) ─────────────────────────────────────

@dataclass
class HeartbeatData:
    """Data for one heartbeat packet — mapped to LoraHeartbeat.msg by the node."""
    robot_id:     int
    battery_pct:  int
    latitude:     float
    longitude:    float
    status_flags: int
    seq_number:   int


# ── E-stop result ─────────────────────────────────────────────────────────────

@dataclass
class EstopResult:
    success:      bool
    ack_time_ms:  float   # total delay across attempts; -1 if all lost
    attempts:     int
    message:      str


# ── LoRa model ────────────────────────────────────────────────────────────────

class LoraModel:
    """
    Stateful LoRa link simulator.

    Tracks duty cycle, packet stats, and per-robot sequence numbers.
    All timing uses monotonic seconds passed in from the caller —
    no internal clock calls, making it fully testable.
    """

    def __init__(self, config: LoraModelConfig) -> None:
        self.config   = config
        self.active   = True

        # ── Packet stats ──────────────────────────────────────────────────────
        self.packets_sent:    int = 0
        self.packets_dropped: int = 0

        # ── Duty cycle tracker ────────────────────────────────────────────────
        self._tx_time_ms:    float = 0.0
        self._window_start_s: float = 0.0
        self._window_initialized: bool = False

        # ── Sequence counters ─────────────────────────────────────────────────
        self._heartbeat_seqs: Dict[int, int] = {
            r: 0 for r in range(1, config.num_robots + 1)
        }
        self._estop_seq: int = 0

        # ── Uptime ────────────────────────────────────────────────────────────
        self._start_s: float = 0.0
        self._started: bool  = False

    # ── Public interface ──────────────────────────────────────────────────────

    def start(self, now_s: float) -> None:
        """Call once when the node starts."""
        self._start_s = now_s
        self._window_start_s = now_s
        self._started = True

    def uptime_s(self, now_s: float) -> float:
        if not self._started:
            return 0.0
        return now_s - self._start_s

    def duty_cycle_used_pct(self) -> float:
        max_tx_ms = DUTY_CYCLE_WINDOW_S * self.config.duty_cycle_limit * 1000.0
        if max_tx_ms == 0:
            return 0.0
        return min(100.0, (self._tx_time_ms / max_tx_ms) * 100.0)

    # ── Heartbeat ─────────────────────────────────────────────────────────────

    def try_heartbeat(self, robot_id: int, now_s: float) -> "HeartbeatData | None":
        """
        Attempt to send a heartbeat for robot_id.

        Returns HeartbeatData if the packet gets through, None if dropped
        (duty cycle exceeded or packet lost).
        """
        if not self.active:
            self.packets_dropped += 1
            return None

        self._refresh_duty_window(now_s)

        if not self._check_duty_cycle(HEARTBEAT_AIRTIME_MS):
            self.packets_dropped += 1
            return None

        if random() < self.config.base_loss_rate:
            self.packets_dropped += 1
            return None

        self.packets_sent += 1
        self._heartbeat_seqs[robot_id] += 1

        return HeartbeatData(
            robot_id     = robot_id,
            battery_pct  = randint(50, 100),
            latitude     = 0.0,
            longitude    = 0.0,
            status_flags = 0x01,   # armed
            seq_number   = self._heartbeat_seqs[robot_id],
        )

    def heartbeat_delay_s(self) -> float:
        """Simulated LoRa airtime delay for a heartbeat packet."""
        jitter = uniform(-self.config.delay_jitter_ms, self.config.delay_jitter_ms)
        return max(0.0, (self.config.base_delay_ms + jitter) / 1000.0)

    # ── E-stop ────────────────────────────────────────────────────────────────

    def try_estop(self, now_s: float) -> EstopResult:
        """
        Attempt to deliver an e-stop command.  Retries up to 3 times.

        E-stop uses smaller payload (faster airtime) and 3 retry attempts
        to bring effective loss rate from 7% to ~0.03%.
        """
        if not self.active:
            return EstopResult(
                success=False, ack_time_ms=-1, attempts=0,
                message="LoRa inactive"
            )

        self._refresh_duty_window(now_s)

        if not self._check_duty_cycle(ESTOP_AIRTIME_MS):
            return EstopResult(
                success=False, ack_time_ms=-1, attempts=0,
                message="Duty cycle exceeded"
            )

        total_delay_ms = 0.0
        for attempt in range(1, 4):
            delay_ms = self.config.base_delay_ms + uniform(
                -self.config.delay_jitter_ms, self.config.delay_jitter_ms
            )
            total_delay_ms += delay_ms

            if random() > self.config.base_loss_rate:
                self.packets_sent += 1
                self._estop_seq += 1
                return EstopResult(
                    success     = True,
                    ack_time_ms = total_delay_ms,
                    attempts    = attempt,
                    message     = f"Delivered after {total_delay_ms:.0f} ms ({attempt} attempt(s))",
                )

        # All 3 attempts lost
        self.packets_dropped += 3
        return EstopResult(
            success=False, ack_time_ms=-1, attempts=3,
            message="All 3 attempts lost"
        )

    def next_estop_seq(self) -> int:
        return self._estop_seq

    # ── Internal helpers ──────────────────────────────────────────────────────

    def _refresh_duty_window(self, now_s: float) -> None:
        """Reset duty cycle window if it has expired."""
        if now_s - self._window_start_s > DUTY_CYCLE_WINDOW_S:
            self._tx_time_ms    = 0.0
            self._window_start_s = now_s

    def _check_duty_cycle(self, airtime_ms: float) -> bool:
        """
        Return True if TX is allowed.  Deduct airtime if allowed.
        """
        max_tx_ms = DUTY_CYCLE_WINDOW_S * self.config.duty_cycle_limit * 1000.0
        if self._tx_time_ms + airtime_ms > max_tx_ms:
            return False
        self._tx_time_ms += airtime_ms
        return True