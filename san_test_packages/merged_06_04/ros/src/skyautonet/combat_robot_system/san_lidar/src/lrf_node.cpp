// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 Phase 2-E Turn 7 — LrfNode implementation.

#include "san_lidar/lrf_node.hpp"

#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>

#include "san_lidar/lrf_parser.hpp"

namespace san_lidar
{

using namespace std::chrono_literals;
using LrfMsg = combat_robot_msgs::msg::LrfReading;

// ─── Stub serial backend ────────────────────────────────────────────────

namespace
{
class StubLrfSerial : public LrfSerialInterface
{
public:
  bool open(const std::string & device, int baud) override
  {
    std::cerr << "[san_lidar][STUB-LRF-SERIAL] open(\"" << device
              << "\", " << baud << ") — no real serial linked.\n";
    return false;
  }
  void close() override {}
  std::string readLine(std::chrono::milliseconds) override {return {};}
  bool isOpen() const override {return false;}
};
}  // namespace

std::unique_ptr<LrfSerialInterface> makeRealLrfSerial()
{
  return std::make_unique<StubLrfSerial>();
}

// ─── ctors / dtor ───────────────────────────────────────────────────────

LrfNode::LrfNode(const rclcpp::NodeOptions & opts)
: LrfNode(opts, makeRealLrfSerial()) {}

LrfNode::LrfNode(
  const rclcpp::NodeOptions & opts,
  std::unique_ptr<LrfSerialInterface> serial)
: rclcpp::Node("lrf_node", opts), serial_(std::move(serial))
{
  if (!serial_) {
    throw std::runtime_error("LrfNode: null serial injected");
  }
  declareParameters();
  loadParameters();

  lrf_pub_ = create_publisher<LrfMsg>(
    "~/range", rclcpp::QoS(5).reliable());
  // Phase 1: latched stub-status.
  stub_status_pub_ = create_publisher<std_msgs::msg::Bool>(
    "~/stub_status", rclcpp::QoS(1).reliable().transient_local());

  if (!serial_->open(device_, baud_)) {
    if (stub_on_no_serial_) {
      RCLCPP_WARN(
        get_logger(),
        "LRF serial unavailable (%s @ %d) — STUB mode @ %.1f Hz",
        device_.c_str(), baud_, rate_hz_);
      stub_mode_ = true;
      const auto period_ms = std::chrono::milliseconds(
        static_cast<int64_t>(1000.0 / rate_hz_));
      stub_timer_ = create_wall_timer(
        period_ms, std::bind(&LrfNode::onStubTick, this));
    } else {
      throw std::runtime_error("LrfNode: cannot open " + device_);
    }
  } else {
    running_ = true;
    reader_thread_ = std::thread(&LrfNode::readerLoop, this);
  }

  health_timer_ = create_wall_timer(
    1s, std::bind(&LrfNode::onHealthTick, this));

  // Latch stub status now that the mode is settled.
  {
    std_msgs::msg::Bool m;
    m.data = stub_mode_;
    stub_status_pub_->publish(m);
  }

  RCLCPP_INFO(
    get_logger(),
    "LrfNode UP: device=%s baud=%d frame=%s stub=%d",
    device_.c_str(), baud_, frame_id_.c_str(),
    static_cast<int>(stub_mode_));
}

LrfNode::~LrfNode()
{
  running_ = false;
  if (reader_thread_.joinable()) {reader_thread_.join();}
  if (serial_) {serial_->close();}
}

// ─── params ─────────────────────────────────────────────────────────────

void LrfNode::declareParameters()
{
  declare_parameter<std::string>("device", "/dev/ttyUSB0");
  declare_parameter<int>("baud", 115200);
  declare_parameter<std::string>("frame_id", "lrf");
  declare_parameter<double>("rate_hz", 5.0);
  declare_parameter<double>("max_range_m", 200.0);
  declare_parameter<bool>("stub_on_no_serial", true);
}

void LrfNode::loadParameters()
{
  device_ = get_parameter("device").as_string();
  baud_ = static_cast<int>(get_parameter("baud").as_int());
  frame_id_ = get_parameter("frame_id").as_string();
  rate_hz_ = get_parameter("rate_hz").as_double();
  max_range_m_ =
    static_cast<float>(get_parameter("max_range_m").as_double());
  stub_on_no_serial_ = get_parameter("stub_on_no_serial").as_bool();
  if (rate_hz_ <= 0.0 || rate_hz_ > 50.0) {
    throw std::runtime_error("LrfNode: rate_hz out of range");
  }
}

// ─── reader thread (real mode) ──────────────────────────────────────────

void LrfNode::readerLoop()
{
  const auto timeout = std::chrono::milliseconds(
    static_cast<int64_t>(2 * 1000.0 / rate_hz_));     // 2× period
  while (running_) {
    auto line = serial_->readLine(timeout);
    if (line.empty()) {continue;}
    auto s = parseLrfLine(line, max_range_m_);
    if (!s) {
      ++dropped_count_;
      continue;
    }
    publishReading(s->range_m, s->return_strength, s->valid);
  }
}

void LrfNode::onStubTick()
{
  // Plausible 5 m reading slowly oscillating ±30 cm — matches Python stub.
  const float t = std::chrono::duration<float>(
    std::chrono::steady_clock::now().time_since_epoch()).count();
  const float r = 5.0f + 0.3f * std::sin(t);
  publishReading(r, 0.85f, true);
}

void LrfNode::publishReading(float range_m, float strength, bool valid)
{
  LrfMsg msg;
  msg.header.stamp = now();
  msg.header.frame_id = frame_id_;
  msg.range_m = range_m;
  msg.return_strength = strength;
  msg.valid = valid;
  msg.dropped_count = dropped_count_.load();
  msg.last_read_timestamp_ms =
    static_cast<uint64_t>(now().nanoseconds() / 1'000'000);
  lrf_pub_->publish(msg);

  last_range_m_ = range_m;
  last_valid_ = valid;
  ++reading_count_;
}

void LrfNode::onHealthTick()
{
  RCLCPP_INFO(
    get_logger(),
    "lrf mode=%s readings=%u drops=%u last=%.2f m valid=%d",
    stub_mode_ ? "STUB" : "REAL",
    reading_count_.load(), dropped_count_.load(),
    last_range_m_, static_cast<int>(last_valid_));
}

}  // namespace san_lidar
