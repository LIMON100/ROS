#include <algorithm>
#include <cmath>
#include <chrono>
#include <functional>
#include <string>
#include <unordered_map>

#include "rclcpp/rclcpp.hpp"

#include "combat_robot_msgs/msg/bounding_box2d.hpp"
#include "combat_robot_msgs/msg/pan_tilt_state.hpp"
#include "combat_robot_msgs/msg/target_point.hpp"
#include "std_msgs/msg/float64.hpp"
#include "std_msgs/msg/int32.hpp"
#include "std_msgs/msg/int8.hpp"

namespace combat_robot_system {

enum class Scenario {
  FULL_CYCLE,
  IDLE_ONLY,
  SURVEILLANCE_SWEEP,
  TRACKING_LOCK,
  ATTACK_FIRE,
  ERROR_RECOVERY,
};

class CombatRobotTestDummyNode : public rclcpp::Node {
public:
  CombatRobotTestDummyNode()
  : Node("combat_robot_test_dummy_node")
  {
    m_script_duration_sec = this->declare_parameter("script_duration_sec", 24.0);
    m_loop = this->declare_parameter("loop", true);

    const std::string scenario_name =
      this->declare_parameter<std::string>("scenario", "full_cycle");
    m_scenario = parseScenario(scenario_name);

    m_pub_pan_tilt_enabled = this->declare_parameter("pub_pan_tilt_state", true);
    m_pub_target_enabled = this->declare_parameter("pub_target_point", true);
    m_pub_gun_enabled = this->declare_parameter("pub_gun_status", true);
    m_pub_zoom_enabled = this->declare_parameter("pub_zoom_level", true);
    m_pub_distance_enabled = this->declare_parameter("pub_laser_distance", true);

    m_pub_pan_tilt_state =
      this->create_publisher<combat_robot_msgs::msg::PanTiltState>("/pan_tilt_state", 10);
    m_pub_target_point =
      this->create_publisher<combat_robot_msgs::msg::TargetPoint>(
        "/human_detector/human/target_point", 10);
    m_pub_gun_status =
      this->create_publisher<std_msgs::msg::Int8>("/gun_trigger/status", 10);
    m_pub_zoom_level =
      this->create_publisher<std_msgs::msg::Int32>("/zoom_level", 10);
    m_pub_laser_distance =
      this->create_publisher<std_msgs::msg::Float64>("/sensor/distance", 10);

    m_start_time = this->now();
    m_timer = this->create_wall_timer(
      std::chrono::milliseconds(100),
      std::bind(&CombatRobotTestDummyNode::onTimer, this));

    RCLCPP_INFO(this->get_logger(),
      "Combat robot test dummy node started [scenario=%s loop=%s duration=%.1fs]",
      scenario_name.c_str(), m_loop ? "true" : "false", m_script_duration_sec);
  }

private:
  static Scenario parseScenario(const std::string& t_name)
  {
    static const std::unordered_map<std::string, Scenario> table = {
      {"full_cycle", Scenario::FULL_CYCLE},
      {"idle_only", Scenario::IDLE_ONLY},
      {"surveillance_sweep", Scenario::SURVEILLANCE_SWEEP},
      {"tracking_lock", Scenario::TRACKING_LOCK},
      {"attack_fire", Scenario::ATTACK_FIRE},
      {"error_recovery", Scenario::ERROR_RECOVERY},
    };
    const auto it = table.find(t_name);
    return it == table.end() ? Scenario::FULL_CYCLE : it->second;
  }

  void onTimer()
  {
    const rclcpp::Time stamp = this->now();
    const double elapsed_sec = (stamp - m_start_time).seconds();
    const double duration = std::max(1.0, m_script_duration_sec);

    if (!m_loop && elapsed_sec >= duration) {
      if (!m_finished_logged) {
        RCLCPP_INFO(this->get_logger(),
          "Scenario duration elapsed; publisher idle (loop=false).");
        m_finished_logged = true;
      }
      return;
    }

    const double phase_sec = std::fmod(elapsed_sec, duration);
    dispatchScenario(stamp, phase_sec);
  }

