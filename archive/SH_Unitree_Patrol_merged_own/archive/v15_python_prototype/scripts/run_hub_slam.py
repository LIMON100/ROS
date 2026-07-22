"""Hub UGV SBC#1 SLAM-container entry point.

This script is the container's PID 1. It proves the SlamAggregator
module graph loads inside the built image and blocks on SIGTERM so the
container stays up. Real process wiring (queue drain + AggregatedMap
publish + follower-loss timer + force_event on formation change) lands
in a follow-up PR — this entry point is intentionally inert so
`docker compose up` succeeds today without surprise side effects.

Set `--smoke` to import + log + exit 0 (used by tests).
"""
from __future__ import annotations

import argparse
import logging
import os
import signal
import sys
from typing import Optional

from core.schema_version import LOCAL_VERSION  # noqa: F401  (load check)

# Import the modules the future process will actually use — a failure
# here means the image is broken (e.g. Pillow missing) and surfaces at
# container start rather than at first message.
from mapping.slam_aggregator import (  # noqa: F401  (load check)
    MODE_DEFAULT,
    PERIOD_BY_MODE,
    SlamAggregator,
)

_LOG_FORMAT = "%(asctime)s.%(msecs)03d %(levelname)s %(name)s: %(message)s"


def _parse_args(argv: Optional[list] = None) -> argparse.Namespace:
    p = argparse.ArgumentParser(prog="run_hub_slam",
                                 description="Hub SBC#1 SLAM entry point")
    p.add_argument("--smoke", action="store_true",
                    help="Verify imports + log banner, then exit 0.")
    return p.parse_args(argv)


def _setup_logging() -> logging.Logger:
    logging.basicConfig(
        level=logging.INFO, format=_LOG_FORMAT, datefmt="%H:%M:%S")
    return logging.getLogger("hub-slam")


def _log_banner(log: logging.Logger) -> None:
    log.info("Hub UGV SBC#1 SLAM container — schema %s, role=%s",
              LOCAL_VERSION, os.environ.get("HUB_ROLE", "unset"))
    # Construct an aggregator to confirm everything wires; discard it.
    agg = SlamAggregator(mode=MODE_DEFAULT)
    log.info(
        "SlamAggregator ready: mode=%s, period=%.0fs, periods_by_mode=%s",
        agg.mode, agg.period_s, dict(PERIOD_BY_MODE))
    log.warning(
        "Process wiring TODO — drain queues.slam_local_delta + "
        "publish queues.hub_aggregated_map (follow-up PR)")


def _block_until_signalled(log: logging.Logger) -> int:
    """Sleep until SIGTERM / SIGINT — keeps the container alive."""
    import threading
    latch = threading.Event()

    def _handler(signum, frame):
        log.info("received signal %s, shutting down", signum)
        latch.set()

    signal.signal(signal.SIGTERM, _handler)
    signal.signal(signal.SIGINT, _handler)
    latch.wait()
    return 0


def main(argv: Optional[list] = None) -> int:
    args = _parse_args(argv)
    log = _setup_logging()
    _log_banner(log)
    if args.smoke:
        log.info("smoke mode — exiting 0")
        return 0
    return _block_until_signalled(log)


if __name__ == "__main__":
    sys.exit(main())
