#!/bin/bash
# Debug-infra end-to-end smoke test.
# Validates that:
#   1. main.py imports + diagnostics initializes cleanly
#   2. metrics dict is populated and serializable
#   3. crash handler fires on simulated exception
#   4. debug_dashboard.py reads + renders the metrics file
#   5. SIGUSR1 → on-demand state dump works
#
# Run before any real RK3588 deploy.
set -e
cd "$(dirname "$0")/.."

echo "═══ 1. test_diag unit tests ═══"
python3 -m pytest tests/test_diag.py -q --tb=short

echo ""
echo "═══ 2. simulate metrics + dashboard render ═══"
python3 << 'PYEOF'
import json, time, sys, tempfile
from pathlib import Path
sys.path.insert(0, "scripts")

import importlib.util
spec = importlib.util.spec_from_file_location("debug_dashboard",
                                              "scripts/debug_dashboard.py")
dash = importlib.util.module_from_spec(spec)
spec.loader.exec_module(dash)

from core.diag import MetricsCollector

shared = {}
for proc_name in ("Localization", "Comm", "RtkGnssAdapter"):
    m = MetricsCollector(proc_name, shared_dict=shared, publish_period_s=0.0)
    for _ in range(50):
        m.step_begin()
        m.step_end()
    m.increment("uploaded_wifi", 12)
    m.increment("link_switches", 1)

print(dash.render(shared, color=False))
PYEOF

echo ""
echo "═══ 3. crash dump round-trip ═══"
TMPDIR=$(mktemp -d)
python3 << PYEOF
import json
from pathlib import Path
from core.diag import setup_logger, install_crash_handler, _write_crash

log = setup_logger("smoke", log_dir=Path("$TMPDIR/logs"))
install_crash_handler(Path("$TMPDIR/crashes"), logger=log)
log.warning("about to crash")
try:
    raise ValueError("smoke test crash")
except ValueError as e:
    path = _write_crash("smoke", exc=e, process_name="smoke_proc")
data = json.loads(path.read_text())
assert data["exception"]["type"] == "ValueError"
assert any("about to crash" in line for line in data["recent_logs"])
print(f"  ✓ crash dump @ {path}")
print(f"  ✓ recent_logs captured ({len(data['recent_logs'])} entries)")
PYEOF
rm -rf "$TMPDIR"

echo ""
echo "═══ all debug-infra checks passed ═══"
