/**
 * @file test_fsm_transitions.cpp
 * @brief FSM 상태 전환 유닛 테스트 (하드웨어 없는 GTest)
 *
 * 전략:
 *   - CombatRobotOperationSystem을 rclcpp::NodeOptions로 생성
 *   - 별도 테스트 노드(MockPublisherNode)가 mock 토픽을 publish
 *   - spin_some()으로 콜백을 처리해 FSM 상태를 유도
 *   - /operation_state 토픽을 구독해 실제 상태 전환을 검증
 *
 * 테스트 항목:
 *   [T1] INIT_STATE → IDLE (Pan/Tilt 초기화 완료 시)
 *   [T2] IDLE → SURVEILLANCE_STATE (사용자 명령)
 *   [T3] IDLE → ATTACKING_STATE (사용자 명령)
 *   [T4] IDLE → EMERGENCY_STOP_STATE (긴급 정지)
 *   [T5] SURVEILLANCE_STATE → TRACKING_STATE (타겟 락)
 *   [T6] TRACKING_STATE → SURVEILLANCE_STATE (타겟 락 해제)
 *   [T7] 어느 상태에서든 EMERGENCY_STOP_STATE 즉시 전환
 *   [T8] 센서 타임아웃 → ERROR_STATE (check_pantilt_status=false로 비활성화 후 detector만 테스트)
 *   [T14] demo deployment RECON → 데모 시퀀스 → IDLE
 *   [T15] demo deployment PROTECT_GENERAL → MOVE_STATE (데모)
 *   [T16] demo deployment ASSAULT → MOVE_STATE (데모)
 *   [T17] demo 자연 종료 후 latch 해제 → 재진입 안 함
 *   [T18] demo 다중 타겟 큐 (target_count=3) — 큐 3개 모두 engage
 *   [T19] demo REVERSE 단계에서 linear_velocity < 0 부호 검증
 *   [T20] demo SCAN 타임아웃 + 검출 없음 → ENGAGE 건너뛰고 REVERSE → IDLE, fire 없음
 *   [T21] demo 도중 E-Stop → EMERGENCY_STOP_STATE + 추가 fire 없음, drive=0
 *   [T22] demo ENGAGE 단계에서 /gun_trigger/cmd (data=1) 실제 발행 확인
 *   [T23] demo 도중 pan/tilt 워치독 타임아웃 → ERROR_STATE + 추가 fire 없음, drive=0
 */

#include <gtest/gtest.h>
#include <rclcpp/rclcpp.hpp>
#include <chrono>
#include <cstdint>
#include <limits>
#include <memory>
#include <thread>

#include "combat_robot_msgs/msg/target_point.hpp"
#include "combat_robot_msgs/msg/pan_tilt_state.hpp"
#include "combat_robot_msgs/msg/mission_control_command.hpp"
#include "combat_robot_msgs/msg/operation_state.hpp"
#include "combat_robot_msgs/msg/swarm_path_command.hpp"
#include "combat_robot_msgs/msg/drive_command.hpp"
#include "std_msgs/msg/int8.hpp"
#include "std_msgs/msg/int32.hpp"

#include "combat_robot_operation_system.hpp"

using namespace std::chrono_literals;
using combat_robot_msgs::msg::TargetPoint;
using combat_robot_msgs::msg::PanTiltState;
using combat_robot_msgs::msg::MissionControlCommand;
using combat_robot_msgs::msg::OperationState;
using combat_robot_msgs::msg::SwarmPathCommand;
using combat_robot_msgs::msg::DriveCommand;
using combat_robot_system::e_operation_state;

// ─────────────────────────────────────────────────────────────────────────────
// 테스트 헬퍼: Mock 토픽 퍼블리셔 노드
// ─────────────────────────────────────────────────────────────────────────────

class MockPublisherNode : public rclcpp::Node {
public:
  MockPublisherNode() : Node("mock_publisher") {
    pub_pan_tilt_state_ = create_publisher<PanTiltState>("/pan_tilt_state", 10);
    pub_target_point_   = create_publisher<TargetPoint>("/human_detector/human/target_point", 1);
    pub_mission_control_command_ = create_publisher<MissionControlCommand>("/mission_control_command", 10);
    // ★ swarm/path_command 구독은 프로덕션에서 TRANSIENT_LOCAL+RELIABLE(f55cb15:
    //   팔로워 늦은매칭 LOAD 유실 방지). 발행 QoS 를 맞춰야 durability 호환(volatile
    //   발행은 transient_local 구독과 매칭 안돼 LOAD 미전달 → ASSAULT READY 실패).
    pub_swarm_path_command_ = create_publisher<SwarmPathCommand>(
        "/swarm/path_command", rclcpp::QoS(rclcpp::KeepLast(10)).transient_local().reliable());
    pub_swarm_mission_state_ = create_publisher<OperationState>("/swarm/mission_state", 10);
    pub_gun_status_     = create_publisher<std_msgs::msg::Int8>("/gun_trigger/status", 10);
    pub_zoom_level_     = create_publisher<std_msgs::msg::Int32>("/zoom_level", 10);
  }

  // Pan/Tilt 상태를 특정 각도로 publish (초기화 완료 시뮬레이션)
  void publishPanTiltState(float h_angle = 0.0f, float v_angle = 0.0f) {
    PanTiltState msg;
    msg.horizontal_angle = h_angle;
    msg.vertical_angle   = v_angle;
    pub_pan_tilt_state_->publish(msg);
  }

  // 타겟 포인트 publish (is_locked=true: 타겟 획득, false: 소실)
  void publishTargetPoint(
    bool is_locked,
    float x = 0.5f,
    float y = 0.5f,
    uint8_t class_id = 0,
    int32_t track_id = 1) {
    TargetPoint msg;
    msg.header.stamp = rclcpp::Clock().now();
    msg.is_locked    = static_cast<uint8_t>(is_locked);
    msg.x            = x;
    msg.y            = y;
    msg.height       = 0.3f;
    msg.track_id     = is_locked ? track_id : -1;
    msg.class_id     = is_locked ? class_id : 0;
    pub_target_point_->publish(msg);
  }

