"""
PerceptionProcess: camera frames → NPU inference → AnomalyEvents.

Threads:
  • CameraConsumer    : pull CameraFrameRef, decode (HW codec when available)
  • PpeWorker         : NPU core 0, PPE detection
  • HazardWorker      : NPU core 1, hazard detection
  • Aggregator        : merge results, emit AnomalyEvent

Decoding strategy (RK3588):
  • H.265 frames from Go2 → use RGA / MPP HW decoder when available
  • Fallback: PyAV / opencv (CPU)
"""
from __future__ import annotations

import queue
import threading
from typing import List, Optional

import numpy as np

from control.pantilt_controller import DEFAULT_HFOV_DEG, PanTiltController
from core.audit_log import publish_audit
from core.base_process import BaseProcess
from core.ipc import consume, publish
from core.messages import (
    AnomalyEvent,
    Detection,
    Header,
    Pose6D,
    SectorAssign,
    ThermalFrameRef,
)
from core.permission_guard import GuardViolation, PermissionGuard, guarded_publish
from core.shm_pool import ShmPool
from swarm.sector_assign import filter_for_robot

from .rknn_inference import RawDetection, RknnRunner
from .thermal_rgb_fusion import (
    CameraCalib,
    FuserConfig,
    StereoExtrinsic,
    ThermalRgbFuser,
)


