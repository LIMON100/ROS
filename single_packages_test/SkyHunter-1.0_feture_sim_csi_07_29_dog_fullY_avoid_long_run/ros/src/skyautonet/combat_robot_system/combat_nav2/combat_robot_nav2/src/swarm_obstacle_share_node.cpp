// swarm_obstacle_share_node.cpp — 편대 장애물 공유(C++ 포팅, swarm_obstacle_share.py 동등)
//
//  내보내기: 자기 local costmap(odom, rolling)의 lethal 셀(>=99)
//            → voxel 다운샘플 → TF(map←odom) → /swarm/obstacle_cloud(글로벌, intensity=rid)
//  받아오기: /swarm/obstacle_cloud 에서 남의 점만 TTL 병합 → 자기 base_footprint 프레임으로
//            /sN/local_costmap/swarm_obstacles 재발행 → obstacle_layer swarm_shared(marking).
//  실행(보드): ros2 run combat_robot_nav2 swarm_obstacle_share_node --ros-args \
//    -r __ns:=/s2 -p robot_id:=2 -r /tf:=/s2/tf -r /tf_static:=/s2/tf_static
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <rclcpp/rclcpp.hpp>
#include <builtin_interfaces/msg/time.hpp>
#include <geometry_msgs/msg/transform_stamped.hpp>
#include <nav_msgs/msg/occupancy_grid.hpp>
#include <sensor_msgs/msg/point_cloud2.hpp>
#include <sensor_msgs/msg/point_field.hpp>
#include <tf2/time.h>
#include <tf2_ros/buffer.h>
#include <tf2_ros/transform_listener.h>

using namespace std::chrono_literals;

namespace {
// steady_clock 초 (단조; .py time.time() 대응, TTL/echo_hold 용)
double now_s() {
  return std::chrono::duration<double>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
}
// voxel 키 (ix,iy) → int64 인코딩
inline int64_t vkey(int ix, int iy) {
  return (static_cast<int64_t>(ix) << 32) |
         static_cast<int64_t>(static_cast<uint32_t>(iy));
}
// points:[(x,y,z,intensity)] → PointCloud2 (xyzi float32, .py make_cloud 동일 와이어포맷)
sensor_msgs::msg::PointCloud2 make_cloud(
    const std::vector<std::array<float, 4>> & points,
    const std::string & frame_id,
    const builtin_interfaces::msg::Time & stamp) {
  sensor_msgs::msg::PointCloud2 msg;
  msg.header.frame_id = frame_id;
  msg.header.stamp = stamp;
  msg.height = 1;
  msg.width = static_cast<uint32_t>(points.size());
  const char * names[4] = {"x", "y", "z", "intensity"};
  msg.fields.resize(4);
  for (int i = 0; i < 4; ++i) {
    msg.fields[i].name = names[i];
    msg.fields[i].offset = static_cast<uint32_t>(i * 4);
    msg.fields[i].datatype = sensor_msgs::msg::PointField::FLOAT32;
    msg.fields[i].count = 1;
  }
  msg.is_bigendian = false;
  msg.point_step = 16;
  msg.row_step = 16u * msg.width;
  msg.is_dense = true;
  msg.data.resize(static_cast<size_t>(16) * points.size());
  size_t off = 0;
  for (const auto & p : points) {
    std::memcpy(msg.data.data() + off, p.data(), 16);
    off += 16;
  }
  return msg;
}
}  // namespace