  // 미션 제어 명령 publish
  void publishMissionControlCommand(uint8_t command_id, bool estop_requested = false) {
    MissionControlCommand msg;
    msg.command_id = command_id;
    msg.estop_requested = estop_requested;
    pub_mission_control_command_->publish(msg);
  }

  void publishSwarmPathCommand(uint8_t command, uint16_t num_waypoints = 0) {
    SwarmPathCommand msg;
    msg.header.stamp = rclcpp::Clock().now();
    msg.command = command;
    msg.num_waypoints = num_waypoints;
    pub_swarm_path_command_->publish(msg);
  }

  // 차량 path_executor 의 실측 nav 상태 publish (/swarm/mission_state).
  // 실제 편대 도착(FollowPath 완료)을 mission_status=MISSION_REACHED 로 시뮬레이션한다.
  void publishSwarmMissionState(uint8_t mission_status) {
    OperationState msg;
    msg.mission_status = mission_status;
    pub_swarm_mission_state_->publish(msg);
  }

  // 총기 상태 publish (0=IDLE, 1=FIRING, -1=ERROR)
  void publishGunStatus(int8_t status) {
    std_msgs::msg::Int8 msg;
    msg.data = status;
    pub_gun_status_->publish(msg);
  }

  // 줌 레벨 publish
  void publishZoomLevel(int32_t level) {
    std_msgs::msg::Int32 msg;
    msg.data = level;
    pub_zoom_level_->publish(msg);
  }

private:
  rclcpp::Publisher<PanTiltState>::SharedPtr pub_pan_tilt_state_;
  rclcpp::Publisher<TargetPoint>::SharedPtr  pub_target_point_;
  rclcpp::Publisher<MissionControlCommand>::SharedPtr pub_mission_control_command_;
  rclcpp::Publisher<SwarmPathCommand>::SharedPtr pub_swarm_path_command_;
  rclcpp::Publisher<OperationState>::SharedPtr pub_swarm_mission_state_;
  rclcpp::Publisher<std_msgs::msg::Int8>::SharedPtr pub_gun_status_;
  rclcpp::Publisher<std_msgs::msg::Int32>::SharedPtr pub_zoom_level_;
};

// ─────────────────────────────────────────────────────────────────────────────
// 테스트 픽스처
// ─────────────────────────────────────────────────────────────────────────────

class FsmTest : public ::testing::Test {
protected:
  virtual std::string deploymentMode() const { return "production"; }
  virtual int demoTargetCount() const { return 1; }
  virtual double demoScanDurationSec() const { return 0.5; }
  virtual bool checkPantiltStatus() const { return false; }
  virtual double pantiltStatusTimeoutSec() const { return 3.0; }

  void SetUp() override {
    // DUT 노드 생성 (하드웨어 체크 비활성화)
    rclcpp::NodeOptions options;
    options.append_parameter_override("deployment_mode", deploymentMode());
    options.append_parameter_override("checks.detector_status", false);
    options.append_parameter_override("checks.pantilt_status",  checkPantiltStatus());
    options.append_parameter_override("checks.pantilt_status_timeout_sec", pantiltStatusTimeoutSec());
    options.append_parameter_override("checks.gun_status",      false);
    options.append_parameter_override("demo.forward_distance_m", 0.05);
    options.append_parameter_override("demo.reverse_distance_m", 0.05);
    options.append_parameter_override("demo.drive_speed_mps", 0.5);
    options.append_parameter_override("demo.fire_duration_sec", 0.2);
    options.append_parameter_override("demo.fire_duration_multi_sec", 0.2);
    options.append_parameter_override("demo.scan_duration_sec", demoScanDurationSec());
    options.append_parameter_override("demo.target_count", demoTargetCount());

    dut_ = std::make_shared<combat_robot_system::CombatRobotOperationSystem>(options);
    mock_ = std::make_shared<MockPublisherNode>();

    // /operation_state 구독 → 상태 변화 캡처
    state_sub_ = mock_->create_subscription<OperationState>(
      "/operation_state", 10,
      [this](const OperationState::SharedPtr msg) {
        last_state_ = static_cast<e_operation_state>(msg->state);
        last_operation_state_ = *msg;
        has_operation_state_ = true;
        max_total_waypoints_seen_ =
          std::max(max_total_waypoints_seen_, msg->total_waypoints);
        max_current_waypoint_index_seen_ =
          std::max(max_current_waypoint_index_seen_, msg->current_waypoint_index);
      });
    drive_sub_ = mock_->create_subscription<DriveCommand>(
      "/drive_command", 10,
      [this](const DriveCommand::SharedPtr msg) {
        last_drive_command_ = *msg;
        has_drive_command_ = true;
        max_abs_linear_velocity_seen_ =
          std::max(max_abs_linear_velocity_seen_, std::abs(msg->linear_velocity));
        min_linear_velocity_seen_ =
          std::min(min_linear_velocity_seen_, static_cast<float>(msg->linear_velocity));
        max_linear_velocity_seen_ =
          std::max(max_linear_velocity_seen_, static_cast<float>(msg->linear_velocity));
      });
    gun_cmd_sub_ = mock_->create_subscription<std_msgs::msg::Int8>(
      "/gun_trigger/cmd", 50,
      [this](const std_msgs::msg::Int8::SharedPtr msg) {
        last_gun_cmd_ = msg->data;
        ++gun_cmd_total_count_;
        if (msg->data == 1) {
          ++gun_cmd_fire_count_;
        }
      });

    executor_ = std::make_shared<rclcpp::executors::SingleThreadedExecutor>();
    executor_->add_node(dut_);
    executor_->add_node(mock_);
  }

  void TearDown() override {
    executor_->cancel();
    dut_.reset();
    mock_.reset();
  }

  /**
   * spin_some을 일정 시간 동안 반복 실행해 콜백이 처리되도록 함
   * @param duration_ms 최대 대기 시간(ms)
   */
  void spinFor(int duration_ms = 200) {
    auto start = std::chrono::steady_clock::now();
    auto deadline = start + std::chrono::milliseconds(duration_ms);
    while (std::chrono::steady_clock::now() < deadline) {
      executor_->spin_some(50ms);
      std::this_thread::sleep_for(5ms);
    }
  }

