"""
SwarmBridgeProcess — bridges our Python MissionProcess to the C++ swarm leader.

Two-way bridge:
  ── Outgoing (Python → C++ leader) ─────────────────────────────────────
  • Active patrol route's waypoints → publish as nav_msgs/Path on /patrol/plan
  • Mission state changes → publish as std_msgs/Int8 on /swarm/formation_command
                            (0=V_SHAPE during normal patrol, 1=SINGLE_FILE during
                             dense-obstacle waypoints flagged with checks=("narrow",))
  • sw/breadcrumb        — leader-only: BreadcrumbPoint stream (P0 RELIABLE)
                            for T4 escape recovery (SDD Rev.A.5 §6.7).
  • sw/follower_target   — leader-only: per-follower 1 s lookahead targets
                            (10 Hz, P0 RELIABLE; SDD §7.3).

  ── Incoming (C++ leader → Python) ─────────────────────────────────────
  • /leader_state — pose, velocity, target info → drives our existing
                    Pose6D feed to LocalizationProcess (alternative to TF-only)
  • /swarm/heartbeat — count of alive followers → expose as RobotStatus side info
  • Follower battery levels → fed back to MissionProcess BT for "wait for swarm"
                              behavior (don't outrun followers)

Threading:
  • RosBridgeThread: rclpy single-threaded executor (simple, low-rate)
  • PathPublisherThread: re-publish path on route change events

Dependencies:
  • rclpy (ROS 2 Python client)
  • unitree_swarm_leader.msg (custom messages)

Fallback when rclpy is not installed (dev box):
  • Logs warning and runs in stub mode — Python platform still works.
"""
from __future__ import annotations

import multiprocessing as mp
import threading
import time
from typing import List, Optional

import numpy as np

from core.base_process import BaseProcess
from core.ipc import TopicQueues, consume, publish
from core.messages import Header, Pose6D, Waypoint

# ─── Optional rclpy import (graceful fallback) ───
try:
    import rclpy
    from geometry_msgs.msg import PoseStamped
    from nav_msgs.msg import Path
    from rclpy.node import Node
    from rclpy.qos import HistoryPolicy, QoSProfile, ReliabilityPolicy
    from std_msgs.msg import Int8
    # Generated from CMake:  source install/setup.bash before running
    try:
        from unitree_swarm_leader.msg import LeaderState, SwarmHeartbeat
        SWARM_MSGS_AVAILABLE = True
    except ImportError:
        SWARM_MSGS_AVAILABLE = False
    RCLPY_AVAILABLE = True
except ImportError:
    RCLPY_AVAILABLE = False
    SWARM_MSGS_AVAILABLE = False


