// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SkyHunter v1.5.3 — DCN-2026-014 D-055.
//
// KPP-1 round-trip latency probe. Replaces the earlier shell-script
// proposal with a rclcpp executable so the measurement runs inside the
// same RMW + QoS regime production uses.
//
// Message
// -------
//   std_msgs::msg::Header — `stamp` carries the send-time (set by ping
//   on publish); `frame_id` is the sequence number formatted as decimal
//   string. Both fields are preserved verbatim by the pong side so the
//   ping side can match echoes 1:1 even under burst loss.
//
// Modes
// -----
//   mode:=pong   Stateless echo: subscribes topic_out, republishes the
//                identical Header on topic_in. Run on the peer robot
//                under test.
//   mode:=ping   Probe: publishes Header at rate_hz on topic_out for
//                duration_sec seconds, records RTT from each echo on
//                topic_in. Writes CSV and exits.
//
// CSV layout
// ----------
//   epoch_us,rtt_us,rmw_implementation
//   ... per-sample rows ...
//   # summary count=N min=... max=... mean=... p50=... p95=... p99=...
//
/* Usage
 * -----
 *   # On peer:
 *   ros2 run san_rtk_gnss measure_kpp1_latency_node --ros-args -p mode:=pong
 *
 *   # On host (one line; wrapped here for readability):
 *   ros2 run san_rtk_gnss measure_kpp1_latency_node --ros-args \
 *        -p mode:=ping -p duration_sec:=60 -p output_csv:=/tmp/kpp1.csv
 */

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "rclcpp/rclcpp.hpp"
#include "rmw/get_topic_endpoint_info.h"
#include "rmw/rmw.h"
#include "std_msgs/msg/header.hpp"

#include "san_rtk_gnss/kpp1_latency.hpp"

namespace san_rtk_gnss
{

namespace
{

const char * rmwImplName()
{
  // Source of truth = rmw_get_implementation_identifier() (returns the
  // actual RMW the process bound to, regardless of env). Falls back to
  // the env var only if rmw refuses to report (shouldn't happen post-
  // rclcpp::init), then to a literal placeholder so the CSV row never
  // breaks parsing on a NULL.
  const char * id = rmw_get_implementation_identifier();
  if (id && *id) {return id;}
  const char * env = std::getenv("RMW_IMPLEMENTATION");
  return env ? env : "(unknown)";
}

}  // namespace

class PongNode : public rclcpp::Node
{
public:
  PongNode(const std::string & topic_in, const std::string & topic_out)
  : Node("measure_kpp1_latency_pong")
  {
    // Match production telemetry QoS (best-effort, KEEP_LAST 50).
    const auto qos = rclcpp::QoS(rclcpp::KeepLast(50)).best_effort();
    pub_ = create_publisher<std_msgs::msg::Header>(topic_out, qos);
    sub_ = create_subscription<std_msgs::msg::Header>(
      topic_in, qos,
      [this](std_msgs::msg::Header::SharedPtr msg) {
        pub_->publish(*msg);
      });
    RCLCPP_INFO(
      get_logger(),
      "[kpp1-pong] %s -> %s", topic_in.c_str(), topic_out.c_str());
  }

private:
  rclcpp::Publisher<std_msgs::msg::Header>::SharedPtr pub_;
  rclcpp::Subscription<std_msgs::msg::Header>::SharedPtr sub_;
};

class PingNode : public rclcpp::Node
{
public:
  PingNode(
    const std::string & topic_out, const std::string & topic_in,
    int duration_sec, double rate_hz, std::string output_csv)
  : Node("measure_kpp1_latency_ping"),
    output_csv_(std::move(output_csv))
  {
    samples_.reserve(static_cast<size_t>(duration_sec * rate_hz));

    const auto qos = rclcpp::QoS(rclcpp::KeepLast(50)).best_effort();
    pub_ = create_publisher<std_msgs::msg::Header>(topic_out, qos);
    sub_ = create_subscription<std_msgs::msg::Header>(
      topic_in, qos,
      [this](std_msgs::msg::Header::SharedPtr msg) {
        onEcho(*msg);
      });

    const auto period_ns =
      std::chrono::nanoseconds(static_cast<int64_t>(1e9 / rate_hz));
    publish_timer_ =
      create_wall_timer(period_ns, [this]() {onTick();});

    end_time_ = now() + rclcpp::Duration::from_seconds(duration_sec);
    deadline_timer_ =
      create_wall_timer(
      std::chrono::milliseconds(250),
      [this]() {onDeadline();});

    RCLCPP_INFO(
      get_logger(),
      "[kpp1-ping] %s -> %s | %ds @ %.1fHz | rmw=%s | csv=%s",
      topic_out.c_str(), topic_in.c_str(),
      duration_sec, rate_hz, rmwImplName(),
      output_csv_.c_str());
  }

private:
  void onTick()
  {
    if (finalised_) {
      return;
    }
    std_msgs::msg::Header h;
    h.stamp = now();
    h.frame_id = std::to_string(seq_++);
    pub_->publish(h);
  }