  /**
   * 특정 상태가 될 때까지 spin (최대 timeout_ms 대기)
   * @return 원하는 상태가 됐으면 true
   */
  bool waitForState(e_operation_state target, int timeout_ms = 1000) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (std::chrono::steady_clock::now() < deadline) {
      executor_->spin_some(50ms);
      std::this_thread::sleep_for(10ms);
      if (last_state_ == target) return true;
    }
    return last_state_ == target;
  }

  bool waitForMissionStatus(uint8_t target, int timeout_ms = 1000) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (std::chrono::steady_clock::now() < deadline) {
      executor_->spin_some(50ms);
      std::this_thread::sleep_for(10ms);
      if (has_operation_state_ && last_operation_state_.mission_status == target) return true;
    }
    return has_operation_state_ && last_operation_state_.mission_status == target;
  }

  // Pan/Tilt 상태를 반복 publish해 INIT_STATE를 통과시킨다
  void initializeSystem() {
    // INIT_STATE: Pan/Tilt 모듈 초기화 대기 (angle≈0도 도달)
    for (int i = 0; i < 5; ++i) {
      mock_->publishPanTiltState(0.0f, 0.0f);
      spinFor(100);
    }
    // IDLE로 전환될 때까지 대기
    ASSERT_TRUE(waitForState(combat_robot_system::IDLE, 2000))
      << "시스템이 INIT → IDLE 전환에 실패했습니다.";
  }

  std::shared_ptr<combat_robot_system::CombatRobotOperationSystem> dut_;
  std::shared_ptr<MockPublisherNode> mock_;
  rclcpp::Subscription<OperationState>::SharedPtr state_sub_;
  rclcpp::Subscription<DriveCommand>::SharedPtr drive_sub_;
  rclcpp::Subscription<std_msgs::msg::Int8>::SharedPtr gun_cmd_sub_;
  std::shared_ptr<rclcpp::executors::SingleThreadedExecutor> executor_;
  e_operation_state last_state_{combat_robot_system::INIT_STATE};
  OperationState last_operation_state_{};
  DriveCommand last_drive_command_{};
  bool has_operation_state_{false};
  bool has_drive_command_{false};
  float max_abs_linear_velocity_seen_{0.0f};
  float min_linear_velocity_seen_{std::numeric_limits<float>::max()};
  float max_linear_velocity_seen_{std::numeric_limits<float>::lowest()};
  int gun_cmd_total_count_{0};
  int gun_cmd_fire_count_{0};
  int8_t last_gun_cmd_{0};
  uint16_t max_total_waypoints_seen_{0};
  uint16_t max_current_waypoint_index_seen_{0};
};

class FsmDemoDeploymentTest : public FsmTest {
protected:
  std::string deploymentMode() const override { return "demo"; }
};

class FsmDemoMultiTargetTest : public FsmDemoDeploymentTest {
protected:
  int demoTargetCount() const override { return 3; }
};

class FsmDemoSensorErrorTest : public FsmDemoDeploymentTest {
protected:
  bool checkPantiltStatus() const override { return true; }
  double pantiltStatusTimeoutSec() const override { return 0.3; }
};

// ─────────────────────────────────────────────────────────────────────────────
// [T1] INIT_STATE → IDLE
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(FsmTest, T1_InitToIdle) {
  // Pan/Tilt 상태(0도)를 publish하면 InitPanTiltModule()이 성공하고 IDLE로 전환
  for (int i = 0; i < 5; ++i) {
    mock_->publishPanTiltState(0.0f, 0.0f);
    spinFor(80);
  }
  EXPECT_TRUE(waitForState(combat_robot_system::IDLE, 2000))
    << "Pan/Tilt 초기화 완료 후 IDLE로 전환되어야 합니다.";
}

// ─────────────────────────────────────────────────────────────────────────────
// [T2] IDLE → SURVEILLANCE_STATE
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(FsmTest, T2_IdleToSurveillance) {
  ASSERT_NO_FATAL_FAILURE(initializeSystem());

  mock_->publishMissionControlCommand(MissionControlCommand::PROTECT_GENERAL);
  spinFor(100);

  EXPECT_TRUE(waitForState(combat_robot_system::SURVEILLANCE_STATE, 2000))
    << "PROTECT_GENERAL 명령 후 SURVEILLANCE_STATE로 전환되어야 합니다.";
}

// ─────────────────────────────────────────────────────────────────────────────
// [T3] IDLE → ATTACKING_STATE
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(FsmTest, T3_IdleToAttacking) {
  ASSERT_NO_FATAL_FAILURE(initializeSystem());

  mock_->publishMissionControlCommand(MissionControlCommand::DEBUG_ATTACK);
  spinFor(100);

  EXPECT_TRUE(waitForState(combat_robot_system::ATTACKING_STATE, 2000))
    << "DEBUG_ATTACK 명령 후 ATTACKING_STATE로 전환되어야 합니다.";
}

// ─────────────────────────────────────────────────────────────────────────────
// [T4] IDLE → EMERGENCY_STOP_STATE (E-Stop)
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(FsmTest, T4_IdleToEmergencyStop) {
  ASSERT_NO_FATAL_FAILURE(initializeSystem());

  mock_->publishMissionControlCommand(MissionControlCommand::IDLE, true);
  spinFor(100);

  EXPECT_TRUE(waitForState(combat_robot_system::EMERGENCY_STOP_STATE, 2000))
    << "ESTOP 명령 후 즉시 EMERGENCY_STOP_STATE로 전환되어야 합니다.";
}

