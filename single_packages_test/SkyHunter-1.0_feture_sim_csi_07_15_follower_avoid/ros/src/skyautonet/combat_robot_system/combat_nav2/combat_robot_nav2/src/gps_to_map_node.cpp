// gps_to_map_node.cpp
// C++ 포팅: script/gps_to_map.py (GpsToMap) feature-parity 재구현.
//   NavSatFix(/fix) → nav_msgs/Odometry(/odometry/gps_map) 직접 변환.
//   robot_localization navsat_transform_node 가 (datum/transform 은 계산하면서도)
//   출력 position 을 하드 0 으로 내보내는 버그를 우회한다. fix 를 datum 기준
//   로컬 ENU(East=x, North=y) 미터로 변환해 ekf_map(odom0=/odometry/gps_map,
//   world=map) 가 바로 fuse 하도록 발행한다. orientation 은 ekf 가 imu 로 채우므로
//   identity.
#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/nav_sat_fix.hpp>
#include <nav_msgs/msg/odometry.hpp>
#include <robot_localization/srv/from_ll.hpp>
#include <robot_localization/srv/to_ll.hpp>
#include <geographic_msgs/msg/geo_pose.hpp>

#include <chrono>
#include <cmath>
#include <optional>
#include <string>

namespace {
constexpr double kEarthMPerDeg = 111320.0;  // 위도 1도당 미터 (경도는 cos(lat) 보정)
}  // namespace

class GpsToMap : public rclcpp::Node
{
public:
  GpsToMap()
  : Node("gps_to_map")
  {
    fix_topic_ = declare_parameter<std::string>("fix_topic", "/fix");
    out_topic_ = declare_parameter<std::string>("output_topic", "/odometry/gps_map");
    map_frame_ = declare_parameter<std::string>("map_frame", "map");
    base_frame_ = declare_parameter<std::string>("base_frame", "base_footprint");
    double stddev = declare_parameter<double>("xy_stddev", 0.3);
    var_ = stddev * stddev;

    // 편대 datum 공유: 리더(robot_id==leader_robot_id)가 첫 fix 를 datum 으로
    // /swarm/datum(latched)에 방송, 팔로워는 이를 채택 → 전 로봇 sN/map 원점 통일.
    // robot_id==0(기본, 단독)이면 공유 비활성 = 기존 first-fix 동작 그대로.
    robot_id_ = declare_parameter<int>("robot_id", 0);
    leader_robot_id_ = declare_parameter<int>("leader_robot_id", 0);
    is_leader_ = (robot_id_ == leader_robot_id_);
    datum_sync_ = (robot_id_ > 0);

    pub_ = create_publisher<nav_msgs::msg::Odometry>(out_topic_, 10);
    // fix 구독은 best_effort(sensor QoS): reliable/best_effort 발행 모두와 매칭되고,
    // 보드 재부팅 후 reliable 상호 재매칭 실패(실측: s4 fix 미수신→ekf 발산) 내성.
    subscribe_fix();

    if (datum_sync_) {
      rclcpp::QoS latched(1);
      latched.transient_local().reliable();
      datum_pub_ = create_publisher<geographic_msgs::msg::GeoPose>("/swarm/datum", latched);
      datum_sub_ = create_subscription<geographic_msgs::msg::GeoPose>(
        "/swarm/datum", latched,
        std::bind(&GpsToMap::on_datum, this, std::placeholders::_1));
      RCLCPP_INFO(get_logger(), "datum 공유 %s (robot_id=%d leader=%d)",
                  is_leader_ ? "리더(방송)" : "팔로워(대기)", robot_id_, leader_robot_id_);
      // ★ datum 주기적 재방송(2s): 단발 방송은 손실 많은 mesh 에서 transient_local
      //   late-join 전달이 드롭되면 재시도가 없어 팔로워가 영구 대기(실측: s4 fix
      //   4만개 보류·map TF 미발행)했다. 재방송은 뒤늦은 discovery/유실 후에도 팔로워가
      //   결국 채택하게 한다(팔로워는 on_datum 에서 1회만 채택 → 반복 무해). tiny GeoPose
      //   0.5Hz 라 대역 무시가능. (앵커 2Hz 재방송과 동일 대응 패턴)
      if (is_leader_) {
        datum_rebroadcast_timer_ = create_wall_timer(
          std::chrono::seconds(2), [this]() { if (lat0_) broadcast_datum(false); });
      }
    }
    status_timer_ = create_wall_timer(
      std::chrono::seconds(5), std::bind(&GpsToMap::status_timer, this));

    // swarm_path_executor 가 상대명 fromLL/toLL 로 호출(/sN/fromLL, /sN/toLL).
    // navsat_transform_node 대신 여기서 datum 기준 flat-earth ENU 로 직접 변환 제공.
    from_ll_srv_ = create_service<robot_localization::srv::FromLL>(
      "fromLL", std::bind(&GpsToMap::from_ll_cb, this,
                          std::placeholders::_1, std::placeholders::_2));
    to_ll_srv_ = create_service<robot_localization::srv::ToLL>(
      "toLL", std::bind(&GpsToMap::to_ll_cb, this,
                        std::placeholders::_1, std::placeholders::_2));

    RCLCPP_INFO(get_logger(),
      "gps_to_map: %s -> %s (frame_id=\"%s\", child=\"%s\")",
      fix_topic_.c_str(), out_topic_.c_str(), map_frame_.c_str(), base_frame_.c_str());
    RCLCPP_INFO(get_logger(), "fromLL/toLL 서비스 제공 (datum 기준 flat-earth ENU)");
  }

private:
  void subscribe_fix()
  {
    auto qos = rclcpp::SensorDataQoS();   // best_effort, depth 5
    sub_ = create_subscription<sensor_msgs::msg::NavSatFix>(
      fix_topic_, qos, std::bind(&GpsToMap::cb, this, std::placeholders::_1));
  }

