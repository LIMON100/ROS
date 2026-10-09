// 호스트(combatrobot1, jazzy)에서 차량 nav2의 mission_control_node(humble)로
// /swarm/path_command (SwarmPathCommand) 를 발행해 주행을 테스트하는 도구.
// 태블릿→robot_server 가 발행하는 것과 동일한 토픽/메시지를 흉내낸다.
//
// 사용 예:
//   ros2 run swarm_path_test send_path                       # 기본 미션(LOAD_PATH→START)
//   ros2 run swarm_path_test send_path --ros-args \
//        -p cmd:=mission -p waypoints:="36.61015,127.28769;36.61011,127.28700"
//   ros2 run swarm_path_test send_path --ros-args -p cmd:=stop
//   ros2 run swarm_path_test send_path --ros-args -p cmd:=pause
//   ros2 run swarm_path_test send_path --ros-args -p cmd:=resume
//
// cmd: mission | load | start | stop | pause | resume

#include <chrono>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

#include "rclcpp/rclcpp.hpp"
#include "combat_robot_msgs/msg/swarm_path_command.hpp"

using SwarmPathCommand = combat_robot_msgs::msg::SwarmPathCommand;
using namespace std::chrono_literals;

namespace {

// "lat,lon;lat,lon;..." -> [(lat,lon), ...]
std::vector<std::pair<double, double>> parseWaypoints(const std::string &s)
{
  std::vector<std::pair<double, double>> out;
  std::stringstream ss(s);
  std::string item;
  while (std::getline(ss, item, ';')) {
    if (item.find_first_not_of(" \t\r\n") == std::string::npos) continue;
    const auto comma = item.find(',');
    if (comma == std::string::npos) continue;
    try {
      double lat = std::stod(item.substr(0, comma));
      double lon = std::stod(item.substr(comma + 1));
      out.emplace_back(lat, lon);
    } catch (const std::exception &) {
      // skip malformed token
    }
  }
  return out;
}

// mission_control 의 parsePathJson 가 받는 형식: {"waypoints":[{"lat":..,"lon":..}, ...]}
std::string buildPathJson(const std::vector<std::pair<double, double>> &wps)
{
  std::ostringstream os;
  os.setf(std::ios::fixed);
  os.precision(9);
  os << "{\"waypoints\":[";
  for (std::size_t i = 0; i < wps.size(); ++i) {
    if (i) os << ',';
    os << "{\"lat\":" << wps[i].first << ",\"lon\":" << wps[i].second << "}";
  }
  os << "]}";
  return os.str();
}

}  // namespace

int main(int argc, char **argv)
{
  rclcpp::init(argc, argv);
  auto node = rclcpp::Node::make_shared("swarm_path_test_sender");

  const std::string cmd = node->declare_parameter<std::string>("cmd", "mission");
  const std::string wp_str = node->declare_parameter<std::string>(
      "waypoints",
      "36.610149526,127.28768799;"
      "36.610115470,127.286999417;"
      "36.610110535,127.286555705");
  const int gap_ms = node->declare_parameter<int>("load_start_gap_ms", 1000);
  const double discovery_timeout_s =
      node->declare_parameter<double>("discovery_timeout_s", 5.0);
  const int linger_ms = node->declare_parameter<int>("linger_ms", 1500);

  auto pub = node->create_publisher<SwarmPathCommand>("/swarm/path_command", 10);

  // 차량 mission_control(또는 호스트 operation_system)이 구독 매칭될 때까지 대기.
  const auto t0 = node->now();
  RCLCPP_INFO(node->get_logger(), "/swarm/path_command 구독자 매칭 대기...");
  while (rclcpp::ok() && pub->get_subscription_count() == 0 &&
         (node->now() - t0).seconds() < discovery_timeout_s) {
    rclcpp::sleep_for(100ms);
  }
  const auto subs = pub->get_subscription_count();
  if (subs == 0) {
    RCLCPP_WARN(node->get_logger(),
                "구독자 0 — 그래도 발행 진행(차량 nav2/DDS 연결 확인 필요)");
  } else {
    RCLCPP_INFO(node->get_logger(), "구독자 %zu개 매칭됨", subs);
  }

  auto make = [&](uint8_t command, const std::string &json, uint16_t n) {
    SwarmPathCommand m;
    m.header.stamp = node->now();
    m.header.frame_id = "map";
    m.command = command;
    m.num_waypoints = n;
    m.path_json = json;
    return m;
  };

  auto flush = [&](int ms) {
    const auto te = node->now();
    while (rclcpp::ok() && (node->now() - te).seconds() < ms / 1000.0) {
      rclcpp::spin_some(node);
      rclcpp::sleep_for(20ms);
    }
  };

  if (cmd == "mission" || cmd == "load" || cmd == "start") {
    const auto wps = parseWaypoints(wp_str);
    if ((cmd != "start") && wps.empty()) {
      RCLCPP_ERROR(node->get_logger(), "waypoints 파싱 실패: '%s'", wp_str.c_str());
      rclcpp::shutdown();
      return 1;
    }

    if (cmd == "mission" || cmd == "load") {
      const std::string json = buildPathJson(wps);
      RCLCPP_INFO(node->get_logger(), "LOAD_PATH 발행: %zu wps json=%s",
                  wps.size(), json.c_str());
      pub->publish(make(SwarmPathCommand::CMD_LOAD_PATH, json,
                        static_cast<uint16_t>(wps.size())));
      flush(gap_ms);
    }
    if (cmd == "mission" || cmd == "start") {
      RCLCPP_INFO(node->get_logger(), "START 발행");
      pub->publish(make(SwarmPathCommand::CMD_START, "", 0));
    }
  } else if (cmd == "stop") {
    RCLCPP_INFO(node->get_logger(), "STOP 발행");
    pub->publish(make(SwarmPathCommand::CMD_STOP, "", 0));
  } else if (cmd == "pause") {
    RCLCPP_INFO(node->get_logger(), "PAUSE 발행");
    pub->publish(make(SwarmPathCommand::CMD_PAUSE, "", 0));
  } else if (cmd == "resume") {
    RCLCPP_INFO(node->get_logger(), "RESUME 발행");
    pub->publish(make(SwarmPathCommand::CMD_RESUME, "", 0));
  } else {
    RCLCPP_ERROR(node->get_logger(),
                 "알 수 없는 cmd='%s' (mission|load|start|stop|pause|resume)",
                 cmd.c_str());
    rclcpp::shutdown();
    return 1;
  }

  // 메시지 전달 보장을 위해 잠시 유지 후 종료.
  flush(linger_ms);
  RCLCPP_INFO(node->get_logger(), "완료, 종료");
  rclcpp::shutdown();
  return 0;
}