// ─────────────────────────────────────────────────────────────────────────────
// [T5] SURVEILLANCE_STATE → TRACKING_STATE (타겟 락)
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(FsmTest, T5_SurveillanceToTracking) {
  ASSERT_NO_FATAL_FAILURE(initializeSystem());

  // SURVEILLANCE 진입
  mock_->publishMissionControlCommand(MissionControlCommand::PROTECT_GENERAL);
  ASSERT_TRUE(waitForState(combat_robot_system::SURVEILLANCE_STATE, 2000));

  // Pan/Tilt 상태 지속 publish (Scanning 로직이 Pan/Tilt 각도를 참조)
  mock_->publishPanTiltState(0.0f, 0.0f);
  spinFor(50);

  // 타겟 락 → TRACKING으로 전환
  mock_->publishTargetPoint(true, 0.5f, 0.5f);
  spinFor(100);

  EXPECT_TRUE(waitForState(combat_robot_system::TRACKING_STATE, 2000))
    << "타겟 락(is_locked=true) 시 SURVEILLANCE → TRACKING 전환되어야 합니다.";
}

// ─────────────────────────────────────────────────────────────────────────────
// [T6] TRACKING_STATE → SURVEILLANCE_STATE (타겟 소실)
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(FsmTest, T6_TrackingToSurveillance) {
  ASSERT_NO_FATAL_FAILURE(initializeSystem());

  // SURVEILLANCE → TRACKING 진입
  mock_->publishMissionControlCommand(MissionControlCommand::PROTECT_GENERAL);
  ASSERT_TRUE(waitForState(combat_robot_system::SURVEILLANCE_STATE, 2000));

  mock_->publishPanTiltState(0.0f, 0.0f);
  mock_->publishTargetPoint(true, 0.5f, 0.5f);
  ASSERT_TRUE(waitForState(combat_robot_system::TRACKING_STATE, 2000));

  // 타겟 소실 → SURVEILLANCE로 복귀
  mock_->publishTargetPoint(false);
  spinFor(100);

  EXPECT_TRUE(waitForState(combat_robot_system::SURVEILLANCE_STATE, 2000))
    << "타겟 소실(is_locked=false) 시 TRACKING → SURVEILLANCE 복귀해야 합니다.";
}

// ─────────────────────────────────────────────────────────────────────────────
// [T7] SURVEILLANCE 중 EMERGENCY_STOP (어느 상태에서든 E-Stop 즉시 전환)
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(FsmTest, T7_EStopFromSurveillance) {
  ASSERT_NO_FATAL_FAILURE(initializeSystem());

  mock_->publishMissionControlCommand(MissionControlCommand::PROTECT_GENERAL);
  ASSERT_TRUE(waitForState(combat_robot_system::SURVEILLANCE_STATE, 2000));

  mock_->publishMissionControlCommand(MissionControlCommand::PROTECT_GENERAL, true);
  spinFor(100);

  EXPECT_TRUE(waitForState(combat_robot_system::EMERGENCY_STOP_STATE, 2000))
    << "SURVEILLANCE 중에도 ESTOP 명령은 즉시 E-Stop 상태로 전환해야 합니다.";
}

// ─────────────────────────────────────────────────────────────────────────────
// [T8] EMERGENCY_STOP_STATE → IDLE (STOP 명령으로 해제)
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(FsmTest, T8_EStopRelease) {
  ASSERT_NO_FATAL_FAILURE(initializeSystem());

  // E-Stop 진입
  mock_->publishMissionControlCommand(MissionControlCommand::IDLE, true);
  ASSERT_TRUE(waitForState(combat_robot_system::EMERGENCY_STOP_STATE, 2000));

  // IDLE 명령으로 IDLE 복귀
  mock_->publishMissionControlCommand(MissionControlCommand::IDLE);
  spinFor(100);

  EXPECT_TRUE(waitForState(combat_robot_system::IDLE, 2000))
    << "EMERGENCY_STOP_STATE에서 IDLE 명령 수신 시 IDLE로 복귀해야 합니다.";
}

// ─────────────────────────────────────────────────────────────────────────────
// [T9] 잘못된 상태 전환 거부 (IDLE에서 TRACKING_STATE 직접 진입 불가)
// ─────────────────────────────────────────────────────────────────────────────
// NOTE: TRACKING_STATE는 SURVEILLANCE/DRONE_SURVEILLANCE에서만 진입 가능.
//       단, debug 경로(DEBUG_TRACKING 명령)가 있어 이 케이스는 해당 경로 검증.

TEST_F(FsmTest, T9_IdleToTrackingViaModeCommand) {
  ASSERT_NO_FATAL_FAILURE(initializeSystem());

  // DEBUG_TRACKING 명령은 디버그 경로로 허용됨
  mock_->publishMissionControlCommand(MissionControlCommand::DEBUG_TRACKING);
  spinFor(100);

  EXPECT_TRUE(waitForState(combat_robot_system::TRACKING_STATE, 2000))
    << "DEBUG_TRACKING 명령(디버그)은 IDLE에서도 TRACKING_STATE 진입이 허용됩니다.";
}

// ─────────────────────────────────────────────────────────────────────────────
// [T10] IDLE → DRONE_SURVEILLANCE_STATE
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(FsmTest, T10_IdleToDroneSurveillance) {
  ASSERT_NO_FATAL_FAILURE(initializeSystem());

  mock_->publishMissionControlCommand(MissionControlCommand::PROTECT_DRONE);
  spinFor(100);

  EXPECT_TRUE(waitForState(combat_robot_system::DRONE_SURVEILLANCE_STATE, 2000))
    << "PROTECT_DRONE 명령 후 DRONE_SURVEILLANCE_STATE로 전환되어야 합니다.";
}

// ─────────────────────────────────────────────────────────────────────────────
// [T11] IDLE → RTH_STATE
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(FsmTest, T11_IdleToRTH) {
  ASSERT_NO_FATAL_FAILURE(initializeSystem());

  mock_->publishMissionControlCommand(MissionControlCommand::RETURN_TO_HOME);
  spinFor(100);

  EXPECT_TRUE(waitForState(combat_robot_system::RTH_STATE, 2000))
    << "RETURN_TO_HOME 명령 후 RTH_STATE로 전환되어야 합니다.";
}

