"""Tests for iptime_provision.sh (P2-9) — basic invocation."""
from __future__ import annotations

import os
import shutil
import subprocess
from pathlib import Path

import pytest

# Phase 2-E Turn 14-17: tests/ moved to archive/v15_python_prototype/.
# scripts/ moved with it (parent.parent), but docs/ stayed at repo root.
_ARCHIVE_ROOT = Path(__file__).resolve().parent.parent
REPO_ROOT = _ARCHIVE_ROOT.parent.parent  # archive/v15_python_prototype → repo root
SCRIPT = _ARCHIVE_ROOT / "scripts" / "iptime_provision.sh"
DOC = REPO_ROOT / "docs" / "05_Supplementary" / "iptime_setup.md"


def _need_bash():
    if shutil.which("bash") is None:
        pytest.skip("bash not on PATH")


def test_script_exists():
    assert SCRIPT.exists()


def test_help_when_missing_psk():
    """Missing MESH_PSK should fail with usage."""
    _need_bash()
    env = os.environ.copy()
    env.pop("MESH_PSK", None)
    r = subprocess.run(["bash", str(SCRIPT)],
                       capture_output=True, text=True, env=env, timeout=10)
    assert r.returncode != 0
    combined = r.stdout + r.stderr
    assert "MESH_PSK" in combined


def test_doc_exists():
    """Manual fallback doc must exist."""
    assert DOC.exists()
    content = DOC.read_text(encoding="utf-8")
    assert "AX2004M" in content
    assert "IGMP" in content


def test_unix_executable_bit():
    if os.name == "nt":
        pytest.skip("Windows: no exec bit semantics")
    assert os.access(str(SCRIPT), os.X_OK)
