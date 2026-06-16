"""Structural checks for the Robosense E1 LiDAR config files."""
from __future__ import annotations

import pathlib

import pytest
import yaml

# Phase 2-E Turn 14-17: tests/ moved under archive/v15_python_prototype/.
# parents[3] still resolves to the real repo root (which still owns config/).
_REPO_ROOT = pathlib.Path(__file__).resolve().parents[3]
_DRIVER_YAML = _REPO_ROOT / "config" / "rslidar_e1.yaml"
_SLAM_YAML   = _REPO_ROOT / "config" / "slam_toolbox_e1.yaml"
_README      = _REPO_ROOT / "config" / "README-lidar.md"


@pytest.fixture(scope="module")
def driver_cfg():
    return yaml.safe_load(_DRIVER_YAML.read_text(encoding="utf-8"))


@pytest.fixture(scope="module")
def slam_cfg():
    return yaml.safe_load(_SLAM_YAML.read_text(encoding="utf-8"))


# ─── presence ──────────────────────────────────────────────────────────

def test_driver_yaml_exists():
    assert _DRIVER_YAML.is_file()


def test_slam_yaml_exists():
    assert _SLAM_YAML.is_file()


def test_readme_exists():
    assert _README.is_file()


# ─── rslidar_e1.yaml ───────────────────────────────────────────────────

def test_driver_msg_source_is_online_lidar(driver_cfg):
    assert driver_cfg["common"]["msg_source"] == 1


def test_driver_sends_point_cloud_not_packet(driver_cfg):
    assert driver_cfg["common"]["send_packet_ros"] is False
    assert driver_cfg["common"]["send_point_cloud_ros"] is True


def test_driver_lidar_type_is_rse1(driver_cfg):
    assert driver_cfg["lidar"][0]["driver"]["lidar_type"] == "RSE1"


def test_driver_uses_default_udp_ports(driver_cfg):
    d = driver_cfg["lidar"][0]["driver"]
    assert d["msop_port"]  == 6699
    assert d["difop_port"] == 7788


def test_driver_fov_120deg_centred(driver_cfg):
    d = driver_cfg["lidar"][0]["driver"]
    assert d["start_angle"] == -60
    assert d["end_angle"]   ==  60
    # Sanity: total = 120°
    assert d["end_angle"] - d["start_angle"] == 120


def test_driver_distance_envelope_matches_datasheet(driver_cfg):
    d = driver_cfg["lidar"][0]["driver"]
    assert d["min_distance"] == 0.4
    assert d["max_distance"] == 200.0


def test_driver_topic_layout(driver_cfg):
    ros = driver_cfg["lidar"][0]["ros"]
    assert ros["ros_frame_id"] == "robosense_e1_link"
    assert ros["ros_send_point_cloud_topic"] == "rslidar_points"


# ─── slam_toolbox_e1.yaml ──────────────────────────────────────────────

def test_slam_max_range_bumped_to_200m(slam_cfg):
    p = slam_cfg["slam_toolbox"]["ros__parameters"]
    assert p["max_laser_range"] == 200.0


def test_slam_voxel_filter_5cm_for_high_density(slam_cfg):
    # 921k pts/s firehose needs downsampling before scan-matching.
    p = slam_cfg["slam_toolbox"]["ros__parameters"]
    assert p["voxel_filter_size"] == 0.05


def test_slam_uses_scan_barycenter(slam_cfg):
    p = slam_cfg["slam_toolbox"]["ros__parameters"]
    assert p["use_scan_barycenter"] is True


def test_slam_scan_buffer_size_ten(slam_cfg):
    p = slam_cfg["slam_toolbox"]["ros__parameters"]
    assert p["scan_buffer_size"] == 10


def test_slam_minimum_travel_distance_half_meter(slam_cfg):
    p = slam_cfg["slam_toolbox"]["ros__parameters"]
    assert p["minimum_travel_distance"] == 0.5
    assert p["minimum_travel_heading"] == 0.5


# ─── README content ────────────────────────────────────────────────────

def test_readme_documents_blind_spot_compensation():
    """The cliff-detector reference must stay in the README so a new
    operator sees the rationale for the IMU monitor next to the LiDAR
    config."""
    text = _README.read_text(encoding="utf-8")
    assert "cliff_detector" in text
    assert "120°" in text or "120 deg" in text.lower()


def test_readme_lists_leader_keeps_unitree_l1():
    text = _README.read_text(encoding="utf-8")
    # The Leader doesn't switch to E1 — that's a deliberate platform
    # call (Go2 integrates the L1). README must reflect the decision.
    assert "Unitree L1" in text
    assert "Leader" in text
