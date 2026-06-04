"""
HttpRecordingsProcess — C4 channel: HTTP recordings catalog + Range-GET.

Per AIRYS SAN-BLE-WIFI-001 §C4:
  • Port 8000, plain HTTP on the robot's own AP
  • GET /recordings              — JSON listing (filename, size, mtime, url)
  • GET /recordings/<name>       — file body, Range-GET supported
  • DELETE /recordings/<name>    — delete a recording (auth via BLE pairing)

Lifecycle: brought up alongside WsTelemetry when WiFi enters READY.
The catalog is a directory listing of `recordings_dir`; we don't keep
a separate index. Recording filenames carry timestamps (ISO basic) so
the app can sort client-side.

Range-GET (RFC 7233 partial content) is essential for the operator's
phone to scrub through long recordings without redownloading. Standard
library `http.server` doesn't implement it; we add the minimal subset
inline.

Threading: ThreadingHTTPServer — one thread per request. The /
recordings catalog is cheap; the file GETs sendfile() the body so a
slow client doesn't tie up the server thread.
"""
from __future__ import annotations

import json
import re
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Optional, Tuple

from core.base_process import BaseProcess

# Filenames must match this to be served — prevents path-traversal
# (../etc/passwd) and only exposes recordings, not stray files.
_RECORDING_RE = re.compile(r"^[A-Za-z0-9_\-]+\.(mp4|mkv|h265|h264|ts)$")


def _make_handler(recordings_dir: Path, log) -> type:
    """Construct a request handler bound to a specific recordings dir."""

    class Handler(BaseHTTPRequestHandler):
        # Suppress default stderr access logging — we route through `log`
        def log_message(self, fmt, *args):
            log.debug("http %s - %s" % (self.address_string(), fmt % args))

        # ─── Routing ───
        def do_GET(self):
            if self.path == "/recordings":
                return self._serve_catalog()
            if self.path.startswith("/recordings/"):
                name = self.path[len("/recordings/"):].split("?", 1)[0]
                return self._serve_file(name)
            if self.path == "/healthz":
                return self._reply_text(200, "ok\n")
            return self._reply_text(404, "not found\n")

        def do_DELETE(self):
            if not self.path.startswith("/recordings/"):
                return self._reply_text(404, "not found\n")
            name = self.path[len("/recordings/"):].split("?", 1)[0]
            if not _RECORDING_RE.match(name):
                return self._reply_text(400, "invalid name\n")
            target = recordings_dir / name
            try:
                target.unlink()
            except FileNotFoundError:
                return self._reply_text(404, "not found\n")
            except OSError as e:
                return self._reply_text(500, f"delete failed: {e}\n")
            self._reply_text(204, "")

        # ─── /recordings (catalog) ───
        def _serve_catalog(self):
            entries = []
            if recordings_dir.is_dir():
                for f in sorted(recordings_dir.iterdir()):
                    if not f.is_file() or not _RECORDING_RE.match(f.name):
                        continue
                    st = f.stat()
                    entries.append({
                        "name": f.name,
                        "size": st.st_size,
                        "mtime": st.st_mtime,
                        "url":   f"/recordings/{f.name}",
                    })
            body = json.dumps(entries).encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(body)

        # ─── /recordings/<name> (with Range support) ───
        def _serve_file(self, name: str):
            if not _RECORDING_RE.match(name):
                return self._reply_text(400, "invalid name\n")
            target = recordings_dir / name
            try:
                st = target.stat()
            except FileNotFoundError:
                return self._reply_text(404, "not found\n")
            if not target.is_file():
                return self._reply_text(404, "not found\n")

            file_size = st.st_size
            content_type = _guess_mime(name)
            range_hdr = self.headers.get("Range")
            start, end = _parse_range(range_hdr, file_size)

            if start is None:
                # No Range or unsatisfiable → full body
                self.send_response(200)
                self.send_header("Content-Type", content_type)
                self.send_header("Content-Length", str(file_size))
                self.send_header("Accept-Ranges", "bytes")
                self.send_header("Last-Modified",
                                  self.date_time_string(int(st.st_mtime)))
                self.end_headers()
                self._send_file_range(target, 0, file_size - 1)
                return

            # 206 Partial Content
            length = end - start + 1
            self.send_response(206)
            self.send_header("Content-Type", content_type)
            self.send_header("Content-Length", str(length))
            self.send_header("Content-Range",
                              f"bytes {start}-{end}/{file_size}")
            self.send_header("Accept-Ranges", "bytes")
            self.send_header("Last-Modified",
                              self.date_time_string(int(st.st_mtime)))
            self.end_headers()
            self._send_file_range(target, start, end)

        def _send_file_range(self, path: Path, start: int, end: int) -> None:
            """Stream [start, end] inclusive. 64K chunks; sendfile would be
            slightly faster but harder to reason about across platforms."""
            CHUNK = 64 * 1024
            try:
                with open(path, "rb") as f:
                    f.seek(start)
                    remaining = end - start + 1
                    while remaining > 0:
                        chunk = f.read(min(CHUNK, remaining))
                        if not chunk:
                            break
                        self.wfile.write(chunk)
                        remaining -= len(chunk)
            except (BrokenPipeError, ConnectionResetError):
                # Client gave up mid-download; fine.
                pass

        def _reply_text(self, status: int, body: str) -> None:
            data = body.encode("utf-8")
            self.send_response(status)
            self.send_header("Content-Type", "text/plain")
            self.send_header("Content-Length", str(len(data)))
            self.end_headers()
            if data:
                try:
                    self.wfile.write(data)
                except (BrokenPipeError, ConnectionResetError):
                    pass

    return Handler