class SwarmObstacleShare : public rclcpp::Node {
public:
  SwarmObstacleShare() : rclcpp::Node("swarm_obstacle_share") {
    rid_ = declare_parameter<int>("robot_id", 0);
    voxel_ = std::max(0.05, declare_parameter<double>("voxel_m", 0.3));  // 0 나눗셈 하한
    ttl_ = declare_parameter<double>("ttl_s", 6.0);
    echo_hold_ = declare_parameter<double>("echo_hold_s", 20.0);
    self_clear_ = declare_parameter<double>("self_clear_radius", 1.6);
    max_pts_ = declare_parameter<int>("max_points", 400);
    std::string ns = get_namespace();  // "/s2"
    if (!ns.empty() && ns.front() == '/') ns.erase(0, 1);
    map_frame_ = ns + "/map";
    base_frame_ = ns + "/base_footprint";
    if (rid_ == 0) {
      RCLCPP_WARN(get_logger(),
        "robot_id=0 (기본값) — 다른 노드와 같으면 echo 필터가 서로의 실제 "
        "장애물을 무시한다. -p robot_id:=N 으로 반드시 지정하라.");
    }
    tf_buffer_ = std::make_shared<tf2_ros::Buffer>(get_clock());
    tf_listener_ = std::make_shared<tf2_ros::TransformListener>(*tf_buffer_);

    sub_costmap_ = create_subscription<nav_msgs::msg::OccupancyGrid>(
      "local_costmap/costmap", rclcpp::QoS(2),
      std::bind(&SwarmObstacleShare::on_costmap, this, std::placeholders::_1));
    auto qg = rclcpp::QoS(5).best_effort();  // 글로벌: 손실 많은 mesh
    pub_global_ = create_publisher<sensor_msgs::msg::PointCloud2>(
      "/swarm/obstacle_cloud", qg);
    sub_global_ = create_subscription<sensor_msgs::msg::PointCloud2>(
      "/swarm/obstacle_cloud", qg,
      std::bind(&SwarmObstacleShare::on_shared, this, std::placeholders::_1));
    pub_local_ = create_publisher<sensor_msgs::msg::PointCloud2>(
      "local_costmap/swarm_obstacles", rclcpp::QoS(1).reliable());  // 로컬
    timer_ = create_wall_timer(
      500ms, std::bind(&SwarmObstacleShare::republish_local, this));
    RCLCPP_INFO(get_logger(),
      "swarm_obstacle_share 준비 — rid=%d base=%s voxel=%.2f ttl=%.1fs "
      "echo_hold=%.1fs self_clear=%.1fm",
      rid_, base_frame_.c_str(), voxel_, ttl_, echo_hold_, self_clear_);
  }

private:
  // ---- 내보내기: costmap lethal → map 점 → 글로벌 방송 ----
  void on_costmap(const nav_msgs::msg::OccupancyGrid::SharedPtr g) {
    geometry_msgs::msg::TransformStamped tf;
    try {
      tf = tf_buffer_->lookupTransform(
        map_frame_, g->header.frame_id, tf2::TimePointZero);
    } catch (const std::exception &) {
      return;
    }
    const double tx = tf.transform.translation.x, ty = tf.transform.translation.y;
    const double qz = tf.transform.rotation.z, qw = tf.transform.rotation.w;
    const double yaw = std::atan2(2.0 * qw * qz, 1.0 - 2.0 * qz * qz);
    const double c = std::cos(yaw), s = std::sin(yaw);
    const double res = g->info.resolution;
    const double ox = g->info.origin.position.x, oy = g->info.origin.position.y;
    const uint32_t w = g->info.width;
    const double now = now_s();
    // 재공유 echo 차단: seen(echo_hold≫ttl) + 8이웃 voxel set, 셀당 O(1) 룩업.
    std::unordered_map<int64_t, char> inc_keys;
    for (const auto & kv : seen_) {
      if (now - kv.second >= echo_hold_) continue;
      const int ix = static_cast<int>(kv.first >> 32);
      const int iy = static_cast<int>(static_cast<int32_t>(kv.first & 0xffffffff));
      for (int dx = -1; dx <= 1; ++dx)
        for (int dy = -1; dy <= 1; ++dy)
          inc_keys[vkey(ix + dx, iy + dy)] = 1;
    }
    std::unordered_map<int64_t, char> vox;
    std::vector<std::array<float, 4>> pts;
    for (size_t i = 0; i < g->data.size(); ++i) {
      if (g->data[i] < 99) continue;  // lethal(100)+inscribed(99)만
      const double gx = ox + (static_cast<double>(i % w) + 0.5) * res;
      const double gy = oy + (static_cast<double>(i / w) + 0.5) * res;
      const double mx = tx + c * gx - s * gy;  // odom→map
      const double my = ty + s * gx + c * gy;
      const int64_t key = vkey(static_cast<int>(mx / voxel_),
                               static_cast<int>(my / voxel_));
      if (vox.count(key) || inc_keys.count(key)) continue;
      vox[key] = 1;
      pts.push_back({static_cast<float>(mx), static_cast<float>(my), 0.5f,
                     static_cast<float>(rid_)});
      if (static_cast<int>(vox.size()) >= max_pts_) break;
    }
    pub_global_->publish(make_cloud(pts, map_frame_, g->header.stamp));
  }
  // ---- 받아오기: 남의 점 TTL 병합 ----
  void on_shared(const sensor_msgs::msg::PointCloud2::SharedPtr msg) {
    const double now = now_s();
    const size_t step = msg->point_step;
    // ★ 강건화(리뷰): point_step<16 이면 아래 16바이트 memcpy 가 버퍼를 넘겨 읽고(OOB),
    //   step==0 이면 off 가 안 늘어 무한루프. /swarm/obstacle_cloud 는 외부발행 가능한
    //   메시 토픽이라(악성/오발행 대비) 조기 반환. (py 는 struct.error 로 안전했음)
    if (step < 16) return;
    const uint8_t * d = msg->data.data();
    for (size_t off = 0; off + step <= msg->data.size(); off += step) {
      float x, y, z, inten;
      std::memcpy(&x, d + off + 0, 4);
      std::memcpy(&y, d + off + 4, 4);
      std::memcpy(&z, d + off + 8, 4);
      std::memcpy(&inten, d + off + 12, 4);
      (void)z;
      if (static_cast<int>(inten) == rid_) continue;  // 내 방송 echo 무시
      const int64_t key = vkey(static_cast<int>(x / voxel_),
                               static_cast<int>(y / voxel_));
      incoming_[key] = {static_cast<double>(x), static_cast<double>(y), now};
      seen_[key] = now;  // echo 억제(장수명)
    }
  }