  void onEcho(const std_msgs::msg::Header & msg)
  {
    if (finalised_) {
      return;
    }
    const auto recv = now();
    const rclcpp::Time send_t(msg.stamp);
    const double rtt_us =
      static_cast<double>((recv - send_t).nanoseconds()) / 1000.0;
    if (rtt_us < 0.0) {
      // Clock domain mismatch — skip and warn (steady-clock vs
      // wall-clock between hosts).
      RCLCPP_WARN(
        get_logger(),
        "[kpp1-ping] negative RTT (%.2f us); skipped", rtt_us);
      return;
    }
    const int64_t epoch_us =
      static_cast<int64_t>(recv.nanoseconds() / 1000);
    raw_rows_.push_back({epoch_us, rtt_us});
    samples_.push_back(rtt_us);
  }

  void onDeadline()
  {
    if (now() < end_time_ || finalised_) {
      return;
    }
    finalise();
  }

  void finalise()
  {
    finalised_ = true;
    writeCsv();
    RCLCPP_INFO(
      get_logger(),
      "[kpp1-ping] done; samples=%zu csv=%s",
      samples_.size(), output_csv_.c_str());
    rclcpp::shutdown();
  }

  void writeCsv()
  {
    std::ofstream out(output_csv_);
    if (!out) {
      RCLCPP_ERROR(
        get_logger(),
        "[kpp1-ping] failed to open %s for write",
        output_csv_.c_str());
      return;
    }
    const std::string rmw = rmwImplName();
    out << "epoch_us,rtt_us,rmw_implementation\n";
    for (const auto & r : raw_rows_) {
      out << r.epoch_us << ',' << r.rtt_us << ',' << rmw << '\n';
    }
    try {
      const auto s = kpp1::summarise(samples_);
      out << "# summary count=" << s.count
          << " min=" << s.min_us
          << " max=" << s.max_us
          << " mean=" << s.mean_us
          << " p50=" << s.p50_us
          << " p95=" << s.p95_us
          << " p99=" << s.p99_us
          << " rmw=" << rmw
          << '\n';
    } catch (const std::exception & e) {
      out << "# summary unavailable: " << e.what() << '\n';
    }
  }

  struct Row
  {
    int64_t epoch_us;
    double rtt_us;
  };

  rclcpp::Publisher<std_msgs::msg::Header>::SharedPtr pub_;
  rclcpp::Subscription<std_msgs::msg::Header>::SharedPtr sub_;
  rclcpp::TimerBase::SharedPtr publish_timer_;
  rclcpp::TimerBase::SharedPtr deadline_timer_;
  rclcpp::Time end_time_;
  std::string output_csv_;
  uint64_t seq_ = 0;
  bool finalised_ = false;
  std::vector<double> samples_;
  std::vector<Row> raw_rows_;
};

}  // namespace san_rtk_gnss

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);

  auto cfg = std::make_shared<rclcpp::Node>("measure_kpp1_latency_config");
  cfg->declare_parameter<std::string>("mode", "ping");
  cfg->declare_parameter<std::string>("topic_out", "/kpp1/probe");
  cfg->declare_parameter<std::string>("topic_in", "/kpp1/echo");
  cfg->declare_parameter<int>("duration_sec", 60);
  // rate_hz accepts either DOUBLE or INTEGER from the CLI. We declare
  // with a PARAMETER_NOT_SET default so the param can be overridden
  // with either type, then coerce internally — sidesteps the rclcpp
  // pet peeve where `-p rate_hz:=100` (int literal) aborts a node
  // declared as double. See post-review HIGH #10.
  cfg->declare_parameter(
    "rate_hz",
    rclcpp::ParameterValue(100.0),
    rcl_interfaces::msg::ParameterDescriptor{},
    /*ignore_override=*/ false);
  cfg->declare_parameter<std::string>(
    "output_csv",
    "kpp1_baseline_easymesh.csv");

  const auto mode = cfg->get_parameter("mode").as_string();
  const auto topic_out = cfg->get_parameter("topic_out").as_string();
  const auto topic_in = cfg->get_parameter("topic_in").as_string();
  const auto duration_sec = cfg->get_parameter("duration_sec").as_int();
  // Robust rate_hz read: accept DOUBLE or INTEGER (ros2 CLI parses
  // bare numerics as int by default).
  double rate_hz = 100.0;
  {
    const auto p = cfg->get_parameter("rate_hz");
    if (p.get_type() == rclcpp::ParameterType::PARAMETER_DOUBLE) {
      rate_hz = p.as_double();
    } else if (p.get_type() == rclcpp::ParameterType::PARAMETER_INTEGER) {
      rate_hz = static_cast<double>(p.as_int());
    } else {
      RCLCPP_WARN(
        cfg->get_logger(),
        "rate_hz has unexpected type; using default 100.0");
    }
  }
  const auto output_csv = cfg->get_parameter("output_csv").as_string();
  cfg.reset();

  rclcpp::Node::SharedPtr node;
  if (mode == "pong") {
    node = std::make_shared<san_rtk_gnss::PongNode>(topic_out, topic_in);
  } else if (mode == "ping") {
    node = std::make_shared<san_rtk_gnss::PingNode>(
      topic_out, topic_in, static_cast<int>(duration_sec),
      rate_hz, output_csv);
  } else {
    RCLCPP_FATAL(
      rclcpp::get_logger("measure_kpp1_latency"),
      "mode must be 'ping' or 'pong'; got '%s'",
      mode.c_str());
    rclcpp::shutdown();
    return 2;
  }

  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
