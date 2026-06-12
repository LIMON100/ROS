#!/usr/bin/env python3
# Copyright 2026 SkyAutoNet Inc.
#
# Proprietary and confidential. Unauthorized copying, distribution, or use
# of this file, via any medium, is strictly prohibited.

"""
RF Path Loss Model - Log-distance path loss with shadow fading
"""

import math
import random
from dataclasses import dataclass
from typing import List


@dataclass
class RFModelConfig:
    """RF model configuration """
    path_loss_exponent: float
    reference_rssi_dbm: float
    reference_distance_m: float
    shadow_fading_std: float
    disconnect_threshold_dbm: float


@dataclass
class DelayModelConfig:
    """Delay model configuration"""
    base_delay_ms: float
    contention_delay_per_robot_ms: float


@dataclass
class PacketLossConfig:
    """Packet loss model configuration """
    thresholds_dbm: List[float]
    loss_percentages: List[float]


class PathLossModel:
    """
    Log-distance path loss model with shadow fading.

    RSSI = reference_rssi - 10 * n * log10(d / d_ref) + noise
    """

    def __init__(
        self,
        rf_config: RFModelConfig,
        delay_config: DelayModelConfig,
        loss_config: PacketLossConfig
    ):
        self.rf = rf_config
        self.delay = delay_config
        self.loss = loss_config

    def compute_rssi(self, distance_m: float) -> float:
        """
        Compute RSSI using log-distance path loss model.

        Args:
            distance_m: Distance between nodes in meters

        Returns:
            RSSI in dBm
        """
        # Avoid log(0)
        if distance_m < 0.01:
            distance_m = 0.01

        # Log-distance path loss
        rssi = self.rf.reference_rssi_dbm - 10 * self.rf.path_loss_exponent * math.log10(
            distance_m / self.rf.reference_distance_m
        )

        # Add shadow fading (Gaussian noise)
        rssi += random.gauss(0, self.rf.shadow_fading_std)

        return rssi

    def is_connected(self, rssi_dbm: float) -> bool:
        """Check if link is connected based on RSSI threshold."""
        return rssi_dbm > self.rf.disconnect_threshold_dbm

    def compute_packet_loss(self, rssi_dbm: float) -> float:
        """
        Compute packet loss percentage based on RSSI.

        Uses step function based on thresholds.

        Args:
            rssi_dbm: Signal strength in dBm

        Returns:
            Packet loss percentage (0-100)
        """
        for i, threshold in enumerate(self.loss.thresholds_dbm):
            if rssi_dbm > threshold:
                return self.loss.loss_percentages[i]

        # Below all thresholds = disconnected
        return self.loss.loss_percentages[-1]

    def compute_delay(self, active_link_count: int) -> float:
        """
        Compute delay based on network contention.

        Args:
            active_link_count: Number of active links

        Returns:
            Delay in milliseconds
        """
        base = (
            self.delay.base_delay_ms +
            self.delay.contention_delay_per_robot_ms * active_link_count
            )
        jitter = random.uniform(-2.0, 2.0)  # ±2ms jitter
        return max(5.0, min(20.0, base + jitter))  # clamp to valid range

    def compute_link_metrics(
        self,
        distance_m: float,
        active_link_count: int,
        jam_attenuation_db: float = 0.0
    ) -> dict:
        """
        Compute all metrics for a link.

        Args:
            distance_m: Distance between nodes
            active_link_count: Number of active links (for delay)
            jam_attenuation_db: Additional attenuation from jamming

        Returns:
            Dict with rssi_dbm, connected, packet_loss_pct, delay_ms, jammed
        """
        rssi = self.compute_rssi(distance_m)
        rssi -= jam_attenuation_db  # Apply jam

        return {
            'rssi_dbm': rssi,
            'connected': self.is_connected(rssi),
            'packet_loss_pct': self.compute_packet_loss(rssi),
            'delay_ms': self.compute_delay(active_link_count),
            'jammed': jam_attenuation_db > 0
        }