  void republish_local() {
    const double now = now_s();
    // 마킹 TTL / echo 억제 이력 만료 정리
    for (auto it = incoming_.begin(); it != incoming_.end();)
      it = (now - it->second[2] >= ttl_) ? incoming_.erase(it) : std::next(it);
    for (auto it = seen_.begin(); it != seen_.end();)
      it = (now - it->second >= echo_hold_) ? seen_.erase(it) : std::next(it);
    // 자기 pose(map←base): ①팀원이 본 '나' 제거 ②점을 base 프레임으로 변환
    //   (base 발행이라야 obstacle_max_range 가 datum 아닌 로봇 기준으로 측정됨).
    geometry_msgs::msg::TransformStamped tf;
    try {
      tf = tf_buffer_->lookupTransform(
        map_frame_, base_frame_, tf2::TimePointZero);
    } catch (const std::exception &) {
      return;  // pose 없으면 이번 주기 skip
    }
    const double bx = tf.transform.translation.x, by = tf.transform.translation.y;
    const double qz = tf.transform.rotation.z, qw = tf.transform.rotation.w;
    const double yaw = std::atan2(2.0 * qw * qz, 1.0 - 2.0 * qz * qz);
    const double c = std::cos(yaw), s = std::sin(yaw);
    std::vector<std::array<float, 4>> pts;
    for (const auto & kv : incoming_) {
      const double x = kv.second[0], y = kv.second[1];
      const double dx = x - bx, dy = y - by;
      if (std::hypot(dx, dy) < self_clear_) continue;  // 팀원이 본 '나' 제거
      // map→base 회전(전치): base = R(-yaw)·(map - t)
      pts.push_back({static_cast<float>(c * dx + s * dy),
                     static_cast<float>(-s * dx + c * dy), 0.5f, 1.0f});
    }
    pub_local_->publish(make_cloud(pts, base_frame_, this->now()));
  }

  int rid_{0};
  double voxel_{0.3}, ttl_{6.0}, echo_hold_{20.0}, self_clear_{1.6};
  int max_pts_{400};
  std::string map_frame_, base_frame_;
  std::shared_ptr<tf2_ros::Buffer> tf_buffer_;
  std::shared_ptr<tf2_ros::TransformListener> tf_listener_;
  rclcpp::Subscription<nav_msgs::msg::OccupancyGrid>::SharedPtr sub_costmap_;
  rclcpp::Publisher<sensor_msgs::msg::PointCloud2>::SharedPtr pub_global_, pub_local_;
  rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr sub_global_;
  rclcpp::TimerBase::SharedPtr timer_;
  // 수신 저장 {vkey: [x,y,stamp]} (남의 점, map), echo 억제 이력 {vkey: recv_time}
  std::unordered_map<int64_t, std::array<double, 3>> incoming_;
  std::unordered_map<int64_t, double> seen_;
};

int main(int argc, char ** argv) {
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<SwarmObstacleShare>());
  rclcpp::shutdown();
  return 0;
}
