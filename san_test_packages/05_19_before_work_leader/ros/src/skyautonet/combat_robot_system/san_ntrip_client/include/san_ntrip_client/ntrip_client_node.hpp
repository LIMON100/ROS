// SAN v1.5 Phase 2-E Turn 4 — NtripClientNode.
//
// Connects to an NTRIP caster (TCP + HTTP GET + RTCM3 stream),
// publishes RTCM frames on ~/rtcm_corrections, and forwards GGA
// strings (received from RtkGnssNode on ~/gga_latest) back to the
// caster (VRS positioning).

#ifndef SAN_NTRIP_CLIENT__NTRIP_CLIENT_NODE_HPP_
#define SAN_NTRIP_CLIENT__NTRIP_CLIENT_NODE_HPP_

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/string.hpp>
#include <std_msgs/msg/u_int8_multi_array.hpp>

namespace san_ntrip_client {

/// Minimal TCP socket abstraction so tests don't need a real network.
class TcpSocketInterface {
public:
  virtual ~TcpSocketInterface() = default;
  virtual bool connect(const std::string& host, int port) = 0;
  virtual void close() = 0;
  virtual int  send(const std::vector<uint8_t>& bytes) = 0;
  virtual std::vector<uint8_t> recv(size_t max_bytes, int timeout_ms) = 0;
  virtual bool isConnected() const = 0;
};
std::unique_ptr<TcpSocketInterface> makeRealTcpSocket();

class NtripClientNode : public rclcpp::Node {
public:
  explicit NtripClientNode(
      const rclcpp::NodeOptions& opts = rclcpp::NodeOptions());
  NtripClientNode(
      const rclcpp::NodeOptions& opts,
      std::unique_ptr<TcpSocketInterface> sock);
  ~NtripClientNode() override;

private:
  void declareParameters();
  void loadParameters();
  void workerLoop();
  bool connectAndAuthenticate();
  void onGga(const std_msgs::msg::String::SharedPtr msg);
  void onHealthTick();
  std::string buildHttpRequest() const;

  // Members
  std::unique_ptr<TcpSocketInterface> sock_;
  rclcpp::Publisher<std_msgs::msg::UInt8MultiArray>::SharedPtr rtcm_pub_;
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr       gga_sub_;
  rclcpp::TimerBase::SharedPtr                                  health_timer_;

  // Params
  std::string host_;
  int         port_;
  std::string mount_;
  std::string user_;
  std::string password_;
  std::string user_agent_;
  bool        stub_on_no_network_;

  // Worker
  std::thread        worker_;
  std::atomic<bool>  running_{false};

  // Latest GGA forwarded upstream
  std::mutex         gga_mutex_;
  std::string        latest_gga_;

  // Stats
  std::atomic<uint32_t> rtcm_emit_count_{0};
  std::atomic<uint32_t> gga_uplink_count_{0};
  std::atomic<bool>     connected_{false};
};

}  // namespace san_ntrip_client

#endif  // SAN_NTRIP_CLIENT__NTRIP_CLIENT_NODE_HPP_
