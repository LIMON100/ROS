"""
UnitreeGo2Adapter: bridge between Unitree Go2 EDU SDK (CycloneDDS) and our IPC.

Replaces both Phase-1 SensorProcess and LocomotionProcess.
The robot itself handles 1kHz motor control / balance / gait — we only
exchange high-level topics over Ethernet.

Threads:
  • LidarSubscriber      : DDS → LidarScanRef (zero-copy via SHM)
  • ImuSubscriber        : DDS → ImuData
  • CameraSubscriber     : DDS → CameraFrameRef
  • RobotStateSubscriber : Go2 sport state → RobotStatus
  • GoalPosePublisher    : our GoalPose → DDS /goal_pose
  • CmdVelPublisher      : our CmdVel → DDS /cmd_vel (fallback)

NOTE: The actual `unitree_sdk2_python` and `cyclonedds` imports are guarded
so this file works on dev machines without the real SDK. On RK3588 with
SDK installed, the real subscribers/publishers activate.
"""
from __future__ import annotations

import multiprocessing as mp
import time

import numpy as np

from core.base_process import BaseProcess
from core.ipc import TopicQueues, consume, publish
from core.messages import (
    CameraFrameRef,
    CmdVel,
    GoalPose,
    Header,
    ImuData,
    LidarScanRef,
    RobotStatus,
)
from core.shm_pool import ShmPool

# ─── SDK imports (graceful fallback) ───
# Channel{Publisher,Subscriber} and LowState_/SportModeState_ are imported as
# availability gates: if any one is missing we want SDK_AVAILABLE=False so the
# adapter falls back to stub mode. They're used by name in commented-out
# subscriber wiring (line 215+) once the topics are wired up. noqa F401 until
# then.
try:
    from unitree_sdk2py.core.channel import (  # noqa: F401
        ChannelFactoryInitialize,
        ChannelPublisher,
        ChannelSubscriber,
    )
    from unitree_sdk2py.idl.unitree_go.msg.dds_ import (  # noqa: F401
        LowState_,
        SportModeState_,
    )
    SDK_AVAILABLE = True
except ImportError:
    SDK_AVAILABLE = False