// ─────────────────────────────────────────────────────────────────────────────
// [T12] ASSAULT mission executor lifecycle
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(FsmTest, T12_AssaultMissionExecutorLifecycle) {
  ASSERT_NO_FATAL_FAILURE(initializeSystem());

  mock_->publishMissionControlCommand(MissionControlCommand::ASSAULT);
  ASSERT_TRUE(waitForState(combat_robot_system::ASSAULT_STATE, 2000));

  mock_->publishSwarmPathCommand(SwarmPathCommand::CMD_LOAD_PATH, 3);
  ASSERT_TRUE(waitForMissionStatus(OperationState::MISSION_READY, 2000))
    << "경로 로드 후 ASSAULT mission_status는 READY여야 합니다.";
  EXPECT_EQ(last_operation_state_.total_waypoints, 3);
  EXPECT_EQ(last_operation_state_.current_waypoint_index, 0);
  EXPECT_FLOAT_EQ(last_operation_state_.current_speed_mps, 0.0f);
  EXPECT_FLOAT_EQ(last_operation_state_.progress_ratio, 0.0f);

  mock_->publishSwarmPathCommand(SwarmPathCommand::CMD_START);
  ASSERT_TRUE(waitForMissionStatus(OperationState::MISSION_MOVING, 2000))
    << "START 후 mission_status는 MOVING이어야 합니다.";
  EXPECT_FLOAT_EQ(last_operation_state_.progress_ratio, 0.0f);
  mock_->publishSwarmPathCommand(SwarmPathCommand::CMD_PAUSE);
  ASSERT_TRUE(waitForMissionStatus(OperationState::MISSION_PAUSED, 2000))
    << "PAUSE 후 mission_status는 PAUSED여야 합니다.";
  EXPECT_FLOAT_EQ(last_operation_state_.current_speed_mps, 0.0f);

  mock_->publishSwarmPathCommand(SwarmPathCommand::CMD_RESUME);
  ASSERT_TRUE(waitForMissionStatus(OperationState::MISSION_MOVING, 2000))
    << "RESUME 후 mission_status는 MOVING이어야 합니다.";
  EXPECT_FLOAT_EQ(last_operation_state_.progress_ratio, 0.0f);

  mock_->publishSwarmPathCommand(SwarmPathCommand::CMD_STOP);
  ASSERT_TRUE(waitForMissionStatus(OperationState::MISSION_NONE, 2000))
    << "STOP 후 mission_status는 NONE으로 초기화되어야 합니다.";
  EXPECT_EQ(last_operation_state_.total_waypoints, 0);
  EXPECT_EQ(last_operation_state_.current_waypoint_index, 0);
  EXPECT_FLOAT_EQ(last_operation_state_.progress_ratio, 0.0f);
}

// ─────────────────────────────────────────────────────────────────────────────
// [T13] RTH does not synthesize mission progress
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(FsmTest, T13_RthKeepsMissionNoneWithoutPathFollowerFeedback) {
  ASSERT_NO_FATAL_FAILURE(initializeSystem());

  mock_->publishMissionControlCommand(MissionControlCommand::RETURN_TO_HOME);
  ASSERT_TRUE(waitForState(combat_robot_system::RTH_STATE, 2000));
  EXPECT_EQ(last_operation_state_.mission_status, OperationState::MISSION_NONE);
  EXPECT_EQ(last_operation_state_.total_waypoints, 0);
  EXPECT_EQ(last_operation_state_.current_waypoint_index, 0);
  EXPECT_FLOAT_EQ(last_operation_state_.current_speed_mps, 0.0f);
  EXPECT_FLOAT_EQ(last_operation_state_.distance_to_goal_m, 0.0f);
}

// ─────────────────────────────────────────────────────────────────────────────
// [T13c] Hybrid ASSAULT: 경로 로드 → 주행(MOVE) → 실측 도착 → 스캔(ASSAULT) → 교전(TRACKING)
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(FsmTest, T13c_HybridAssaultDrivesThenScansThenTracksOnRealArrival) {
  ASSERT_NO_FATAL_FAILURE(initializeSystem());

  // 경로 로드 상태에서 ASSAULT → 목표까지 주행(MOVE_STATE)으로 진입해야 한다.
  mock_->publishSwarmPathCommand(SwarmPathCommand::CMD_LOAD_PATH, 3);
  ASSERT_TRUE(waitForMissionStatus(OperationState::MISSION_READY, 2000));
  mock_->publishMissionControlCommand(MissionControlCommand::ASSAULT);
  ASSERT_TRUE(waitForState(combat_robot_system::MOVE_STATE, 2000))
    << "경로 로드 상태의 ASSAULT는 목표 주행을 위해 MOVE_STATE로 진입해야 합니다.";
  EXPECT_EQ(last_operation_state_.active_mode_id, OperationState::ACTIVE_MODE_ASSAULT);

  // executor 실측 도착(/swarm/mission_state=REACHED)만으로 스캔 단계로 넘어가야 한다
  // (운용자 CMD_COMPLETE 없이) — 도착 신호 브릿지 검증.
  mock_->publishSwarmMissionState(OperationState::MISSION_REACHED);
  ASSERT_TRUE(waitForState(combat_robot_system::ASSAULT_STATE, 2000))
    << "실측 도착(MISSION_REACHED) 수신 시 MOVE → ASSAULT_STATE(스캔)로 전환되어야 합니다.";

  // 사람(class 0) 타겟 락 → TRACKING 교전으로 전환.
  mock_->publishTargetPoint(true, 0.5f, 0.5f, 0, 7);
  EXPECT_TRUE(waitForState(combat_robot_system::TRACKING_STATE, 2000))
    << "ASSAULT 스캔 중 사람 타겟 락 시 TRACKING_STATE로 교전 전환되어야 합니다.";
  EXPECT_EQ(last_operation_state_.active_mode_id, OperationState::ACTIVE_MODE_ASSAULT);

  // 타겟 소실 → grace 후 다시 ASSAULT_STATE(스캔)로 복귀.
  mock_->publishTargetPoint(false);
  EXPECT_TRUE(waitForState(combat_robot_system::ASSAULT_STATE, 2000))
    << "타겟 소실 시 TRACKING → ASSAULT_STATE(스캔)로 복귀해야 합니다.";
}

