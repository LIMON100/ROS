// 호스트(jazzy)에서 차량(humble)을 안전 범위 내로 jog 시키며 map→odom yaw 흔들림을 측정한다.
// 동작: 전진 fwd_dist → (정지) → 후진 fwd_dist → (정지) → 제자리 회전 rotate_revs바퀴 → 정지.
// 거리/각도는 /odom(휠) 적분으로 하드 바운드. 각 구간에서 tf map→odom yaw를 샘플링해
// std / peak-to-peak 를 리포트(흔들림 지표). /cmd_vel 은 can_reader가 소비.
//
// 안전: 구간별 타임아웃, 전체 타임아웃, 종료/예외 시 zero cmd_vel 반복 발행.
//
// 예: ros2 run swarm_path_test jog_tune --ros-args -p fwd_dist:=1.5 -p rotate_revs:=1.0 \
//        -p linear_speed:=0.15 -p angular_speed:=0.3

#include <algorithm>
#include <cmath>
#include <vector>

#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/twist.hpp"
#include "nav_msgs/msg/odometry.hpp"
#include "tf2_ros/buffer.h"
#include "tf2_ros/transform_listener.h"
#include "tf2/utils.h"

using namespace std::chrono_literals;

namespace {
double yawFromQuat(double x, double y, double z, double w) {
  return std::atan2(2.0 * (w * z + x * y), 1.0 - 2.0 * (y * y + z * z));
}
double angDiff(double a, double b) {  // a-b wrapped to [-pi,pi]
  double d = a - b;
  while (d > M_PI) d -= 2 * M_PI;
  while (d < -M_PI) d += 2 * M_PI;
  return d;
}
struct Stat { double std_deg, pp_deg, n; };
Stat statOf(const std::vector<double> &v) {  // v in radians (already unwrapped rel)
  if (v.size() < 2) return {0, 0, (double)v.size()};
  double mean = 0; for (double x : v) mean += x; mean /= v.size();
  double var = 0, mn = v[0], mx = v[0];
  for (double x : v) { var += (x - mean) * (x - mean); mn = std::min(mn, x); mx = std::max(mx, x); }
  var /= v.size();
  return {std::sqrt(var) * 180.0 / M_PI, (mx - mn) * 180.0 / M_PI, (double)v.size()};
}
}  // namespace

class JogTune : public rclcpp::Node {
public:
  JogTune() : Node("jog_tune") {
    fwd_dist_   = declare_parameter<double>("fwd_dist", 1.5);
    rotate_revs_= declare_parameter<double>("rotate_revs", 1.0);
    lin_spd_    = declare_parameter<double>("linear_speed", 0.15);
    ang_spd_    = declare_parameter<double>("angular_speed", 0.3);
    do_fb_      = declare_parameter<bool>("do_forward_back", true);
    do_rot_     = declare_parameter<bool>("do_rotate", true);
    pause_s_    = declare_parameter<double>("pause_sec", 1.5);
    // 하드 클램프(안전)
    fwd_dist_   = std::clamp(fwd_dist_, 0.0, 2.0);
    lin_spd_    = std::clamp(lin_spd_, 0.0, 0.25);
    ang_spd_    = std::clamp(ang_spd_, 0.0, 0.5);
    rotate_revs_= std::clamp(rotate_revs_, 0.0, 2.0);

    pub_ = create_publisher<geometry_msgs::msg::Twist>("/cmd_vel", 10);
    odom_sub_ = create_subscription<nav_msgs::msg::Odometry>(
        "/odom", rclcpp::SensorDataQoS(),
        [this](nav_msgs::msg::Odometry::SharedPtr m) {
          ox_ = m->pose.pose.position.x; oy_ = m->pose.pose.position.y;
          oyaw_ = yawFromQuat(m->pose.pose.orientation.x, m->pose.pose.orientation.y,
                              m->pose.pose.orientation.z, m->pose.pose.orientation.w);
          have_odom_ = true; last_odom_ = now();
        });
    tf_buffer_ = std::make_shared<tf2_ros::Buffer>(get_clock());
    tf_listener_ = std::make_shared<tf2_ros::TransformListener>(*tf_buffer_);
  }

  void publishZero(int n = 5) {
    geometry_msgs::msg::Twist z;
    for (int i = 0; i < n && rclcpp::ok(); ++i) { pub_->publish(z); rclcpp::sleep_for(20ms); }
  }

  bool getMapOdomYaw(double *yaw) {
    try {
      auto t = tf_buffer_->lookupTransform("map", "odom", tf2::TimePointZero);
      *yaw = yawFromQuat(t.transform.rotation.x, t.transform.rotation.y,
                         t.transform.rotation.z, t.transform.rotation.w);
      return true;
    } catch (const std::exception &) { return false; }
  }