class SwarmBridgeProcess(BaseProcess):
    def __init__(self, queues: TopicQueues, shutdown_event: mp.Event, config, **diag):
        super().__init__(
            name="SwarmBridge",
            shutdown_event=shutdown_event,
            rate_hz=2.0,                   # diagnostics rate
            cpu_affinity=config.get("system", "cpu_affinity", "swarm_bridge")
                         or [0],           # share with comm (A55)
            **diag,
        )
        self.queues = queues
        self.cfg = config

        # State (touched from multiple threads → guarded)
        self._lock: Optional[threading.Lock] = None
        self._latest_route_waypoints: List[Waypoint] = []
        self._latest_leader_state = None
        self._alive_followers: dict = {}      # robot_id → last_seen_monotonic
        self._stats = {"path_tx": 0, "leader_rx": 0, "hb_rx": 0}

        # rclpy objects (created in setup() inside child process)
        self._ros_node = None
        self._spin_thread: Optional[threading.Thread] = None

    def setup(self) -> None:
        self._lock = threading.Lock()

        if not RCLPY_AVAILABLE:
            self.log.warning("rclpy not installed — SwarmBridge in STUB mode")
            self.spawn_thread(self._mission_state_consumer, name="MissionCnsm")
            return
        if not SWARM_MSGS_AVAILABLE:
            self.log.warning(
                "unitree_swarm_leader.msg not on PYTHONPATH — "
                "did you `source ~/patrol_ws/install/setup.bash`?")

        rclpy.init(args=None)
        self._ros_node = _SwarmBridgeRosNode(self)
        self._spin_thread = threading.Thread(
            target=rclpy.spin, args=(self._ros_node,),
            name="rclpy_spin", daemon=True)
        self._spin_thread.start()
        self.log.info("rclpy node started — bridging /patrol/plan ↔ /leader_state")

        # Background workers
        self.spawn_thread(self._mission_state_consumer, name="MissionCnsm")
        self.spawn_thread(self._path_republish_loop,    name="PathPub")
        self.spawn_thread(self._slam_broadcast_loop,    name="SlamBcast")
        self.spawn_thread(self._breadcrumb_relay,       name="BreadcrumbRx")
        self.spawn_thread(self._follower_target_relay,  name="FollowerTgtRx")

    def step(self) -> None:
        s = self._stats
        with self._lock:
            n_followers = sum(1 for t in self._alive_followers.values()
                              if (time.monotonic() - t) < 5.0)
        self.log.info(
            f"swarm  path_tx={s['path_tx']} leader_rx={s['leader_rx']} "
            f"hb_rx={s['hb_rx']} alive_followers={n_followers}")

    def teardown(self) -> None:
        if self._ros_node is not None:
            try:
                self._ros_node.destroy_node()
                rclpy.shutdown()
            except Exception:
                pass

    # ── internal: consume MissionProcess's state changes & emit path ──
    def _mission_state_consumer(self):
        """Watch mission_state queue for route activation."""
        while self.is_running():
            ms = consume(self.queues.mission_state, timeout=0.2)
            if ms is None:
                continue
            # Mission publishes on completion; on activation we'd need a separate
            # signal. For now, latch latest route from MissionProcess via shared
            # cfg (a richer impl would add a "route_active" topic to ipc.py).
            self.log.info(f"mission_state: {ms}")

    def _path_republish_loop(self):
        """
        Re-publish the active route's path every 1 s.
        In production you'd publish on route activation events, not periodically.
        """
        while self.is_running():
            time.sleep(1.0)
            with self._lock:
                wps = list(self._latest_route_waypoints)
            if not wps or self._ros_node is None:
                continue
            self._ros_node.publish_path(wps)
            self._stats["path_tx"] += 1

    def _slam_broadcast_loop(self):
        """
        Leader-only: chunk + zlib-compress incoming cumulative SLAM updates,
        emit on shared_map_out for the C++ swarm leader (or DDS) to forward
        to followers over WiFi6 mesh.

        Skipped if this robot is not the swarm leader (we infer leadership
        from /local_role on `unitree_01`'s ROS namespace via leader_state
        receipts; if no leader_rx, we assume this is the leader).
        """
        import zlib
        chunk_size = 32 * 1024              # 32 KB per chunk
        seq = 0
        while self.is_running():
            time.sleep(0.5)                  # broadcast at 2 Hz max
            update = consume(self.queues.cumulative_update, timeout=0.5)
            if update is None:
                continue
            # Only broadcast if we are the leader (no incoming leader_state =
            # we ARE the leader). This is a heuristic; for robust gating use
            # local_role from leadership_manager.
            with self._lock:
                got_leader_msg = self._stats.get("leader_rx", 0) > 0
            if got_leader_msg:
                continue
            # Serialize the occupancy grid
            grid = getattr(update, "occupancy", None)
            if grid is None or not hasattr(grid, "tobytes"):
                continue
            raw = grid.astype("int8").tobytes()
            compressed = zlib.compress(raw, level=3)
            n_chunks = max(1, (len(compressed) + chunk_size - 1) // chunk_size)
            from core.messages import Header as _H
            from core.messages import SharedMapChunk
            map_id = f"map_{seq}"
            for i in range(n_chunks):
                payload = compressed[i*chunk_size:(i+1)*chunk_size]
                publish(self.queues.shared_map_out, SharedMapChunk(
                    header=_H.now(frame_id="map", seq=seq),
                    map_id=map_id, chunk_id=i, n_chunks=n_chunks,
                    payload=payload,
                    origin_xy=getattr(update, "origin_xy",
                                      __import__("numpy").zeros(2, dtype="float32")),
                    resolution_m=float(getattr(update, "resolution_m", 0.10)),
                ))
            seq += 1
            self._stats["slam_bcast_tx"] = self._stats.get("slam_bcast_tx", 0) + 1

    # ── Rev.A.5 swarm topic relays ──
    def _breadcrumb_relay(self):
        """Drain `sw_breadcrumb` (followers) — counts only, real DDS bridge
        in production publishes them onto the leader's outgoing channel.
        Stays a thin counter here so the IPC topic is exercised end-to-end
        in tests."""
        while self.is_running():
            bp = consume(self.queues.sw_breadcrumb, timeout=0.5)
            if bp is None:
                continue
            self._stats["breadcrumb_rx"] = self._stats.get("breadcrumb_rx", 0) + 1

    def _follower_target_relay(self):
        """Drain `sw_follower_target` — same pattern as breadcrumb relay.
        Followers consume directly; this thread just provides a single
        integration touch-point for diagnostics."""
        while self.is_running():
            ft = consume(self.queues.sw_follower_target, timeout=0.5)
            if ft is None:
                continue
            self._stats["ftgt_rx"] = self._stats.get("ftgt_rx", 0) + 1

    # ── public hook: MissionProcess can call this to update active route ──
    def update_active_route(self, waypoints: List[Waypoint]):
        with self._lock:
            self._latest_route_waypoints = waypoints

    # ── callbacks invoked by rclpy node thread ──
    def _on_leader_state(self, msg):
        with self._lock:
            self._latest_leader_state = msg
        self._stats["leader_rx"] += 1
        # Optionally feed into Pose6D queue so other Python processes see leader pose:
        if self.queues.pose is not None:
            p = Pose6D(
                header=Header.now(frame_id="map"),
                position=np.array([msg.pose.position.x, msg.pose.position.y,
                                   msg.pose.position.z], dtype=np.float32),
                orientation=np.array([msg.pose.orientation.x, msg.pose.orientation.y,
                                      msg.pose.orientation.z, msg.pose.orientation.w],
                                     dtype=np.float32),
            )
            publish(self.queues.pose, p)

    def _on_heartbeat(self, msg):
        with self._lock:
            self._alive_followers[msg.robot_id] = time.monotonic()
        self._stats["hb_rx"] += 1


# ─────────────────────── rclpy node (lives in spin thread) ───────────────────────
if RCLPY_AVAILABLE:
    class _SwarmBridgeRosNode(Node):
        def __init__(self, bridge: SwarmBridgeProcess):
            super().__init__("swarm_bridge_py")
            self._bridge = bridge

            qos_rel = QoSProfile(reliability=ReliabilityPolicy.RELIABLE,
                                 history=HistoryPolicy.KEEP_LAST, depth=10)
            self._pub_path = self.create_publisher(Path, "/patrol/plan", qos_rel)
            self._pub_form = self.create_publisher(Int8, "/swarm/formation_command", 10)

            if SWARM_MSGS_AVAILABLE:
                self._sub_leader = self.create_subscription(
                    LeaderState, "/leader_state",
                    lambda m: self._bridge._on_leader_state(m), qos_rel)
                self._sub_hb = self.create_subscription(
                    SwarmHeartbeat, "/swarm/heartbeat",
                    lambda m: self._bridge._on_heartbeat(m), 10)

        def publish_path(self, waypoints: List[Waypoint]):
            msg = Path()
            msg.header.frame_id = "map"
            msg.header.stamp = self.get_clock().now().to_msg()
            for wp in waypoints:
                ps = PoseStamped()
                ps.header = msg.header
                ps.pose.position.x = float(wp.pose.position[0])
                ps.pose.position.y = float(wp.pose.position[1])
                ps.pose.position.z = float(wp.pose.position[2])
                ps.pose.orientation.x = float(wp.pose.orientation[0])
                ps.pose.orientation.y = float(wp.pose.orientation[1])
                ps.pose.orientation.z = float(wp.pose.orientation[2])
                ps.pose.orientation.w = float(wp.pose.orientation[3])
                msg.poses.append(ps)
            self._pub_path.publish(msg)

        def publish_formation(self, kind: int):
            msg = Int8()
            msg.data = int(kind)
            self._pub_form.publish(msg)