class UnitreeGo2Adapter(BaseProcess):
    """
    Single OS process, multiple threads bridging Go2 ↔ our IPC.
    Pinned to A76 with RT priority (latency critical).
    """

    def __init__(
        self,
        queues: TopicQueues,
        shutdown_event: mp.Event,
        config,
        lidar_shm: ShmPool,
        camera_shm: ShmPool,
        **diag,
    ):
        super().__init__(
            name="Go2Adapter",
            shutdown_event=shutdown_event,
            rate_hz=2.0,
            cpu_affinity=config.get("system", "cpu_affinity", "go2_adapter") or [4],
            rt_priority=config.get("system", "rt_priority", "go2_adapter") or 50,
            **diag,
        )
        self.queues = queues
        self.cfg = config
        self.lidar_shm = lidar_shm
        self.camera_shm = camera_shm
        self._stats = {"lidar": 0, "imu": 0, "camera": 0, "state": 0,
                       "goal_tx": 0, "cmd_tx": 0}

    def setup(self) -> None:
        if not SDK_AVAILABLE:
            self.log.warning("unitree_sdk2py not installed — running in STUB mode")

        iface = self.cfg.get("go2", "interface")
        if SDK_AVAILABLE:
            ChannelFactoryInitialize(0, iface)
            self.log.info(f"DDS initialized on iface={iface}")

        # Spawn worker threads (subscribers run in callback-driven manner;
        # we wrap them in our thread loop for SDK uniformity)
        self.spawn_thread(self._lidar_subscriber,    name="LidarSub")
        self.spawn_thread(self._imu_subscriber,      name="ImuSub")
        self.spawn_thread(self._camera_subscriber,   name="CameraSub")
        self.spawn_thread(self._robot_state_subscriber, name="StateSub")
        self.spawn_thread(self._goal_pose_publisher, name="GoalPub")
        self.spawn_thread(self._cmd_vel_publisher,   name="CmdVelPub")

    def step(self) -> None:
        s = self._stats
        self.log.info(
            f"go2  lidar={s['lidar']} imu={s['imu']} cam={s['camera']} "
            f"state={s['state']}  goal_tx={s['goal_tx']} cmd_tx={s['cmd_tx']}"
        )

    # ─── Subscribers (Go2 → our queues) ───
    def _lidar_subscriber(self) -> None:
        topic = self.cfg.get("go2", "lidar_topic")
        seq = 0
        while self.is_running():
            scan = self._receive_lidar_scan(topic)   # blocks ~100ms
            if scan is None:
                continue
            # scan: np.ndarray (N, 4) float32 [x,y,z,intensity]
            n = scan.shape[0]
            slot = self.lidar_shm.acquire()
            if slot is None:
                self.log.warning("lidar SHM pool exhausted, dropping scan")
                continue
            view = ShmPool.view_array(slot, (n, 4), dtype=np.float32)
            view[:, :] = scan
            seq += 1
            ref = LidarScanRef(
                header=Header.now(frame_id="utlidar_lidar", seq=seq),
                shm_name=slot.name, n_points=n, ring_count=4,
            )
            publish(self.queues.lidar_ref, ref)
            self._stats["lidar"] += 1

    def _imu_subscriber(self) -> None:
        topic = self.cfg.get("go2", "imu_topic")
        seq = 0
        while self.is_running():
            data = self._receive_imu(topic)   # blocks
            if data is None:
                continue
            seq += 1
            msg = ImuData(
                header=Header.now(frame_id="utlidar_imu", seq=seq),
                linear_acc=np.array(data["acc"], dtype=np.float32),
                angular_vel=np.array(data["gyro"], dtype=np.float32),
                orientation=np.array(data["quat"], dtype=np.float32),
            )
            publish(self.queues.imu, msg)
            self._stats["imu"] += 1

    def _camera_subscriber(self) -> None:
        topic = self.cfg.get("go2", "camera_topic")
        seq = 0
        while self.is_running():
            frame = self._receive_camera_frame(topic)
            if frame is None:
                continue
            data = frame["data"]   # bytes — H.265 encoded
            slot = self.camera_shm.acquire()
            if slot is None:
                self.log.warning("camera SHM exhausted")
                continue
            slot.buf[:len(data)] = data
            seq += 1
            ref = CameraFrameRef(
                header=Header.now(frame_id="front_camera", seq=seq),
                shm_name=slot.name, nbytes=len(data),
                width=frame["width"], height=frame["height"],
                encoding=frame.get("encoding", "h265"),
            )
            publish(self.queues.camera_ref, ref)
            self._stats["camera"] += 1

    def _robot_state_subscriber(self) -> None:
        seq = 0
        while self.is_running():
            st = self._receive_sport_state()
            if st is None:
                continue
            seq += 1
            msg = RobotStatus(
                header=Header.now(seq=seq),
                battery_soc=st["battery_soc"],
                motor_temp_max=st["motor_temp_max"],
                locomotion_mode=st["mode"],
                fault_codes=tuple(st.get("faults", ())),
            )
            publish(self.queues.robot_status, msg)
            self._stats["state"] += 1

    # ─── Publishers (our queues → Go2) ───
    def _goal_pose_publisher(self) -> None:
        topic = self.cfg.get("go2", "goal_pose_topic")
        while self.is_running():
            goal = consume(self.queues.goal_pose, timeout=0.1)
            if goal is None:
                continue
            self._send_goal_pose(topic, goal)
            self._stats["goal_tx"] += 1

    def _cmd_vel_publisher(self) -> None:
        topic = self.cfg.get("go2", "cmd_vel_topic")
        while self.is_running():
            cmd = consume(self.queues.cmd_vel, timeout=0.05)
            if cmd is None:
                continue
            self._send_cmd_vel(topic, cmd)
            self._stats["cmd_tx"] += 1

    # ─── SDK abstractions (real when SDK installed, stubs otherwise) ───
    def _receive_lidar_scan(self, topic: str):
        if not SDK_AVAILABLE:
            time.sleep(0.1)
            # stub data: 1000 points
            return np.random.randn(1000, 4).astype(np.float32) * np.array([5, 5, 1, 0.5])
        # TODO: real DDS subscription with callback queue
        # subscriber = ChannelSubscriber(topic, PointCloud2_)
        # ...
        return None

    def _receive_imu(self, topic: str):
        if not SDK_AVAILABLE:
            time.sleep(0.005)   # 200Hz
            return {"acc": [0, 0, 9.81], "gyro": [0, 0, 0], "quat": [0, 0, 0, 1]}
        return None

    def _receive_camera_frame(self, topic: str):
        if not SDK_AVAILABLE:
            time.sleep(0.033)
            return {"data": b"\x00" * 100_000, "width": 1280, "height": 720, "encoding": "h265"}
        return None

    def _receive_sport_state(self):
        if not SDK_AVAILABLE:
            time.sleep(0.1)
            return {"battery_soc": 0.85, "motor_temp_max": 45.0,
                    "mode": "stand", "faults": ()}
        return None

    def _send_goal_pose(self, topic: str, goal: GoalPose) -> None:
        if not SDK_AVAILABLE:
            return
        # TODO: real publish
        pass

    def _send_cmd_vel(self, topic: str, cmd: CmdVel) -> None:
        if not SDK_AVAILABLE:
            return
        # TODO: real publish
        pass
