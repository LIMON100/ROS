# Mesh 보드 (mesh_board1/2) 데이터 형식 & 시뮬레이션 런북

RK3588 idea3588 보드 2대(ROS2 Jazzy, `combatrobot_1` test_nav2)로 구성한 GNSS/nav2 편대주행 시스템.
- **mesh_board1 = s1 (리더)**, IP 192.168.1.51 (eth1 mesh) / 192.168.1.102 (eth0 lidar)
- **mesh_board2 = s2 (팔로워)**, IP 192.168.1.52 / 192.168.1.102
- 네트워크: mesh(eth1)·lidar(eth0)만 사용, wifi/wwan/p2p 영구 off. DDS domain 0.
  - 보드끼리(eth1) 멀티캐스트 통과 → 기본 발견. **호스트(wifi)는 유니캐스트 프로파일 필요**(`rviz_host.xml`).
  - **보드 DDS는 eth1만 whitelist**(`mesh_dds.xml`) — 안 하면 lidar eth0(.102)가 locator로 새어 호스트 발견 실패.

---

## 1. 보드가 내보내는 데이터 (토픽 / 타입 / rate / frame)

네임스페이스 `/sN` (s1, s2). 대표 rate는 s1 실측(2026-07-01).

### 센서 / 상태 (mock_board가 흉내내는 것)
| 토픽 | 타입 | rate | 설명 / frame |
|---|---|---|---|
| `/sN/fix` | sensor_msgs/NavSatFix | 20Hz | GPS RTK. status=2(RTK/SBAS), frame `gps` |
| `/sN/gps/filtered` | sensor_msgs/NavSatFix | 20Hz | navsat 필터 출력 |
| `/sN/gps/heading_imu` | sensor_msgs/Imu | 20Hz | GNSS dual-antenna heading을 Imu로 |
| `/sN/edge_heading` | std_msgs/Float64 | 20Hz | **dual-antenna heading [deg, compass 0=N cw]** (raw) |
| `/sN/vel` | geometry_msgs/TwistStamped | 20Hz | GPS 속도(ENU: x=E, y=N). 이동시 course=atan2(x,y) |
| `/sN/imu/data` | sensor_msgs/Imu | ~78Hz | Xsens MTi (frame `sN/imu_link`) |
| `/sN/odom` | nav_msgs/Odometry | 20Hz | **휠 오도(CAN can_reader)**. can_reader 미실행시 없음 |
| `/sN/odometry/local` | nav_msgs/Odometry | 20Hz | EKF local (odom frame) |
| `/sN/odometry/global` | nav_msgs/Odometry | 20Hz | **EKF global (map frame) = 최종 위치추정** |
| `/sN/odometry/gps` `/sN/odometry/gps_map` | nav_msgs/Odometry | | navsat_transform 출력 |
| `/sN/rslidar_points` | sensor_msgs/PointCloud2 | 10Hz | RS-E1 라이다(loopback 격리, 호스트로 안 감) |
| `/sN/joint_states` | sensor_msgs/JointState | | |

### TF
| | frame_id → child | 종류 | 발행자 |
|---|---|---|---|
| `/sN/tf` | `sN/map → sN/odom` | dynamic 20Hz | navsat/ekf(map) |
| `/sN/tf` | `sN/odom → sN/base_footprint` | dynamic 20Hz | ekf(odom) |
| `/sN/tf_static` | `sN/base_footprint → sN/base_link → 센서들` | static(latched) | robot_state_publisher |

### nav2 (costmap / 경로)
| 토픽 | 타입 | 비고 |
|---|---|---|
| `/sN/local_costmap/costmap` | nav_msgs/OccupancyGrid | **10m×10m @0.05 (200×200)** rolling, ~1.7Hz |
| `/sN/global_costmap/costmap` | nav_msgs/OccupancyGrid | static map 기반 |
| `/sN/map` | nav_msgs/OccupancyGrid | **static map 100m×100m @0.05 (map/incheon)** |
| `/sN/plan` `/sN/plan_smoothed` `/sN/transformed_global_plan` | nav_msgs/Path | 주행 중에만 |
| `/sN/cmd_vel` `/sN/cmd_vel_nav` `/sN/cmd_vel_smoothed` | geometry_msgs/Twist | 제어출력 → can_reader |

