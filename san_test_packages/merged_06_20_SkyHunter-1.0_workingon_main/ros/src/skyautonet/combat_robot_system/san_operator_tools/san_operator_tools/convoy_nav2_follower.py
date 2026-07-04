#!/usr/bin/env python3
# Convoy 추종자 nav2 연동 (DCN-2026-029 P3/P4) — 참조경로 → nav2.
#
# 리더가 발행하는 참조 차선 /convoy/ref_path/r{id}(P1, breadcrumb 슬롯 경로)를 받아 그 UGV
# 의 nav2 로 추종한다(설계 §4 Option A: Go2 가 global planner, nav2 는 추종+로컬 회피).
# convoy_ugv 의 직접 cmd_vel 을 대체 — 둘은 동시에 cmd_vel 을 내면 충돌하므로 런처에서
# 택일(follow_ref_path).
#
# 두 모드(mode):
#   - "follow_path"(P3, 기본): controller_server 의 follow_path 액션으로 참조 Path 추종.
#     최단·경량이나 **recovery 미동작**(controller 단독 — 막히면 실패만).
#   - "navigate_poses"(P4): bt_navigator 의 navigate_through_poses 액션으로 참조경로를
#     솎은 경유점(via-pose) 통과. **기본 BT 가 recovery(clear costmap→backup→spin) 포함** →
#     돌발 장애물로 막히면 recovery 후 참조 재획득. 돌발상황(§6) 대응 모드.
#
# 네임스페이스(robot_<id>)로 인스턴스화: 액션명 상대 → /robot_<id>/{follow_path|
# navigate_through_poses}, /robot_<id>/cmd_vel 은 nav2 controller 가 발행 → bridge → DiffDrive.
#
# 프레임: 참조경로는 "odom"(전역 지상진실, /odom_gt 와 동일 좌표)으로 온다. 네임스페이스
# nav2 의 고정 프레임은 "map"(런처 static map→<ns>/odom identity). sim 지상진실에서
# map ≡ 전역 odom 이므로 경로 frame 을 path_frame(기본 "map")으로 re-stamp 한다(§9 O-2).
#
# 재전송/재획득: 참조경로는 @5Hz 갱신되나 매 goal 전송은 preempt 를 유발 → 직전 전송 경로의
# head(리더 최신점)에서 resend_move_tol 이상 이동 시에만 갱신(should_resend_path 순수판정).
# navigate_poses 모드에서 goal 이 ABORTED/CANCELED(recovery 소진 등) 시 prev_head 를
# 리셋해 다음 tick 에 강제 재전송 → 참조 재획득(§6).
import math

import rclpy
from action_msgs.msg import GoalStatus
from geometry_msgs.msg import PoseStamped
from nav2_msgs.action import FollowPath, NavigateThroughPoses, NavigateToPose
from nav_msgs.msg import Odometry, Path
from rclpy.action import ActionClient
from rclpy.node import Node
from rclpy.qos import qos_profile_sensor_data

from san_operator_tools import convoy_avoidance as av


