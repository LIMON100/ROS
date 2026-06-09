// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 Phase 2-E Turn 4 — NtripClientNode implementation.

#include "san_ntrip_client/ntrip_client_node.hpp"

#include <chrono>
#include <iostream>
#include <sstream>
#include <stdexcept>

#include "san_ntrip_client/rtcm3_frame_parser.hpp"

namespace san_ntrip_client
{

using namespace std::chrono_literals;

// ─── Base64 encode (for HTTP Basic auth) ────────────────────────────────

namespace
{
const char kB64[] =
  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string b64(const std::string & in)
{
  std::string out;
  int val = 0, valb = -6;
  for (unsigned char c : in) {
    val = (val << 8) | c;
    valb += 8;
    while (valb >= 0) {
      out.push_back(kB64[(val >> valb) & 0x3F]);
      valb -= 6;
    }
  }
  if (valb > -6) {out.push_back(kB64[((val << 8) >> (valb + 8)) & 0x3F]);}
  while (out.size() % 4 != 0) {out.push_back('=');}
  return out;
}

// ─── Stub TCP socket — used when network not available ──────────────────
class StubTcpSocket : public TcpSocketInterface
{
public:
  bool connect(const std::string & host, int port) override
  {
    std::cerr << "[san_ntrip_client][STUB-TCP] connect(\"" << host
              << "\", " << port << ") — no real backend linked.\n";
    return false;
  }
  void close() override {}
  int send(const std::vector<uint8_t> &) override {return -1;}
  std::vector<uint8_t> recv(size_t, int) override {return {};}
  bool isConnected() const override {return false;}
};
}  // namespace

std::unique_ptr<TcpSocketInterface> makeRealTcpSocket()
{
  return std::make_unique<StubTcpSocket>();
}

// ─── NtripClientNode ────────────────────────────────────────────────────

NtripClientNode::NtripClientNode(const rclcpp::NodeOptions & opts)
: NtripClientNode(opts, makeRealTcpSocket()) {}

NtripClientNode::NtripClientNode(
  const rclcpp::NodeOptions & opts,
  std::unique_ptr<TcpSocketInterface> sock)
: rclcpp::Node("ntrip_client_node", opts), sock_(std::move(sock))
{
  if (!sock_) {
    throw std::runtime_error("NtripClientNode: null socket injected");
  }
  declareParameters();
  loadParameters();

  rtcm_pub_ = create_publisher<std_msgs::msg::UInt8MultiArray>(
    "~/rtcm_corrections", rclcpp::QoS(10).reliable());

  gga_sub_ = create_subscription<std_msgs::msg::String>(
    "~/gga_latest",
    rclcpp::QoS(1).reliable().transient_local(),
    std::bind(&NtripClientNode::onGga, this, std::placeholders::_1));

  running_ = true;
  worker_ = std::thread(&NtripClientNode::workerLoop, this);
  health_timer_ = create_wall_timer(
    1s, std::bind(&NtripClientNode::onHealthTick, this));

  RCLCPP_INFO(
    get_logger(),
    "NtripClientNode UP: host=%s:%d mount=%s",
    host_.c_str(), port_, mount_.c_str());
}

NtripClientNode::~NtripClientNode()
{
  running_ = false;
  if (worker_.joinable()) {worker_.join();}
  if (sock_) {sock_->close();}
}

void NtripClientNode::declareParameters()
{
  declare_parameter<std::string>("host", "rts2.ngii.go.kr");
  declare_parameter<int>("port", 2101);
  declare_parameter<std::string>("mount", "VRS-RTCM31");
  declare_parameter<std::string>("user", "");
  declare_parameter<std::string>("password", "");
  declare_parameter<std::string>("user_agent", "NTRIP SkyHunter/1.5");
  declare_parameter<bool>("stub_on_no_network", true);
}

void NtripClientNode::loadParameters()
{
  host_ = get_parameter("host").as_string();
  port_ = static_cast<int>(get_parameter("port").as_int());
  mount_ = get_parameter("mount").as_string();
  user_ = get_parameter("user").as_string();
  password_ = get_parameter("password").as_string();
  user_agent_ = get_parameter("user_agent").as_string();
  stub_on_no_network_ = get_parameter("stub_on_no_network").as_bool();
}

// ─── HTTP request builder ───────────────────────────────────────────────

std::string NtripClientNode::buildHttpRequest() const
{
  std::ostringstream os;
  os << "GET /" << mount_ << " HTTP/1.0\r\n"
     << "User-Agent: " << user_agent_ << "\r\n"
     << "Host: " << host_ << ":" << port_ << "\r\n"
     << "Accept: */*\r\n";
  if (!user_.empty()) {
    os << "Authorization: Basic " << b64(user_ + ":" + password_) << "\r\n";
  }
  os << "Connection: close\r\n\r\n";
  return os.str();
}

// ─── connect+auth ───────────────────────────────────────────────────────

bool NtripClientNode::connectAndAuthenticate()
{
  if (!sock_->connect(host_, port_)) {
    return false;
  }
  const auto req = buildHttpRequest();
  std::vector<uint8_t> bytes(req.begin(), req.end());
  if (sock_->send(bytes) < 0) {
    sock_->close();
    return false;
  }
  // Read HTTP response (up to first \r\n\r\n)
  std::string header;
  for (int i = 0; i < 20; ++i) {
    auto chunk = sock_->recv(1024, 200);
    if (chunk.empty()) {break;}
    header.append(chunk.begin(), chunk.end());
    if (header.find("\r\n\r\n") != std::string::npos) {break;}
  }
  if (header.find("ICY 200 OK") == std::string::npos &&
    header.find("HTTP/1.0 200") == std::string::npos &&
    header.find("HTTP/1.1 200") == std::string::npos)
  {
    RCLCPP_ERROR(
      get_logger(),
      "NTRIP caster rejected: %s",
      header.substr(0, 200).c_str());
    sock_->close();
    return false;
  }
  return true;
}

// ─── worker loop ────────────────────────────────────────────────────────

void NtripClientNode::workerLoop()
{
  std::vector<uint8_t> buf;
  buf.reserve(8192);
  while (running_) {
    if (!sock_->isConnected()) {
      if (!connectAndAuthenticate()) {
        std::this_thread::sleep_for(2s);
        continue;
      }
      connected_ = true;
    }

    // Periodically uplink GGA for VRS positioning
    {
      std::lock_guard<std::mutex> g(gga_mutex_);
      if (!latest_gga_.empty()) {
        std::vector<uint8_t> gga(latest_gga_.begin(), latest_gga_.end());
        gga.push_back('\r'); gga.push_back('\n');
        sock_->send(gga);
        ++gga_uplink_count_;
        latest_gga_.clear();
      }
    }

    auto chunk = sock_->recv(4096, 200);
    if (chunk.empty()) {continue;}
    buf.insert(buf.end(), chunk.begin(), chunk.end());

    // Try to extract complete RTCM3 frames
    while (true) {
      size_t consumed = 0;
      auto frame = parseRtcm3(buf, &consumed);
      if (!frame) {
        if (consumed > 0) {buf.erase(buf.begin(), buf.begin() + consumed);}
        break;
      }
      buf.erase(buf.begin(), buf.begin() + consumed);
      std_msgs::msg::UInt8MultiArray msg;
      msg.data = std::move(frame->bytes);
      rtcm_pub_->publish(msg);
      ++rtcm_emit_count_;
    }
  }
}

void NtripClientNode::onGga(const std_msgs::msg::String::SharedPtr msg)
{
  std::lock_guard<std::mutex> g(gga_mutex_);
  latest_gga_ = msg->data;
}

void NtripClientNode::onHealthTick()
{
  RCLCPP_INFO(
    get_logger(),
    "ntrip connected=%d rtcm_emit=%u gga_up=%u",
    static_cast<int>(connected_.load()),
    rtcm_emit_count_.load(), gga_uplink_count_.load());
}

}  // namespace san_ntrip_client
