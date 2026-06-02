"""Smoke + assertion tests for the v1.1 acceptance scripts.

These cover scripts/kpp_report.py and scripts/bandwidth_compare.py — the
PHASE 10 deliverables that close out the SAN-TST-INT-001 v1.1 §7 ledger.
Both scripts are imported directly (not subprocess'd) so the assertions
work the same on Windows + Linux CI without console-encoding noise.
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

import pytest

REPO_ROOT = Path(__file__).resolve().parent.parent
SCRIPTS = REPO_ROOT / "scripts"
if str(SCRIPTS) not in sys.path:
    sys.path.insert(0, str(SCRIPTS))

import bandwidth_compare  # noqa: E402
import kpp_report  # noqa: E402

# ─── kpp_report.py ─────────────────────────────────────────────────────

def test_kpp_report_renders_with_no_measured_input(tmp_path):
    """Missing input JSON → declared-only mode still produces HTML."""
    out = tmp_path / "report.html"
    rc = kpp_report.main([
        "--input", str(tmp_path / "does-not-exist.json"),
        "--output", str(out),
    ])
    assert rc == 0
    body = out.read_text(encoding="utf-8")
    assert "<title>v1.1 KPP Acceptance Report</title>" in body
    # Declared rows from source-of-truth modules should be present.
    assert "KPP-7" in body
    assert "S15-1" in body
    assert "S15-3" in body
    assert "S15-4" in body


def test_kpp_report_includes_measured_rows(tmp_path):
    measured = {
        "KPP-1": {"measured": 0.5, "threshold": 2.0,
                  "passed": True, "unit": "m", "name": "Formation error"},
        "KPP-X": {"measured": 99, "threshold": 50,
                  "passed": False, "unit": "ms", "name": "Synthetic fail"},
    }
    in_path = tmp_path / "kpp.json"
    in_path.write_text(json.dumps(measured), encoding="utf-8")
    out = tmp_path / "report.html"
    rc = kpp_report.main([
        "--input", str(in_path),
        "--output", str(out),
    ])
    assert rc == 0
    body = out.read_text(encoding="utf-8")
    assert "Formation error" in body
    assert "Synthetic fail" in body
    assert "PASS" in body and "FAIL" in body


def test_kpp_report_deferred_rows_only_when_flag_set(tmp_path):
    out_a = tmp_path / "a.html"
    out_b = tmp_path / "b.html"
    kpp_report.main(["--input", str(tmp_path / "x"), "--output", str(out_a)])
    kpp_report.main(["--input", str(tmp_path / "x"), "--output", str(out_b),
                     "--include-deferred"])
    body_a = out_a.read_text(encoding="utf-8")
    body_b = out_b.read_text(encoding="utf-8")
    # The footer mentions LIVE-ONLY in explanatory prose; gate on the
    # badge form so we're actually checking row presence.
    assert ">LIVE-ONLY<" not in body_a
    assert ">LIVE-ONLY<" in body_b
    assert "S15-2" in body_b and "S15-5" in body_b and "S15-6" in body_b


def test_kpp_report_rosbag_flag_passes_through_as_label(tmp_path):
    out = tmp_path / "r.html"
    kpp_report.main(["--input", str(tmp_path / "x"),
                     "--output", str(out),
                     "--rosbag", "latest"])
    body = out.read_text(encoding="utf-8")
    assert "rosbag tag: latest" in body


# ─── bandwidth_compare.py ──────────────────────────────────────────────

def test_bandwidth_compare_default_inputs_match_adr_002_target():
    """ADR-002 promises ≥100× reduction in default mode. Default-flag
    inputs should comfortably clear that bar — if a future regression
    bumps the cadence or payload estimate to violate it, this test
    fails."""
    md = bandwidth_compare.build_markdown(
        baseline_rate_hz=bandwidth_compare.DEFAULT_BASELINE_RATE_HZ,
        baseline_payload_bytes=bandwidth_compare.DEFAULT_BASELINE_PAYLOAD_BYTES,
        current_payload_bytes=bandwidth_compare.DEFAULT_CURRENT_PAYLOAD_BYTES,
        baseline_label="v1.0",
        current_label="v1.1",
    )
    assert "ADR-002" in md
    # Default-mode line must show PASS, not FAIL.
    assert "Default-mode reduction" in md
    assert "**PASS**" in md
    assert "**FAIL**" not in md


def test_bandwidth_compare_fail_path_triggers_when_payloads_match():
    """Force a degenerate baseline that doesn't beat the target —
    confirm the script reports FAIL instead of swallowing it."""
    md = bandwidth_compare.build_markdown(
        baseline_rate_hz=1.0,
        baseline_payload_bytes=32_768,    # same payload as v1.1
        current_payload_bytes=32_768,
        baseline_label="synthetic",
        current_label="synthetic",
    )
    # At 30s cadence the ratio is 30×, below the 100× ADR target.
    assert "**FAIL**" in md
    assert "Below the ADR-002 target" in md


def test_bandwidth_compare_each_mode_row_present():
    md = bandwidth_compare.build_markdown(
        baseline_rate_hz=1.0,
        baseline_payload_bytes=3_200_000,
        current_payload_bytes=32_768,
    )
    for mode in ("wide", "default", "narrow", "obstacle"):
        assert f"`{mode}`" in md, f"missing row for mode={mode}"


def test_bandwidth_compare_writes_file(tmp_path):
    out = tmp_path / "bw.md"
    rc = bandwidth_compare.main([
        "--baseline", "v1.0_baseline.bag",
        "--current",  "latest.bag",
        "--output",   str(out),
    ])
    assert rc == 0
    body = out.read_text(encoding="utf-8")
    assert body.startswith("# SLAM aggregation bandwidth")
    assert "v1.0_baseline.bag" in body
    assert "latest.bag" in body


# ─── module-level helpers ──────────────────────────────────────────────

def test_baseline_bytes_per_sec_pure_function():
    assert bandwidth_compare.baseline_bytes_per_sec(1.0, 1000) == 1000
    assert bandwidth_compare.baseline_bytes_per_sec(2.0, 500) == 1000


@pytest.mark.parametrize(
    "rate, baseline_payload, period, current_payload, expected_ratio",
    [
        # Default v1.1 vs documented v1.0 envelope (ADR-002).
        (1.0, 3_200_000, 30.0, 32_768, pytest.approx(2929.6875, rel=1e-3)),
        # Degenerate equal-rate case: ratio = 1.
        (1.0, 1000,       1.0,  1000,  pytest.approx(1.0)),
    ],
)
def test_reduction_math(rate, baseline_payload, period,
                        current_payload, expected_ratio):
    baseline = bandwidth_compare.baseline_bytes_per_sec(rate, baseline_payload)
    report = bandwidth_compare.ModeReport(
        mode="x", period_sec=period, payload_bytes=current_payload)
    ratio = bandwidth_compare.reductions(baseline, [report])[0]
    assert ratio == expected_ratio