  // 한 구간 실행: linear/angular 속도로 목표(거리 or 각도) 도달까지. map→odom yaw 샘플 수집.
  // mode: 0=translate(거리 target_m), 1=rotate(각도 target_rad)
  void runSegment(const char *name, int mode, double speed, double target,
                  std::vector<double> *samples) {
    if (!have_odom_) { RCLCPP_ERROR(get_logger(), "%s: /odom 없음 — 중단", name); return; }
    const double sx = ox_, sy = oy_;
    double acc_yaw = 0.0, prev_yaw = oyaw_;
    const double timeout = std::fabs(target) / std::max(0.05, std::fabs(speed)) * 1.8 + 3.0;
    const auto t0 = now();
    double y0_mapodom = 0; bool have_base = getMapOdomYaw(&y0_mapodom);
    rclcpp::Rate rate(20.0);
    RCLCPP_INFO(get_logger(), "%s 시작 (target=%.2f, speed=%.2f, to=%.1fs)", name, target, speed, timeout);
    while (rclcpp::ok()) {
      rclcpp::spin_some(get_node_base_interface());
      // 안전: odom stale
      if ((now() - last_odom_).seconds() > 1.0) {
        RCLCPP_ERROR(get_logger(), "%s: /odom stale — 정지", name); break;
      }
      double progress;
      if (mode == 0) {
        progress = std::hypot(ox_ - sx, oy_ - sy);
      } else {
        acc_yaw += angDiff(oyaw_, prev_yaw); prev_yaw = oyaw_;
        progress = std::fabs(acc_yaw);
      }
      // 샘플: map→odom yaw 상대값(deg는 statOf에서)
      double mo; if (getMapOdomYaw(&mo)) {
        if (!have_base) { y0_mapodom = mo; have_base = true; }
        samples->push_back(angDiff(mo, y0_mapodom));
      }
      if (progress >= std::fabs(target)) { RCLCPP_INFO(get_logger(), "%s 목표 도달 (%.2f)", name, progress); break; }
      if ((now() - t0).seconds() > timeout) { RCLCPP_WARN(get_logger(), "%s 타임아웃", name); break; }
      geometry_msgs::msg::Twist cmd;
      if (mode == 0) cmd.linear.x = speed; else cmd.angular.z = speed;
      pub_->publish(cmd);
      rate.sleep();
    }
    publishZero(8);
  }

  void run() {
    // 준비 대기: odom + tf(map->odom) 둘 다 잡힐 때까지 (느린 링크 discovery 고려, 최대 25s)
    const auto t0 = now();
    double tmp; bool tf_ok = false;
    while (rclcpp::ok() && (now() - t0).seconds() < 25.0) {
      rclcpp::spin_some(get_node_base_interface());
      tf_ok = getMapOdomYaw(&tmp);
      if (have_odom_ && tf_ok) break;
      rclcpp::sleep_for(200ms);
    }
    if (!have_odom_ || !tf_ok) {
      RCLCPP_ERROR(get_logger(), "준비 실패 (odom=%d tf=%d) — 안전상 미동작", have_odom_, tf_ok);
      publishZero(); return;
    }
    RCLCPP_INFO(get_logger(), "준비 완료. fwd=%.2fm rot=%.2frev lin=%.2f ang=%.2f",
                fwd_dist_, rotate_revs_, lin_spd_, ang_spd_);

    std::vector<double> s_fwd, s_back, s_rot;
    if (do_fb_) {
      runSegment("전진", 0, +lin_spd_, fwd_dist_, &s_fwd);
      rclcpp::sleep_for(std::chrono::milliseconds((int)(pause_s_ * 1000)));
      runSegment("후진", 0, -lin_spd_, fwd_dist_, &s_back);
      rclcpp::sleep_for(std::chrono::milliseconds((int)(pause_s_ * 1000)));
    }
    if (do_rot_) {
      runSegment("회전", 1, +ang_spd_, rotate_revs_ * 2 * M_PI, &s_rot);
    }
    publishZero(10);

    auto report = [&](const char *n, const std::vector<double> &v) {
      Stat st = statOf(v);
      RCLCPP_INFO(get_logger(), "[흔들림] %s: map->odom yaw std=%.3f° pp=%.3f° (n=%.0f)",
                  n, st.std_deg, st.pp_deg, st.n);
    };
    RCLCPP_INFO(get_logger(), "================ 결과 ================");
    if (do_fb_) { report("전진", s_fwd); report("후진", s_back); }
    if (do_rot_) report("회전", s_rot);
    RCLCPP_INFO(get_logger(), "=====================================");
  }

private:
  double fwd_dist_, rotate_revs_, lin_spd_, ang_spd_, pause_s_;
  bool do_fb_, do_rot_;
  double ox_=0, oy_=0, oyaw_=0; bool have_odom_=false; rclcpp::Time last_odom_;
  rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr pub_;
  rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr odom_sub_;
  std::shared_ptr<tf2_ros::Buffer> tf_buffer_;
  std::shared_ptr<tf2_ros::TransformListener> tf_listener_;
};

int main(int argc, char **argv) {
  rclcpp::init(argc, argv);
  auto node = std::make_shared<JogTune>();
  try {
    node->run();
  } catch (const std::exception &e) {
    RCLCPP_ERROR(node->get_logger(), "예외: %s", e.what());
    node->publishZero(10);
  }
  node->publishZero(10);  // 종료 시 확실히 정지
  rclcpp::shutdown();
  return 0;
}