class PerceptionProcess(BaseProcess):
    # ── Domain: 군집제어 소형전술 로봇 ──
    # Single general-purpose detector (YOLOv5 COCO) replaces the previous
    # two-model PPE+Hazard split. Severity is derived from the detected
    # class set rather than per-model semantics.
    #
    # Personnel/vehicle classes are flagged at "warning"; multiple personnel
    # in close formation are flagged at "critical".
    PERSONNEL_CLASSES = {0}                    # COCO: person
    VEHICLE_CLASSES   = {1, 2, 3, 5, 7}        # bicycle, car, motorcycle, bus, truck

    def __init__(self, queues, shutdown_event, config, camera_shm: ShmPool,
                 **diag):
        super().__init__(
            name="Perception",
            shutdown_event=shutdown_event,
            rate_hz=2.0,
            cpu_affinity=config.get("system", "cpu_affinity", "perception"),
            **diag,
        )
        self.queues = queues
        self.cfg = config
        self.camera_shm = camera_shm

        self._lock: threading.Lock = None
        self._latest_pose: Pose6D = None
        self._detector: Optional[RknnRunner] = None
        self._classes_of_interest: Optional[set] = None
        self._frame_queue: queue.Queue = None      # decoded frames in
        self._results_queue: queue.Queue = None    # detections out
        self._stats = {"frames": 0, "detections": 0, "anomalies": 0,
                       "guard_blocks": 0, "sector_updates": 0,
                       "pantilt_cmds": 0}
        # Active surveillance sector assigned by the swarm leader. Updated
        # by _sector_assign_consumer; pan-tilt controller derives head
        # commands from each accepted sector.
        self._active_sector: Optional[SectorAssign] = None
        self._my_robot_id: int = 0
        # PanTiltController instance — initialized in setup() once
        # robot_id + HFOV have been read from config.
        self._pantilt: Optional[PanTiltController] = None
        # AI permission tripwire (SDD §8, P2-14). Constructed eagerly so
        # tests that build this process via __new__ + bypass setup() still
        # find a guard to interrogate. The audit callback fires only on
        # BLOCK decisions and lands in the system-wide audit_event bus
        # via publish_audit().
        self._guard = PermissionGuard(audit_callback=self._on_guard_violation)

    def setup(self) -> None:
        self._lock = threading.Lock()
        self._frame_queue = queue.Queue(maxsize=4)
        self._results_queue = queue.Queue(maxsize=8)

        det_hw = self.cfg.get("perception", "detector_input_hw",
                               default=[640, 640])
        self._detector = RknnRunner(
            model_path=self.cfg.get("perception", "detector_model_path"),
            core="core0",
            input_size=tuple(det_hw),
        )
        try:
            self._detector.load()
        except Exception as e:
            self.log.warning(
                f"RKNN load failed for {self._detector.model_path}: {e}")
        coi = self.cfg.get("perception", "classes_of_interest",
                            default=[0, 1, 2, 3, 5, 7])
        self._classes_of_interest = set(coi)

        # Thermal-RGB fuser — calibrations come from config (with sane defaults)
        rgb_calib = CameraCalib(
            fx=self.cfg.get("perception", "rgb_fx", default=2400.0),
            fy=self.cfg.get("perception", "rgb_fy", default=2400.0),
            cx=self.cfg.get("perception", "rgb_cx", default=1920.0),
            cy=self.cfg.get("perception", "rgb_cy", default=1080.0),
            width=3840, height=2160,
        )
        thermal_calib = CameraCalib(
            fx=self.cfg.get("perception", "thermal_fx", default=540.0),
            fy=self.cfg.get("perception", "thermal_fy", default=540.0),
            cx=self.cfg.get("perception", "thermal_cx", default=320.0),
            cy=self.cfg.get("perception", "thermal_cy", default=256.0),
            width=640, height=512,
        )
        # 5 cm horizontal baseline (thermal mounted to right of RGB)
        baseline = self.cfg.get("perception", "thermal_baseline_m", default=0.05)
        extrinsic = StereoExtrinsic(
            R=np.eye(3),
            t=np.array([baseline, 0.0, 0.0], dtype=np.float64),
        )
        self._fuser = ThermalRgbFuser(
            cfg=FuserConfig(
                rgb_calib=rgb_calib,
                thermal_calib=thermal_calib,
                extrinsic=extrinsic,
                sync_tolerance_s=0.10,
                default_depth_m=5.0,
            ),
        )

        self._my_robot_id = int(self.cfg.get("system", "robot_id", default=0))
        hfov_deg = float(self.cfg.get(
            "perception", "rgb_hfov_deg", default=DEFAULT_HFOV_DEG))
        self._pantilt = PanTiltController(
            robot_id=self._my_robot_id, hfov_deg=hfov_deg)

        self.spawn_thread(self._pose_sub,        name="PoseSub")
        self.spawn_thread(self._camera_consumer, name="CamCnsm")
        self.spawn_thread(self._thermal_consumer, name="ThermalCnsm")
        self.spawn_thread(self._detector_worker, name="DetectorWorker")
        self.spawn_thread(self._aggregator,      name="Aggregator")
        self.spawn_thread(self._sector_assign_consumer, name="SectorSub")

    def step(self) -> None:
        s = self._stats
        self.log.info(
            f"perception  frames={s['frames']} detections={s['detections']} "
            f"anomalies={s['anomalies']}"
        )

    def teardown(self) -> None:
        if self._detector:
            self._detector.close()

    # ─── workers ───
    def _pose_sub(self):
        while self.is_running():
            p = consume(self.queues.pose, timeout=0.05)
            if p is not None:
                self._latest_pose = p

    def _sector_assign_consumer(self):
        """Drain sw_sector_assign, keep only assignments for this robot.

        Each fan-out from the leader emits one message per follower; we
        filter by robot_id and drop expired ones. Camera-control hookup
        is deferred — for now we just cache and log transitions.
        """
        import time as _time
        while self.is_running():
            msg = consume(self.queues.sw_sector_assign, timeout=0.5)
            if msg is None:
                continue
            mine = filter_for_robot(
                msg, self._my_robot_id, now_ms=int(_time.time() * 1000))
            if mine is None:
                continue
            prev = self._active_sector
            self._active_sector = mine
            self._stats["sector_updates"] += 1
            changed = (
                prev is None
                or prev.sector_start_deg != mine.sector_start_deg
                or prev.sector_end_deg   != mine.sector_end_deg
                or prev.priority         != mine.priority
                or prev.mode_hint        != mine.mode_hint)
            if changed:
                self.log.info(
                    f"sector: [{mine.sector_start_deg:+.1f}°, "
                    f"{mine.sector_end_deg:+.1f}°] "
                    f"prio={mine.priority} mode={mine.mode_hint} "
                    f"seq={mine.sequence}")
            if self._pantilt is not None and changed:
                cmd = self._pantilt.on_sector(
                    mine, now_ms=int(_time.time() * 1000))
                if cmd is not None:
                    publish(self.queues.pantilt_command, cmd)
                    self._stats["pantilt_cmds"] += 1

    def _camera_consumer(self):
        min_conf = float(self.cfg.get("perception", "min_confidence", default=0.45))
        while self.is_running():
            ref = consume(self.queues.camera_ref, timeout=0.1)
            if ref is None:
                continue
            try:
                shm = ShmPool.attach(ref.shm_name)
                # Real: HW decode H.265 → RGB/BGR ndarray (H, W, 3)
                # Stub: zero image
                img = np.zeros((ref.height, ref.width, 3), dtype=np.uint8)
                shm.close()
                # broadcast to both workers via dedicated channel pattern
                pkg = {"img": img, "ref": ref, "min_conf": min_conf,
                       "pose": self._latest_pose}
                try:
                    self._frame_queue.put_nowait(pkg)
                    self._stats["frames"] += 1
                except queue.Full:
                    pass
            finally:
                self.camera_shm.release(ref.shm_name)

    def _detector_worker(self):
        """Single general-purpose detector worker.

        Pulls frames from the camera queue, runs the YOLOv5 RKNN inference
        with the configured class filter, and pushes results to the
        aggregator. Replaces the previous ppe/hazard split.
        """
        while self.is_running():
            try:
                pkg = self._frame_queue.get(timeout=0.1)
            except queue.Empty:
                continue
            dets = self._detector.infer(
                pkg["img"], pkg["min_conf"],
                classes_of_interest=self._classes_of_interest,
            )
            self._push_results("detection", dets, pkg)

    def _push_results(self, category: str, dets: List[RawDetection], pkg: dict):
        if not dets:
            return
        self._results_queue.put({"category": category, "dets": dets, "pkg": pkg})
        self._stats["detections"] += len(dets)

    def _thermal_consumer(self):
        """Decode thermal frames into the fuser's time-sync buffer.

        Reads ThermalFrameRef from the queue, fetches the mono16 raw bytes
        from SHM, reshapes into (H, W) uint16, hands to fuser. Decoupled
        from RGB inference so a slow PPE/hazard run won't drop thermal sync.
        """
        while self.is_running():
            ref: ThermalFrameRef = consume(self.queues.thermal_ref, timeout=0.1)
            if ref is None:
                continue
            try:
                raw = self.camera_shm.read(ref.shm_name, ref.nbytes)
            except Exception as e:
                self.log.warning(f"thermal SHM read failed: {e}")
                continue
            if raw is None:
                continue
            # Decode mono16. STUB sends 2KB placeholder — guard against that.
            expected = ref.width * ref.height * 2
            if len(raw) < expected:
                # STUB or truncated frame: synthesize a uniform field at room temp
                arr = np.full((ref.height, ref.width), 32768, dtype=np.uint16)
            else:
                arr = np.frombuffer(raw[:expected], dtype=np.uint16) \
                        .reshape(ref.height, ref.width)
            self._fuser.add_thermal_frame(ref.header.stamp, arr)

    def _aggregator(self):
        while self.is_running():
            try:
                item = self._results_queue.get(timeout=0.1)
            except queue.Empty:
                continue
            cat = item["category"]
            dets = item["dets"]
            pose = item["pkg"]["pose"]
            ref = item["pkg"]["ref"]
            severity = self._severity_for(cat, dets)

            # Enrich detections with thermal stats when fuser has synced data
            enriched: List[Detection] = []
            thermal_hits = 0
            for d in dets:
                bbox = tuple(int(v) for v in d.bbox)
                stats = self._fuser.enrich(
                    rgb_stamp=ref.header.stamp,
                    bbox_rgb=bbox,
                )
                desc = d.label
                if stats is not None:
                    thermal_hits += 1
                    desc = (f"{d.label} (thermal "
                            f"{stats['min_c']:.1f}/{stats['mean_c']:.1f}/"
                            f"{stats['max_c']:.1f}°C)")
                enriched.append(Detection(
                    label=desc, confidence=d.confidence,
                    bbox=d.bbox, pose_at_detect=pose,
                ))

            ev = AnomalyEvent(
                header=Header.now(frame_id="map"),
                severity=severity,
                category=cat,
                description=self._describe(cat, dets),
                detections=enriched,
                image_ref=ref,
            )
            # All AI outputs route through the permission guard. For
            # `anomaly` (an AI-permitted topic) this is a pass-through;
            # the value is that any future bug introducing a publish to
            # cmd_vel / leader_pose / etc. from this process would be
            # caught and audited at this gate.
            ok = guarded_publish(
                self._guard, self.queues, "anomaly", ev,
                source="perception_ai")
            if ok:
                self._stats["anomalies"] += 1
            else:
                self._stats["guard_blocks"] += 1
            if thermal_hits > 0:
                self._stats.setdefault("thermal_enriched", 0)
                self._stats["thermal_enriched"] += thermal_hits

    # ─── Permission guard plumbing ───
    def _on_guard_violation(self, v: GuardViolation) -> None:
        """PermissionGuard audit callback. Fires only on BLOCK decisions
        (the ALLOW path never invokes this). Pushed onto the system
        audit bus so violations land in the hash-chained log alongside
        PIN-auth events; the local _stats counter mirrors the count for
        per-process observability."""
        try:
            publish_audit(
                self.queues, category="permission",
                event="guard_block",
                actor=v.source,
                params={
                    "topic": v.topic,
                    "reason": v.reason,
                })
        except Exception as e:
            # PermissionGuard.check() already wraps audit_callback in a
            # try/except so a raise here would just be swallowed — but
            # emitting a log line is still useful for live triage.
            if self.log:
                self.log.error(
                    f"guard violation audit publish failed: {e}")

    @staticmethod
    def _severity_for(cat: str, dets: List[RawDetection]) -> str:
        """Severity rule for general object detection in tactical recon.

        • "info"     — single non-personnel detection (vehicle, etc.)
        • "warning"  — at least one personnel (COCO class 0)
        • "critical" — multiple personnel in close proximity (potential
                        contact group); proximity is approximated by counting
                        person-class detections in the same frame.
        """
        person_count = sum(1 for d in dets if d.class_id == 0)
        if person_count >= 3:
            return "critical"
        if person_count >= 1:
            return "warning"
        return "info"

    @staticmethod
    def _describe(cat: str, dets: List[RawDetection]) -> str:
        # Group by label for a compact summary
        from collections import Counter
        counts = Counter(d.label for d in dets)
        top = counts.most_common(4)
        return cat + ": " + ", ".join(f"{lbl}×{n}" for lbl, n in top)
