"""Hub UGV SBC#2 Comm + video-relay container entry point.

Loads the GStreamerRelay module graph + warns if gst-launch-1.0 isn't
on PATH. The real process wiring (tablet WS endpoint, follower-bound
start_cmd publisher, VideoRequest→relay dispatch, VideoResponse publish
on tablet_video_response queue) is a follow-up PR.

Set `--smoke` to import + log + exit 0 (used by tests).
"""
from __future__ import annotations

import argparse
import logging
import os
import shutil
import signal
import sys
import threading
from typing import Optional

from core.schema_version import LOCAL_VERSION  # noqa: F401  (load check)

# Import the modules the future process will actually use — fail at
# container start if the build is missing GStreamer or libsrt.
from streaming.gstreamer_relay import (  # noqa: F401  (load check)
    BASE_SRT_PORT_HUB_TO_TABLET,
    BASE_UDP_PORT_FOLLOWER_TO_HUB,
    DEFAULT_SRT_LATENCY_MS,
    GStreamerRelay,
)

_LOG_FORMAT = "%(asctime)s.%(msecs)03d %(levelname)s %(name)s: %(message)s"
_DEFAULT_HUB_IP = "10.0.0.99"


def _parse_args(argv: Optional[list] = None) -> argparse.Namespace:
    p = argparse.ArgumentParser(prog="run_hub_comm",
                                 description="Hub SBC#2 comm entry point")
    p.add_argument("--smoke", action="store_true",
                    help="Verify imports + log banner, then exit 0.")
    return p.parse_args(argv)


def _setup_logging() -> logging.Logger:
    logging.basicConfig(
        level=logging.INFO, format=_LOG_FORMAT, datefmt="%H:%M:%S")
    return logging.getLogger("hub-comm")


def _log_banner(log: logging.Logger) -> None:
    hub_ip = os.environ.get("HUB_EXTERNAL_IP", _DEFAULT_HUB_IP)
    log.info("Hub UGV SBC#2 comm container — schema %s, role=%s, hub_ip=%s",
              LOCAL_VERSION, os.environ.get("HUB_ROLE", "unset"), hub_ip)
    log.info(
        "GStreamerRelay ports: udp_base=%d, srt_base=%d, srt_latency=%dms",
        BASE_UDP_PORT_FOLLOWER_TO_HUB, BASE_SRT_PORT_HUB_TO_TABLET,
        DEFAULT_SRT_LATENCY_MS)
    if shutil.which("gst-launch-1.0") is None:
        # The Dockerfile installs gstreamer1.0-tools, so a missing
        # binary at runtime means the image wasn't built from
        # Dockerfile.hub-comm — fail loudly.
        log.warning("gst-launch-1.0 not on PATH — relay will fail to spawn")
    else:
        log.info("gst-launch-1.0 detected")
    # Build a relay to confirm wiring; throw it away.
    GStreamerRelay(hub_external_ip=hub_ip)
    log.warning(
        "Process wiring TODO — drain queues.tablet_video_request + "
        "publish queues.tablet_video_response + follower start cmds "
        "(follow-up PR)")


def _block_until_signalled(log: logging.Logger) -> int:
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