  void dispatchScenario(const rclcpp::Time& t_stamp, double t_phase_sec)
  {
    switch (m_scenario) {
      case Scenario::IDLE_ONLY:
        runIdleOnly(t_stamp);
        break;
      case Scenario::SURVEILLANCE_SWEEP:
        runSurveillanceSweep(t_stamp, t_phase_sec);
        break;
      case Scenario::TRACKING_LOCK:
        runTrackingLock(t_stamp, t_phase_sec);
        break;
      case Scenario::ATTACK_FIRE:
        runAttackFire(t_stamp, t_phase_sec);
        break;
      case Scenario::ERROR_RECOVERY:
        runErrorRecovery(t_stamp, t_phase_sec);
        break;
      case Scenario::FULL_CYCLE:
      default:
        runFullCycle(t_stamp, t_phase_sec);
        break;
    }
  }

  // Original 24s composite used as default regression script.
  void runFullCycle(const rclcpp::Time& t_stamp, double t_phase_sec)
  {
    publishPanTiltSweep(t_stamp, t_phase_sec);

    const bool locked = (t_phase_sec >= 10.0 && t_phase_sec < 18.0);
    const int8_t class_id = (t_phase_sec >= 14.0 && t_phase_sec < 18.0) ? 1 : 0;
    publishTargetPoint(t_stamp, t_phase_sec, locked, class_id);

    publishGunStatus(0);
    publishZoomLevel((t_phase_sec >= 14.0 && t_phase_sec < 18.0) ? 8 : 4);
    publishLaserDistance(t_phase_sec);
  }

  void runIdleOnly(const rclcpp::Time& t_stamp)
  {
    publishPanTiltStatic(t_stamp);
    publishTargetPoint(t_stamp, 0.0, /*locked=*/false, /*class_id=*/0);
    publishGunStatus(0);
    publishZoomLevel(4);
    publishLaserDistance(0.0);
  }

  void runSurveillanceSweep(const rclcpp::Time& t_stamp, double t_phase_sec)
  {
    publishPanTiltSweep(t_stamp, t_phase_sec);
    publishTargetPoint(t_stamp, t_phase_sec, /*locked=*/false, /*class_id=*/0);
    publishGunStatus(0);
    publishZoomLevel(4);
    publishLaserDistance(t_phase_sec);
  }

  void runTrackingLock(const rclcpp::Time& t_stamp, double t_phase_sec)
  {
    publishPanTiltSweep(t_stamp, t_phase_sec);
    publishTargetPoint(t_stamp, t_phase_sec, /*locked=*/true, /*class_id=*/0);
    publishGunStatus(0);
    publishZoomLevel(6);
    publishLaserDistance(t_phase_sec);
  }

  void runAttackFire(const rclcpp::Time& t_stamp, double t_phase_sec)
  {
    publishPanTiltSweep(t_stamp, t_phase_sec);
    // Lock immediately, transition to "fire" class after a short ramp-up.
    const bool fire_phase = t_phase_sec >= 3.0;
    publishTargetPoint(t_stamp, t_phase_sec,
      /*locked=*/true, /*class_id=*/fire_phase ? 1 : 0);
    publishGunStatus(0);
    publishZoomLevel(fire_phase ? 8 : 6);
    publishLaserDistance(t_phase_sec);
  }

  // Drop watched topics mid-script to exercise watchdog -> ERROR_STATE,
  // then resume to test recovery. Requires the relevant checks.* params
  // to be enabled for the operation_system to flip state.
  void runErrorRecovery(const rclcpp::Time& t_stamp, double t_phase_sec)
  {
    const bool blackout = (t_phase_sec >= 6.0 && t_phase_sec < 12.0);

    if (!blackout) {
      publishPanTiltSweep(t_stamp, t_phase_sec);
      publishTargetPoint(t_stamp, t_phase_sec, /*locked=*/false, /*class_id=*/0);
    }
    publishGunStatus(0);
    publishZoomLevel(4);
    publishLaserDistance(t_phase_sec);
  }

  void publishPanTiltStatic(const rclcpp::Time& t_stamp)
  {
    if (!m_pub_pan_tilt_enabled) {
      return;
    }
    combat_robot_msgs::msg::PanTiltState msg;
    msg.stamp = t_stamp;
    msg.control_mode = 0;
    msg.horizontal_angle = 0.0f;
    msg.vertical_angle = 0.0f;
    msg.pan_speed = 0;
    msg.tilt_speed = 0;
    m_pub_pan_tilt_state->publish(msg);
  }

