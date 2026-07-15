// SAN v1.5 Phase 2-E Turn 3 — LteModemNode implementation.

#include "san_lte_redundancy/lte_modem_node.hpp"

#include <chrono>
#include <stdexcept>

#include "san_lte_redundancy/at_response_parser.hpp"

namespace san_lte_redundancy {

using namespace std::chrono_literals;
using StatusMsg = combat_robot_msgs::msg::LteModemStatus;

// ─── ctors ──────────────────────────────────────────────────────────────

LteModemNode::LteModemNode(const rclcpp::NodeOptions& opts)
    : LteModemNode(opts, makeRealAtCommand()) {}

LteModemNode::LteModemNode(const rclcpp::NodeOptions& opts,
                            std::unique_ptr<AtCommandInterface> at)
    : rclcpp::Node("lte_modem_node", opts), at_(std::move(at)) {
  if (!at_) {
    throw std::runtime_error(
        "LteModemNode: null AT command interface injected");
  }

  declareParameters();
  loadParameters();

  // BEST_EFFORT depth=1 — only-latest semantics, 0.5 Hz heartbeat.
  status_pub_ = create_publisher<StatusMsg>(
      "~/modem_status", rclcpp::QoS(1).best_effort());

  initializeAtPort();
  resetState();

  // Period from poll_hz_ (default 0.5 Hz = 2 s).
  const auto period_ms = std::chrono::milliseconds(
      static_cast<int64_t>(1000.0 / poll_hz_));
  poll_timer_ = create_wall_timer(
      period_ms, std::bind(&LteModemNode::onPollTick, this));

  RCLCPP_INFO(get_logger(),
              "LteModemNode UP: device=%s baud=%d poll_hz=%.2f stub=%d",
              at_device_.c_str(), baud_, poll_hz_,
              static_cast<int>(stub_mode_));
}

// ─── parameters ─────────────────────────────────────────────────────────

void LteModemNode::declareParameters() {
  declare_parameter<std::string>("at_device",       "/dev/ttyUSB2");
  declare_parameter<int>("baud",                    115200);
  declare_parameter<double>("poll_hz",              0.5);
  declare_parameter<bool>("stub_on_no_modem",       true);
}

void LteModemNode::loadParameters() {
  at_device_         = get_parameter("at_device").as_string();
  baud_              = static_cast<int>(get_parameter("baud").as_int());
  poll_hz_           = get_parameter("poll_hz").as_double();
  stub_on_no_modem_  = get_parameter("stub_on_no_modem").as_bool();
  if (poll_hz_ <= 0.0 || poll_hz_ > 100.0) {
    throw std::runtime_error(
        "LteModemNode: poll_hz must be (0, 100]; got " +
        std::to_string(poll_hz_));
  }
}

// ─── AT port init ───────────────────────────────────────────────────────

void LteModemNode::initializeAtPort() {
  if (!at_->open(at_device_, baud_)) {
    if (stub_on_no_modem_) {
      RCLCPP_WARN(get_logger(),
          "LTE modem AT port unavailable: %s @ %d. Operating in "
          "STUB mode (canned LteModemStatus).",
          at_device_.c_str(), baud_);
      stub_mode_ = true;
      return;
    }
    throw std::runtime_error(
        "LteModemNode: cannot open AT port: " + at_device_);
  }

  // Echo off + verbose error codes — best-effort, ignore errors.
  at_->send("ATE0",    300ms);
  at_->send("AT+CMEE=2", 300ms);
}

void LteModemNode::resetState() {
  state_ = StatusMsg();
  state_.header.frame_id = "lte";
  state_.registered      = StatusMsg::REG_UNKNOWN;
  state_.rsrp_dbm        = INT32_MIN;
  state_.rsrq_db         = INT32_MIN;
  state_.sinr_db         = INT32_MIN;
}

// ─── poll tick ──────────────────────────────────────────────────────────

void LteModemNode::onPollTick() {
  if (stub_mode_) {
    publishStub();
    return;
  }

  // Helper to grep a single line from a response set by tag.
  auto findTagged = [](const std::vector<std::string>& lines,
                        const std::string& tag) -> std::string {
    for (const auto& l : lines) {
      if (l.find(tag) != std::string::npos) return l;
    }
    return std::string();
  };

  try {
    // Registration
    for (const auto& l : at_->send("AT+CREG?", 300ms)) {
      if (auto v = parseCreg(l)) {
        state_.registered = static_cast<uint8_t>(*v);
      }
    }

    // Operator
    for (const auto& l : at_->send("AT+COPS?", 300ms)) {
      if (auto v = parseCops(l)) {
        state_.operator_name = *v;
      }
    }

    // Signal — Quectel QCSQ preferred (richer); fall back to CESQ
    bool got_signal = false;
    for (const auto& l : at_->send("AT+QCSQ", 300ms)) {
      if (auto r = parseQcsq(l)) {
        state_.rat       = r->rat;
        state_.rsrp_dbm  = r->rsrp_dbm;
        state_.sinr_db   = r->sinr_db;
        state_.rsrq_db   = r->rsrq_db;
        got_signal = true;
        break;
      }
    }
    if (!got_signal) {
      for (const auto& l : at_->send("AT+CESQ", 300ms)) {
        if (auto r = parseCesq(l)) {
          state_.rsrp_dbm = r->rsrp_dbm;
          state_.rsrq_db  = r->rsrq_db;
          break;
        }
      }
    }

    // IP / PDP
    state_.pdp_active = false;
    state_.ip_address = "0.0.0.0";
    for (const auto& l : at_->send("AT+CGPADDR=1", 300ms)) {
      if (auto ip = parseCgpaddr(l)) {
        state_.ip_address = *ip;
        state_.pdp_active = true;
      }
    }
  } catch (const std::exception& e) {
    ++dropped_count_;
    RCLCPP_ERROR(get_logger(),
        "AT poll error: %s — switching to STUB mode", e.what());
    stub_mode_ = true;
    return;
  }

  state_.dropped_at_count = dropped_count_;
  publishCurrent();
}

// ─── publish ────────────────────────────────────────────────────────────

void LteModemNode::publishCurrent() {
  state_.header.stamp = now();
  state_.last_poll_timestamp_ms =
      static_cast<uint64_t>(now().nanoseconds() / 1'000'000);
  ++seq_;
  status_pub_->publish(state_);
}

void LteModemNode::publishStub() {
  StatusMsg s;
  s.header.stamp    = now();
  s.header.frame_id = "lte";
  s.registered      = StatusMsg::REG_HOME;
  s.operator_name   = "STUB-LTE";
  s.rat             = "LTE";
  s.rsrp_dbm        = -95;
  s.rsrq_db         = -12;
  s.sinr_db         = 5;
  s.pdp_active      = true;
  s.ip_address      = "10.64.0.42";
  s.apn             = "lte.stub";
  s.dropped_at_count = dropped_count_;
  s.last_poll_timestamp_ms =
      static_cast<uint64_t>(now().nanoseconds() / 1'000'000);
  ++seq_;
  status_pub_->publish(s);
}

}  // namespace san_lte_redundancy
