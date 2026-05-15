#!/usr/bin/env python3
"""
Robot discovery module - Auto-discovers robots from TF tree
"""

import re
from typing import List
from dataclasses import dataclass


@dataclass
class RobotConfig:
    """Robot naming configuration """
    prefix: str
    base_link: str
    odom_frame: str
    common_frame: str


class RobotDiscovery:
    """Discovers robots from TF tree based on naming convention."""

    def __init__(self, config: RobotConfig, logger=None):
        self.config = config
        self.logger = logger
        self._robots: List[int] = []
        self._pattern = self._build_pattern()

    def _build_pattern(self) -> re.Pattern:
        """Build regex pattern for robot frame names."""
        # Matches: SH_01/base_link, SH_02/base_link, etc. (always 2 digits)
        prefix_escaped = re.escape(self.config.prefix)
        return re.compile(
            rf"{prefix_escaped}(\d{{2}})\/{self.config.base_link}"
        )

    def get_robot_frame(self, robot_id: int) -> str:
        """Generate full frame name for robot."""
        return f"{self.config.prefix}{robot_id:02d}/{self.config.base_link}"

    def get_robot_odom(self, robot_id: int) -> str:
        """Generate odom frame name for robot."""
        return f"{self.config.prefix}{robot_id:02d}/{self.config.odom_frame}"

    # def discover_from_tf(self, tf_buffer) -> List[int]:
    #     """
    #     Discover robots by scanning TF frames.

    #     Args:
    #         tf_buffer: tf2_ros.Buffer instance

    #     Returns:
    #         Sorted list of robot IDs found
    #     """
    #     robots = set()
    #     try:
    #         frames_yaml = tf_buffer.all_frames_as_yaml()
    #         for match in self._pattern.finditer(frames_yaml):
    #             robot_id = int(match.group(1))
    #             robots.add(robot_id)

    #         self._robots = sorted(robots)

    #         if self.logger:
    #             self.logger.debug(f"Discovered {len(self._robots)} robots: {self._robots}")

    #     except Exception as e:
    #         if self.logger:
    #             self.logger.warn(f"Robot discovery failed: {e}")

    #     return self._robots

    def discover_from_tf(self, tf_buffer) -> List[int]:
        robots = set()
        try:
            frames_yaml = tf_buffer.all_frames_as_yaml()
            
            # 1. Check for the Global Leader (Robot 1)
            # If 'base_link' exists without a prefix, it's the leader
            if "base_link" in frames_yaml:
                robots.add(1)

            # 2. Check for namespaced followers (SH_02, SH_03...)
            for match in self._pattern.finditer(frames_yaml):
                robot_id = int(match.group(1))
                robots.add(robot_id)

            self._robots = sorted(robots)
        except Exception as e:
            if self.logger:
                self.logger.warn(f"Robot discovery failed: {e}")
        return self._robots

    def get_robot_pairs(self) -> List[tuple]:
        """
        Get all unique robot pairs for mesh links.

        Returns:
            List of (robot_a, robot_b) tuples where a < b
        """
        pairs = []
        for i, a in enumerate(self._robots):
            for b in self._robots[i + 1:]:
                pairs.append((a, b))
        return pairs

    @property
    def robots(self) -> List[int]:
        """Get current list of discovered robots."""
        return self._robots

    @property
    def robot_count(self) -> int:
        """Get number of discovered robots."""
        return len(self._robots)

    @property
    def link_count(self) -> int:
        """Get number of mesh links (n*(n-1)/2)."""
        n = len(self._robots)
        return n * (n - 1) // 2