// ─────────────────────────────────────────────────────────────────────────────
// [T13d] ASSAULT_STATE → IDLE 전이 (운용자 IDLE 명령으로 정상 종료)
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(FsmTest, T13d_AssaultReturnsToIdleOnOperatorCommand) {
  ASSERT_NO_FATAL_FAILURE(initializeSystem());

  // 경로 없이 ASSAULT → 즉시 ASSAULT_STATE(스캔) 진입.
  mock_->publishMissionControlCommand(MissionControlCommand::ASSAULT);
  ASSERT_TRUE(waitForState(combat_robot_system::ASSAULT_STATE, 2000));

  // 운용자 IDLE 명령으로 ASSAULT_STATE → IDLE 로 빠져나올 수 있어야 한다
  // (기존엔 transitState IDLE 허용목록에 ASSAULT_STATE 가 없어 갇혔음).
  mock_->publishMissionControlCommand(MissionControlCommand::IDLE);
  EXPECT_TRUE(waitForState(combat_robot_system::IDLE, 2000))
    << "ASSAULT 중 IDLE 명령 시 IDLE 상태로 복귀해야 합니다.";
}

// ─────────────────────────────────────────────────────────────────────────────
// [T14] demo deployment: RECON → demo sequence → IDLE
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(FsmDemoDeploymentTest, T14_ReconRunsDemoSequenceAndReturnsToIdle) {
  ASSERT_NO_FATAL_FAILURE(initializeSystem());

  mock_->publishMissionControlCommand(MissionControlCommand::RECON);
  ASSERT_TRUE(waitForState(combat_robot_system::MOVE_STATE, 2000));
  EXPECT_EQ(last_operation_state_.active_mode_id, OperationState::ACTIVE_MODE_RECON);

  for (int i = 0; i < 12; ++i) {
    mock_->publishPanTiltState(0.0f, 0.0f);
    spinFor(30);
  }

  for (int i = 0; i < 12; ++i) {
    mock_->publishPanTiltState(0.0f, 0.0f);
    mock_->publishTargetPoint(true, 0.5f, 0.5f, 1, 42);
    spinFor(30);
  }

  EXPECT_TRUE(waitForState(combat_robot_system::IDLE, 3000))
    << "demo deployment에서 RECON 명령 후 데모 시퀀스를 마치면 IDLE로 복귀해야 합니다.";
  EXPECT_TRUE(has_drive_command_);
  EXPECT_GT(max_abs_linear_velocity_seen_, 0.0f);
  EXPECT_EQ(last_operation_state_.active_mode_id, OperationState::ACTIVE_MODE_IDLE);
}

TEST_F(FsmDemoDeploymentTest, T15_ProtectRunsDemoInsteadOfSurveillance) {
  ASSERT_NO_FATAL_FAILURE(initializeSystem());

  mock_->publishMissionControlCommand(MissionControlCommand::PROTECT_GENERAL);
  spinFor(100);

  EXPECT_TRUE(waitForState(combat_robot_system::MOVE_STATE, 2000))
    << "demo deployment에서는 PROTECT_GENERAL도 SURVEILLANCE 대신 데모(MOVE_STATE)로 진입해야 합니다.";
  EXPECT_EQ(last_operation_state_.active_mode_id, OperationState::ACTIVE_MODE_PROTECT_GENERAL);
}

TEST_F(FsmDemoDeploymentTest, T16_AssaultRunsDemoInsteadOfAssaultState) {
  ASSERT_NO_FATAL_FAILURE(initializeSystem());

  mock_->publishMissionControlCommand(MissionControlCommand::ASSAULT);
  spinFor(100);

  EXPECT_TRUE(waitForState(combat_robot_system::MOVE_STATE, 2000))
    << "demo deployment에서는 ASSAULT도 ASSAULT_STATE 대신 데모(MOVE_STATE)로 진입해야 합니다.";
  EXPECT_EQ(last_operation_state_.active_mode_id, OperationState::ACTIVE_MODE_ASSAULT);
}

// ─────────────────────────────────────────────────────────────────────────────
// [T17] demo 자연 종료 후 RECON latch가 해제되어 자동 재진입 안 함
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(FsmDemoDeploymentTest, T17_DemoDoesNotReEnterAfterCompletion) {
  ASSERT_NO_FATAL_FAILURE(initializeSystem());

  mock_->publishMissionControlCommand(MissionControlCommand::RECON);
  ASSERT_TRUE(waitForState(combat_robot_system::MOVE_STATE, 2000));

  for (int i = 0; i < 12; ++i) {
    mock_->publishPanTiltState(0.0f, 0.0f);
    spinFor(30);
  }

  for (int i = 0; i < 12; ++i) {
    mock_->publishPanTiltState(0.0f, 0.0f);
    mock_->publishTargetPoint(true, 0.5f, 0.5f, 1, 42);
    spinFor(30);
  }

  ASSERT_TRUE(waitForState(combat_robot_system::IDLE, 3000))
    << "데모 자연 종료 후 IDLE로 복귀해야 합니다.";

  for (int i = 0; i < 20; ++i) {
    mock_->publishPanTiltState(0.0f, 0.0f);
    spinFor(50);
  }

  EXPECT_EQ(last_state_, combat_robot_system::IDLE)
    << "RECON latch가 해제되어 데모 자동 재진입 안 해야 합니다.";
  EXPECT_EQ(last_operation_state_.active_mode_id, OperationState::ACTIVE_MODE_IDLE);
}