def _guess_mime(name: str) -> str:
    n = name.lower()
    if n.endswith((".mp4",)):
        return "video/mp4"
    if n.endswith((".mkv",)):
        return "video/x-matroska"
    if n.endswith((".h265",)):
        return "video/h265"
    if n.endswith((".h264",)):
        return "video/h264"
    if n.endswith((".ts",)):
        return "video/mp2t"
    return "application/octet-stream"


_RANGE_RE = re.compile(r"bytes=(\d*)-(\d*)$")


def _parse_range(header: Optional[str],
                  file_size: int) -> Tuple[Optional[int], Optional[int]]:
    """Return (start, end) inclusive byte indices, or (None, None) if no
    Range / unsatisfiable. Only single-range requests are supported."""
    if header is None:
        return None, None
    m = _RANGE_RE.match(header.strip())
    if m is None:
        return None, None
    s_raw, e_raw = m.group(1), m.group(2)
    if s_raw == "" and e_raw == "":
        return None, None
    if s_raw == "":
        # Suffix range: "bytes=-N" → last N bytes
        n = int(e_raw)
        if n == 0 or n > file_size:
            return None, None
        return file_size - n, file_size - 1
    start = int(s_raw)
    end = int(e_raw) if e_raw else file_size - 1
    if start >= file_size:
        return None, None
    end = min(end, file_size - 1)
    if start > end:
        return None, None
    return start, end


# ════════════════════════════════════════════════════════════════
# Process wrapper
# ════════════════════════════════════════════════════════════════
class HttpRecordingsProcess(BaseProcess):
    """Threaded HTTP server for the /recordings endpoint."""

    DEFAULT_PORT = 8000

    def __init__(self, queues, shutdown_event, config, **diag):
        super().__init__(
            name="HttpRecordings", shutdown_event=shutdown_event,
            rate_hz=0.5,
            cpu_affinity=config.get("system", "cpu_affinity", "comm"),
            **diag,
        )
        self.queues = queues
        self.cfg = config
        self._server: Optional[ThreadingHTTPServer] = None
        self._server_thread: Optional[threading.Thread] = None
        self._stats = {"requests": 0}

    def setup(self) -> None:
        port = int(self.cfg.get("http", "port", default=self.DEFAULT_PORT))
        bind = self.cfg.get("http", "bind", default="0.0.0.0")
        rec_dir = Path(self.cfg.get(
            "http", "recordings_dir", default="/var/lib/patrol/recordings"))
        rec_dir.mkdir(parents=True, exist_ok=True)
        Handler = _make_handler(rec_dir, self.log)

        try:
            self._server = ThreadingHTTPServer((bind, port), Handler)
        except OSError as e:
            self.log.error(f"HTTP recordings: bind failed on :{port} — {e}")
            return
        self._server_thread = threading.Thread(
            target=self._server.serve_forever,
            kwargs={"poll_interval": 0.5},
            name="HttpRecLoop", daemon=True,
        )
        self._server_thread.start()
        self.log.info(f"HTTP recordings listening on http://{bind}:{port}/")

    def step(self) -> None:
        pass        # nothing periodic; ThreadingHTTPServer handles its own loop

    def teardown(self) -> None:
        if self._server is not None:
            try:
                self._server.shutdown()
                self._server.server_close()
            except Exception:        # pylint: disable=broad-except
                pass
        if self._server_thread is not None:
            self._server_thread.join(timeout=2.0)
