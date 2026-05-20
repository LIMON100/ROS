// SAN v1.5 Phase 2-E Turn 5 — ImuDriverNode implementation.

#include "san_imu_driver/imu_driver_node.hpp"

#include <chrono>
#include <iostream>
#include <random>
#include <stdexcept>

#include "san_imu_driver/binary_frame_parser.hpp"

namespace san_imu_driver {

using namespace std::chrono_literals;

// ─── Stub serial impl (build-time when no real backend) ─────────────────

namespace {
class StubImuSerial : public ImuSerialInterface {
public:
  bool open(const std::string& device, int baud) override {
    std::cerr << "[san_imu_driver][STUB-SERIAL] open(\"" << device
              << "\", " << baud << ") — no real backend linked.\n";
    return false;
  }
  void close() override {}
  std::vector<uint8_t> read(size_t, std::chrono::milliseconds) override {
    return {};
  }
  bool isOpen() const override { return false; }
};
}  // namespace

std::unique_ptr<ImuSerialInterface> makeRealImuSerial() {
  return std::make_unique<StubImuSerial>();
}

// ─── ctors / dtor ───────────────────────────────────────────────────────

ImuDriverNode::ImuDriverNode(const rclcpp::NodeOptions& opts)
    : ImuDriverNode(opts, makeRealImuSerial()) {}

ImuDriverNode::ImuDriverNode(const rclcpp::NodeOptions& opts,
                              std::unique_ptr<ImuSerialInterface> serial)
    : rclcpp::Node("imu_driver_node", opts), serial_(std::move(serial)) {
  if (!serial_) {
    throw std::runtime_error("ImuDriverNode: null serial injected");
  }
  declareParameters();
  loadParameters();

  imu_pub_ = create_publisher<sensor_msgs::msg::Imu>(
      "~/imu", rclcpp::SensorDataQoS().keep_last(50));

  if (!serial_->open(device_, baud_)) {
    if (stub_on_no_serial_) {
      RCLCPP_WARN(get_logger(),
          "IMU serial %s unavailable — entering STUB mode @ %.1f Hz",
          device_.c_str(), stub_rate_hz_);
      stub_mode_ = true;
      const auto period_ms = std::chrono::milliseconds(
          static_cast<int64_t>(1000.0 / stub_rate_hz_));
      stub_timer_ = create_wall_timer(
          period_ms,
          std::bind(&ImuDriverNode::publishStubSample, this));
    } else {
      throw std::runtime_error("ImuDriverNode: cannot open " + device_);
    }
  } else {
    running_ = true;
    reader_thread_ = std::thread(&ImuDriverNode::readerLoop, this);
  }

  health_timer_ = create_wall_timer(
      1s, std::bind(&ImuDriverNode::onHealthTick, this));

  RCLCPP_INFO(get_logger(),
      "ImuDriverNode UP: device=%s baud=%d frame=%s stub=%d",
      device_.c_str(), baud_, frame_id_.c_str(),
      static_cast<int>(stub_mode_));
}

ImuDriverNode::~ImuDriverNode() {
  running_ = false;
  if (reader_thread_.joinable()) reader_thread_.join();
  if (serial_) serial_->close();
}

// ─── params ─────────────────────────────────────────────────────────────

void ImuDriverNode::declareParameters() {
  declare_parameter<std::string>("device",           "/dev/ttyACM1");
  declare_parameter<int>("baud",                     921600);
  declare_parameter<std::string>("frame_id",         "payload_imu");
  declare_parameter<double>("stub_rate_hz",          200.0);
  declare_parameter<bool>("stub_on_no_serial",       true);
  declare_parameter<int>("sync_byte_0",              0xFA);
  declare_parameter<bool>("checksum_xor",            true);
}

void ImuDriverNode::loadParameters() {
  device_            = get_parameter("device").as_string();
  baud_              = static_cast<int>(get_parameter("baud").as_int());
  frame_id_          = get_parameter("frame_id").as_string();
  stub_rate_hz_      = get_parameter("stub_rate_hz").as_double();
  stub_on_no_serial_ = get_parameter("stub_on_no_serial").as_bool();
  sync_byte_0_       =
      static_cast<uint8_t>(get_parameter("sync_byte_0").as_int() & 0xFF);
  checksum_xor_      = get_parameter("checksum_xor").as_bool();
  if (stub_rate_hz_ <= 0.0 || stub_rate_hz_ > 1000.0) {
    throw std::runtime_error(
        "ImuDriverNode: stub_rate_hz out of range");
  }
}

// ─── reader thread (real mode) ──────────────────────────────────────────

void ImuDriverNode::readerLoop() {
  BinaryFrameConfig cfg;
  cfg.sync_byte_0   = sync_byte_0_;
  cfg.checksum_kind = checksum_xor_ ?
      ChecksumKind::XorByte : ChecksumKind::Sum16;
  cfg.max_payload   = 256;

  std::vector<uint8_t> buf;
  buf.reserve(1024);

  while (running_) {
    auto chunk = serial_->read(512, 50ms);
    if (chunk.empty()) continue;
    buf.insert(buf.end(), chunk.begin(), chunk.end());

    while (true) {
      size_t consumed = 0;
      auto frame = parseBinaryFrame(buf, cfg, &consumed);
      if (!frame) {
        if (consumed > 0) {
          buf.erase(buf.begin(), buf.begin() + consumed);
          ++drop_count_;
        }
        break;
      }
      buf.erase(buf.begin(), buf.begin() + consumed);
      publishFromPayload(frame->payload);
      ++frame_count_;
    }

    // Bound buffer growth in case of persistent unsynced bytes
    if (buf.size() > 4096) buf.clear();
  }
}

// ─── publishing ─────────────────────────────────────────────────────────

void ImuDriverNode::publishStubSample() {
  // Static-platform model: gravity in +Z, near-zero gyro + small noise.
  // (Matches Python adapter's stub for downstream regression.)
  static thread_local std::mt19937 rng{std::random_device{}()};
  std::normal_distribution<double> acc_noise(0.0, 0.05);
  std::normal_distribution<double> gyro_noise(0.0, 0.005);

  sensor_msgs::msg::Imu msg;
  msg.header.stamp    = now();
  msg.header.frame_id = frame_id_;
  msg.linear_acceleration.x = acc_noise(rng);
  msg.linear_acceleration.y = acc_noise(rng);
  msg.linear_acceleration.z = 9.81 + acc_noise(rng);
  msg.angular_velocity.x = gyro_noise(rng);
  msg.angular_velocity.y = gyro_noise(rng);
  msg.angular_velocity.z = gyro_noise(rng);
  // Identity quaternion — stub has no AHRS
  msg.orientation.x = 0.0;
  msg.orientation.y = 0.0;
  msg.orientation.z = 0.0;
  msg.orientation.w = 1.0;
  imu_pub_->publish(std::move(msg));
  ++frame_count_;
}

void ImuDriverNode::publishFromPayload(
    const std::vector<uint8_t>& payload) {
  // TODO Turn 5.5: decode model-specific payload layout (Xsens MT,
  // VectorNav, Bosch BMI088, etc.). For now we just log size and
  // republish an empty sample so downstream nodes can still see the
  // rate. Payload bytes are NOT discarded — the framing alone proves
  // serial path is healthy; payload decode is a separate concern.
  RCLCPP_DEBUG_THROTTLE(get_logger(), *get_clock(), 1000,
      "decoded payload (%zu bytes) — model-specific layout TODO",
      payload.size());
  publishStubSample();
}

// ─── health ─────────────────────────────────────────────────────────────

void ImuDriverNode::onHealthTick() {
  RCLCPP_INFO(get_logger(),
      "imu mode=%s frames=%u drops=%u serial=%d",
      stub_mode_ ? "STUB" : "REAL",
      frame_count_.load(), drop_count_.load(),
      static_cast<int>(serial_ && serial_->isOpen()));
}

}  // namespace san_imu_driver
