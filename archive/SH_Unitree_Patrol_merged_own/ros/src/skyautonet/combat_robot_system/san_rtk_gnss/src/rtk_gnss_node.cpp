// SAN v1.5 Phase 2-E Turn 4 — RtkGnssNode implementation.

#include "san_rtk_gnss/rtk_gnss_node.hpp"

#include <chrono>
#include <cmath>      // [DCN-2026-006 EXT D-022] std::sin/std::cos
#include <stdexcept>

#include "san_rtk_gnss/nmea_parser.hpp"

namespace san_rtk_gnss {

using namespace std::chrono_literals;
using RtkStatusMsg = combat_robot_msgs::msg::RtkFixStatus;

// ─── ctors / dtor ───────────────────────────────────────────────────────

RtkGnssNode::RtkGnssNode(const rclcpp::NodeOptions& opts)
    : RtkGnssNode(opts, makeRealSerial()) {}

RtkGnssNode::RtkGnssNode(const rclcpp::NodeOptions& opts,
                          std::unique_ptr<SerialInterface> serial)
    : rclcpp::Node("rtk_gnss_node", opts), serial_(std::move(serial)) {
  if (!serial_) {
    throw std::runtime_error("RtkGnssNode: null serial injected");
  }
  declareParameters();
  loadParameters();

  fix_pub_    = create_publisher<sensor_msgs::msg::NavSatFix>(
      "~/fix", rclcpp::SensorDataQoS().keep_last(5));
  status_pub_ = create_publisher<RtkStatusMsg>(
      "~/rtk_status", rclcpp::QoS(5).reliable());
  gga_pub_    = create_publisher<std_msgs::msg::String>(
      "~/gga_latest", rclcpp::QoS(1).reliable().transient_local());

  // [DCN-2026-006 EXT D-022] Dual-antenna heading (Imu) publisher.
  // SensorDataQoS so robot_localization's EKF imu subscription
  // (best_effort by default) matches without a QoS mismatch warning.
  heading_pub_ = create_publisher<sensor_msgs::msg::Imu>(
      "~/heading", rclcpp::SensorDataQoS().keep_last(5));

  rtcm_sub_   = create_subscription<std_msgs::msg::UInt8MultiArray>(
      "~/rtcm_corrections",
      rclcpp::QoS(10).reliable(),
      std::bind(&RtkGnssNode::onRtcm, this, std::placeholders::_1));

  if (!serial_->open(device_, baud_)) {
    if (stub_on_no_serial_) {
      RCLCPP_WARN(get_logger(),
          "RTK serial unavailable (%s @ %d) — reader thread idle.",
          device_.c_str(), baud_);
    } else {
      throw std::runtime_error("RtkGnssNode: cannot open " + device_);
    }
  }

  running_ = true;
  reader_thread_ = std::thread(&RtkGnssNode::readerLoop, this);
  health_timer_  = create_wall_timer(
      1s, std::bind(&RtkGnssNode::onHealthTick, this));

  RCLCPP_INFO(get_logger(),
      "RtkGnssNode UP: device=%s baud=%d frame=%s",
      device_.c_str(), baud_, frame_id_.c_str());
}

RtkGnssNode::~RtkGnssNode() {
  running_ = false;
  if (reader_thread_.joinable()) reader_thread_.join();
  if (serial_) serial_->close();
}

// ─── params ─────────────────────────────────────────────────────────────

void RtkGnssNode::declareParameters() {
  declare_parameter<std::string>("device",   "/dev/ttyACM0");
  declare_parameter<int>("baud",             115200);
  declare_parameter<std::string>("frame_id", "gnss");
  declare_parameter<bool>("stub_on_no_serial", true);
}

void RtkGnssNode::loadParameters() {
  device_            = get_parameter("device").as_string();
  baud_              = static_cast<int>(get_parameter("baud").as_int());
  frame_id_          = get_parameter("frame_id").as_string();
  stub_on_no_serial_ = get_parameter("stub_on_no_serial").as_bool();
}

// ─── reader thread ──────────────────────────────────────────────────────

void RtkGnssNode::readerLoop() {
  while (running_) {
    if (!serial_->isOpen()) {
      std::this_thread::sleep_for(500ms);
      continue;
    }
    auto line = serial_->readLine(200ms);
    if (line.empty()) continue;
    ++nmea_count_;

    // [DCN-2026-006 EXT D-022] Multi-sentence dispatch — GGA carries
    // the fix info, HDT carries dual-antenna heading. We do not gate
    // them against each other; HDT is published only when present.
    if (line.find("GGA") != std::string::npos) {
        publishFix(line);
    } else if (line.find("HDT") != std::string::npos) {
        publishHeading(line);
    }
    // Other sentences ($GxGSA, $GxRMC, ...) are dropped for now.
  }
}

// ─── publishFix ─────────────────────────────────────────────────────────

void RtkGnssNode::publishFix(const std::string& gga_line) {
  // 1. Forward raw GGA for NTRIP VRS uplink
  std_msgs::msg::String gga_msg;
  gga_msg.data = gga_line;
  gga_pub_->publish(gga_msg);

  // 2. Parse
  auto r = parseGga(gga_line);
  if (!r) {
    ++dropped_count_;
    return;
  }
  last_fix_type_ = static_cast<uint8_t>(r->fix_type);

  // 3. NavSatFix
  sensor_msgs::msg::NavSatFix fix;
  fix.header.stamp     = now();
  fix.header.frame_id  = frame_id_;
  fix.latitude         = r->latitude_deg;
  fix.longitude        = r->longitude_deg;
  fix.altitude         = r->altitude_m;
  // status: -1 NO_FIX, 0 FIX, 1 SBAS_FIX, 2 GBAS_FIX (RTK)
  fix.status.status =
      (r->fix_type == FixType::No)        ? sensor_msgs::msg::NavSatStatus::STATUS_NO_FIX :
      (r->fix_type == FixType::RtkFix ||
       r->fix_type == FixType::RtkFloat)  ? sensor_msgs::msg::NavSatStatus::STATUS_GBAS_FIX :
      (r->fix_type == FixType::Dgps)      ? sensor_msgs::msg::NavSatStatus::STATUS_SBAS_FIX :
                                            sensor_msgs::msg::NavSatStatus::STATUS_FIX;
  fix.status.service = sensor_msgs::msg::NavSatStatus::SERVICE_GPS;
  fix_pub_->publish(fix);

  // 4. RtkFixStatus
  RtkStatusMsg s;
  s.header.stamp                = fix.header.stamp;
  s.header.frame_id             = frame_id_;
  s.fix_type                    = static_cast<uint8_t>(r->fix_type);
  s.num_satellites_view         = r->num_satellites_used;   // GGA only has "used"
  s.num_satellites_used         = r->num_satellites_used;
  s.latitude_deg                = r->latitude_deg;
  s.longitude_deg               = r->longitude_deg;
  s.altitude_m                  = r->altitude_m;
  s.geoid_separation_m          = r->geoid_separation_m;
  s.hdop                        = r->hdop;
  s.vdop                        = 0.0f;                     // populated by GSA later
  s.pdop                        = 0.0f;
  s.rtk_correction_age_s        = r->rtk_correction_age_s;
  s.reference_station_id        = r->reference_station_id;
  s.dropped_nmea_count          = dropped_count_.load();
  s.rtcm_inject_count           = rtcm_inject_count_.load();
  // Static-analysis hardening: stamp.sec is int32. The previous
  // `sec * 1000ULL` promoted a negative sec to an enormous uint64 and
  // produced a meaningless last_fix_timestamp_ms (downstream age math
  // then wrapped a second time). Go through rclcpp::Time so an unset
  // stamp returns 0 cleanly.
  const int64_t stamp_ns = rclcpp::Time(fix.header.stamp).nanoseconds();
  s.last_fix_timestamp_ms       =
      stamp_ns > 0 ? static_cast<uint64_t>(stamp_ns / 1'000'000LL) : 0ULL;
  status_pub_->publish(s);
}

// ─── publishHeading (D-022) ─────────────────────────────────────────────

// [DCN-2026-006 EXT D-022] $GxHDT → sensor_msgs/Imu.
//
// Yaw-only quaternion: q = (0, 0, sin(yaw/2), cos(yaw/2)).
// NMEA HDT heading is degrees clockwise from True North; ROS REP-103
// yaw is radians counter-clockwise from East. Conversion:
//   yaw_rep103 = π/2 - heading_rad
// (i.e. North == π/2 in REP-103 frame, increase counter-clockwise).
//
// Covariance per RTK fix quality, indexed at orientation yaw (3,3):
//   FIX   → 0.0017 rad² (≈ 2.4°² — dual-antenna 0.1°)
//   FLOAT → 0.030  rad² (≈ 10°²  — degraded)
//   else  → 1.0    rad² (≈ 57°²  — large; EKF effectively ignores)
//
// Angular velocity / linear acceleration covariance [0] is set to -1
// per robot_localization convention to mean "no data for this axis".
sensor_msgs::msg::Imu RtkGnssNode::buildHeadingMsg(
    double heading_deg,
    uint8_t fix_type,
    const std::string& frame_id,
    const rclcpp::Time& stamp)
{
  // NMEA True-North-clockwise → REP-103 East-counter-clockwise.
  constexpr double kPi      = 3.14159265358979323846;
  const double heading_rad  = heading_deg * kPi / 180.0;
  const double yaw_rep103   = (kPi / 2.0) - heading_rad;

  sensor_msgs::msg::Imu m;
  m.header.stamp    = stamp;
  m.header.frame_id = frame_id;

  m.orientation.x = 0.0;
  m.orientation.y = 0.0;
  m.orientation.z = std::sin(yaw_rep103 / 2.0);
  m.orientation.w = std::cos(yaw_rep103 / 2.0);

  // Covariance — 3×3 row-major, [0..8]. yaw is index 8 (row 2, col 2).
  // Roll / pitch get a large value (no information).
  using ft = san_rtk_gnss::FixType;
  double yaw_var;
  if      (fix_type == static_cast<uint8_t>(ft::RtkFix))   yaw_var = 0.0017;
  else if (fix_type == static_cast<uint8_t>(ft::RtkFloat)) yaw_var = 0.030;
  else                                                     yaw_var = 1.0;

  for (auto& v : m.orientation_covariance) v = 0.0;
  m.orientation_covariance[0] = 1.0;        // roll  — no info
  m.orientation_covariance[4] = 1.0;        // pitch — no info
  m.orientation_covariance[8] = yaw_var;    // yaw   — RTK-quality scaled

  // Mark angular velocity + linear acceleration as "no data".
  m.angular_velocity_covariance[0]    = -1.0;
  m.linear_acceleration_covariance[0] = -1.0;
  return m;
}

void RtkGnssNode::publishHeading(const std::string& hdt_line) {
  auto r = parseHdt(hdt_line);
  if (!r || !r->valid) {
    ++dropped_count_;
    return;
  }
  heading_pub_->publish(buildHeadingMsg(
      r->heading_deg, last_fix_type_.load(), frame_id_, now()));
}

// ─── RTCM injection ─────────────────────────────────────────────────────

void RtkGnssNode::onRtcm(const std_msgs::msg::UInt8MultiArray::SharedPtr msg) {
  if (!serial_ || !serial_->isOpen()) return;
  serial_->write(msg->data);
  ++rtcm_inject_count_;
}

// ─── health ─────────────────────────────────────────────────────────────

void RtkGnssNode::onHealthTick() {
  static const char* names[] = {
      "NO", "AUTO_2D", "DGPS", "PPS",
      "RTK_FIX", "RTK_FLOAT", "EST", "MAN", "SIM"};
  const uint8_t ft = last_fix_type_.load();
  RCLCPP_INFO(get_logger(),
      "rtk healthy=%d nmea=%u dropped=%u rtcm_inj=%u fix=%s",
      static_cast<int>(serial_->isOpen()),
      nmea_count_.load(), dropped_count_.load(), rtcm_inject_count_.load(),
      (ft <= 8) ? names[ft] : "?");
}

}  // namespace san_rtk_gnss