// ─────────────────────────────────────────────────────────────────────────────
// [T18] demo 다중 타겟 큐 — target_count=3 인 경우 세 타겟 모두 engage
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(FsmDemoMultiTargetTest, T18_MultiTargetEngagesAllQueuedTracks) {
  ASSERT_NO_FATAL_FAILURE(initializeSystem());

  mock_->publishMissionControlCommand(MissionControlCommand::RECON);
  ASSERT_TRUE(waitForState(combat_robot_system::MOVE_STATE, 2000));

  // FORWARD 단계 통과 대기 (forward_distance/drive_speed = 0.1s)
  for (int i = 0; i < 5; ++i) {
    mock_->publishPanTiltState(0.0f, 0.0f);
    spinFor(30);
  }

  // SCAN 단계: 3개 track_id 를 라운드로빈으로 노출해 큐(quota=3)를 채운다.
  const int32_t track_ids[] = {101, 102, 103};
  for (int i = 0; i < 12; ++i) {
    mock_->publishPanTiltState(0.0f, 0.0f);
    mock_->publishTargetPoint(true, 0.5f, 0.5f, 1, track_ids[i % 3]);
    spinFor(30);
  }

  // ENGAGE 단계: 실제 검출기처럼 '한 번에 한 타깃'만 안정적으로 노출한다.
  // ENGAGE 는 현재 잠긴 track_id 를 즉시 채택(adopt)하므로, 매 프레임 타깃을
  // 번갈아 보내면 발사 윈도우가 계속 리셋되어 교전이 영원히 끝나지 않는다.
  // 교전 완료 수(current_waypoint_index)가 오르면 다음 타깃을 노출한다.
  for (int i = 0; i < 120; ++i) {
    const uint16_t engaged =
      std::min<uint16_t>(max_current_waypoint_index_seen_, 2);
    mock_->publishPanTiltState(0.0f, 0.0f);
    mock_->publishTargetPoint(true, 0.5f, 0.5f, 1, track_ids[engaged]);
    spinFor(40);
    if (last_state_ == combat_robot_system::IDLE) {
      break;
    }
  }

  ASSERT_TRUE(waitForState(combat_robot_system::IDLE, 5000))
    << "다중 타겟 데모 완료 후 IDLE 복귀해야 합니다.";

  // 데모 reset 으로 최종 OperationState 가 0 으로 돌아가므로 진행 중 max 값을 검증
  EXPECT_EQ(max_total_waypoints_seen_, 3u)
    << "demo.target_count=3 이면 total_waypoints=3 으로 보고되어야 합니다.";
  EXPECT_EQ(max_current_waypoint_index_seen_, 3u)
    << "3개 타겟이 모두 engage 완료되어 current_waypoint_index 가 3 까지 증가해야 합니다.";
  EXPECT_GE(gun_cmd_fire_count_, 3)
    << "다중 타겟 데모에서 fire 명령이 타겟 수만큼(>=3) 발행되어야 합니다.";
}

// ─────────────────────────────────────────────────────────────────────────────
// [T19] demo REVERSE 단계 — linear_velocity 음수 부호 검증 (후진)
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(FsmDemoDeploymentTest, T19_DemoReverseDrivesWithNegativeLinearVelocity) {
  ASSERT_NO_FATAL_FAILURE(initializeSystem());

  mock_->publishMissionControlCommand(MissionControlCommand::RECON);
  ASSERT_TRUE(waitForState(combat_robot_system::MOVE_STATE, 2000));

  // 타겟 없이 FORWARD → SCAN 타임아웃 → REVERSE 경로
  for (int i = 0; i < 40; ++i) {
    mock_->publishPanTiltState(0.0f, 0.0f);
    spinFor(40);
  }

  ASSERT_TRUE(waitForState(combat_robot_system::IDLE, 3000))
    << "타겟 없이 SCAN 타임아웃 후 REVERSE → IDLE 복귀해야 합니다.";

  EXPECT_GT(max_linear_velocity_seen_, 0.05f)
    << "FORWARD 단계에서 linear_velocity 가 양수여야 합니다.";
  EXPECT_LT(min_linear_velocity_seen_, -0.05f)
    << "REVERSE 단계에서 linear_velocity 가 음수여야 합니다 (후진).";
}

// ─────────────────────────────────────────────────────────────────────────────
// [T20] demo SCAN 타임아웃 + 검출 없음 — ENGAGE 건너뛰고 REVERSE → IDLE, fire 0건
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(FsmDemoDeploymentTest, T20_ScanTimeoutWithoutTargetSkipsEngageAndDoesNotFire) {
  ASSERT_NO_FATAL_FAILURE(initializeSystem());

  mock_->publishMissionControlCommand(MissionControlCommand::RECON);
  ASSERT_TRUE(waitForState(combat_robot_system::MOVE_STATE, 2000));

  for (int i = 0; i < 40; ++i) {
    mock_->publishPanTiltState(0.0f, 0.0f);
    spinFor(40);
  }

  ASSERT_TRUE(waitForState(combat_robot_system::IDLE, 3000))
    << "검출 없는 데모는 SCAN 윈도우 경과 후 곧장 REVERSE → IDLE 로 복귀해야 합니다.";

  EXPECT_EQ(gun_cmd_fire_count_, 0)
    << "ENGAGE 단계를 건너뛴 경우 fire 명령이 한 번도 발행되면 안 됩니다.";
}

