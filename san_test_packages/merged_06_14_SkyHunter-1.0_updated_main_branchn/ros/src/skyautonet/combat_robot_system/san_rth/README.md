# san_rth

Return-to-Home action server (Tier 1 C++ per [[ADR-008]]).

이 패키지는 `san_mission` (ament_python, Tier 2) 과 분리되어 있다.
DCN-2026-002 D-007 (first rclpy node) 의 architectural decision 을
유지하면서도 ADR-008 의 새 safety-critical C++ 코드 요구사항을
충족하기 위함이다.

## Architecture

```
san_mission/mission_bt.py        (Tier 2, Python)
   │
   │ rclpy ActionClient
   ▼
/rth  (combat_robot_msgs/action/ReturnToHome)
   │
   │ rclcpp_action server
   ▼
san_rth/rth_action_node          (Tier 1, C++)
   │
   │ Nav2 action client
   ▼
/navigate_to_pose
```

Cross-language boundary 는 ROS action interface 이며 ADR-006 §4 의
권고와 일치한다 (rclcpp_action layer 가 standard cross-language
boundary).

## Action interface

`combat_robot_msgs/action/ReturnToHome`:

| Section | Field | 의미 |
|---|---|---|
| Goal | `bool reset_home_pose` | true 이면 새 home pose 를 lock 후 RTH 시작 |
| Result | `bool success` | ±2 m / ±10° 내 도달 성공 여부 |
| Result | `float64 final_distance_m` | 완료 시점의 home 으로부터 거리 |
| Result | `float64 final_yaw_error_rad` | 완료 시점의 yaw 오차 |
| Result | `string termination_reason` | `OK` / `TIMEOUT` / `GPS_LOSS_DEAD_RECKONING` / `CANCELLED` |
| Feedback | `float64 distance_remaining_m` | 진행 중 남은 거리 |
| Feedback | `string current_state` | `PLANNING` / `MOVING` / `VERIFYING` / `RTK_LOSS` |
| Feedback | `float64 rtk_yaw_covariance` | RTK heading covariance (loss 판정용) |

## Features (DCN-2026-017)

  - **D-081 — Home pose auto-recording**: 첫 `/odometry/filtered/global`
    fix 가 들어오면 `(x, y, yaw)` 를 `home_pose_` 로 capture 하고
    `/run/skyautonet/home_pose.yaml` 에 persist.
  - **D-082 — ±2 m / ±10° 검증**: RTH 완료 시점에 distance 와 yaw
    error 를 threshold 와 비교 → `success` 필드 결정.
  - **D-083 — GPS loss fallback**: `/rtk_gnss_node/heading` covariance
    가 5 초 이상 임계치 초과 시 dead-reckoning 모드 진입,
    `termination_reason = GPS_LOSS_DEAD_RECKONING`.

## Build & test

```bash
colcon build --packages-select combat_robot_msgs san_rth --symlink-install
source install/setup.bash

# 1. action interface 가 생성되었는지 확인
ros2 interface show combat_robot_msgs/action/ReturnToHome

# 2. unit test (pure-logic helpers — DDS 불필요)
colcon test --packages-select san_rth --ctest-args -R test_rth_helpers
colcon test-result --verbose
```

## Runtime

```bash
ros2 launch san_rth rth.launch.xml use_sim_time:=false

# Manual trigger
ros2 action send_goal /rth combat_robot_msgs/action/ReturnToHome \
  "{reset_home_pose: false}"
```

## References

  - [[ADR-008]] — Tier-based language policy (this package's raison d'être)
  - [[ADR-006]] §4 — IPC unification (rclcpp_action as cross-language boundary)
  - [[ADR-007]] — RMW FastDDS 통합
  - DCN-2026-002 D-007 — first rclpy node (san_mission ament_python)
  - DCN-2026-017 — this DCN (san_rth creation)
  - DCN-2026-006 EXT D-022/023 — RTK heading covariance publisher