### swarm 명령 / 상태 (네임스페이스)
| 토픽 | 타입 | 비고 |
|---|---|---|
| `/sN/swarm/path_command` | combat_robot_msgs/SwarmPathCommand | **주행 트리거**(cmd 5=LOAD_PATH+path_json, 1=START) |
| `/sN/swarm/mission_state` | combat_robot_msgs/OperationState | executor→FSM |
| `/sN/swarm/control_command` | combat_robot_msgs/SwarmControlCommand | |
| `/sN/mission_input` | combat_robot_msgs/WaypointList | |

### 전역 `/swarm/*` 버스 (cross-board 공유, 편대 조율)
| 토픽 | 타입 | 비고 |
|---|---|---|
| `/swarm/leader_pose` | geometry_msgs/PoseStamped | 리더 pose (dynamic follower용) |
| `/swarm/leader_ref_path` | nav_msgs/Path | 리더 경로 공유 (static follower용) |
| `/swarm/formation_anchor` `/swarm/formation_ready` `/swarm/reform_ready` | geometry_msgs/PointStamped | form-up 핸드셰이크 |
| `/swarm/progress` | geometry_msgs/PointStamped | 편대 진행도 (~10Hz, static 조율 핵심) |
| `/swarm/robot_poses` `/swarm/robot_world_pos` | geometry_msgs/PoseStamped/PointStamped | |
| `/swarm/follower/sN/status` | combat_robot_msgs/SwarmFollowerStatus | 팔로워 GPS 집계(리더가 구독→태블릿) |
| `/swarm/path_command` `/swarm/control_command` | combat_robot_msgs/* | 전역 명령 |

**좌표계**: map = ENU (x=East, y=North). heading yaw: 0°=East, 90°=North. GPS datum = 각 보드 navsat 첫 fix.
두 보드 map은 별개 프레임 → rviz 병합시 `sN/map` 브리지 필요(리더 datum 기준).

---

## 2. 보드 없이 시뮬레이션 (mock_board)

보드 하드웨어 없이 위 데이터를 흉내내 **데이터 송수신 / rviz / swarm 조율**을 테스트.
보드 1대 = `mock_board.py` 1개. **ns만 s1/s2로 바꿔** 실행.

```bash
# 단독
python3 script/mock_board.py --ros-args -p ns:=s1
python3 script/mock_board.py --ros-args -p ns:=s2 -p spawn_x:=-2.0

# 헬퍼(2대 한번에)
script/run_sim.sh            # s1(0,0)+s2(-2,0) 정지
script/run_sim.sh move       # 둘 다 전진(course/heading 테스트)
script/run_sim.sh rviz       # mock + 호스트 merged rviz
script/run_sim.sh stop       # 종료
```

주요 파라미터: `ns`(s1/s2), `spawn_x/y`(map 위치), `heading_deg`(ENU), `move`(전진), `speed`,
`antenna_rev`(True=안테나 반대 상황 재현), `datum_lat/lon`. 자세한 건 `mock_board.py` docstring.

mock이 발행하는 토픽: `/sN/fix, imu/data, edge_heading, vel, odom, odometry/global,
local_costmap/costmap, tf, tf_static`. (nav2 풀스택·swarm executor는 별도로 얹어 테스트)

---

## 3. 실보드 기동 / 호스트 시각화

- 보드 기동: `mesh/board1_full.sh`(s1 리더+FSM), `mesh/board2_full.sh`(s2 팔로워). 홈(~)에 두고 `nohup setsid bash ~/boardN_full.sh &`.
  - lidar loopback 프로파일 `lidar_local.xml`(→/tmp), eth1 전용 DDS `mesh_dds.xml`(→~).
- 호스트 rviz(두 보드): `mesh/rviz_both.sh [xoff yoff]` (유니캐스트 `rviz_host.xml` + s1/s2 tf relay + s1/map→s2/map 브리지 + `swarm_view2.rviz`).
- heading 진단(안테나 반대 확인): `mesh/heading_check.py sN` — 이동중 GPS course vs edge_heading 비교(~180°면 안테나 반대).

### 알려진 주의
- **재기동시 옛 스택이 안 죽을 수 있음** → PID kill로 `remaining=0` 확인 후 재기동(안 그러면 옛 costmap/param이 남음).
- 정지중 GNSS dual-antenna heading 드리프트(수십°) → heading/TF 불안정. 이동시 안정.
- **안테나 좌우(측면) 장착 → heading이 전진축과 90° 틀어짐. `antenna_yaw_offset_deg:=90.0` 보정(board{1,2}_full.sh 반영, 양쪽 동일). heading=raw−offset.** 이동테스트로 검증(2026-07-02).
- can_reader GUI는 항상 호스트 화면(X포워딩, Enable Control 클릭 필요).