  void status_timer()
  {
    // fix 무수신 워치독: 보드 재부팅 후 discovery/재매칭 실패가 실측된 바 있어(수동
    // 재기동으로만 복구), 침묵 시 원인별 로그 + 구독 재생성으로 재-discovery 유도.
    const double now = this->now().seconds();
    if (fix_seen_) {
      if (now - last_fix_sec_ > 10.0) {
        RCLCPP_WARN(get_logger(),
          "[gps_to_map] %s 수신 %.0fs 중단 — GNSS 노드/토픽 확인 필요",
          fix_topic_.c_str(), now - last_fix_sec_);
      }
    } else if (++no_fix_periods_ >= 2) {   // 기동 후 10s 이상 무수신
      RCLCPP_WARN(get_logger(),
        "[gps_to_map] %s 최초 fix 미수신(%ds, pub=%zu) — 구독 재생성(재-discovery)",
        fix_topic_.c_str(), no_fix_periods_ * 5,
        count_publishers(fix_topic_));
      sub_.reset();
      subscribe_fix();
      no_fix_periods_ = 0;   // 다음 재생성까지 다시 10s 대기
    }
    if (datum_sync_ && !is_leader_ && !lat0_) {
      RCLCPP_WARN(get_logger(),
        "[gps_to_map] 리더 datum 대기중 — fix %d개 보류(발행 0). 리더 기동/순서 확인",
        held_while_waiting_);
    }
    RCLCPP_INFO(get_logger(), "[gps_to_map] published %d in 5s", count_);
    count_ = 0;
  }

  void cb(const sensor_msgs::msg::NavSatFix::SharedPtr msg)
  {
    fix_seen_ = true;
    last_fix_sec_ = this->now().seconds();
    // NO_FIX(-1) 만 배제. 2D/3D/RTK(0,1,2) 는 사용.
    if (msg->status.status < 0) return;
    if (!lat0_) {
      // 팔로워(공유 모드)는 리더 datum 을 받을 때까지 자기 fix 로 datum 을 잡지 않고
      // 발행 보류 → 리더 프레임에 정렬. 리더/단독은 자기 첫 fix 로 datum 설정.
      if (datum_sync_ && !is_leader_) { held_while_waiting_++; return; }
      set_datum(msg->latitude, msg->longitude);
      if (datum_sync_ && is_leader_) broadcast_datum();
    }

    double north = (msg->latitude - *lat0_) * kEarthMPerDeg;
    double east = (msg->longitude - *lon0_) * kEarthMPerDeg * cos_lat0_;

    nav_msgs::msg::Odometry odom;
    odom.header.stamp = msg->header.stamp;
    odom.header.frame_id = map_frame_;
    odom.child_frame_id = base_frame_;
    odom.pose.pose.position.x = east;   // ENU: x = East
    odom.pose.pose.position.y = north;  // ENU: y = North
    odom.pose.pose.position.z = 0.0;
    odom.pose.pose.orientation.w = 1.0;  // ekf 는 imu 로 yaw 채움(무시됨)

    odom.pose.covariance[0] = var_;    // x
    odom.pose.covariance[7] = var_;    // y
    odom.pose.covariance[14] = 1e6;    // z (미사용)
    odom.pose.covariance[21] = 1e6;    // roll
    odom.pose.covariance[28] = 1e6;    // pitch
    odom.pose.covariance[35] = 1e6;    // yaw

    pub_->publish(odom);
    count_++;
  }