  void publishPanTiltSweep(const rclcpp::Time& t_stamp, double t_phase_sec)
  {
    if (!m_pub_pan_tilt_enabled) {
      return;
    }
    combat_robot_msgs::msg::PanTiltState msg;
    msg.stamp = t_stamp;
    msg.control_mode = 0;
    msg.horizontal_angle = static_cast<float>(6.0 * std::sin(t_phase_sec * 0.35));
    msg.vertical_angle = static_cast<float>(-8.0 + 2.5 * std::cos(t_phase_sec * 0.28));
    msg.pan_speed = 0;
    msg.tilt_speed = 0;
    m_pub_pan_tilt_state->publish(msg);
  }

  void publishTargetPoint(const rclcpp::Time& t_stamp, double t_phase_sec,
                          bool t_locked, int8_t t_class_id)
  {
    if (!m_pub_target_enabled) {
      return;
    }
    combat_robot_msgs::msg::TargetPoint msg;
    msg.header.stamp = t_stamp;
    msg.header.frame_id = "dummy_test_frame";

    const double x = 0.5 + 0.08 * std::sin(t_phase_sec * 0.75);
    const double y = 0.5 + 0.05 * std::cos(t_phase_sec * 0.52);

    msg.is_locked = t_locked;
    msg.x = x;
    msg.y = y;
    msg.height = t_locked ? 0.34f : 0.22f;
    msg.class_id = t_class_id;
    msg.track_id = t_locked ? 1001 : -1;

    combat_robot_msgs::msg::BoundingBox2d box;
    box.x = static_cast<int32_t>(std::clamp((x - 0.08) * 640.0, 0.0, 639.0));
    box.y = static_cast<int32_t>(std::clamp((y - 0.12) * 360.0, 0.0, 359.0));
    box.width = 96;
    box.height = 144;
    msg.box = box;

    m_pub_target_point->publish(msg);
  }

  void publishGunStatus(int8_t t_value)
  {
    if (!m_pub_gun_enabled) {
      return;
    }
    std_msgs::msg::Int8 msg;
    msg.data = t_value;
    m_pub_gun_status->publish(msg);
  }

  void publishZoomLevel(int32_t t_level)
  {
    if (!m_pub_zoom_enabled) {
      return;
    }
    std_msgs::msg::Int32 msg;
    msg.data = t_level;
    m_pub_zoom_level->publish(msg);
  }

  void publishLaserDistance(double t_phase_sec)
  {
    if (!m_pub_distance_enabled) {
      return;
    }
    std_msgs::msg::Float64 msg;
    msg.data = 120.0 + 15.0 * std::sin(t_phase_sec * 0.45);
    m_pub_laser_distance->publish(msg);
  }

  Scenario m_scenario{Scenario::FULL_CYCLE};
  double m_script_duration_sec{24.0};
  bool m_loop{true};
  bool m_finished_logged{false};

  bool m_pub_pan_tilt_enabled{true};
  bool m_pub_target_enabled{true};
  bool m_pub_gun_enabled{true};
  bool m_pub_zoom_enabled{true};
  bool m_pub_distance_enabled{true};

  rclcpp::Time m_start_time{0, 0, RCL_SYSTEM_TIME};
  rclcpp::TimerBase::SharedPtr m_timer;

  rclcpp::Publisher<combat_robot_msgs::msg::PanTiltState>::SharedPtr m_pub_pan_tilt_state;
  rclcpp::Publisher<combat_robot_msgs::msg::TargetPoint>::SharedPtr m_pub_target_point;
  rclcpp::Publisher<std_msgs::msg::Int8>::SharedPtr m_pub_gun_status;
  rclcpp::Publisher<std_msgs::msg::Int32>::SharedPtr m_pub_zoom_level;
  rclcpp::Publisher<std_msgs::msg::Float64>::SharedPtr m_pub_laser_distance;
};

}  // namespace combat_robot_system

int main(int argc, char** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<combat_robot_system::CombatRobotTestDummyNode>());
  rclcpp::shutdown();
  return 0;
}