// ─────────────────────────────────────────────────────────────────────────────
// [T21] demo 도중 E-Stop — EMERGENCY_STOP 전환, 이후 fire 없음, drive=0
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(FsmDemoDeploymentTest, T21_EStopDuringDemoHaltsFireAndDrive) {
  ASSERT_NO_FATAL_FAILURE(initializeSystem());

  mock_->publishMissionControlCommand(MissionControlCommand::RECON);
  ASSERT_TRUE(waitForState(combat_robot_system::MOVE_STATE, 2000));

  // FORWARD → SCAN → ENGAGE 진입 유도
  for (int i = 0; i < 6; ++i) {
    mock_->publishPanTiltState(0.0f, 0.0f);
    spinFor(30);
  }
  for (int i = 0; i < 6; ++i) {
    mock_->publishPanTiltState(0.0f, 0.0f);
    mock_->publishTargetPoint(true, 0.5f, 0.5f, 1, 77);
    spinFor(30);
  }

  // E-Stop publish — RECON 명령 위에 estop_requested=true 를 얹어 즉시 E-stop 전이
  mock_->publishMissionControlCommand(MissionControlCommand::RECON, true);
  ASSERT_TRUE(waitForState(combat_robot_system::EMERGENCY_STOP_STATE, 2000))
    << "데모 도중 ESTOP 시 EMERGENCY_STOP_STATE 로 전이해야 합니다.";

  const int fire_count_at_estop = gun_cmd_fire_count_;

  // E-Stop 이후 fire 카운트가 더 늘어나지 않고, drive_command 가 0 으로 떨어지는지
  for (int i = 0; i < 12; ++i) {
    mock_->publishPanTiltState(0.0f, 0.0f);
    mock_->publishTargetPoint(true, 0.5f, 0.5f, 1, 77);
    spinFor(40);
  }

  EXPECT_EQ(gun_cmd_fire_count_, fire_count_at_estop)
    << "E-Stop 이후 추가 fire 명령이 발행되어선 안 됩니다.";
  EXPECT_EQ(last_state_, combat_robot_system::EMERGENCY_STOP_STATE)
    << "ESTOP 계속 유지되는 동안 EMERGENCY_STOP_STATE 에 머물러야 합니다.";
  EXPECT_FLOAT_EQ(last_drive_command_.linear_velocity, 0.0)
    << "E-Stop 후 drive_command.linear_velocity 는 0 이어야 합니다.";
}

// ─────────────────────────────────────────────────────────────────────────────
// [T22] demo ENGAGE 단계에서 /gun_trigger/cmd 가 실제로 data=1 로 발행됨
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(FsmDemoDeploymentTest, T22_DemoEngagePublishesFireCommandOnGunTriggerTopic) {
  ASSERT_NO_FATAL_FAILURE(initializeSystem());

  mock_->publishMissionControlCommand(MissionControlCommand::RECON);
  ASSERT_TRUE(waitForState(combat_robot_system::MOVE_STATE, 2000));

  for (int i = 0; i < 6; ++i) {
    mock_->publishPanTiltState(0.0f, 0.0f);
    spinFor(30);
  }
  for (int i = 0; i < 20; ++i) {
    mock_->publishPanTiltState(0.0f, 0.0f);
    mock_->publishTargetPoint(true, 0.5f, 0.5f, 1, 88);
    spinFor(30);
  }

  EXPECT_GT(gun_cmd_fire_count_, 0)
    << "ENGAGE 단계에서 /gun_trigger/cmd 가 한 번 이상 발행되어야 합니다.";

  // fire(1) 수신 자체는 gun_cmd_fire_count_ 로 검증된다. 데모가 끝나 IDLE 로
  // 복귀하면 매 tick cease-fire(0) 가 발행되므로(IDLE = everything off),
  // 마지막으로 관측된 gun 명령은 0 이어야 한다.
  ASSERT_TRUE(waitForState(combat_robot_system::IDLE, 3000))
    << "데모 시퀀스 완료 후 IDLE 로 복귀해야 합니다.";
  spinFor(100);
  EXPECT_EQ(last_gun_cmd_, 0)
    << "IDLE 복귀 후 마지막 gun 명령은 cease-fire(0) 이어야 합니다.";
}

// ─────────────────────────────────────────────────────────────────────────────
// [T23] demo 도중 pan/tilt 워치독 만료 — ERROR_STATE 전이, demo reset, fire 없음
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(FsmDemoSensorErrorTest, T23_PantiltWatchdogDuringDemoTransitionsToErrorAndResetsDemo) {
  ASSERT_NO_FATAL_FAILURE(initializeSystem());

  mock_->publishMissionControlCommand(MissionControlCommand::RECON);
  ASSERT_TRUE(waitForState(combat_robot_system::MOVE_STATE, 2000));

  // FORWARD → SCAN → ENGAGE 진입 유도 (panTilt + target 계속 publish)
  for (int i = 0; i < 10; ++i) {
    mock_->publishPanTiltState(0.0f, 0.0f);
    mock_->publishTargetPoint(true, 0.5f, 0.5f, 1, 55);
    spinFor(30);
  }

  // panTilt publish 만 중단 → 0.3s 후 워치독 만료 → ERROR_STATE
  // (대기 중에도 target 은 계속 publish — 검출 워치독을 분리해 pan/tilt 경로만 검증)
  auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(2000);
  while (std::chrono::steady_clock::now() < deadline &&
         last_state_ != combat_robot_system::ERROR_STATE) {
    mock_->publishTargetPoint(true, 0.5f, 0.5f, 1, 55);
    spinFor(40);
  }

  ASSERT_EQ(last_state_, combat_robot_system::ERROR_STATE)
    << "pan/tilt 워치독 만료 시 ERROR_STATE 로 전이해야 합니다.";
  EXPECT_EQ(last_operation_state_.error_code,
            static_cast<uint8_t>(combat_robot_system::PANTILT_ERROR))
    << "error_code 가 PANTILT_ERROR 로 보고되어야 합니다.";

  // ERROR_STATE 진입 후의 fire 카운트를 기준으로 잡고, 추가 fire/drive 가 없는지 검증
  const int fire_count_after_error = gun_cmd_fire_count_;

  for (int i = 0; i < 8; ++i) {
    mock_->publishTargetPoint(true, 0.5f, 0.5f, 1, 55);
    spinFor(40);
  }

  EXPECT_EQ(gun_cmd_fire_count_, fire_count_after_error)
    << "ERROR_STATE 진입 후 fire 명령은 더 이상 발행되어선 안 됩니다.";
  EXPECT_FLOAT_EQ(last_drive_command_.linear_velocity, 0.0)
    << "ERROR_STATE 에서 drive 가 정지되어야 합니다.";
  EXPECT_EQ(last_operation_state_.mission_status,
            static_cast<uint8_t>(OperationState::MISSION_NONE))
    << "데모 reset 으로 mission_status 는 MISSION_NONE 이어야 합니다.";
}

// ─────────────────────────────────────────────────────────────────────────────
// main
// ─────────────────────────────────────────────────────────────────────────────

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  ::testing::InitGoogleTest(&argc, argv);
  int result = RUN_ALL_TESTS();
  rclcpp::shutdown();
  return result;
}
