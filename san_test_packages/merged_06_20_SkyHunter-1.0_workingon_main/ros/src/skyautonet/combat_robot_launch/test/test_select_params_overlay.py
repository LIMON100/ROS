"""SAN v1.3 PHASE 0 - select_params_overlay unit test (ament_python)."""

import pytest
from launch.combat_robot_device import select_params_overlay


def test_production_returns_no_overlay():
    assert select_params_overlay("production") is None


def test_demo_returns_demo_yaml():
    assert select_params_overlay("demo") == "params.demo.yaml"


def test_lab_test_returns_lab_test_yaml():
    assert select_params_overlay("lab_test") == "params.lab_test.yaml"


def test_bench_returns_bench_yaml():
    assert select_params_overlay("bench") == "params.bench.yaml"


def test_development_returns_development_yaml():
    assert select_params_overlay("development") == "params.development.yaml"


def test_invalid_mode_raises():
    with pytest.raises(RuntimeError, match="Invalid deployment_mode"):
        select_params_overlay("invalid_mode")
