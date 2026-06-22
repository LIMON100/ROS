"""Tests for mission_brief.sh (P2-7) — invocation + arg parsing."""
from __future__ import annotations

import os
import shutil
import subprocess
from pathlib import Path

import pytest

SCRIPT = Path(__file__).resolve().parent.parent / "scripts" / "mission_brief.sh"


def _need_bash():
    if shutil.which("bash") is None:
        pytest.skip("bash not on PATH")


def test_script_exists():
    assert SCRIPT.exists()


def test_help_message():
    _need_bash()
    r = subprocess.run(["bash", str(SCRIPT), "-h"],
                       capture_output=True, text=True, timeout=10)
    combined = (r.stdout + r.stderr).lower()
    assert "usage:" in combined


def test_missing_mission_id_fails():
    _need_bash()
    r = subprocess.run(
        ["bash", str(SCRIPT), "-b", "37,127,38,128"],
        capture_output=True, text=True, timeout=10)
    assert r.returncode != 0
    combined = (r.stdout + r.stderr).lower()
    assert "required" in combined


def test_missing_bbox_and_kml_fails():
    _need_bash()
    r = subprocess.run(
        ["bash", str(SCRIPT), "-m", "M_test"],
        capture_output=True, text=True, timeout=10)
    assert r.returncode != 0


def test_unix_executable_bit():
    """Posix only — Windows filesystems don't carry the executable bit."""
    if os.name == "nt":
        pytest.skip("Windows: no exec bit semantics")
    assert os.access(str(SCRIPT), os.X_OK), "must be executable"