  void set_datum(double lat, double lon)
  {
    lat0_ = lat;
    lon0_ = lon;
    cos_lat0_ = std::cos(lat * M_PI / 180.0);
    RCLCPP_INFO(get_logger(), "datum set: lat=%.8f lon=%.8f", lat, lon);
  }

  void broadcast_datum(bool verbose = true)
  {
    if (!lat0_ || !datum_pub_) return;
    geographic_msgs::msg::GeoPose gp;
    gp.position.latitude = *lat0_;
    gp.position.longitude = *lon0_;
    gp.position.altitude = 0.0;
    gp.orientation.w = 1.0;
    datum_pub_->publish(gp);
    if (!verbose) return;                 // 주기적 재방송은 로그 생략(스팸 방지)
    RCLCPP_INFO(get_logger(),
                "리더 datum 브로드캐스트: lat=%.8f lon=%.8f", *lat0_, *lon0_);
  }

  // 팔로워: 리더가 방송한 datum 을 채택(한 번만 — 좌표계 안정).
  void on_datum(const geographic_msgs::msg::GeoPose::SharedPtr gp)
  {
    if (lat0_) return;   // 이미 설정(리더 자신 또는 이미 채택)
    set_datum(gp->position.latitude, gp->position.longitude);
    RCLCPP_INFO(get_logger(), "팔로워: 리더 datum 채택");
  }

  // 위경도(GeoPoint) -> map(Point). datum 미설정이면 0.
  void from_ll_cb(
    const std::shared_ptr<robot_localization::srv::FromLL::Request> req,
    std::shared_ptr<robot_localization::srv::FromLL::Response> res)
  {
    if (!lat0_) {
      res->map_point.x = 0.0;
      res->map_point.y = 0.0;
      res->map_point.z = 0.0;
      return;
    }
    res->map_point.x =
      (req->ll_point.longitude - *lon0_) * kEarthMPerDeg * cos_lat0_;  // East
    res->map_point.y =
      (req->ll_point.latitude - *lat0_) * kEarthMPerDeg;               // North
    res->map_point.z = 0.0;
  }

  // map(Point) -> 위경도(GeoPoint). datum 미설정이면 0.
  void to_ll_cb(
    const std::shared_ptr<robot_localization::srv::ToLL::Request> req,
    std::shared_ptr<robot_localization::srv::ToLL::Response> res)
  {
    if (!lat0_) {
      res->ll_point.latitude = 0.0;
      res->ll_point.longitude = 0.0;
      res->ll_point.altitude = 0.0;
      return;
    }
    res->ll_point.latitude = *lat0_ + req->map_point.y / kEarthMPerDeg;
    res->ll_point.longitude =
      *lon0_ + req->map_point.x / (kEarthMPerDeg * cos_lat0_);
    res->ll_point.altitude = 0.0;
  }

  // 파라미터
  std::string fix_topic_, out_topic_, map_frame_, base_frame_;
  double var_{0.09};

  // datum
  std::optional<double> lat0_;
  std::optional<double> lon0_;
  double cos_lat0_{1.0};

  // 편대 datum 공유
  int robot_id_{0};
  int leader_robot_id_{0};
  bool is_leader_{true};
  bool datum_sync_{false};

  int count_{0};

  // fix 워치독
  bool fix_seen_{false};
  double last_fix_sec_{0.0};
  int no_fix_periods_{0};
  int held_while_waiting_{0};

  rclcpp::Publisher<nav_msgs::msg::Odometry>::SharedPtr pub_;
  rclcpp::Subscription<sensor_msgs::msg::NavSatFix>::SharedPtr sub_;
  rclcpp::Publisher<geographic_msgs::msg::GeoPose>::SharedPtr datum_pub_;
  rclcpp::Subscription<geographic_msgs::msg::GeoPose>::SharedPtr datum_sub_;
  rclcpp::TimerBase::SharedPtr status_timer_;
  rclcpp::TimerBase::SharedPtr datum_rebroadcast_timer_;   // 리더 datum 2s 재방송
  rclcpp::Service<robot_localization::srv::FromLL>::SharedPtr from_ll_srv_;
  rclcpp::Service<robot_localization::srv::ToLL>::SharedPtr to_ll_srv_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<GpsToMap>());
  rclcpp::shutdown();
  return 0;
}