class ConvoyNav2Follower(Node):
    def __init__(self):
        super().__init__("convoy_nav2_follower")
        self.id = self.declare_parameter("robot_id", 3).value
        self.path_frame = self.declare_parameter("path_frame", "map").value
        # "follow_path"(P3) | "navigate_poses"(P4, recovery 포함).
        # self.mode = self.declare_parameter("mode", "follow_path").value
        self.mode = self.declare_parameter("mode", "navigate_to_pose").value
        self.controller_id = self.declare_parameter("controller_id", "FollowPath").value
        self.goal_checker_id = self.declare_parameter(
            "goal_checker_id", "general_goal_checker"
        ).value
        self.resend_period = self.declare_parameter("resend_period_s", 1.0).value
        self.resend_tol = self.declare_parameter("resend_move_tol_m", 0.5).value
        self.min_points = self.declare_parameter("min_points", 2).value
        # navigate_poses 경유점 솎기 간격(조밀 breadcrumb → via-pose).
        self.pose_spacing = self.declare_parameter("pose_spacing_m", 1.0).value
        # goal ABORTED/CANCELED 시 다음 tick 강제 재전송(참조 재획득, P4 §6).
        self.reacquire_on_abort = self.declare_parameter(
            "reacquire_on_abort", True
        ).value

        self.latest = None  # 최신 참조경로(Path)
        self.prev_head = None  # 직전 전송 경로 head(x,y) — 재전송 판정용
        self.goal_handle = None
        self.active = False

        # 참조경로는 전역(절대) 토픽 — 코디네이터가 /convoy/ref_path/r{id} 로 발행.
        self.create_subscription(
            Path, f"/convoy/ref_path/r{self.id}", self.on_ref_path, 1
        )   
        self.report_pub = self.create_publisher(
            Odometry, f"/convoy/report/r{self.id}", 10
        )   
        self.create_subscription(
            Odometry, f"/robot_{self.id}/odom", self._on_odom, qos_profile_sensor_data
        )  
        
        if self.mode == "navigate_to_pose":
            self.action = "navigate_to_pose"
            self.client = ActionClient(self, NavigateToPose, self.action)
        elif self.mode == "navigate_poses":
            self.action = "navigate_through_poses"
            self.client = ActionClient(self, NavigateThroughPoses, self.action)
        else:
            self.action = "follow_path"
            self.client = ActionClient(self, FollowPath, self.action)
        self.create_timer(self.resend_period, self.tick)
        self.get_logger().info(
            f"convoy_nav2_follower id={self.id} mode={self.mode}: "
            f"/convoy/ref_path/r{self.id} → {self.action} (frame={self.path_frame})"
        )

    def on_ref_path(self, m):
        self.latest = m
        
    def _on_odom(self, m):
        self.report_pub.publish(m) 

    def tick(self):
        if self.latest is None or len(self.latest.poses) < self.min_points:
            return
        if not self.client.server_is_ready():
            self.get_logger().warn(f"{self.action} action 서버 대기 중...", once=True)
            return
        if self.mode == "navigate_to_pose" and self.active:
            return
        head = self.latest.poses[-1].pose.position
        new_head = (head.x, head.y)
        if not av.should_resend_path(self.prev_head, new_head, self.resend_tol):
            return

        if self.mode == "navigate_to_pose":
            goal = self._gap_goal()
        elif self.mode == "navigate_poses":
            goal = NavigateThroughPoses.Goal()
            goal.poses = self._via_poses()
        else:
            goal = FollowPath.Goal()
            goal.path = self._restamped_path()
            goal.controller_id = self.controller_id
            goal.goal_checker_id = self.goal_checker_id
        self.client.send_goal_async(goal).add_done_callback(self._on_goal_response)
        self.prev_head = new_head
        self.active = True

    def _restamped_path(self):
        # 전역 좌표 참조경로를 nav2 고정 프레임(map)으로 re-stamp(sim 지상진실 동일 좌표).
        now = self.get_clock().now().to_msg()
        path = Path()
        path.header.frame_id = self.path_frame
        path.header.stamp = now
        for ps in self.latest.poses:
            ps.header.frame_id = self.path_frame
            ps.header.stamp = now
            path.poses.append(ps)
        return path

    def _via_poses(self):
        # 조밀 참조경로 → pose_spacing 솎기(순수 downsample) + 경로 접선 방향 heading.
        pts = av.downsample_path(
            [(p.pose.position.x, p.pose.position.y) for p in self.latest.poses],
            self.pose_spacing,
        )
        now = self.get_clock().now().to_msg()
        poses = []
        yaw = 0.0
        for i, (x, y) in enumerate(pts):
            if i + 1 < len(pts):
                nx, ny = pts[i + 1]
                if (nx, ny) != (x, y):
                    yaw = math.atan2(ny - y, nx - x)  # 마지막 점은 직전 heading 유지
            ps = PoseStamped()
            ps.header.frame_id = self.path_frame
            ps.header.stamp = now
            ps.pose.position.x = x
            ps.pose.position.y = y
            ps.pose.orientation.z = math.sin(yaw / 2.0)
            ps.pose.orientation.w = math.cos(yaw / 2.0)
            poses.append(ps)
        return poses
    
    def _gap_goal(self):
        # C2/C3 핵심: 갭 유지 목표 = 참조경로의 슬롯점(poses[0]) = 리더 head 뒤 slot_arc.
        # FollowPath(컨트롤러 단독)은 로봇에 연결 안 된 먼 경로조각을 추종 못 해 "0 poses"
        # abort → NavigateToPose 로 보내 nav2 planner 가 로봇 현재위치→슬롯점 연결경로 생성.
        # 목표가 head 가 아닌 슬롯점이라 갭 유지(overtake 방지). recovery 는 NavigateToPose
        # BT 내장(돌발대응 P4). frame = map(지상진실 동일좌표).
        poses = self.latest.poses
        slot = poses[0].pose.position
        yaw = 0.0
        if len(poses) > 1:
            nxt = poses[1].pose.position
            if (nxt.x, nxt.y) != (slot.x, slot.y):
                yaw = math.atan2(nxt.y - slot.y, nxt.x - slot.x)
        goal = NavigateToPose.Goal()
        goal.pose.header.frame_id = self.path_frame
        goal.pose.header.stamp = self.get_clock().now().to_msg()
        goal.pose.pose.position.x = slot.x
        goal.pose.pose.position.y = slot.y
        goal.pose.pose.orientation.z = math.sin(yaw / 2.0)
        goal.pose.pose.orientation.w = math.cos(yaw / 2.0)
        return goal

    def _on_goal_response(self, future):
        gh = future.result()
        if gh is None or not gh.accepted:
            self.get_logger().warn(f"{self.action} goal 거부됨")
            self.active = False
            if self.reacquire_on_abort:
                self.prev_head = None  # 다음 tick 재전송
            return
        self.goal_handle = gh  # 다음 전송이 자연 preempt(단일 goal 서버)
        gh.get_result_async().add_done_callback(self._on_result)

    def _on_result(self, future):
        status = future.result().status
        self.active = False
        # ABORTED(recovery 소진 등)/CANCELED → 참조 재획득(다음 tick 강제 재전송, P4 §6).
        if self.reacquire_on_abort and status in (
            GoalStatus.STATUS_ABORTED,
            GoalStatus.STATUS_CANCELED,
        ):
            self.get_logger().warn(f"{self.action} status={status} → 참조 재획득")
            self.prev_head = None


def main():
    rclpy.init()
    rclpy.spin(ConvoyNav2Follower())
    rclpy.shutdown()


if __name__ == "__main__":
    main()
