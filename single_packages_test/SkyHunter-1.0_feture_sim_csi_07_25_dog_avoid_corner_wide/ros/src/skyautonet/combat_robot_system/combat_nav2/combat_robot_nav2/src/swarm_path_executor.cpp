#include <memory>
#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "rclcpp/rclcpp.hpp"
#include "rclcpp_action/rclcpp_action.hpp"
#include "nav2_msgs/action/navigate_to_pose.hpp"
#include "nav2_msgs/action/follow_path.hpp"
#include "nav2_msgs/srv/is_path_valid.hpp"
#include "nav_msgs/msg/path.hpp"
#include "visualization_msgs/msg/marker_array.hpp"

#include "robot_localization/srv/from_ll.hpp"
#include "robot_localization/srv/to_ll.hpp"
#include "geographic_msgs/msg/geo_point.hpp"

#include "combat_robot_msgs/msg/waypoint.hpp"
#include "combat_robot_msgs/msg/waypoint_list.hpp"
#include "combat_robot_msgs/msg/operation_state.hpp"
#include "combat_robot_msgs/msg/swarm_path_command.hpp"
#include "combat_robot_msgs/msg/swarm_control_command.hpp"
#include "nav2_msgs/msg/speed_limit.hpp"
#include "geometry_msgs/msg/point_stamped.hpp"

#include "sensor_msgs/msg/nav_sat_fix.hpp"
#include "sensor_msgs/msg/point_cloud2.hpp"
#include "sensor_msgs/point_cloud2_iterator.hpp"
#include "std_msgs/msg/float64.hpp"
#include "nav_msgs/msg/odometry.hpp"
#include "geometry_msgs/msg/pose_stamped.hpp"
#include "geometry_msgs/msg/twist.hpp"
#include "std_msgs/msg/bool.hpp"
#include "std_msgs/msg/u_int8.hpp"
#include "tf2/utils.h"
#include "tf2_geometry_msgs/tf2_geometry_msgs.hpp"

#include "combat_robot_nav2/swarm_convoy_geometry.hpp"
#include "combat_robot_nav2/early_column.hpp"

using std::placeholders::_1;
using std::placeholders::_2;
using namespace std::chrono_literals;

namespace
{
// SwarmPathCommand command 값
constexpr uint8_t CMD_START = 1;
constexpr uint8_t CMD_STOP = 2;
constexpr uint8_t CMD_PAUSE = 3;
constexpr uint8_t CMD_RESUME = 4;
constexpr uint8_t CMD_LOAD_PATH = 5;

constexpr uint8_t MISSION_ERROR_NONE = 0;
constexpr uint8_t MISSION_ERROR_INVALID_PATH_PAYLOAD = 1;
constexpr uint8_t MISSION_ERROR_PATH_NOT_LOADED = 3;
constexpr uint8_t MISSION_ERROR_INVALID_PATH_COMMAND = 4;

// 대형 종류. tablet/SwarmControlCommand.formation_type 매핑(필요 시 조정).
// 대형 추가 = formationSlot() 에 case 한 줄 추가; 나머지 기계는 전부 공통.
constexpr uint8_t FORMATION_LINE_ABREAST = 0;   // 나란히 (옆으로)
constexpr uint8_t FORMATION_COLUMN       = 1;   // 일렬 (뒤로)
constexpr uint8_t FORMATION_WEDGE        = 2;   // 쐐기/V (옆+뒤 대각)
constexpr uint8_t FORMATION_DIAMOND      = 3;   // 다이아몬드 (마름모/측면호위)
constexpr uint8_t FORMATION_ARC          = 4;   // 160° 돌격 원호 (POC)

// SwarmControlCommand.formation_type 의 '진짜' 의미는 작전 모드(메시지 상수와 일치):
//   RECON=1 / PROTECT=2 / ASSAULT=3. 모드당 대형 1개(formation_number 추후 1~4 확장).
//   모드 → 기하학 대형 매핑이 단일 진입점. (앱·FSM 은 모드만 보내면 대형이 따라옴)
constexpr uint8_t MODE_NONE    = 0;
constexpr uint8_t MODE_RECON   = 1;   // → COLUMN  (일렬, 리더 궤적 추종)
constexpr uint8_t MODE_PROTECT = 2;   // → DIAMOND (측면 호위)
constexpr uint8_t MODE_ASSAULT = 3;   // → WEDGE   (쐐기 돌격)
constexpr uint8_t MODE_ARC     = 4;   // → ARC     (돌격 원호, POC 시나리오)

inline uint8_t geometryForMode(uint8_t mode, uint8_t /*number*/)
{
  switch (mode) {
    case MODE_RECON:   return FORMATION_COLUMN;
    case MODE_PROTECT: return FORMATION_DIAMOND;
    case MODE_ASSAULT: return FORMATION_WEDGE;
    case MODE_ARC:     return FORMATION_ARC;
    default:           return FORMATION_LINE_ABREAST;  // NONE → 나란히(스폰과 일치, 안전 출발)
  }
}

// 대형 슬롯: 경로 진행방향 프레임에서의 2D 오프셋(단위=spacing).
//   cross : 횡(+ = 진행방향 좌측),  along : 종(+ = 진행방향 전방, 보통 팔로워는 음수=뒤)
// cross → 경로 횡 평행이동, along → 속도동기화의 arc-length 목표 오프셋.
struct FormationSlot { double cross = 0.0; double along = 0.0; };

// rank(=robot_id-leader_id, 1..) → 좌우 교대 횡 인덱스: 1→+1 2→−1 3→+2 4→−2 …
inline int alternatingLateral(int rank)
{
  return (rank % 2 == 1) ? (rank + 1) / 2 : -(rank / 2);
}

// 대형종류 + rank → 2D 슬롯. 여기만 대형별로 다르고, 호출하는 기계는 전부 공통.
inline FormationSlot formationSlotFor(uint8_t type, int rank)
{
  if (rank <= 0) return {0.0, 0.0};            // 리더(자기) = 기준
  switch (type) {
    case FORMATION_COLUMN:
      // 일렬(뒤로만). 1.5×rank: spacing 2m 기준 rank1→−3m(리더 inflation 2m 밖→mask=false
      // planner 가 우회 접근, 스침 방지), 차간 3m(forward-gap 안전대 2.7m 위→오감속 없음).
      return {0.0, -1.5 * static_cast<double>(rank)};
    case FORMATION_WEDGE: {
      // 쐐기/화살촉: 옆으로 벌리고 '깊게 뒤로' — 명확한 V. 리더서 멀리(옆 2m+뒤 4m@rank1).
      const double c = static_cast<double>(alternatingLateral(rank));
      return {c, -2.0 * std::abs(c)};
    }
    case FORMATION_DIAMOND: {
      // 마름모/측면호위: 리더 좌우로 '넓게'(±3m) 벌린 호위 라인. 쐐기(뒤로 깊은 V)와 명확히 구별.
      // 1→좌, 2→우(넓은 측면), 3→정후, 4→좌후, 5→우후.
      static const FormationSlot kDiamond[] = {
        {+1.5, 0.0}, {-1.5, 0.0}, {0.0, -2.5}, {+1.5, -2.5}, {-1.5, -2.5}};
      const int i = rank - 1;
      if (i < static_cast<int>(sizeof(kDiamond) / sizeof(kDiamond[0]))) return kDiamond[i];
      // 초과 인원은 쐐기로 확장(뒤로 계속 벌림).
      const double c = static_cast<double>(alternatingLateral(rank));
      return {c, -std::abs(c) - 1.0};
    }
    case FORMATION_ARC: {
      // 160° 돌격 원호(POC) — 명시적 슬롯표(diamond 방식). 안쪽 2대=가깝고 뒤,
      // 바깥 2대=넓고 앞 → 전방으로 열린 곡선. ※팔로워 4대(5-robot)라야 호가 보임.
      static const FormationSlot kArc[] = {
        {+1.0, -1.5},   // rank1 안쪽 좌
        {-1.0, -1.5},   // rank2 안쪽 우
        {+2.5, -0.3},   // rank3 바깥 좌
        {-2.0, -0.5},   // rank4 바깥 우
      };
      const int i = rank - 1;
      if (i < static_cast<int>(sizeof(kArc) / sizeof(kArc[0]))) return kArc[i];
      const double c = static_cast<double>(alternatingLateral(rank));
      return {c, -0.5};
    }
    case FORMATION_LINE_ABREAST:
    default:
      return {static_cast<double>(alternatingLateral(rank)), 0.0};  // 나란히: 옆으로만
  }
}

// ----- 태블릿 path_json 최소 파서 -----
// 지원: {"waypoints":[{"lat":..,"lon":..}, ...]}
//       {"coordinates":[[lon,lat], ...]}                (GeoJSON)
//       [{"lat":..,"lon":..}, ...]
std::size_t findMatchingDelimiter(const std::string &t, std::size_t pos, char o, char c)
{
  if (pos >= t.size() || t[pos] != o) return std::string::npos;
  int d = 0;
  for (std::size_t i = pos; i < t.size(); ++i) {
    if (t[i] == o) ++d;
    else if (t[i] == c) { --d; if (d == 0) return i; }
  }
  return std::string::npos;
}

bool extractJsonNumberField(const std::string &obj, const char *name, double *out)
{
  const std::string key = "\"" + std::string(name) + "\"";
  const std::size_t k = obj.find(key);
  if (k == std::string::npos) return false;
  const std::size_t col = obj.find(':', k + key.size());
  if (col == std::string::npos) return false;
  std::size_t s = col + 1;
  while (s < obj.size() && std::isspace(static_cast<unsigned char>(obj[s]))) ++s;
  if (s >= obj.size()) return false;
  char *end = nullptr;
  *out = std::strtod(obj.c_str() + s, &end);
  return end != obj.c_str() + s;
}

bool extractObjectLatLon(const std::string &obj, double *lat, double *lon)
{
  double la = 0.0, lo = 0.0;
  const bool hl = extractJsonNumberField(obj, "lat", &la) ||
                  extractJsonNumberField(obj, "latitude", &la);
  const bool hn = extractJsonNumberField(obj, "lon", &lo) ||
                  extractJsonNumberField(obj, "lng", &lo) ||
                  extractJsonNumberField(obj, "longitude", &lo);
  if (!hl || !hn) return false;
  *lat = la; *lon = lo;
  return true;
}

bool extractNextNumber(const std::string &t, std::size_t *pos, double *out)
{
  std::size_t s = *pos;
  while (s < t.size() && !std::isdigit(static_cast<unsigned char>(t[s])) && t[s] != '-') ++s;
  if (s >= t.size()) return false;
  char *end = nullptr;
  *out = std::strtod(t.c_str() + s, &end);
  if (end == t.c_str() + s) return false;
  *pos = static_cast<std::size_t>(end - t.c_str());
  return true;
}

enum class PathFormat { OBJECT_ARRAY, GEOJSON_COORDINATES };

bool findArrayBounds(const std::string &p, std::size_t *s, std::size_t *e, PathFormat *f)
{
  std::size_t k = p.find("\"waypoints\"");
  if (k != std::string::npos) {
    *s = p.find('[', k); if (*s == std::string::npos) return false;
    *e = findMatchingDelimiter(p, *s, '[', ']'); if (*e == std::string::npos) return false;
    *f = PathFormat::OBJECT_ARRAY; return true;
  }
  k = p.find("\"coordinates\"");
  if (k != std::string::npos) {
    *s = p.find('[', k); if (*s == std::string::npos) return false;
    *e = findMatchingDelimiter(p, *s, '[', ']'); if (*e == std::string::npos) return false;
    *f = PathFormat::GEOJSON_COORDINATES; return true;
  }
  const std::size_t fn = p.find_first_not_of(" \t\r\n");
  if (fn != std::string::npos && p[fn] == '[') {
    *s = fn;
    *e = findMatchingDelimiter(p, *s, '[', ']'); if (*e == std::string::npos) return false;
    *f = PathFormat::OBJECT_ARRAY; return true;
  }
  return false;
}

std::vector<std::pair<double, double>> parsePathJson(const std::string &payload)
{
  std::vector<std::pair<double, double>> out;
  std::size_t a_s = 0, a_e = 0;
  PathFormat fmt = PathFormat::OBJECT_ARRAY;
  if (!findArrayBounds(payload, &a_s, &a_e, &fmt)) return out;

  std::size_t pos = a_s + 1;
  while (pos < a_e) {
    double lat = 0.0, lon = 0.0;
    bool ok = false;
    if (fmt == PathFormat::OBJECT_ARRAY) {
      const std::size_t os = payload.find('{', pos);
      if (os == std::string::npos || os > a_e) break;
      const std::size_t oe = findMatchingDelimiter(payload, os, '{', '}');
      if (oe == std::string::npos || oe > a_e) break;
      ok = extractObjectLatLon(payload.substr(os, oe - os + 1), &lat, &lon);
      pos = oe + 1;
    } else {
      const std::size_t cs = payload.find('[', pos);
      if (cs == std::string::npos || cs > a_e) break;
      const std::size_t ce = findMatchingDelimiter(payload, cs, '[', ']');
      if (ce == std::string::npos || ce > a_e) break;
      const std::string coord = payload.substr(cs, ce - cs + 1);
      std::size_t inner = 0;
      double parsed_lon = 0.0, parsed_lat = 0.0;
      if (extractNextNumber(coord, &inner, &parsed_lon) &&
          extractNextNumber(coord, &inner, &parsed_lat))
      {
        lat = parsed_lat; lon = parsed_lon; ok = true;
      }
      pos = ce + 1;
    }
    if (ok) out.emplace_back(lat, lon);
  }
  return out;
}

}  // namespace


class SwarmPathExecutorNode : public rclcpp::Node
{
public:
    using NavigateToPose = nav2_msgs::action::NavigateToPose;
    using GoalHandleNav = rclcpp_action::ClientGoalHandle<NavigateToPose>;
    using FollowPath = nav2_msgs::action::FollowPath;
    using GoalHandleFP = rclcpp_action::ClientGoalHandle<FollowPath>;

    SwarmPathExecutorNode() : Node("swarm_path_executor")
    {
        // 명령 입력은 모두 상대 토픽 — push-ros-namespace(/sN) 안에서 실행되면 /sN/... 로,
        // [per-robot 보드 모델] 경로/대형 명령은 FSM 게이트를 거쳐 들어온다:
        //   command_server → /sN/swarm/path_command → FSM(/sN) → /sN/mission/path_command → executor
        // 토픽은 파라미터(상대경로)로, 기본값은 FSM 게이트 출력. FSM 없이 직결 테스트하려면
        // path_command_topic:=swarm/path_command 로 바꾸면 command_server 미러를 직접 구독한다.
        const std::string path_topic =
            this->declare_parameter<std::string>("path_command_topic", "mission/path_command");
        const std::string control_topic =
            this->declare_parameter<std::string>("control_command_topic", "mission/control_command");
        path_cmd_sub_ = this->create_subscription<combat_robot_msgs::msg::SwarmPathCommand>(
            path_topic, 10,
            std::bind(&SwarmPathExecutorNode::path_command_callback, this, _1));

        // 내부/테스트용 직접 입력 (way_test.py 등). 필요 없으면 통째로 제거 가능.
        mission_sub_ = this->create_subscription<combat_robot_msgs::msg::WaypointList>(
            "mission_input", 10,
            std::bind(&SwarmPathExecutorNode::mission_callback, this, _1));

        nav_client_ = rclcpp_action::create_client<NavigateToPose>(this, "navigate_to_pose");
        follow_path_client_ = rclcpp_action::create_client<FollowPath>(this, "follow_path");
        from_ll_client_ = this->create_client<robot_localization::srv::FromLL>("fromLL");
        // 제어 방식: "follow_path"(주어진 오프셋 직선을 planner 없이 직접 추종 → 대칭·연속·무진동)
        //            | "navigate"(과거: waypoint별 NavigateToPose 재계획).
        control_mode_ = this->declare_parameter<std::string>("control_mode", "follow_path");
        // FollowPath 경로 시각화(rviz Path display 가 보는 /sN/plan) + 장애물 로컬 우회.
        // 늦게 뜨는 rviz 도 마지막 경로를 받게 latched(transient_local) + status_timer 가 주기 재발행.
        fp_path_pub_ = this->create_publisher<nav_msgs::msg::Path>(
            "plan", rclcpp::QoS(1).transient_local());
        dbg_marker_pub_ = this->create_publisher<visualization_msgs::msg::MarkerArray>(
            "swarm_debug", rclcpp::QoS(5));
        // 차량 라이트 상태표시(can_reader 가 구독해 CAN light 바이트로 발행):
        //   주행=하이빔(3), form-up/대형전환=하향등 점멸(2↔1), 대기=하향등(2).
        light_pub_ = this->create_publisher<std_msgs::msg::UInt8>("headlight", 10);
        is_path_valid_client_ =
            this->create_client<nav2_msgs::srv::IsPathValid>("is_path_valid");
        detour_skip_m_ = this->declare_parameter<double>("formation.detour_skip_m", 6.0);
        detour_clear_margin_m_ =
            this->declare_parameter<double>("formation.detour_clear_margin_m", 2.0);
        obstacle_lookahead_m_ =
            this->declare_parameter<double>("formation.obstacle_lookahead_m", 8.0);
        detour_trigger_dist_ =
            this->declare_parameter<double>("formation.detour_trigger_dist_m", 8.0);
        // 장애물 '감지' 범위(회피 발동과 별개). avoid_radius(로봇간 감속 6m)와 분리 —
        // E1 라이다는 30m 급이라 12m 선행 감지로 코앞 회피(구 6.5m 감지→5m 트리거) 해소.
        obstacle_detect_range_ =
            this->declare_parameter<double>("formation.obstacle_detect_range_m", 12.0);
        wide_obstacle_span_m_ =
            this->declare_parameter<double>("formation.wide_obstacle_span_m", 3.5);
        lidar_self_range_ =
            this->declare_parameter<double>("formation.lidar_self_range_m", 1.5);
        min_cluster_pts_ =
            this->declare_parameter<int>("formation.min_cluster_pts", 6);
        obstacle_persist_ticks_ =
            this->declare_parameter<int>("formation.obstacle_persist_ticks", 1);
        startup_grace_s_ =
            this->declare_parameter<double>("formation.startup_grace_s", 6.0);
        formup_timeout_s_ =
            this->declare_parameter<double>("formation.formup_timeout_s", 0.0);  // 0=강제출발 비활성(대형완성까지 대기)
        follower_lost_s_ =
            this->declare_parameter<double>("formation.follower_lost_s", 3.0);   // 하트비트 무신호 이 초과=링크다운
        formup_grace_s_ =
            this->declare_parameter<double>("formation.formup_grace_s", 20.0);   // form-up 중 링크다운 대기 유예
        corner_ff_enabled_ =
            this->declare_parameter<bool>("formation.corner_ff_enabled", true);
        // 스워브 중 차체가 비스듬해지며 모서리가 평폭(0.43)보다 더 바깥으로 쓸린다.
        // footprint [0.66,0.43] → 대각반경 hypot=0.79. 그 사이값으로 swept 반폭 잡아 측면 충돌 방지.
        swerve_footprint_half_w_ =
            this->declare_parameter<double>("formation.swerve_footprint_half_w_m", 0.7);
        // 긴급정지: 장애물이 차폭 내 정면·이 거리 이내인데 스워브가 못 비킨 상황 → 정지(밀고
        // 들어감 방지). collision_monitor 대체 안전망(executor 견고 감지라 sim 오탐 없음).
        hard_stop_dist_ =
            this->declare_parameter<double>("formation.hard_stop_dist_m", 1.3);
        // 장애물 감지 z 하한. -0.4 는 sharp 코너서 skid-steer 피치 시 지면(z≈-0.6)이 콘에
        // 새들어와(피치 ~4°@3m) 헛스워브 유발. 0.0 으로 올려 피치된 지면 배제(피치 ~11°@3m
        // 까지 견딤). 실장애물(차량/벽/박스)은 모두 지면 위라 유지. 저높이 턱은 미감지(허용).
        obstacle_z_min_ =
            this->declare_parameter<double>("formation.obstacle_z_min_m", 0.0);
        // footprint 고려: 로봇 반폭(~0.43) + 장애물 반폭 + 여유. box(0.7m)엔 1.5면 충분.
        swerve_clearance_m_ =
            this->declare_parameter<double>("formation.swerve_clearance_m", 1.5);
        swerve_transition_m_ =
            this->declare_parameter<double>("formation.swerve_transition_m", 5.0);
        swerve_hold_past_m_ =
            this->declare_parameter<double>("formation.swerve_hold_past_m", 3.0);
        corner_radius_m_ =
            this->declare_parameter<double>("formation.corner_radius_m", 6.0);
        align_gain_ = this->declare_parameter<double>("formation.align_gain", 0.25);
        corner_influence_m_ =
            this->declare_parameter<double>("formation.corner_influence_m", 10.0);
        swerve_min_speed_frac_ =
            this->declare_parameter<double>("formation.min_speed_frac", 0.35);
        swerve_cooldown_s_ =
            this->declare_parameter<double>("formation.swerve_cooldown_s", 1.5);
        fp_start_time_ = this->now();
        obstacle_check_timer_ = this->create_wall_timer(
            200ms, std::bind(&SwarmPathExecutorNode::obstacle_check_tick, this));
        // body-frame 전방장애물 감지(obstacle_dist_)는 모든 모드(FollowPath 포함)에서 필요 →
        // 무조건 구독. 필터된 클라우드(swarm_lidar_filter 가 팀원 제거)를 써서 코너에서 로봇끼리
        // 서로를 장애물로 오인(=거짓우회 폭발)하는 것 차단. 프레임 무관(필터가 일관 처리).
        lidar_sub_ = this->create_subscription<sensor_msgs::msg::PointCloud2>(
            "rslidar_points_filtered", rclcpp::SensorDataQoS(),
            std::bind(&SwarmPathExecutorNode::on_lidar, this, _1));
        mission_state_pub_ = this->create_publisher<combat_robot_msgs::msg::OperationState>(
            "swarm/mission_state", 10);
        status_timer_ = this->create_wall_timer(
            500ms, std::bind(&SwarmPathExecutorNode::publish_mission_status, this));
        // 라이트 상태 250ms 주기 발행(점멸 = 2Hz).
        light_timer_ = this->create_wall_timer(
            250ms, std::bind(&SwarmPathExecutorNode::light_tick, this));

        // 태블릿 위치표시용: 로봇 GPS 위경도/heading/속도 캐싱 → OperationState 에 채워 발행.
        fix_sub_ = this->create_subscription<sensor_msgs::msg::NavSatFix>(
            "fix", 10,
            [this](sensor_msgs::msg::NavSatFix::SharedPtr m){
                gps_lat_ = m->latitude; gps_lon_ = m->longitude; });
        heading_sub_ = this->create_subscription<std_msgs::msg::Float64>(
            "edge_heading", 10,   // gnss_heading 의 컴퍼스 heading(deg)
            [this](std_msgs::msg::Float64::SharedPtr m){
                gps_heading_ = static_cast<float>(m->data); });
        odom_sub_ = this->create_subscription<nav_msgs::msg::Odometry>(
            "odom", 10,
            [this](nav_msgs::msg::Odometry::SharedPtr m){
                current_speed_ = static_cast<float>(std::hypot(
                    m->twist.twist.linear.x, m->twist.twist.linear.y)); });

        // 편대(formation) 명령 — FSM 게이트 출력 /sN/mission/control_command 구독(기본).
        // 자기 슬롯 오프셋 계산에 사용.
        formation_sub_ = this->create_subscription<combat_robot_msgs::msg::SwarmControlCommand>(
            control_topic, 10,
            [this](combat_robot_msgs::msg::SwarmControlCommand::SharedPtr m){
                if (m->formation_number != formation_number_) {
                    formation_number_ = m->formation_number;
                    assignment_valid_ = false;             // 대형변형 변경 → 최적배정 재계산
                }
                apply_formation_mode(m->formation_type);
            });

        // 정적 robot_id 기반 편대 슬롯. leader_robot_id 와 같으면 기준경로 그대로(오프셋 0).
        robot_id_ = static_cast<uint32_t>(this->declare_parameter<int>("robot_id", 1));
        leader_robot_id_ = static_cast<uint32_t>(this->declare_parameter<int>("leader_robot_id", 1));
        // nav2 global frame for goals. Single-robot = "map"; namespaced = "<ns>/map".
        map_frame_ = this->declare_parameter<std::string>("map_frame", "map");
        formation_lateral_spacing_m_ =
            this->declare_parameter<double>("formation.lateral_spacing_m", 2.0);
        formation_enabled_ = this->declare_parameter<bool>("formation.enable", true);
        // 우회 far-edge 게이팅(비-COLUMN 코히어통과). 리더 궤적이 기준경로에서
        // 이 횡거리(m) 이상 벗어난 마지막 지점을 "우회 far-edge"로 본다.
        detour_lat_thresh_m_ =
            this->declare_parameter<double>("formation.detour_lat_thresh_m", 1.0);
        // 이탈 마진(m): 래치된 far-edge 를 이만큼 더 지나야 대형 복원. 벽 지나자마자 꺾으면
        //   벽 모서리에 부딪힐 수 있어(사용자 지시) 충분히 직선으로 지나친 뒤 복귀한다.
        detour_exit_margin_m_ =
            this->declare_parameter<double>("formation.detour_exit_margin_m", 6.0);
        // 조기종대 발동 거리(m): 리더가 이 거리 안에서 전방 장애물을 보면 스워브 전에
        //   COLUMN 브로드캐스트. 스워브 트리거(detour_trigger_dist_m, 8.0)보다 크게 잡아
        //   개방공간에서 미리 종대화(갭 한복판 reform 방지).
        obstacle_pass_lookahead_m_ =
            this->declare_parameter<double>("formation.obstacle_pass_lookahead_m", 15.0);

        // formation.mode: "static"  = each robot drives a lateral-offset copy of the
        //                             reference path independently (open-loop).
        //                 "dynamic" = follower continuously tracks the leader's LIVE
        //                             pose + lateral offset (closed-loop formation).
        formation_mode_ = this->declare_parameter<std::string>("formation.mode", "static");
        // 기준경로 보간 간격(m). 0 이면 보간 안함. 촘촘할수록 RPP 가 명목 직선에 빨리
        // 복귀 → 출발 스크럽 드리프트가 빨리 사라져 편대가 대칭으로 유지.
        formation_densify_m_ = this->declare_parameter<double>("formation.densify_m", 10.0);
        // 리더-상대 횡잠금(cross-track 보정) on/off. 끄면 순수 오프셋경로 개루프.
        // 기본 OFF: 스키드-스티어에서 goal preempt 재전송이 매번 회전-스크럽을 유발해 발산.
        lateral_lock_enabled_ = this->declare_parameter<bool>("formation.lateral_lock", false);
        lateral_lock_gain_ = this->declare_parameter<double>("formation.lateral_lock_gain", 1.0);
        formation_tol_m_ = this->declare_parameter<double>("formation.ready_tol_m", 1.2);
        // ★ form-up 도착 판정(팔로워가 '실제로 슬롯에 도착' 했는지) — ready_tol 보다 좁게.
        //   넓으면(1.2m) 시작부터 슬롯 근처인 팔로워가 즉시 ready → 리더가 팔로워 도착 전
        //   조기출발(대형 안 갖추고 혼자 감). 좁은 tol + 디바운스로 실제 도착 후에만 ready.
        formup_arrive_tol_m_ =
            this->declare_parameter<double>("formation.formup_arrive_tol_m", 0.4);
        // form-up 직렬화 차폭 클리어런스(2·외접반경+마진). 슬롯간격보다 살짝 커야 column 등
        // 후방슬롯 정렬이 한 대씩(s1순서) 직렬화돼 교차충돌이 안 남.
        formup_clearance_m_ =
            this->declare_parameter<double>("formation.formup_clearance_m", 2.5);
        formup_hardstop_m_ =
            this->declare_parameter<double>("formation.formup_hardstop_m", 1.5);
        spawn_x_ = this->declare_parameter<double>("spawn_x", 0.0);
        spawn_y_ = this->declare_parameter<double>("spawn_y", 0.0);
        // 리더: 출발 전 form-up 을 기다릴 팔로워 robot_id 목록. 비우면 게이트 없음(즉시 출발).
        formation_followers_ = this->declare_parameter<std::vector<int64_t>>(
            "formation.followers", std::vector<int64_t>{});
        avoid_radius_ = this->declare_parameter<double>("formation.avoid_radius_m", 6.0);
        avoid_stop_dist_ = this->declare_parameter<double>("formation.avoid_stop_dist_m", 1.5);
        base_speed_mps_ = this->declare_parameter<double>("formation.cruise_speed_mps", 1.4);
        // ★종대 전환 페이싱(사용자 지시): 리더는 후미 대기로 크롤/정지하지 말고 계속 전진.
        //   통신 지연/드롭 대비 간격 데드밴드를 크게(deadband 안이면 감속 0), 크게 벌어질 때만
        //   완만히 감속하되 floor_frac 아래로는 안 떨어뜨림(크롤 금지). 팔로워는 convoy_turn_ok
        //   순번대로 대기 후 한 대씩 리더 뒤에 합류 → 간격은 합류로 회복(리더 정지로 X).
        column_pace_deadband_m_ =
            this->declare_parameter<double>("formation.column_pace_deadband_m", 5.0);
        column_pace_slow_span_m_ =
            this->declare_parameter<double>("formation.column_pace_slow_span_m", 8.0);
        column_pace_floor_frac_ =
            this->declare_parameter<double>("formation.column_pace_floor_frac", 0.5);
        sync_gain_ = this->declare_parameter<double>("formation.sync_gain", 1.2);
        // 종방향 동기 허용오차(m). 작으면 작은 지체에도 리더가 감속 → 한 차 스워브 회피 시
        // 다른 차들이 멈춤. 스워브는 빠르므로 여유(±이만큼)를 줘서 회피 중 다른 차가 안 멈추게.
        // 큰 지체(코너 등)는 코너 배리어가 재정렬 담당.
        sync_tol_m_ = this->declare_parameter<double>("formation.sync_tol_m", 3.0);
        // ★리더 페이싱 하한: 팔로워가 (회피 등으로) 많이 뒤질 때 리더가 완전정지하면,
        //   정지 중 costmap 순간 stale→"0 poses" abort→재전송 churn→MPPI 재초기화로 회피 후
        //   yaw 떨림·거친 재편성 실측. 급정지 대신 이 하한으로 '부드럽게 전진'해 팔로워가 갭을
        //   통과·합류할 시간을 주고 재편성은 벽 너머에서 자연스럽게. (COLUMN 컨보이·hard_stop 제외)
        leader_pace_floor_ = this->declare_parameter<double>("formation.leader_pace_floor_mps", 0.3);
        obstacle_pass_hold_s_ =
            this->declare_parameter<double>("formation.obstacle_pass_hold_s", 4.0);
        obstacle_pass_reform_gap_ =
            this->declare_parameter<double>("formation.obstacle_pass_reform_gap_m", 2.0);
        is_leader_ = (robot_id_ == leader_robot_id_);
        const bool dynamic = (formation_mode_ == "dynamic");

        // 모든(추종 가능한) 로봇이 자기 pose 를 공유 발행 → 팔로워 근접 회피 입력.
        if (is_leader_ || dynamic) {
            robot_pose_pub_ = this->create_publisher<geometry_msgs::msg::PoseStamped>(
                "/swarm/robot_poses", rclcpp::QoS(10));
        }

        if (is_leader_) {
            // Leader publishes its map pose so followers can track it.
            leader_pose_pub_ = this->create_publisher<geometry_msgs::msg::PoseStamped>(
                "/swarm/leader_pose", rclcpp::QoS(10));
            map_pose_sub_ = this->create_subscription<nav_msgs::msg::Odometry>(
                "odom", 10,
                std::bind(&SwarmPathExecutorNode::on_own_map_pose, this, _1));
            // 지정 팔로워의 ready 구독(latched). 전부 ready 전엔 출발 보류.
            // ★로스터는 정적 목록이 아니라 하트비트 기반 라이브 멤버십: formation_followers 는
            //   초기 로스터일 뿐, /swarm/robot_world_pos 하트비트로 새 로봇이 잡히면 편입되고
            //   링크다운이면 배리어 게이트에서 제외된다(all_followers_ready/reform_done).
            for (const int64_t id : formation_followers_) {
                ensure_follower_registered(static_cast<int>(id));
            }
        } else if (dynamic) {
            // Dynamic follower: velocity-feedforward pursuit of the leader's live pose.
            // Drives /sN/cmd_vel directly (nav2 is skipped for dynamic followers, so
            // no cmd_vel contention). Matching the leader's SPEED (feedforward) is what
            // removes the steady-state trailing that a drive-to-goal (nav2) leaves.
            dynamic_follow_ = true;
            cmd_vel_pub_ = this->create_publisher<geometry_msgs::msg::Twist>("cmd_vel", 10);
            // 슬롯 도착 여부(form-up) 를 리더에게 알림(latched).
            formation_ready_pub_ = this->create_publisher<std_msgs::msg::Bool>(
                "formation_ready", rclcpp::QoS(1).transient_local());
            // 다른 로봇 pose 구독(근접 회피용). frame_id 에 robot_id.
            robot_poses_sub_ = this->create_subscription<geometry_msgs::msg::PoseStamped>(
                "/swarm/robot_poses", rclcpp::QoS(10),
                [this](geometry_msgs::msg::PoseStamped::SharedPtr m){
                    const int id = std::atoi(m->header.frame_id.c_str());
                    if (id != static_cast<int>(robot_id_)) {
                        neighbor_pos_[id] = {m->pose.position.x, m->pose.position.y};
                    }
                });
            // 라이다(정적 장애물): 팔로워는 nav2 없으니 직접 전방 장애물 반응.
            lidar_sub_ = this->create_subscription<sensor_msgs::msg::PointCloud2>(
                "rslidar_points", rclcpp::SensorDataQoS(),
                std::bind(&SwarmPathExecutorNode::on_lidar, this, _1));
            leader_pose_sub_ = this->create_subscription<geometry_msgs::msg::PoseStamped>(
                "/swarm/leader_pose", rclcpp::QoS(10),
                std::bind(&SwarmPathExecutorNode::on_leader_pose, this, _1));
            map_pose_sub_ = this->create_subscription<nav_msgs::msg::Odometry>(
                "odom", 10,
                std::bind(&SwarmPathExecutorNode::on_own_map_pose, this, _1));
            follow_timer_ = this->create_wall_timer(
                50ms, std::bind(&SwarmPathExecutorNode::control_tick, this));
        }

        // offset-path 대형(기본): 리더/팔로워 모두 nav2 로 '자기 오프셋 경로'를 독립 구동.
        // arc-length 진행도를 공유해 리더 기준으로 속도를 맞춤(나란히/일렬/마름모 공통).
        // 로봇간 pose 의존이 없어 dynamic 추종의 프레임 버그·뒤쳐짐이 구조적으로 사라짐.
        if (!dynamic && formation_enabled_) {
            progress_pub_ = this->create_publisher<geometry_msgs::msg::PointStamped>(
                "/swarm/progress", rclcpp::QoS(10));
            progress_sub_ = this->create_subscription<geometry_msgs::msg::PointStamped>(
                "/swarm/progress", rclcpp::QoS(10),
                std::bind(&SwarmPathExecutorNode::on_progress, this, _1));
            // FollowPath 팔로워 form-up ready 발행(리더가 /sN/formation_ready 구독해 대기).
            if (!is_leader_ && !formation_ready_pub_) {
                formation_ready_pub_ = this->create_publisher<std_msgs::msg::Bool>(
                    "formation_ready", rclcpp::QoS(1).transient_local());
            }
            // 정렬용 직접 제어는 nav2 체인 입구 cmd_vel_nav 로 발행(smoother·collision_monitor
            // 거쳐 경합 없음). /cmd_vel 직접발행은 collision_monitor 0발행과 경합=strange motion.
            // 리더(제자리 회전, 드리프트 제거)·팔로워(후진으로 깊은슬롯 정렬) 모두 사용.
            if (!repos_cmd_pub_) {
                repos_cmd_pub_ = this->create_publisher<geometry_msgs::msg::Twist>("cmd_vel_nav", 10);
                // 최종 cmd_vel 관찰(웨지 판정용): speed_limit 가 높아도 goal 이 취소돼
                // 컨트롤러가 실제 명령을 안 내는 '정상 정지'에서 웨지가 오발해 후진
                // 폭주(run19 리더 140m 후진 실측) — 실제 명령 기반으로만 접촉 의심.
                cmd_vel_obs_sub_ = this->create_subscription<geometry_msgs::msg::Twist>(
                    "cmd_vel", rclcpp::QoS(10),
                    [this](geometry_msgs::msg::Twist::SharedPtr m){
                        last_cmd_speed_ = std::abs(m->linear.x);
                        last_cmd_t_ = this->now().seconds();
                    });
            }
            // 리더-경로-공유: 리더는 자기 dense 경로 발행, 팔로워는 구독해 cross+along 오프셋만 적용.
            if (is_leader_) {
                leader_ref_path_pub_ = this->create_publisher<nav_msgs::msg::Path>(
                    "/swarm/leader_ref_path", rclcpp::QoS(1).transient_local());
                // COLUMN 컨보이: 리더 실주행 breadcrumb 궤적(스워브 포함) 공유.
                leader_trace_pub_ = this->create_publisher<nav_msgs::msg::Path>(
                    "/swarm/leader_trace", rclcpp::QoS(1).transient_local());
            } else {
                leader_trace_sub_ = this->create_subscription<nav_msgs::msg::Path>(
                    "/swarm/leader_trace", rclcpp::QoS(1).transient_local(),
                    [this](nav_msgs::msg::Path::SharedPtr m){ leader_trace_ = *m; });
                leader_ref_path_sub_ = this->create_subscription<nav_msgs::msg::Path>(
                    "/swarm/leader_ref_path", rclcpp::QoS(1).transient_local(),
                    [this](nav_msgs::msg::Path::SharedPtr m){
                        leader_ref_path_ = *m;
                        leader_ref_valid_ = true;
                        // 재정렬 중인데 아직 새 슬롯 미도착이면 재개 보류(리더가 먼저 재발행해도).
                        if (reforming_ && !reform_done_) return;
                        if (waiting_for_leader_path_ && active_) {
                            waiting_for_leader_path_ = false;
                            reforming_ = false; reform_done_ = false;   // 재정렬 종료 → 재개
                            build_follower_path_from_leader();
                        }
                    });
            }
            // 우선순위 충돌해소: 각 차의 스워브 상태(x=id, y=swerving01, z=fraction) 공유.
            swerve_state_pub_ = this->create_publisher<geometry_msgs::msg::PointStamped>(
                "/swarm/swerve_state", rclcpp::QoS(10));
            swerve_state_sub_ = this->create_subscription<geometry_msgs::msg::PointStamped>(
                "/swarm/swerve_state", rclcpp::QoS(10),
                [this](geometry_msgs::msg::PointStamped::SharedPtr m){
                    const int id = std::atoi(m->header.frame_id.c_str());
                    peer_swerve_[id] = {m->point.y, m->point.z};
                    peer_swerve_seen_[id] = this->now().seconds();
                });
            // 대형전환 재정렬 동기: 각 차의 재정렬-완료(point.x=0/1) 공유. 팔로워는 우선순위
            // (자기보다 작은 id) 전원 done 후에야 자기 재배치 시작 → 직렬화로 교차충돌 방지.
            reform_ready_pub_ = this->create_publisher<geometry_msgs::msg::PointStamped>(
                "/swarm/reform_ready", rclcpp::QoS(10));
            reform_ready_sub_ = this->create_subscription<geometry_msgs::msg::PointStamped>(
                "/swarm/reform_ready", rclcpp::QoS(10),
                [this](geometry_msgs::msg::PointStamped::SharedPtr m){
                    const int id = std::atoi(m->header.frame_id.c_str());
                    if (id == static_cast<int>(robot_id_)) return;
                    reform_done_peers_[id] = m->point.x;
                    reform_seen_[id] = this->now().seconds();
                    if (is_leader_) maybe_start_after_formation();  // 팔로워 done → 리더 깨움
                });
            // ★조기종대: 리더가 장애물통과 개시/종료를 전 로봇에 알림(point.x=활성). 팔로워는
            //   이 신호로 obs_convoy(리더궤적 추종 종대)를 조기 발동 → 대형은 그대로 두고 종대
            //   거동만 유도(정지-reform 없이 매끄러운 롤링 머지). TRANSIENT_LOCAL + 리더 주기
            //   재발행으로 늦은매칭 구독자도 발동 순간 포착(LOAD-유실 교훈). 팔로워만 반영.
            {
                auto ovr_qos = rclcpp::QoS(1).transient_local();
                formation_override_pub_ =
                    this->create_publisher<geometry_msgs::msg::PointStamped>(
                        "/swarm/obstacle_pass", ovr_qos);
                formation_override_sub_ =
                    this->create_subscription<geometry_msgs::msg::PointStamped>(
                        "/swarm/obstacle_pass", ovr_qos,
                        [this](geometry_msgs::msg::PointStamped::SharedPtr m){
                            if (is_leader_) return;   // 리더는 sync_tick 자체 래치가 구동
                            const bool active = (m->point.x > 0.5);
                            if (active == leader_obstacle_pass_active_) return;
                            leader_obstacle_pass_active_ = active;
                            RCLCPP_INFO(this->get_logger(),
                                "[조기종대] 리더 장애물통과 %s 수신 → obs_convoy %s",
                                active ? "개시" : "종료", active ? "조기발동" : "이탈판정 개시");
                        });
            }
            // 장애물 공유 버스: 자기 라이다 감지 장애물을 world(GNSS map) 좌표로 발행
            // (x=worldX, y=worldY, z=반경; frame_id=발행 로봇 id). 팀원이 받아 자기 body-frame
            // 으로 투영해 회피판단에 합산 → 리더가 찾은 박스를 팔로워가 미리 우회. 모든 로봇이
            // 같은 GNSS datum(map) 공유하므로 좌표 비교 가능.
            obstacle_pub_ = this->create_publisher<geometry_msgs::msg::PointStamped>(
                "/swarm/obstacles", rclcpp::QoS(10));
            obstacle_sub_ = this->create_subscription<geometry_msgs::msg::PointStamped>(
                "/swarm/obstacles", rclcpp::QoS(10),
                std::bind(&SwarmPathExecutorNode::on_shared_obstacle, this, _1));
            speed_limit_pub_ = this->create_publisher<nav2_msgs::msg::SpeedLimit>(
                "speed_limit", rclcpp::QoS(10));
            sync_timer_ = this->create_wall_timer(
                100ms, std::bind(&SwarmPathExecutorNode::sync_tick, this));

            // 리더-앵커 form-up: 자기 map pose 추적(odometry/global) + toLL + 앵커 pub/sub.
            own_global_sub_ = this->create_subscription<nav_msgs::msg::Odometry>(
                "odometry/global", 10,
                [this](nav_msgs::msg::Odometry::SharedPtr m){
                    own_map_x_ = m->pose.pose.position.x;
                    own_map_y_ = m->pose.pose.position.y;
                    own_yaw_ = tf2::getYaw(m->pose.pose.orientation);
                    own_pose_valid_ = true;
                    last_global_pose_t_ = this->now().seconds();
                });
            to_ll_client_ = this->create_client<robot_localization::srv::ToLL>("toLL");
            anchor_sub_ = this->create_subscription<geometry_msgs::msg::PointStamped>(
                "/swarm/formation_anchor", rclcpp::QoS(10),
                std::bind(&SwarmPathExecutorNode::on_anchor, this, _1));
            if (is_leader_) {
                anchor_pub_ = this->create_publisher<geometry_msgs::msg::PointStamped>(
                    "/swarm/formation_anchor", rclcpp::QoS(10));
            }
            // 대형 배리어 ready 공유(도달 wp 인덱스).
            ready_pub_ = this->create_publisher<geometry_msgs::msg::PointStamped>(
                "/swarm/formation_ready", rclcpp::QoS(10));
            ready_sub_ = this->create_subscription<geometry_msgs::msg::PointStamped>(
                "/swarm/formation_ready", rclcpp::QoS(10),
                std::bind(&SwarmPathExecutorNode::on_ready, this, _1));
            mask_pub_ = this->create_publisher<std_msgs::msg::Bool>(
                "mask_teammates", rclcpp::QoS(10));
            // 리더 실제 map 위치 구독(/swarm/robot_world_pos, frame_id=id; 라이다필터가 발행).
            leader_world_sub_ = this->create_subscription<geometry_msgs::msg::PointStamped>(
                "/swarm/robot_world_pos", rclcpp::QoS(10),
                [this](geometry_msgs::msg::PointStamped::SharedPtr m){
                    const int id = std::atoi(m->header.frame_id.c_str());
                    const double tw = this->now().seconds();
                    // 속도 추정(유한차분 + 저역통과) — 예측 궤적 반발용.
                    const auto pit = prev_world_.find(id);
                    if (pit != prev_world_.end()) {
                        const double dt = tw - prev_world_t_[id];
                        if (dt > 1e-2) {
                            const double vx = (m->point.x - pit->second.first) / dt;
                            const double vy = (m->point.y - pit->second.second) / dt;
                            auto &v = vel_[id];
                            v.first = 0.6 * v.first + 0.4 * vx;
                            v.second = 0.6 * v.second + 0.4 * vy;
                        }
                    }
                    prev_world_[id] = {m->point.x, m->point.y};
                    prev_world_t_[id] = tw;
                    world_pos_[id] = {m->point.x, m->point.y};   // 반발장(차간 거리)용 전원 저장
                    world_seen_[id] = tw;
                    // ★하트비트 기반 동적 편입: 목록에 없던 로봇도 살아있으면 로스터/ready 구독 등록.
                    if (is_leader_) ensure_follower_registered(id);
                    if (id == static_cast<int>(leader_robot_id_)) {
                        leader_world_x_ = m->point.x;
                        leader_world_y_ = m->point.y;
                        leader_world_valid_ = true;
                    }
                    if (id == static_cast<int>(robot_id_)) {   // 리더와 동일 소스의 내 위치
                        lock_own_x_ = m->point.x;
                        lock_own_y_ = m->point.y;
                        lock_own_valid_ = true;
                    }
                });
            // 팔로워만: 횡잠금 보정 타이머(파라미터로 on/off).
            if (!is_leader_ && lateral_lock_enabled_) {
                lateral_timer_ = this->create_wall_timer(
                    1000ms, std::bind(&SwarmPathExecutorNode::lateral_lock_tick, this));
            }
        }

        RCLCPP_INFO(this->get_logger(),
                    "Swarm Path Executor 준비 — robot_id=%u leader=%u (slot offset %s) "
                    "입력(FSM 게이트): %s · %s · mission_input",
                    static_cast<unsigned>(robot_id_), static_cast<unsigned>(leader_robot_id_),
                    formation_enabled_ ? "ON" : "OFF", path_topic.c_str(), control_topic.c_str());
    }

private:
    // ---- 상태 ----
    std::vector<std::pair<double, double>> active_points_;   // (lat, lon) iterating
    std::size_t active_index_ = 0;
    bool active_ = false;
    bool paused_ = false;   // PAUSE 로 현재 goal 을 cancel 한 상태 (active_index_ 유지)

    // 태블릿 LOAD_PATH로 받아둔 경로 (START 전까지 대기)
    std::vector<std::pair<double, double>> pending_path_;
    // nav2 가 아직 안 떴을 때 보류된 START 경로 + 재시도 타이머.
    std::vector<std::pair<double, double>> pending_start_path_;
    rclcpp::TimerBase::SharedPtr retry_start_timer_;
    // nav2 abort 시 같은 wp 재시도(영구실패 방지).
    int wp_retry_ = 0;
    static constexpr int kMaxWpRetry = 5;
    rclcpp::TimerBase::SharedPtr wp_retry_timer_;

    GoalHandleNav::SharedPtr active_goal_;
    uint8_t mission_status_ = combat_robot_msgs::msg::OperationState::MISSION_NONE;
    uint8_t mission_error_code_ = MISSION_ERROR_NONE;
    float distance_to_next_wp_m_ = 0.0f;
    float distance_to_goal_m_ = 0.0f;

    rclcpp::Subscription<combat_robot_msgs::msg::SwarmPathCommand>::SharedPtr path_cmd_sub_;
    rclcpp::Subscription<combat_robot_msgs::msg::WaypointList>::SharedPtr mission_sub_;
    rclcpp::Publisher<combat_robot_msgs::msg::OperationState>::SharedPtr mission_state_pub_;
    rclcpp::TimerBase::SharedPtr status_timer_;
    rclcpp_action::Client<NavigateToPose>::SharedPtr nav_client_;
    rclcpp_action::Client<FollowPath>::SharedPtr follow_path_client_;
    rclcpp::Client<robot_localization::srv::FromLL>::SharedPtr from_ll_client_;
    // ---- FollowPath 모드 상태 ----
    std::string control_mode_{"follow_path"};
    std::vector<std::pair<double, double>> fp_map_points_;  // 오프셋경로 map(x,y) 변환 누적
    std::size_t fp_convert_index_ = 0;                       // 다음 변환할 GPS wp 인덱스
    double fp_path_length_ = 0.0;                            // FollowPath 경로 총 길이(m)
    double fp_dist_to_goal_ = 0.0;                           // 피드백: 남은 거리(m) → 진행도 산출
    GoalHandleFP::SharedPtr active_fp_goal_;
    rclcpp::Publisher<nav_msgs::msg::Path>::SharedPtr fp_path_pub_;   // rviz 시각화(/sN/plan)
    rclcpp::Publisher<visualization_msgs::msg::MarkerArray>::SharedPtr dbg_marker_pub_;  // 디버그 마커
    rclcpp::Publisher<std_msgs::msg::UInt8>::SharedPtr light_pub_;   // 차량 라이트 상태
    rclcpp::TimerBase::SharedPtr light_timer_;
    bool blink_toggle_ = false;                                     // 하향등 점멸 토글
    // ---- 리더-경로-공유: 팔로워가 리더 dense 경로에 cross+along 오프셋만 적용(동일 곡선 기반) ----
    rclcpp::Publisher<nav_msgs::msg::Path>::SharedPtr leader_ref_path_pub_;   // 리더가 발행
    rclcpp::Subscription<nav_msgs::msg::Path>::SharedPtr leader_ref_path_sub_;
    nav_msgs::msg::Path leader_ref_path_;     // 받은 리더 기준경로(dense, 라운딩됨)
    bool leader_ref_valid_ = false;
    bool waiting_for_leader_path_ = false;    // 팔로워: 리더경로 대기 중
    nav_msgs::msg::Path fp_last_path_;                        // 마지막 전송 경로(검증/시각화)

    // ---- 장애물 로컬 우회(FollowPath 막힐 때만 nav2 로 우회 후 라인 복귀) ----
    // 리더가 대칭 라인을 주고, 장애물에 막힌 로봇만 그 구간을 로컬 재계획(NavigateToPose)으로
    // 돌아 rejoin 점에서 다시 라인을 FollowPath 로 추종. 실차에서도 costmap+planner 그대로.
    rclcpp::Client<nav2_msgs::srv::IsPathValid>::SharedPtr is_path_valid_client_;
    rclcpp::TimerBase::SharedPtr obstacle_check_timer_;
    bool fp_detour_active_ = false;     // 우회(NavigateToPose) 진행 중
    double last_fp_resend_t_ = 0.0;     // ABORTED(장애물없음) 시 FollowPath 재전송 쿨다운(self-루프 방지)
    bool fp_check_inflight_ = false;    // IsPathValid 응답 대기 중(중복요청 방지)
    std::size_t fp_rejoin_index_ = 0;   // 우회 후 라인 복귀할 fp_last_path_(밀집) pose 인덱스
    double detour_skip_m_ = 6.0;        // (폴백) 막힘 위치 모를 때 라인상 앞 거리
    int detour_fail_streak_ = 0;        // 연속 우회 실패 횟수 — rejoin 을 점진 연장(에스컬레이션).
                                        // rejoin 이 장애물 내부면 planner 가 0-pose plan 으로
                                        // 매번 실패하는데, 로봇이 정지라 nearest 불변 → 같은
                                        // rejoin 무한 재시도 deadlock(POC 장애물軍 sim 실측).
    double detour_clear_margin_m_ = 2.0;// 막힌 구간 끝에서 이만큼 더 가서 rejoin(여유)
    double obstacle_lookahead_m_ = 8.0; // (미사용) 과거 IsPathValid 윈도우 길이
    double detour_trigger_dist_ = 3.5;  // body-frame 전방 콘 장애물 이 거리 이내면 우회 발동
    double obstacle_detect_range_ = 12.0; // 전방 콘 장애물 '감지' 최대거리(트리거보다 넓게)
    double lidar_self_range_ = 1.5;     // 이 반경 내 lidar 점은 자기차체/코앞지면 → 무시(거짓우회 방지)
    int min_cluster_pts_ = 6;           // 전방콘에 이 점수 이상 군집해야 실장애물(노이즈 거름)
    int obstacle_persist_ticks_ = 3;    // 이만큼 연속 감지돼야 우회(순간 노이즈 무시)
    int obstacle_persist_count_ = 0;    // 연속 감지 카운터
    double startup_grace_s_ = 6.0;      // 출발 후 이 시간 회피 억제(스폰 settling 피치→지면오감지)
    bool corner_ff_enabled_ = true;     // 코너 곡률 피드포워드 속도배율(바깥 가속/안쪽 감속)
    rclcpp::Time fp_start_time_;        // 최초 FollowPath 전송 시각(grace 기준)
    // ---- COLUMN 컨보이(팔로워 = 리더 실주행 궤적 그대로 추종) ----
    // 장애물軍 통과 시 각자 회피는 후미로 갈수록 실패(5-robot POC sim 실측: 공유장애물·
    // 우선순위 양보로도 s5 스탈). COLUMN 에선 리더만 회피 계산하고, 팔로워는 리더가 실제
    // 지나간 breadcrumb 궤적(+그 너머는 원본 라인)을 추종 — 리더가 통과한 길은 기하학적으로
    // 안전이 보장된다. 팔로워 자체 스워브/planner 우회는 억제(긴급정지·collision_monitor·
    // 끼임복구는 유지).
    rclcpp::Publisher<nav_msgs::msg::Path>::SharedPtr leader_trace_pub_;
    rclcpp::Subscription<nav_msgs::msg::Path>::SharedPtr leader_trace_sub_;
    nav_msgs::msg::Path own_trace_;      // 리더: 자기 실주행 breadcrumb(0.5m 간격, map)
    nav_msgs::msg::Path leader_trace_;   // 팔로워: 수신한 리더 궤적
    nav_msgs::msg::Path fp_base_line_;   // 원본 라인 경로(trace 너머 잔여 구간 합성용)
    double last_trace_pub_t_ = 0.0;      // 리더 trace 발행 스로틀(1Hz)
    double last_convoy_send_t_ = 0.0;    // 팔로워 convoy 경로 재전송 스로틀
    std::size_t convoy_sent_len_ = 0;    // 마지막 재전송 시점의 trace 길이
    // ★대형 단위 코히어 통과: 비-COLUMN 팔로워도 리더가 광폭장애물 회피 중이면 임시로
    //   리더 실궤적(leader_trace_, 우회 포함)을 따라 갭을 통과(임시 종대)한 뒤 대형 복원.
    //   리더 ref_path 는 planner 우회를 안 담아(clean 라인) 슬롯팔로워가 벽으로 직진→각자
    //   회피→지저분한 재편성이던 것을, 리더 궤적 공유로 협조 통과하게 함(사용자 지시).
    bool obs_convoy_ = false;            // 팔로워: 임시 장애물-컨보이(리더궤적 추종) 활성
    double detour_lat_thresh_m_ = 1.0;   // 우회 판정 횡거리 임계(m)
    double detour_exit_margin_m_ = 6.0;  // 이탈 마진(m): far-edge 이만큼 더 지나야 복원
    double detour_exit_s_ = 0.0;         // 래치된 우회 far-edge along-track 좌표(m)
    bool convoy_turn_latched_ = false;   // 롤링 머지: 내 출발 순서 도래(1회 latch)
    int wedge_reverse_ticks_ = 0;        // 웨지(접촉 밀착) 복구 후진 잔여 틱(10Hz)
    double last_speed_limit_ = 0.0;      // 마지막 발행 speed_limit(웨지 판정용)
    bool gap_hold_ = false;              // 앞차 crit 간격 — goal 취소 hold 중(크리핑 차단)
    double wedge_rev_x_ = 0.0, wedge_rev_y_ = 0.0;   // 후진 시작 위치(막힘 감지)
    double last_wedge_t_ = -1e9;         // 웨지 쿨다운(재발동 폭주 방지)
    double detour_start_t_ = 0.0;        // 우회 시작 시각(행 워치독)
    rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr cmd_vel_obs_sub_;
    double last_cmd_speed_ = 0.0;        // 최종 cmd_vel 크기(웨지 = 실제 명령 기반 판정)
    double last_cmd_t_ = -1e9;
    int wedge_count_ = 0;                // 미션당 웨지 상한(폭주 최종 안전판)
    double goalless_since_ = -1.0;       // goal 부재 시작 시각(안전망)
    double last_safety_resend_ = -1e9;   // 안전망 재전송 쿨다운(폭주 방지)

    // ---- 스워브 회피(NavigateToPose dogleg 대신 부드러운 횡오프셋 경로) ----
    bool fp_swerving_ = false;          // 스워브 경로 추종 중(중복 트리거 차단)
    bool ignore_next_fp_result_ = false;// 스워브 위해 옛 goal 취소 시 그 terminal 결과 1회 무시
    double swerve_clearance_m_ = 1.5;   // 횡 피크 이탈량(box 폭+여유엔 충분, 옆라인 침범 최소화)
    double swerve_transition_m_ = 5.0;  // 진입/복귀 램프 길이(클수록 넓고 얕게=멀리서부터)
    double swerve_hold_past_m_ = 3.0;   // 장애물 지난 뒤 이탈 유지 거리(복귀 전)
    double fp_original_length_ = 0.0;   // 미션 원본 경로 총길이(우회/재개에도 불변) — 진행도 분모
    double corner_radius_m_ = 6.0;      // 급코너 필렛 반경(크게=반경비↓=안쪽차 안멈춤)
    double align_gain_ = 0.25;          // fraction 정렬 P게인(뒤지면 가속해 gap 닫기)
    double corner_align_gain_ = 0.35;   // fraction P게인(코너 동시탈출: 뒤지면 강하게 가속)
    double corner_influence_m_ = 10.0;  // 코너 이 거리 이내면 시간동기 차등, 밖이면 직선 P-제어
    double swerve_min_speed_frac_ = 0.35;// 속도 하한 = base × 이값(코너서도 안 멈추게)
    double last_swerve_time_ = -1e9;    // 마지막 스워브 시각(재회피 쿨다운)
    double swerve_cooldown_s_ = 1.5;     // 스워브 후 이 시간은 재트리거 보류(thrash 방지)
    // ---- 우선순위 충돌해소(동시 스워브 충돌 방지) ----
    rclcpp::Publisher<geometry_msgs::msg::PointStamped>::SharedPtr formation_override_pub_;
    rclcpp::Subscription<geometry_msgs::msg::PointStamped>::SharedPtr formation_override_sub_;
    rclcpp::Publisher<geometry_msgs::msg::PointStamped>::SharedPtr swerve_state_pub_;
    rclcpp::Subscription<geometry_msgs::msg::PointStamped>::SharedPtr swerve_state_sub_;
    std::map<int, std::pair<double, double>> peer_swerve_;   // id → (swerving 0/1, fraction)
    std::map<int, double> peer_swerve_seen_;
    bool yielding_ = false;             // 더 높은 우선순위 스워브에 양보 중(감속)
    // ---- 장애물 공유 버스(협조 회피): 팀원 감지 장애물을 world 좌표로 받아 선제 회피 ----
    rclcpp::Publisher<geometry_msgs::msg::PointStamped>::SharedPtr obstacle_pub_;
    rclcpp::Subscription<geometry_msgs::msg::PointStamped>::SharedPtr obstacle_sub_;
    struct SharedObstacle { double x = 0, y = 0, radius = 0, stamp = 0; };
    std::vector<SharedObstacle> shared_obstacles_;   // 팀원이 보고한 장애물(world, 중복병합)
    double shared_obstacle_ttl_s_ = 6.0;   // 이 시간 미갱신 장애물은 만료(지나감/사라짐)
    double shared_merge_dist_ = 1.5;       // 이 이내 보고는 같은 장애물로 병합
    // ---- 차폭 충돌 방지(반발장): 차간 거리 안전 유지 ----
    std::map<int, std::pair<double, double>> world_pos_;  // id → world(x,y) (라이다필터 일관 프레임)
    std::map<int, double> world_seen_;
    double repulse_dist_ = 1.7;         // 이 거리부터 감속(상위 우선순위 차에)
    double repulse_safe_dist_ = 1.3;    // 이 거리면 완전정지(차체 1.32m 길이 고려 — 충돌 방지)
    // 예측 궤적 기반 선제 반발: 각 차 속도(추정)로 예측 최근접거리 → 겹치기 전에 감속.
    std::map<int, std::pair<double, double>> vel_;          // id → world 속도(vx,vy)
    std::map<int, std::pair<double, double>> prev_world_;   // id → 이전 world(x,y)
    std::map<int, double> prev_world_t_;                    // id → 이전 시각
    double predict_horizon_s_ = 3.0;    // 예측 지평(초)
    // 실차 GNSS 노이즈 대응: 종간격오차 저역통과 + 데드밴드(작은 지터엔 무반응).
    double along_err_filt_ = 0.0;       // 저역통과된 종간격오차
    double spacing_deadband_m_ = 0.3;   // 횡대형(DIAMOND/WEDGE) 페이싱 데드밴드. 크면 s2/s3 가
                                        //   ±이만큼 드리프트해 비대칭(레벨 안 맞음). 작게 유지해
                                        //   대칭 슬롯 타이트하게. (COLUMN 은 IDM 이라 이 값 무관.)
    // ---- 끼임 감지(스워브가 다른 장애물에 끼이는 등) → NavigateToPose 복구 ----
    double stuck_ref_x_ = 0.0, stuck_ref_y_ = 0.0, stuck_ref_t_ = 0.0;
    bool stuck_ref_set_ = false;
    double stuck_timeout_s_ = 4.0;      // 이 시간 0.4m 미만 이동이면 끼임
    std::vector<double> corner_arcs_;   // 원본 경로상 코너들의 절대 arc 위치(시간동기 코너탈출용)
    std::map<int, double> peer_corner_dist_;  // peer id → 다음코너까지 남은거리(d_i)
    std::map<int, double> peer_corner_seen_;  // peer id → 마지막 수신시각
    GoalHandleNav::SharedPtr active_detour_goal_;

    // ---- 리더-앵커 form-up (랜덤/임의 위치 스폰 대응) ----
    // 리더가 자기 현재 위치를 GPS 앵커로 방송, 모든 로봇이 경로 앞에 [앵커]를 붙여
    // "리더 위치에서 대형 정렬 후 목표로 출발"이 되게 한다.
    rclcpp::Client<robot_localization::srv::ToLL>::SharedPtr to_ll_client_;
    rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr own_global_sub_;
    rclcpp::Publisher<geometry_msgs::msg::PointStamped>::SharedPtr anchor_pub_;
    rclcpp::Subscription<geometry_msgs::msg::PointStamped>::SharedPtr anchor_sub_;
    rclcpp::TimerBase::SharedPtr anchor_timeout_timer_;
    double anchor_lat_ = 0.0, anchor_lon_ = 0.0;
    bool anchor_valid_ = false;
    bool awaiting_anchor_ = false;
    double last_anchor_bcast_ = 0.0;   // 리더 form-up 대기 중 앵커 재방송 스로틀(초)

    // ---- 대형 배리어: 배리어 wp(시작 form-up + 코너)마다 전원 도착 전엔 출발 보류 ----
    // 직선 구간은 배리어 없음(속도동기화로 나란히). 코너에선 안쪽이 먼저 못 가게 정렬 후 출발.
    rclcpp::Publisher<geometry_msgs::msg::PointStamped>::SharedPtr ready_pub_;
    rclcpp::Subscription<geometry_msgs::msg::PointStamped>::SharedPtr ready_sub_;
    std::map<int, int> swarm_reached_;  // id → 도달한 최고 wp 인덱스
    int reached_index_ = -1;            // 내가 도달한 최고 wp 인덱스
    bool barrier_waiting_ = false;      // 배리어 wp 도착, 전원 대기 중
    std::vector<char> is_barrier_;      // wp별 배리어 여부(0=form-up, 코너)
    // 대형-인지 마스킹: form-up 완료(첫 배리어 통과) 후에만 팀원 마스킹 → 정상대형서
    // 서로 안 밀어냄(대칭). form-up/기동 중엔 마스킹 OFF → 서로 보고 회피(안 박음).
    rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr mask_pub_;

    // ---- 리더-상대 횡잠금(lateral lock) ----
    // 종(앞/뒤)은 미션경로 오프셋대로(앞서가는 대형도 OK), 횡(좌/우)만 리더 수직위치+오프셋
    // 으로 잠가 RPP cross-track 오차를 상쇄 → 모든 대형에서 대칭. 리더 미래경로 불필요.
    rclcpp::Subscription<geometry_msgs::msg::PointStamped>::SharedPtr leader_world_sub_;
    double leader_world_x_ = 0.0, leader_world_y_ = 0.0;
    bool leader_world_valid_ = false;
    double lock_own_x_ = 0.0, lock_own_y_ = 0.0;   // 횡잠금용 내 위치(리더와 동일 소스)
    bool lock_own_valid_ = false;
    double base_goal_x_ = 0.0, base_goal_y_ = 0.0;   // 현재 wp 원본 map 목표
    bool has_base_goal_ = false;
    double sent_goal_x_ = 0.0, sent_goal_y_ = 0.0;   // 마지막으로 nav2에 보낸 목표
    bool lateral_lock_enabled_ = false;
    double lateral_lock_gain_ = 1.0;
    double formation_densify_m_ = 10.0;
    rclcpp::TimerBase::SharedPtr lateral_timer_;
    int goal_seq_ = 0;   // 목표 재전송 시 preempt된 옛 goal의 result 무시용

    rclcpp::Subscription<sensor_msgs::msg::NavSatFix>::SharedPtr fix_sub_;
    rclcpp::Subscription<std_msgs::msg::Float64>::SharedPtr heading_sub_;
    rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr odom_sub_;
    double gps_lat_ = 0.0;
    double gps_lon_ = 0.0;
    float gps_heading_ = 0.0f;
    float current_speed_ = 0.0f;

    // ---- 편대(formation) ----
    rclcpp::Subscription<combat_robot_msgs::msg::SwarmControlCommand>::SharedPtr formation_sub_;
    uint32_t robot_id_ = 1;
    uint32_t leader_robot_id_ = 1;
    std::string map_frame_ = "map";
    double formation_lateral_spacing_m_ = 2.0;
    bool formation_enabled_ = true;
    uint8_t formation_type_ = 0;
    // ★조기종대: 리더가 장애물통과를 조기 감지하면 팔로워를 리더궤적 추종 종대(obs_convoy)로
    //   미리 전환시키는 신호. 대형(formation_type_)은 바꾸지 않고 obs_convoy 로 종대 거동만 유도
    //   → 정지-reform 배리어 없이 리더 전진 + convoy_turn_ok 순차 합류(사용자 지시).
    bool leader_obstacle_pass_active_ = false;          // 리더 장애물통과 개시(전 로봇 공유)
    double obstacle_pass_lookahead_m_ = 15.0;           // 리더 조기 발동 거리(>스워브 트리거)
    double last_override_pub_t_ = -1e9;                 // 브로드캐스트 마지막 발행 시각(주기 재발행)
    combat_robot_nav2::EarlyColumnLatch early_column_latch_;   // 리더 조기 발동/해제 래치
    uint8_t formation_number_ = 0;

    // ---- 동적 리더 추종(dynamic formation) ----
    std::string formation_mode_ = "static";
    bool is_leader_ = true;
    bool dynamic_follow_ = false;
    rclcpp::Publisher<geometry_msgs::msg::PoseStamped>::SharedPtr leader_pose_pub_;
    rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr cmd_vel_pub_;
    rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr repos_cmd_pub_;   // cmd_vel_nav(정렬용)
    // 로봇간 근접 회피: 모든 로봇이 pose 발행, 팔로워가 이웃 위치로 감속/회피.
    rclcpp::Publisher<geometry_msgs::msg::PoseStamped>::SharedPtr robot_pose_pub_;
    rclcpp::Subscription<geometry_msgs::msg::PoseStamped>::SharedPtr robot_poses_sub_;
    std::map<int, std::pair<double, double>> neighbor_pos_;   // id → (x,y)
    double avoid_radius_ = 3.0;     // 이 반경 내 전방 이웃/장애물이면 회피 시작
    double avoid_stop_dist_ = 1.5;  // 이 거리면 정지(로봇 전장 ~1.3m → footprint 여유)
    // 라이다 기반 정적 장애물(팔로워는 nav2 없음 → 직접 반응형 회피).
    rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr lidar_sub_;
    double obstacle_dist_ = 1e9;    // 전방 콘 최근접 장애물점 거리
    double obstacle_lat_ = 0.0;     // 그 점의 좌측 성분(조향 방향 결정)
    double left_clear_ = 1e9;       // 전방 좌측(y>0) 최근접 장애물 거리(스워브 방향 결정)
    double right_clear_ = 1e9;      // 전방 우측(y<0) 최근접 장애물 거리
    double obstacle_lat_lo_ = 0.0;  // 감지 장애물 우측 끝(body y 최소) — 폭 적응 클리어런스
    double obstacle_lat_hi_ = 0.0;  // 감지 장애물 좌측 끝(body y 최대)
    // ★넓은 콘(±64°)으로 잰 장애물의 '실제' 횡폭. 좁은 전방콘(±forward_cone_half_w_)은 벽폭을
    //   최대 ~2m 로만 보여 wide_obs(>임계) 가 어떤 벽에서도 안 켜졌음(planner 우회 死코드).
    //   벽/가벽은 이 실측폭으로 판별 → 스워브(횡 2.8m 상한)로 못 비끼는 넓은 것은 planner 우회.
    double obstacle_wide_span_ = 0.0;
    double footprint_half_w_ = 0.43;// 차폭 반(클리어런스 footprint 고려)
    double swerve_footprint_half_w_ = 0.7;  // 스워브 중 비스듬 차체 swept 반폭(0.43<<0.79)
    double forward_cone_half_w_ = 1.0;  // 전방 감지콘 절대폭 상한(원거리서 콘 안 넓어지게)
    double wide_obstacle_span_m_ = 3.5; // 실측 횡폭이 이보다 넓으면 벽=planner 우회(box<3.5=스워브)
    double hard_stop_dist_ = 1.3;       // 정면·차폭내 장애물 이 거리 이내+스워브실패 → 정지
    double obstacle_z_min_ = 0.0;       // 장애물 감지 z 하한(피치된 지면 배제)
    bool hard_stop_ = false;            // 긴급정지 활성(sync_tick 에서 speed_limit 0)
    rclcpp::Subscription<geometry_msgs::msg::PoseStamped>::SharedPtr leader_pose_sub_;
    rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr map_pose_sub_;
    rclcpp::TimerBase::SharedPtr follow_timer_;
    double leader_x_ = 0.0, leader_y_ = 0.0, leader_yaw_ = 0.0;
    double leader_speed_ = 0.0;
    bool leader_pose_valid_ = false;
    double prev_leader_x_ = 0.0, prev_leader_y_ = 0.0, prev_leader_t_ = 0.0;
    bool have_prev_leader_ = false;
    int leader_stale_ticks_ = 0;   // 리더 pose 끊김 감지(끊기면 정지, 발산 방지)
    double own_map_x_ = 0.0, own_map_y_ = 0.0, own_yaw_ = 0.0;
    double own_yaw_rate_ = 0.0;   // 각속도(댐핑용)
    bool own_pose_valid_ = false;
    double last_global_pose_t_ = -1e9;   // odometry/global 최신 수신시각(휠오돔 폴백 게이트)
    // 편대 pose 소스: gz wheel odom(/sN/odom) + 스폰 오프셋 → 공유 프레임.
    // (GNSS-ekf 가 3로봇서 발산/freeze 하므로 sim 에선 신뢰 가능한 gz odom 사용.)
    double spawn_x_ = 0.0, spawn_y_ = 0.0;

    // ---- form-up 게이트 ----
    // 팔로워: 슬롯 도착 시 ready 발행. 리더: 지정 팔로워 전부 ready 전엔 출발 보류.
    double formation_tol_m_ = 0.6;
    double formup_arrive_tol_m_ = 0.4;      // 슬롯 도착 판정(ready_tol 보다 좁음)
    int ready_streak_ = 0;
    int formup_ready_streak_ = 0;           // static form-up ready 디바운스
    double slot_x_ = 0.0, slot_y_ = 0.0;    // 현재 목표 슬롯(rviz 마커용)
    bool slot_valid_ = false;
    bool formation_ready_ = false;
    rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr formation_ready_pub_;       // 팔로워
    std::vector<int64_t> formation_followers_;                                    // 리더
    std::map<int, bool> follower_ready_;                                          // 리더
    std::vector<rclcpp::Subscription<std_msgs::msg::Bool>::SharedPtr> ready_subs_;
    bool waiting_for_formation_ = false;                                          // 리더
    // ---- FollowPath 출발 form-up: 전원 슬롯 도착 후 동시 출발 ----
    bool forming_up_ = false;           // 출발 form-up 중(슬롯 정렬 대기)
    double form_up_t0_ = 0.0;           // form-up 시작 시각
    double form_up_timeout_s_ = 5.0;    // 안전: 이 시간 지나면 강제 출발(데드락 방지)
    std::vector<std::pair<double, double>> held_path_;                           // 리더
    // 편대 기준 헤딩 = 첫 웨이포인트 방위. form-up/출발 초기에 leader_pose 헤딩으로
    // 발행해 슬롯을 주행방향 기준으로 고정(리더 실제 헤딩이 정렬될 때까지 유지).
    double formation_heading_ = 0.0;
    bool publish_formation_heading_ = false;
    // 새 경로 START 중복(-t 반복) 무시용. LOAD 마다 false 로 리셋 → 첫 START 만 소비.
    bool start_consumed_ = true;
    // ---- 주행 중 대형전환: 정지-재정렬-재개 (우선순위 직렬 NavigateToPose) ----
    // 횡 재구성(예 column x=0→wedge x=±2)은 FollowPath 재전송 시 막힘 abort 연쇄 → 대신
    // 정지 후 팀원이 우선순위 순서로 한 대씩 planner(NavigateToPose)로 새 슬롯에 재배치 후 재개.
    bool reform_in_progress_ = false;   // 리더: 전환 재정렬 대기 중
    bool resume_after_reform_ = false;  // 리더: 재정렬 후 재개 — form-up 배리어 skip(1회성)
    bool reforming_ = false;            // 팔로워: 새 슬롯으로 재배치 중
    bool reform_done_ = false;          // 팔로워: 새 슬롯 도착(재개 신호 대기, latched)
    bool reform_nav_inflight_ = false;  // NavigateToPose goal 진행 중
    double reform_t0_ = 0.0;            // 재정렬 시작시각(타임아웃 기준)
    double reform_retry_t_ = 0.0;       // nav 재시도 쿨다운 기준
    double last_slot_cross_ = 0.0;      // 직전 슬롯 cross(횡변경 감지용)
    double reform_timeout_s_ = 20.0;    // 한 대당 배치 제한시간(직렬이라 충돌 없음). 못 가면 done→다음.
                                        // 35s 로 늘렸더니 reform 정지가 너무 길어(직렬 ~52s) 완주
                                        // 실패. FixA(경로헤딩 간격제어)가 재개 후 압축을 매끄럽게
                                        // 회복하므로 reform 이 완벽할 필요 없음 → 20s 로 원복.
    GoalHandleNav::SharedPtr active_reform_goal_;
    rclcpp::Publisher<geometry_msgs::msg::PointStamped>::SharedPtr reform_ready_pub_;
    rclcpp::Subscription<geometry_msgs::msg::PointStamped>::SharedPtr reform_ready_sub_;
    std::map<int, double> reform_done_peers_;   // id → done(0/1)
    std::map<int, double> reform_seen_;         // id → 마지막 수신시각

    // ---- START form-up: 후방슬롯(column/wedge/diamond) 절대슬롯 선배치 ----
    // abreast(횡만, along=0)는 기존 offset-path form-up 으로 충분(검증됨). 후방슬롯(along<0)은
    // 정지상태서 offset-path 시작점이 리더 뒤(후진방향)라 FollowPath 로 도달 불가 → 절대슬롯
    // NavigateToPose 로 선(先)배치. 리더는 전원 도착까지 진짜 정지(강제출발 발산 제거).
    bool formup_reposition_ = false;    // 팔로워: START 후방슬롯 절대배치 대기 중
    double formup_anchor_x_ = 0.0, formup_anchor_y_ = 0.0;   // form-up 슬롯 기준 리더 START위치
    bool formup_anchor_valid_ = false;  // latch됨(리더 회전 drift로 슬롯 이동 방지)
    bool formup_backed_ = false;        // P0(횡단 전 리더 뒤 안전깊이 후퇴) 완료 latch
    bool formup_phase1_done_ = false;   // 3단계 P1(측면으로 빠짐) 완료 latch
    bool formup_staged_ = false;        // 3단계 P2(깊이 변경) 완료 latch(→final)
    bool leader_formup_cancelled_ = false;  // 리더: form-up 제자리회전 위해 FollowPath 취소함
    bool formup_nav_inflight_ = false;  // form-up NavigateToPose 진행 중
    double formup_retry_t_ = 0.0;       // form-up nav 재시도 쿨다운
    GoalHandleNav::SharedPtr active_formup_goal_;
    double form_up_warn_s_ = 10.0;      // 이 시간 넘게 대기하면 경고(강제출발 안 함)
    double follower_lost_s_ = 3.0;      // ★하트비트(/swarm/robot_world_pos) 무신호 이 초과 = 링크다운
    double formup_grace_s_ = 20.0;      // ★form-up 중 링크다운 팔로워 재접속을 기다리는 유예(그 후 게이트 제외)
    double form_up_last_warn_ = 0.0;
    double formup_timeout_s_ = 0.0;     // ★0=강제출발 비활성(대형완성까지 대기, 사용자 요청). >0이면 그 시간 후 강제출발
    // 차량 footprint 외접반경 ≈ hypot(0.66,0.43)=0.79m. 두 차량 중심거리 < 2·0.79=1.58m 면
    // 물리 충돌. 직렬화 교차판정 클리어런스 = 2·외접반경 + 마진. nav2 는 팀원을 장애물로
    // 안 봐서(planner 가 상대를 뚫고 계획) 직렬화가 유일한 충돌 안전장치 → 폭 반영 필수.
    double formup_clearance_m_ = 2.5;   // 직렬화 교차 클리어런스(엄격=한 대씩 → 뭉침 방지)
    double formup_hardstop_m_ = 1.5;    // 정렬 중 팀원과 이 거리 이내면 즉시 정지(물리접촉 직전)
    // ---- 제곱거리 최적 슬롯 배정(헝가리안 대체: ≤5대 순열 전수) ----
    // 각 로봇이 동일 입력(공유 world_pos_ + 리더프레임)으로 같은 배정 산출 → 분산 일관.
    // form-up/대형변경 시 1회 latch(매틱 재계산=rank 채터 방지). 최적=최소동선·비교차.
    std::map<int, int> assignment_;     // robot_id → 배정 slot rank (리더=0, 팔로워 1..N-1)
    bool assignment_valid_ = false;

    // ---- 리더-기준 속도 동기화 (offset-path 대형) ----
    // 각 로봇이 자기 경로의 arc-length 진행도를 공유 → 팔로워는 (리더arc + along*spacing)
    // 목표로 nav2 speed_limit 조절(앞서면 감속/대기). 리더는 가장 뒤처진 팔로워만큼 거버닝.
    rclcpp::Publisher<geometry_msgs::msg::PointStamped>::SharedPtr progress_pub_;
    rclcpp::Subscription<geometry_msgs::msg::PointStamped>::SharedPtr progress_sub_;
    rclcpp::Publisher<nav2_msgs::msg::SpeedLimit>::SharedPtr speed_limit_pub_;
    rclcpp::TimerBase::SharedPtr sync_timer_;
    std::vector<double> cum_ref_;              // 기준(미오프셋) 경로 누적 arc(m) — 공통 진행도 기준
    std::vector<double> my_cum_;               // 내 오프셋 경로 누적 arc(m) — 세그먼트 fraction 용
    double own_arc_ = 0.0;                      // 내 현재 경로 진행 arc-length(m)
    double leader_arc_ = 0.0;                   // 리더 진행 arc-length(m)
    bool leader_arc_valid_ = false;
    std::map<int, std::pair<double, double>> peer_progress_;  // id → (arc_m, along_target_m)
    std::map<int, double> peer_seen_;                          // id → 마지막 수신시각(sec)
    double base_speed_mps_ = 1.0;              // 대형 기준 순항속도(nav2 max_vel 와 정합)
    double sync_gain_ = 1.2;                    // arc 오차→감속 게인
    double sync_tol_m_ = 0.3;                   // 이 이내 오차는 무시(채터 방지)
    double leader_pace_floor_ = 0.3;            // 리더 페이싱 최소 전진속도(급정지 완화)
    // ★IDM(Intelligent Driver Model) 종방향 car-following 파라미터(P-IDM 논문 참고).
    double idm_s0_ = 2.0;            // 정지 시 최소 간격 s0(m)
    double idm_time_headway_ = 0.6; // time headway T(s)
    double idm_accel_ = 1.2;        // 최대 가속 a_max(m/s²)
    double idm_decel_ = 1.5;        // 편안한 감속 b(m/s²)
    double idm_vdes_mult_ = 1.5;    // 자유주행 목표속도 = base×이값(따라잡기 여유)
    double column_pace_deadband_m_ = 5.0;       // 종대 governor: 이 lag 까지는 감속 0(간격 여유)
    double column_pace_slow_span_m_ = 8.0;      // 데드밴드 초과분을 이 폭에 걸쳐 비례감속
    double column_pace_floor_frac_ = 0.5;       // 종대 리더 최소속도 = base×이값(크롤 금지·계속 전진)
    // 리더 장애물 통과: 이때 리더는 COLUMN 결정적 governor(하한 0.15 크롤·slot+3m 정지)를 써
    //   후미를 확실히 기다리며 trace 를 이어간다(narrow gap 잼 방지).
    bool obstacle_pass_active_ = false;         // 리더: 지금 장애물 통과(종대 governor) 모드
    bool obstacle_pass_engaged_ = false;        // 래치: 스워브 시작~팔로워 재편성 완료까지
    double obstacle_pass_seen_t_ = -1e9;        // 마지막으로 스워브/우회를 본 시각
    double obstacle_pass_hold_s_ = 4.0;         // 스워브 종료 후 최소 이 시간 유지
    double obstacle_pass_reform_gap_ = 2.0;     // 전 팔로워 슬롯+이 거리 이내로 붙으면 통과 종료

    // 내 2D 편대 슬롯(cross 횡, along 종) — (모드,번호)→기하학→rank로 결정. 단일 진입점.
    // formation_type_ 은 모드(RECON/PROTECT/ASSAULT) → geometryForMode 로 기하학 변환.
    FormationSlot mySlot() const
    {
        const int rank = assignedRank();   // 제곱거리 최적 배정 rank(없으면 static)
        const uint8_t geom = geometryForMode(formation_type_, formation_number_);
        return formationSlotFor(geom, rank);
    }
    // 횡 오프셋(경로 평행이동용). 종(along)은 속도동기화에서 사용.
    int formationSlotOffset() const
    {
        return static_cast<int>(std::lround(mySlot().cross));
    }

    // 코너 곡률 피드포워드 배율 = 1 − n·κ.  n=좌+ 횡오프셋(m), κ=좌+ 리더경로 곡률(1/m).
    // 바깥 로봇(n이 회전중심 반대쪽)은 >1로 선제 가속, 안쪽은 <1로 감속 → 코너 동시 탈출.
    // 리더경로(leader_ref_path_)에서 내 현재 진행분율(own_arc_) 위치의 곡률을 geometry로 산출.
    // 균일보간(0.5m)으로 국소 segment길이비는 평탄화되므로, 일정 arc창(±2m)의 방향변화로 계산.
    double corner_ff_mult() const
    {
        if (is_leader_ || !corner_ff_enabled_) return 1.0;
        const double n = mySlot().cross * formation_lateral_spacing_m_;   // 좌+ 횡오프셋(m)
        if (std::abs(n) < 1e-3) return 1.0;   // 컬럼(cross=0): 피드포워드 불필요
        const auto &P = leader_ref_path_.poses;
        const int N = static_cast<int>(P.size());
        if (N < 9) return 1.0;
        const int w = 4;   // ±2m 창(경로 0.5m 간격)
        int ci = static_cast<int>(std::lround(own_arc_ * (N - 1)));
        ci = std::clamp(ci, w, N - 1 - w);
        const double v1x = P[ci].pose.position.x - P[ci - w].pose.position.x;
        const double v1y = P[ci].pose.position.y - P[ci - w].pose.position.y;
        const double v2x = P[ci + w].pose.position.x - P[ci].pose.position.x;
        const double v2y = P[ci + w].pose.position.y - P[ci].pose.position.y;
        const double h1 = std::hypot(v1x, v1y), h2 = std::hypot(v2x, v2y);
        if (h1 < 1e-3 || h2 < 1e-3) return 1.0;
        const double sn = std::clamp((v1x * v2y - v1y * v2x) / (h1 * h2), -1.0, 1.0);
        const double kappa = std::asin(sn) / (0.5 * (h1 + h2));   // 좌+ 곡률(1/m)
        return std::clamp(1.0 - n * kappa, 0.5, 1.8);
    }

    // 기준경로(공통)를 자기 슬롯만큼 횡방향으로 평행이동한 "내 경로" 를 만든다.
    // 각 구간 진행방위(bearing)의 수직방향으로 lateral_m 만큼 이동.
    // 미터 → 위경도 변환은 로컬 평면 근사(WGS-84).
    std::vector<std::pair<double, double>>
    applyFormationOffset(const std::vector<std::pair<double, double>> &pts) const
    {
        const FormationSlot slot = mySlot();
        const double cross_m = slot.cross * formation_lateral_spacing_m_;   // 횡(좌+)
        const double along_m = slot.along * formation_lateral_spacing_m_;   // 종(전+, 보통 −=뒤)
        if (!formation_enabled_ ||
            (std::abs(cross_m) < 1e-6 && std::abs(along_m) < 1e-6) || pts.empty()) {
            return pts;  // 리더이거나 오프셋 0 → 기준경로 그대로
        }
        constexpr double kM = 111320.0;

        // 두 wp 사이 동/북(m) 단위벡터.
        auto unit_en = [&](std::size_t a, std::size_t b, double lat_ref,
                           double *ue, double *un) -> bool {
            const double dn = (pts[b].first - pts[a].first) * kM;
            const double de = (pts[b].second - pts[a].second) * kM *
                              std::cos(lat_ref * M_PI / 180.0);
            const double L = std::hypot(de, dn);
            if (L < 1e-6) return false;
            *ue = de / L; *un = dn / L; return true;
        };

        std::vector<std::pair<double, double>> out;
        out.reserve(pts.size());
        for (std::size_t i = 0; i < pts.size(); ++i) {
            const double lat_ref = pts[i].first;
            // 진행방향 = 들어오는 구간(i-1→i)과 나가는 구간(i→i+1)의 평균(각 bisector).
            // 코너에서 오프셋 경로가 올바른 평행곡선이 되도록(한쪽 구간만 쓰면 코너서 틀어짐).
            double ie = 0, in = 0, oe = 0, on = 0;
            const bool has_in = (i > 0) && unit_en(i - 1, i, lat_ref, &ie, &in);
            const bool has_out = (i + 1 < pts.size()) && unit_en(i, i + 1, lat_ref, &oe, &on);
            double de, dn;
            if (has_in && has_out) { de = ie + oe; dn = in + on; }
            else if (has_out)      { de = oe; dn = on; }
            else if (has_in)       { de = ie; dn = in; }
            else                   { de = 1.0; dn = 0.0; }   // 단일점 fallback: 동쪽
            const double L = std::hypot(de, dn);
            if (L > 1e-6) { de /= L; dn /= L; }
            // 좌측수직(-dn,de)·cross + 전방(de,dn)·along → 2D 슬롯을 경로에 직접 반영.
            // 종(along)도 기하적으로 넣어 시작·주행·도착 내내 대형(간격) 유지 + 팔로워
            // 도착지점이 자기 슬롯이 됨(리더 골에 안 모임).
            const double off_e = (-dn) * cross_m + de * along_m;
            const double off_n = (de) * cross_m + dn * along_m;
            const double dlat = off_n / kM;
            const double dlon = off_e / (kM * std::cos(lat_ref * M_PI / 180.0));
            out.emplace_back(pts[i].first + dlat, pts[i].second + dlon);
        }
        return out;
    }

    // 기준경로를 최대 seg_max_m 간격으로 보간(중간 웨이포인트 삽입).
    // 이유: nav2 NavigateToPose 는 '현재(드리프트된) 위치 → 먼 목표' 로 경로를 새로
    // 계획하므로, 출발 시 스키드-스티어 스크럽으로 0.4m 옆으로 밀리면 그 드리프트가
    // 목표 직전까지 유지된다(긴 대각선 경로). 중간 웨이포인트를 촘촘히 깔면 RPP 가
    // 명목 오프셋 직선으로 ~seg_max_m 안에 복귀해 편대 간격이 대칭으로 유지된다.
    // 직선구간엔 동일선상 점만 추가되어 코너 배리어를 만들지 않는다.
    std::vector<std::pair<double, double>>
    densify_path(const std::vector<std::pair<double, double>> &pts,
                 double seg_max_m) const
    {
        if (pts.size() < 2 || seg_max_m <= 0.1) return pts;
        constexpr double kM = 111320.0;
        std::vector<std::pair<double, double>> out;
        out.reserve(pts.size() * 2);
        out.push_back(pts.front());
        for (std::size_t i = 1; i < pts.size(); ++i) {
            const double lat_ref = pts[i - 1].first;
            const double dn = (pts[i].first - pts[i - 1].first) * kM;
            const double de = (pts[i].second - pts[i - 1].second) * kM *
                              std::cos(lat_ref * M_PI / 180.0);
            const double seg = std::hypot(de, dn);
            const int n = static_cast<int>(std::floor(seg / seg_max_m));
            for (int k = 1; k <= n; ++k) {
                const double f = static_cast<double>(k) / (n + 1);
                out.emplace_back(pts[i - 1].first + (pts[i].first - pts[i - 1].first) * f,
                                 pts[i - 1].second + (pts[i].second - pts[i - 1].second) * f);
            }
            out.push_back(pts[i]);
        }
        return out;
    }

    // ---- 리더-기준 속도 동기화 (offset-path 대형) ----
    void on_progress(const geometry_msgs::msg::PointStamped::SharedPtr m)
    {
        const int id = std::atoi(m->header.frame_id.c_str());
        peer_progress_[id] = {m->point.x, m->point.y};   // (arc_m, along_target_m)
        peer_seen_[id] = this->now().seconds();
        peer_corner_dist_[id] = m->point.z;              // 다음코너까지 남은거리(시간동기)
        peer_corner_seen_[id] = this->now().seconds();
        if (id == static_cast<int>(leader_robot_id_)) {
            leader_arc_ = m->point.x;
            leader_arc_valid_ = true;
        }
    }

    // 내 진행도를 '기준(미오프셋) 경로'의 arc-length(m)로 환산 — 코너에서 바깥/안쪽
    // 오프셋 경로 길이가 달라도 같은 웨이포인트-비율이면 같은 진행도가 되도록(전원 같은
    // 수직선 = 나란히 유지). 내 오프셋 세그먼트로 fraction 만 구해 기준 세그먼트에 투영.
    double compute_own_arc() const
    {
        if (!active_) {
            return 0.0;
        }
        // FollowPath 모드: 진행도 = 경로 fraction(0~1). 절대 arc 가 아니라 fraction 으로
        // 동기화하는 이유 — 원호 코너에서 안/바깥 오프셋 경로 길이가 다르지만((r∓d)θ),
        // 같은 fraction f 면 둘 다 각도 fθ = 같은 방사선 = 같은 횡대형선. 직선에서도
        // 길이 동일하므로 fraction 비교가 곧 거리 비교(정합). sync_tick 에서 대표길이로
        // 다시 m 로 환산해 속도게인(m 단위 tol/gain)과 정합시킨다.
        if (control_mode_ == "follow_path") {
            // 분모는 원본 길이(불변). 우회로 현재 세그먼트(suffix)가 짧아져도 fraction 은
            // 원본 스케일 유지 → 우회/재개 후에도 리더·팔로워가 같은 척도로 비교. 우회 중엔
            // FollowPath 취소로 fp_dist_to_goal_ 가 동결 → fraction 낮게 고정 → 리더가 대기.
            if (fp_original_length_ > 1e-3) {
                return std::clamp((fp_original_length_ - fp_dist_to_goal_) / fp_original_length_,
                                  0.0, 1.0);
            }
            return 0.0;
        }
        if (cum_ref_.empty()) {
            return 0.0;
        }
        const std::size_t n = cum_ref_.size();
        const std::size_t k = active_index_;
        const double rem = static_cast<double>(distance_to_next_wp_m_);
        if (k >= n) {
            return cum_ref_.back();
        }
        if (k == 0) {
            return -rem;   // 첫 wp 접근구간(차등 위해 음수 허용)
        }
        const double seg_ref = cum_ref_[k] - cum_ref_[k - 1];
        const double my_seg = (k < my_cum_.size()) ? (my_cum_[k] - my_cum_[k - 1]) : seg_ref;
        const double frac =
            (my_seg > 1e-3) ? std::clamp(1.0 - rem / my_seg, 0.0, 1.0) : 1.0;
        return cum_ref_[k - 1] + seg_ref * frac;
    }

    // 다음 코너까지 남은 거리(m). 코너 없으면 목표까지. (시간동기 코너 동시탈출용 d_i)
    double remaining_to_next_corner() const
    {
        if (control_mode_ != "follow_path" || fp_original_length_ < 1e-3) {
            return static_cast<double>(distance_to_goal_m_);
        }
        const double progress = fp_original_length_ - fp_dist_to_goal_;
        for (const double ca : corner_arcs_) {
            if (ca > progress + 0.5) return std::max(0.1, ca - progress);
        }
        return std::max(0.1, fp_dist_to_goal_);
    }

    // arc 오차(앞섬 정도) → nav2 speed_limit(m/s). 0 은 nav2 에서 'no limit' 라
    // 절대 0 으로 내리지 않고 소량(crawl)으로 하한 → 앞선 로봇이 사실상 대기.
    double speed_from_excess(double excess) const
    {
        double frac = 1.0;
        if (excess > sync_tol_m_) {
            frac = std::clamp(1.0 - sync_gain_ * (excess - sync_tol_m_), 0.0, 1.0);
        }
        return std::max(0.06, base_speed_mps_ * frac);
    }

    // FSM 작전명령 또는 조기종대 override 로 결정된 '유효 대형모드'를 적용한다. 대형이 바뀌면
    // 리더는 정지 후 전 팔로워 우선순위 재정렬 완료까지 대기하고, 팔로워는 새 슬롯 재배치
    // (NavigateToPose)에 진입한다. formation_sub_(작전명령)와 formation_override_sub_(리더
    // 장애물통과 브로드캐스트) + 리더 자체 래치가 공용 호출 → 검증된 reform 기계를 그대로 재사용.
    void apply_formation_mode(uint8_t t_mode)
    {
        const bool changed = (t_mode != formation_type_);
        formation_type_ = t_mode;
        if (!changed) { return; }
        assignment_valid_ = false;   // 새 대형 → 최적배정 재계산
        // ★rolling merge(사용자 지시): 주행 중 대형 변경 시 리더는 '정지하지 않고' 계속 주행,
        //   팔로워는 정지-재배치(후진) 없이 새 슬롯 오프셋 경로로 주행 중 lane-change 전환한다.
        //   종전엔 리더 정지+전 팔로워 재정렬 대기(reform_in_progress_)라, 팔로워가 정지한 리더
        //   뒤 깊은 슬롯(예: WEDGE −4m)으로 후진했음(실측 s2/s3 15.7→12.4m 후진). 리더가 계속
        //   전진하면 슬롯이 앞으로 이동해 팔로워가 전진으로 falling-in → 후진 없음.
        if (is_leader_ && active_ && !formation_followers_.empty()) {
            RCLCPP_INFO(this->get_logger(),
                        "[Formation] 대형 변경 — rolling merge(리더 계속 주행, 팔로워 주행중 합류)");
            // 리더는 현재 FollowPath 그대로 유지(정지·held_path·reform_in_progress 없음).
        }
        // 팔로워(FollowPath): 새 슬롯 오프셋 경로로 재생성해 주행 중 전환. 횡(cross) 변경은 RPP
        // lookahead 로 대각 lane-change 수렴, 종(along) 변경은 속도제어(IDM/페이싱)가 슬롯 깊이로
        // 정착(리더 전진에 실려 후진 없이). reforming_(정지-후진 재배치) 진입 안 함.
        else if (!is_leader_ && active_ &&
                 control_mode_ == "follow_path" && formation_enabled_) {
            const double new_cross = mySlot().cross * formation_lateral_spacing_m_;
            RCLCPP_INFO(this->get_logger(),
                "[Formation] 대형 변경 — 주행 중 새 슬롯 전환(cross %.1f→%.1f, rolling)",
                last_slot_cross_, new_cross);
            last_slot_cross_ = new_cross;
            if (active_fp_goal_) {            // 옛 슬롯 경로 취소(새 오프셋 경로로 교체)
                ignore_next_fp_result_ = true;
                cancel_goal_safe(follow_path_client_, active_fp_goal_);
                active_fp_goal_.reset();
            }
            if (leader_ref_valid_) { build_follower_path_from_leader(); }
            else { build_and_send_follow_path(); }
        }
    }

    // 조기종대 override 상태를 /swarm/formation_override 로 발행(리더 전용). point.x=활성,
    // point.y=강제 모드(COLUMN), frame_id=리더 id. TRANSIENT_LOCAL 이라 늦은매칭도 포착.
    void publish_formation_override(bool t_active)
    {
        if (!formation_override_pub_) { return; }
        geometry_msgs::msg::PointStamped m;
        m.header.stamp = this->now();
        m.header.frame_id = std::to_string(leader_robot_id_);
        m.point.x = t_active ? 1.0 : 0.0;
        m.point.y = static_cast<double>(combat_robot_nav2::kEarlyColumnMode);
        formation_override_pub_->publish(m);
        last_override_pub_t_ = this->now().seconds();
    }

    // 100ms: 내 진행도 공유 + 리더 기준 속도(nav2 speed_limit) 조절.
    //  · 팔로워: 목표 arc = 리더arc + along*spacing. 내가 앞서면(err>0) 감속/대기.
    //  · 리더  : 가장 뒤처진 팔로워만큼 거버닝(대형 깨지지 않게 리더가 기다림).
    void sync_tick()
    {
        if (!speed_limit_pub_ || !progress_pub_) {
            return;
        }
        // ★배리어 주기 재평가: 링크다운 팔로워는 ready/reform 이벤트를 더 못 보내므로
        //   이벤트 구동만으론 게이트가 영원히 안 풀림 → 하트비트 게이트를 주기적으로 재평가.
        if (is_leader_ && waiting_for_formation_) maybe_start_after_formation();
        // ★리더 장애물 통과 판정(사용자 R2): 스워브/우회 시작 시 래치 → COLUMN 결정적
        //   governor 로 전환해 후미를 확실히 기다린다. 종료 조건은 "리더가 스워브를 멈춘 지
        //   hold 초 경과" + "전 팔로워가 자기 슬롯+reform_gap 이내로 붙음"(=재편성 완료).
        //   ★리더 자신의 스워브 종료(4s)만으로 풀면, 갭을 뒤늦게 통과 중인 후미를 0.3 크롤로
        //   버리는 문제(실측) → 반드시 팔로워 캐치업까지 유지.
        if (is_leader_) {
            const double tnow_op = this->now().seconds();
            if (fp_swerving_ || fp_detour_active_) {
                obstacle_pass_seen_t_ = tnow_op;
                obstacle_pass_engaged_ = true;
            }
            // 전 팔로워 최악 지연(자기 종대 슬롯 대비 뒤처짐, m). 미확인 팔로워가 있으면
            // 1e9(=미재편성). 반응(스워브) 통과 종료 판정 + 조기종대 override 해제에 공용.
            double worst_lag = 0.0;
            const auto me_w = world_pos_.find(static_cast<int>(robot_id_));
            if (me_w != world_pos_.end()) {
                double cy, sy; pathForwardDir(cy, sy);
                const double now_ws = tnow_op;
                for (const int64_t fid : formation_followers_) {
                    const auto it = world_pos_.find(static_cast<int>(fid));
                    const auto st = world_seen_.find(static_cast<int>(fid));
                    if (it == world_pos_.end() || st == world_seen_.end() ||
                        now_ws - st->second > 2.0) { worst_lag = 1e9; break; }
                    const double behind =
                        -(cy * (it->second.first - me_w->second.first) +
                          sy * (it->second.second - me_w->second.second));
                    const double slot_m = 1.5 *
                        static_cast<double>(fid - leader_robot_id_) *
                        formation_lateral_spacing_m_;
                    worst_lag = std::max(worst_lag, behind - slot_m);
                }
            } else {
                worst_lag = 1e9;
            }
            if (obstacle_pass_engaged_) {
                const bool recent = (tnow_op - obstacle_pass_seen_t_) < obstacle_pass_hold_s_;
                if (!recent && worst_lag < obstacle_pass_reform_gap_) {
                    obstacle_pass_engaged_ = false;   // 스워브 끝 + 전원 재편성 → 통과 종료
                }
            }

            // ★조기종대(early-column): 전방 장애물을 lookahead(>스워브 트리거) 안에서 감지하면
            //   스워브 전에 미리 /swarm/obstacle_pass 로 "장애물 통과 개시"를 브로드캐스트한다.
            //   → 팔로워가 개방공간에서 곧바로 리더 궤적 추종 종대(obs_convoy)로 전환하고,
            //   convoy_turn_ok(순번 롤링 머지)로 한 대씩 리더 뒤에 합류(후진 없음). 리더는
            //   정지하지 않고 convoy_column_leader governor 로 전진하며 후미를 기다린다.
            //   해제(브로드캐스트 OFF) = 전방 장애물 없음 + 전 팔로워 캐치업 + 반응 스워브 종료.
            //   ★팔로워의 실제 대형복원은 각자 own_s 가 리더 우회 far-edge 를 지난 뒤(개별) —
            //   리더 중앙 해제가 아니라 후미가 스스로 통과할 때까지 종대 유지(후미 안버림).
            // ★모드 분기(사용자 지시): 종대 전환은 "양쪽 막힌 넓은 장애물/통로"에만. 단일
            //   소형 박스(실폭<wide_obstacle_span_m)는 종대화하지 않고 각 로봇이 개별 회피
            //   (nav2+swerve)로 빠르게 지나간다 — convoy governor(0.15 크롤)를 안 걸어 느려지지
            //   않게. 넓은 장애물만 조기 발동해 리더 뒤 종대로 순차 통과.
            const bool wide_ahead =
                (obstacle_wide_span_ > wide_obstacle_span_m_) &&
                (obstacle_dist_ > 0.0 && obstacle_dist_ < obstacle_pass_lookahead_m_);
            const double eff_obs_dist = wide_ahead ? obstacle_dist_ : -1.0;
            const bool nothing_ahead = !wide_ahead;
            const bool leader_clear =
                nothing_ahead && (worst_lag < obstacle_pass_reform_gap_) &&
                !obstacle_pass_engaged_;
            const bool ov = early_column_latch_.update(
                eff_obs_dist, obstacle_pass_lookahead_m_,
                obstacle_persist_ticks_, leader_clear);
            // 리더도 조기 발동 시 governor 를 미리 켠다(반응 스워브 전부터 후미 대기 전진).
            obstacle_pass_active_ = obstacle_pass_engaged_ || ov;
            if (ov != leader_obstacle_pass_active_) {
                leader_obstacle_pass_active_ = ov;
                RCLCPP_INFO(this->get_logger(),
                    "[조기종대] 장애물통과 %s (obstacle_dist=%.1f worst_lag=%.1f)",
                    ov ? "개시→팔로워 종대합류" : "종료→대형복원", obstacle_dist_, worst_lag);
                publish_formation_override(ov);
            } else if (ov && (tnow_op - last_override_pub_t_) > 0.5) {
                publish_formation_override(true);   // 활성 동안 0.5s 주기 재발행(늦은매칭 대비)
            }
        }
        // 대형전환 재정렬: reform_ready 발행 + (재정렬 중이면) 직렬 NavigateToPose. 재정렬 중인
        // 팔로워는 여기서 정지 유지하고 나머지 속도제어 skip.
        if (reform_tick()) return;
        // ★대형 단위 코히어 통과: 리더 회피 감지 시 팔로워를 임시 종대(리더궤적 추종)로
        //   전환/복원. convoy_follower() 가 이 상태를 반영해 이후 로직(궤적추종·자체회피
        //   억제·간격 governor)이 그대로 재사용됨.
        update_obstacle_convoy();
        // 우회(NavigateToPose) 행 워치독: rejoin 이 장애물 내부면 planner 가 BT 안에서
        // 무한 재계획(결과 콜백 없음 → 에스컬레이션 불가, run17 리더 6분 행 실측).
        // 20s 넘으면 취소하고 실패 누적 → 더 먼 rejoin 으로 재시도.
        if (fp_detour_active_ &&
            this->now().seconds() - detour_start_t_ > 20.0) {
            RCLCPP_WARN(this->get_logger(),
                        "우회 20s 초과(행 의심) — 취소·rejoin 연장 재시도");
            fp_detour_active_ = false;
            if (active_detour_goal_) {
                cancel_goal_safe(nav_client_, active_detour_goal_);
                active_detour_goal_.reset();
            }
            ++detour_fail_streak_;
            trigger_detour(escalated_rejoin_index());
            return;
        }
        // ★goal 부재 안전망: 워치독-결과콜백 인터리브 등으로 goal 이 사라진 채 방치되는
        //   모든 경로의 최종 안전망(run20 실측: 우회 성공했지만 fp_detour_active_ 가
        //   워치독에 선클리어돼 재개 스킵 → 리더 영구 정지). 5s 이상 goal 없으면 재전송.
        //   컨보이 팔로워는 '슬롯 대기'가 정상 상태라 제외(소진 폴백이 담당).
        {
            const double now_g = this->now().seconds();
            if (active_ && control_mode_ == "follow_path" && !forming_up_ &&
                !reforming_ && !fp_detour_active_ && !gap_hold_ &&
                wedge_reverse_ticks_ == 0 && !active_fp_goal_ &&
                !active_detour_goal_ && !convoy_follower()) {
                if (goalless_since_ < 0.0) {
                    goalless_since_ = now_g;
                } else if (now_g - goalless_since_ > 5.0 &&
                           now_g - last_safety_resend_ > 5.0) {
                    // ★쿨다운 필수: async goal-response 지연으로 재전송 직후에도 goal 이
                    //   아직 안 붙어 매 틱 재발동(run25 1579회 폭주 실측). 재전송 간 최소 5s.
                    last_safety_resend_ = now_g;
                    goalless_since_ = now_g;
                    RCLCPP_WARN(this->get_logger(),
                                "[안전망] 활성인데 goal 부재 5s — 경로 재전송");
                    build_and_send_follow_path();
                }
            } else {
                goalless_since_ = -1.0;
            }
        }
        // 웨지(접촉 밀착) 복구: 2s 직선 후진 후 경로 재전송(컨보이면 리더 궤적으로).
        if (wedge_reverse_ticks_ > 0) {
            // 후진도 막힘(뒤에 차/장애물 밀착 — run11 리더가 s2 범퍼락에 무한 후진 공회전)
            // → 1s 시점 이동량 0.1m 미만이면 후진 포기, 그 자리서 경로 재개.
            const bool half = (wedge_reverse_ticks_ == 10);
            const double moved = std::hypot(own_map_x_ - wedge_rev_x_,
                                            own_map_y_ - wedge_rev_y_);
            if (half && moved < 0.1) {
                RCLCPP_WARN(this->get_logger(),
                            "[웨지] 후진도 막힘(이동 %.2fm) — 후진 포기, 경로 재개", moved);
                wedge_reverse_ticks_ = 1;   // 아래서 0 되며 재개
            }
            geometry_msgs::msg::Twist rv;
            rv.linear.x = -0.3;
            if (repos_cmd_pub_) repos_cmd_pub_->publish(rv);
            if (--wedge_reverse_ticks_ == 0) {
                RCLCPP_INFO(this->get_logger(), "[웨지] 후진 완료 — 경로 재개");
                if (convoy_follower() && leader_trace_.poses.size() >= 4) {
                    send_convoy_path();
                } else if (active_) {
                    build_and_send_follow_path();
                }
            }
            return;
        }
        own_arc_ = compute_own_arc();
        // along(종)은 이제 경로 기하에 반영됨 → 동기화는 '동일 기준-진행도'(횡 정렬)만 담당.
        const double my_along_m = 0.0;

        const double d_corner = remaining_to_next_corner();   // 다음 코너까지 남은거리(시간동기)
        geometry_msgs::msg::PointStamped p;
        p.header.stamp = this->now();
        p.header.frame_id = std::to_string(robot_id_);
        // 리더가 form-up 중이면 progress arc=0 발행. 리더가 회전 정렬 중 살짝 전진(drift)해도
        // 팔로워가 leader_arc_>0.01 로 '리더 출발'로 오인해 form-up 탈출·질주하는 것 방지.
        // form-up 완료(forming_up_=false) 후에만 실제 arc 발행 → 팔로워 동시 출발.
        p.point.x = (is_leader_ && forming_up_) ? 0.0 : own_arc_;
        p.point.y = my_along_m;
        p.point.z = d_corner;
        progress_pub_->publish(p);

        record_leader_trace();   // 리더: breadcrumb 기록/발행(컨보이용)
        convoy_tick();           // COLUMN 팔로워: 리더 궤적 추종 경로 갱신

        if (swerve_state_pub_) {   // 스워브 상태 공유(우선순위 양보용)
            geometry_msgs::msg::PointStamped sw;
            sw.header.stamp = this->now();
            sw.header.frame_id = std::to_string(robot_id_);
            // 기동중 = 스워브 or planner 우회(컨보이 팔로워가 리더 기동존 밖에서 대기하는 근거)
            sw.point.y = (fp_swerving_ || fp_detour_active_) ? 1.0 : 0.0;
            sw.point.z = own_arc_;
            swerve_state_pub_->publish(sw);
        }

        // 배리어용: 내가 도달한 wp 인덱스 공유.
        if (ready_pub_) {
            geometry_msgs::msg::PointStamped r;
            r.header.stamp = this->now();
            r.header.frame_id = std::to_string(robot_id_);
            r.point.x = static_cast<double>(reached_index_);
            ready_pub_->publish(r);
        }
        // 대형-인지 마스킹 플래그: form-up 완료(첫 배리어 통과=reached_index_>=0) 후
        // 정상 대형 주행일 때만 마스킹 ON. form-up/배리어 대기 중엔 OFF(회피).
        if (mask_pub_) {
            std_msgs::msg::Bool mb;
            // FollowPath 모드: 미션 중이면 항상 팀원 마스킹 ON(코너서 서로 오감지 방지).
            // 그 외(waypoint): form-up 첫 배리어 통과 후 마스킹.
            // 단, 정렬 재배치 중(formup_reposition_)엔 마스킹 OFF → costmap 이 팀원을 장애물로
            // 인식 → NavigateToPose planner 가 서로 우회·거리유지(사용자: 인식하고 거리유지).
            mb.data = active_ && !barrier_waiting_ && !formup_reposition_ &&
                      (control_mode_ == "follow_path" ? true : reached_index_ >= 0);
            mask_pub_->publish(mb);
        }

        if (!active_) {
            return;   // 미션 중 아니면 속도제한 안 걺(nav2 기본속도)
        }
        // 우회 중(NavigateToPose)엔 자기 속도를 묶지 않는다 — planner 우회를 최고속으로
        // 끝내 빨리 라인 복귀. (리더가 이 로봇을 기다리는 건 frozen-low fraction 으로 처리)
        if (fp_detour_active_) {
            nav2_msgs::msg::SpeedLimit sl;
            sl.header.stamp = this->now();
            sl.percentage = false;
            sl.speed_limit = base_speed_mps_;
            speed_limit_pub_->publish(sl);
            return;
        }
        // 출발 form-up: 전원 자기 슬롯(경로 시작점) 도착 후 동시 출발.
        //  · 팔로워: 슬롯 도착하면 ready 발행 + 정지 대기(리더 출발 전까지). 미도착이면 슬롯으로 주행.
        //  · 리더: 전 팔로워 ready 까지 정지 대기 → 출발. (타임아웃 시 강제 출발=데드락 방지)
        if (forming_up_ && control_mode_ == "follow_path") {
            if (!assignment_valid_) computeAssignment();   // world_pos 준비되면 lazy 최적배정
            const double now_f = this->now().seconds();
            // 슬롯 도착 판정: 후방슬롯(formup_reposition_)은 절대슬롯 기준, 그 외(abreast 횡)는
            // 기존 offset-path 시작점 기준(검증된 경로 무손상).
            double sx = 0.0, sy = 0.0, syaw = 0.0;
            const bool slot_ok = formup_reposition_ &&
                                 compute_reform_slot(&sx, &sy, &syaw);
            bool at_slot;
            if (slot_ok && own_pose_valid_) {
                slot_x_ = sx; slot_y_ = sy; slot_valid_ = true;   // rviz 슬롯 마커용
                // ★ 도착 판정은 좁은 formup_arrive_tol_m_ 로(넓은 ready_tol 아님) → 팔로워가
                //   실제 슬롯에 도착해야 at_slot. 시작부터 근처라고 즉시 ready 되는 것 방지.
                at_slot = std::hypot(own_map_x_ - sx, own_map_y_ - sy) < formup_arrive_tol_m_;
            } else {
                at_slot = (!fp_last_path_.poses.empty() && own_pose_valid_) &&
                    std::hypot(own_map_x_ - fp_last_path_.poses[0].pose.position.x,
                               own_map_y_ - fp_last_path_.poses[0].pose.position.y) < formup_arrive_tol_m_;
            }
            // ★ 디바운스: 도착이 일정 틱(≈1s) 유지돼야 ready → 순간 스침으로 리더가 조기출발
            //   하는 것 방지. (static 경로엔 원래 디바운스가 없어 at_slot 즉시 발행됐음 = #2 버그)
            if (at_slot) { ++formup_ready_streak_; } else { formup_ready_streak_ = 0; }
            const bool slot_ready = (formup_ready_streak_ >= 10);
            if (formation_ready_pub_) {   // 팔로워 ready 공유
                std_msgs::msg::Bool rb; rb.data = slot_ready; formation_ready_pub_->publish(rb);
            }
            bool hold = false;
            if (is_leader_) {
                // ★ 타임아웃 강제출발 제거(사용자 요청) — 전원이 실제로 슬롯에 정렬(fresh ready)될
                //   때까지 리더는 진짜 정지. 조기출발→주행 중 캐치업(직선 흐트러짐) 방지. 장기지연은 경고만.
                if (all_followers_ready()) {
                    forming_up_ = false;   // 전원 정렬 → 출발
                    RCLCPP_INFO(this->get_logger(), "[form-up] 전원 정렬 완료 → 출발");
                    if (leader_formup_cancelled_) {   // form-up 위해 취소했던 FollowPath 재전송
                        leader_formup_cancelled_ = false;
                        build_and_send_follow_path();
                    }
                } else {
                    // ★ 리더 경로방향으로 제자리 회전(cmd_vel_nav, 전진 0 → 드리프트 없음 →
                    //   슬롯이 안 움직여 순차정렬 안정). RPP 회전은 forward속도에 묶여 기어가므로
                    //   FollowPath 를 취소하고 직접 cmd_vel_nav 로 순수 회전(ready 후 재전송).
                    if (!leader_formup_cancelled_ && active_fp_goal_) {
                        ignore_next_fp_result_ = true;
                        cancel_goal_safe(follow_path_client_, active_fp_goal_);
                        active_fp_goal_.reset();
                        leader_formup_cancelled_ = true;
                    }
                    // ★ 앵커 재방송(≈2Hz): 리더의 START시 단발 volatile 방송을 팔로워가 놓치거나,
                    //   팔로워가 자기 LOAD 로 anchor_valid_ 를 지운 뒤 뒤늦게 awaiting 에 들어가도
                    //   form-up 대기 중 프리즈된 앵커를 반복 방송해 확실히 수신시킴(앵커 타임아웃 방지).
                    if (anchor_valid_ && anchor_pub_ && now_f - last_anchor_bcast_ > 0.5) {
                        last_anchor_bcast_ = now_f;
                        geometry_msgs::msg::PointStamped a;
                        a.header.stamp = this->now();
                        a.header.frame_id = std::to_string(leader_robot_id_);
                        a.point.x = anchor_lat_; a.point.y = anchor_lon_;
                        anchor_pub_->publish(a);
                    }
                    double lh = 0.0;
                    const bool have = leaderOwnPathHeading(&lh);
                    auto wrap = [](double a){
                        while (a > M_PI) a -= 2.0 * M_PI;
                        while (a < -M_PI) a += 2.0 * M_PI; return a; };
                    if (have && repos_cmd_pub_) {
                        const double ye = wrap(lh - own_yaw_);
                        geometry_msgs::msg::Twist c;
                        if (std::abs(ye) > 0.08) c.angular.z = std::clamp(1.5 * ye, -0.8, 0.8);
                        repos_cmd_pub_->publish(c);   // 제자리 회전 또는 정지(정렬 완료)
                    }
                    if (now_f - form_up_t0_ > form_up_warn_s_ &&
                        now_f - form_up_last_warn_ > 5.0) {
                        form_up_last_warn_ = now_f;
                        RCLCPP_WARN(this->get_logger(),
                                    "[form-up] 팔로워 슬롯 정렬 대기 중(%.0fs)...",
                                    now_f - form_up_t0_);
                    }
                    return;   // 리더 회전/hold 처리 완료 — 아래 순항제어 안 감
                }
            } else {
                if (leader_arc_valid_ && leader_arc_ > 0.01) {
                    // 리더 출발 → form-up 종료. 후방슬롯이면 이제 offset 경로 빌드(슬롯에 이미 있음).
                    forming_up_ = false;
                    if (formup_reposition_) {
                        formup_reposition_ = false;
                        formup_backed_ = false; formup_staged_ = false; formup_phase1_done_ = false;
                        if (active_formup_goal_) {
                            cancel_goal_safe(nav_client_, active_formup_goal_);
                            active_formup_goal_.reset();
                        }
                        formup_nav_inflight_ = false;
                        if (leader_ref_valid_) build_follower_path_from_leader();
                    }
                } else if (formup_reposition_) {
                    // 후방슬롯 팔로워 — 직접 cmd_vel(cmd_vel_nav, 경합 없음) 후진 정렬.
                    // drive_reposition: 현재 횡 유지한 채 목표 깊이로 곧장 후진(앞차·리더 회피) →
                    // 깊이 도달 후 중앙선 슬라이드 + 헤딩 정렬 → 도달시 정지. s3 깊은슬롯도 곧장 후진.
                    // 예측교차 시 상위 우선순위 먼저 직렬(formup_can_proceed), 아니면 대기 정지.
                    geometry_msgs::msg::Twist zero;
                    // ★ 리더 회전완료 게이트: 슬롯 프레임은 리더 최종(미션) heading 기준인데,
                    //   리더가 아직 그 방향으로 회전 중이면 팔로워는 대기(정지). "슬롯 생성 후
                    //   리더 회전"(팔로워가 도는 리더 쫓아 가로지름)이 아니라 "리더 회전완료 →
                    //   팔로워 슬롯 접근" 순서로. 리더 정착 후 접근이라 가로지름/재추격 최소화.
                    double Lx_g, Ly_g, h_g;
                    bool leader_settled = true;
                    if (leader_pose_valid_ && slotFrame(&Lx_g, &Ly_g, &h_g)) {
                        leader_settled = std::abs(ang_diff(leader_yaw_, h_g)) < 0.20;  // ~11°
                    }
                    if (slot_ok && formup_can_proceed() && leader_settled) {
                        drive_reposition(sx, sy, syaw);
                    } else if (repos_cmd_pub_) {
                        repos_cmd_pub_->publish(zero);   // 차례/리더 회전 대기 — 정지
                    }
                    return;   // 정렬 컨트롤러가 cmd_vel_nav 점유 — 아래 순항제어 적용 안 함
                } else if (at_slot) {
                    hold = true;   // abreast(횡만) 슬롯 도착 — 리더 출발까지 정지
                }
            }
            if (hold) {
                nav2_msgs::msg::SpeedLimit sl;
                sl.header.stamp = this->now(); sl.percentage = false; sl.speed_limit = 0.01;
                speed_limit_pub_->publish(sl);
                return;
            }
        }
        double limit = base_speed_mps_;
        double dbg_lag = 0.0;
        if (control_mode_ == "follow_path") {
            // 종방향 간격 제어: fraction 은 경로 길이차(바깥/안쪽)로 편향됨 → 실제 world 위치를
            // 리더 진행방향에 투영한 '종방향 간격'으로 직접 제어. 뒤지면 가속·앞서면 감속.
            //  target_along = 슬롯 종오프셋(abreast=0, 단종=−2/−4). 횡(2m)은 오프셋경로가 담당.
            double err_behind_m = 0.0;     // +면 내가 (목표보다) 뒤짐
            const auto lw = world_pos_.find(static_cast<int>(leader_robot_id_));
            const auto ow = world_pos_.find(static_cast<int>(robot_id_));
            const auto lv = vel_.find(static_cast<int>(leader_robot_id_));
            bool pos_ok = false;
            if (!is_leader_ && lw != world_pos_.end() && ow != world_pos_.end()) {
                // 리더 진행방향 = 리더 *경로* 헤딩(leaderFrame) 우선. 순간속도(lv)는 reform
                // 직후 리더가 느리면(<0.15) 0 이 돼 pos_ok=false → frac 폴백의 잘못된 arc
                // (종간격오차 −26m)로 팔로워가 영구정지(stuck)했음. 경로헤딩은 항상 정의돼
                // 위치기반 종방향제어가 끊기지 않음(리더 정지 중에도 간격 정확).
                double Lx, Ly, lh, ux = 0.0, uy = 0.0;
                bool dir_ok = false;
                if (leaderFrame(&Lx, &Ly, &lh)) {
                    // ★along-track 방향 = 국소 ref세그먼트 헤딩이 아니라 '전체 경로 방향'(start→end).
                    //   국소 헤딩은 GNSS→map 투영/조밀화로 정동에서 미세하게(~1°) 각져 uy≠0 →
                    //   횡오프셋(ry: +side/−side)과 곱해져 s2/s3 비대칭(s3 ~1.3m 앞섬). 전체 직선
                    //   방향은 그 국소 각을 평균해 uy≈0 → 대칭. (긴 곡선 미션은 corner_ff 가 별도 보정.)
                    const auto &RP = leader_ref_path_.poses;
                    double odx = RP.back().pose.position.x - RP.front().pose.position.x;
                    double ody = RP.back().pose.position.y - RP.front().pose.position.y;
                    const double olen = std::hypot(odx, ody);
                    if (olen > 1e-3) { ux = odx / olen; uy = ody / olen; }
                    else { ux = std::cos(lh); uy = std::sin(lh); }
                    dir_ok = true;
                } else if (lv != vel_.end()) {   // 폴백: 리더경로 미수신 시 순간속도
                    const double hl = std::hypot(lv->second.first, lv->second.second);
                    if (hl > 0.15) { ux = lv->second.first / hl; uy = lv->second.second / hl;
                                     dir_ok = true; }
                }
                // (주의) 유클리드 거리는 부호가 없어 리더를 추월한 팔로워가 "멀다=가속"으로
                // 폭주(run16 실측: s2/s3 리더 +40m 추월 질주) — 반드시 부호 있는 투영 사용.
                if (dir_ok) {
                    const double rx = ow->second.first - lw->second.first;
                    const double ry = ow->second.second - lw->second.second;
                    const double along = rx * ux + ry * uy;   // 리더 기준 종방향 위치(+=앞)
                    const double target = mySlot().along * formation_lateral_spacing_m_;
                    err_behind_m = target - along;   // +면 목표보다 뒤짐 → 가속
                    pos_ok = true;
                }
            }
            // ★컨보이 리더는 레거시 페이싱(아래 world_pos·frac 이중분기) 미사용 — 두 분기가
            //   다른 값(-2 vs -7)으로 600ms 마다 교대해 속도지시가 0↔0.9 채찍질(whipsaw),
            //   평균 크롤→정지판정→웨지 폭주(run13 실측). 컨보이는 뒤의 결정적 간격
            //   governor(worst euclid gap)만 쓴다.
            else if (is_leader_ && !convoy_column_leader() &&
                     lw != world_pos_.end()) {
                // 리더 페이싱(line-abreast 평평화): 개루프로 달리면 리더가 늘 ~데드밴드만큼
                // 앞섬. 팔로워가 자기 종목표보다 뒤지면 리더도 감속해 같은 라인으로 수렴.
                // 팔로워가 앞서도 리더는 가속 안 함(err≤0) — 리더는 페이스메이커지 추격자 아님.
                // ★방향은 속도(움직일 때)·아니면 헤딩(정지 시). 예전엔 hl<0.15 면 계산을
                //   통째로 스킵 → 리더가 후미대기로 멈추면 leader_worst_lag_m_ 가 stale 고착
                //   → 팔로워가 슬롯에 다 붙어도 영구 대기(리더↔팔로워 상호정지 데드락). 정지
                //   중에도 헤딩으로 lag 를 재계산해 팔로워가 붙으면 대기를 푼다.
                double ux, uy;
                const auto lvit = vel_.find(static_cast<int>(leader_robot_id_));
                const double hl = (lvit != vel_.end())
                    ? std::hypot(lvit->second.first, lvit->second.second) : 0.0;
                if (hl > 0.15) { ux = lvit->second.first / hl; uy = lvit->second.second / hl; }
                else { ux = std::cos(own_yaw_); uy = std::sin(own_yaw_); }
                {
                    double max_behind = 0.0;
                    for (const int64_t fid : formation_followers_) {
                        const auto fw = world_pos_.find(static_cast<int>(fid));
                        if (fw == world_pos_.end()) continue;
                        const double along = (fw->second.first - lw->second.first) * ux +
                                             (fw->second.second - lw->second.second) * uy;
                        const int rank = static_cast<int>(fid) -
                                         static_cast<int>(leader_robot_id_);
                        const uint8_t geom = geometryForMode(formation_type_, formation_number_);
                        const double ft = formationSlotFor(geom, rank).along *
                                          formation_lateral_spacing_m_;
                        max_behind = std::max(max_behind, ft - along);   // +=팔로워 뒤짐
                    }
                    err_behind_m = -max_behind;   // 팔로워 뒤지면 리더 감속
                    pos_ok = true;
                }
            }
            if (!pos_ok && leader_arc_valid_ && !is_leader_) {   // 폴백: fraction(리더 정지 등)
                // along 오프셋 반영 — 없으면 리더 정지 시 팔로워가 완전 catch-up 해 간격 붕괴.
                const double target_along_m = mySlot().along * formation_lateral_spacing_m_;
                err_behind_m = (leader_arc_ - own_arc_) * fp_original_length_ + target_along_m;
            }
            // GNSS 노이즈 대응: 저역통과 + 데드밴드. 실차 GNSS 지터에 속도가 출렁이지 않게.
            along_err_filt_ = 0.7 * along_err_filt_ + 0.3 * err_behind_m;
            double err_eff = along_err_filt_;
            if (std::abs(err_eff) < spacing_deadband_m_) err_eff = 0.0;
            else err_eff -= (err_eff > 0 ? spacing_deadband_m_ : -spacing_deadband_m_);
            // 코너 곡률 피드포워드(Frenet/Robot Conga): 바깥 로봇은 반경(R+n)이 큰 더 긴 호를
            // 돌므로 미리 가속, 안쪽은 감속. 피드백만으론 항상 한 박자 늦어 코너서 잔차가 남음.
            //   v = base·(1 − n·κ),  n=좌+ 횡오프셋(m), κ=좌+ 리더경로 곡률(1/m).
            // leader_ref_path_는 정적·dense·rounded 경로라 κ가 노이즈 없는 결정값(GNSS와 무관).
            const double ff_mult = corner_ff_mult();
            // 추종 피드포워드 = 리더 실제 속도(팔로워). 고정 base 면 리더가 멈춰도 팔로워가
            // base 로 계속 가 간격 붕괴 → 리더 속도를 추종해 리더 정지 시 팔로워도 정지.
            double ff_speed = base_speed_mps_;
            if (!is_leader_ && lv != vel_.end()) {
                ff_speed = std::clamp(std::hypot(lv->second.first, lv->second.second),
                                      0.0, base_speed_mps_ * 2.0);
            }
            // ★캐치업 오버슛 댐핑(사용자: 리더 정속인데 팔로워 속도 널뛰기·간격 벌어짐).
            //   피드포워드=리더속도라 정상이면 v≈리더속도(간격 유지). P항(간격오차 보정)이 크면
            //   갭을 과하게 오버슛→헌팅. P항을 리더 대비 ±closing_cap 으로 제한해 리더보다
            //   조금씩만 빠르게/느리게 → 부드럽게 수렴, 정속 추종. (코너 ff 는 별도 유지.)
            const double p_term = corner_align_gain_ * std::clamp(err_eff, -6.0, 6.0);
            const double closing_cap = base_speed_mps_ * 0.5;   // 리더 대비 최대 ± 이만큼
            const double v = ff_speed * ff_mult +
                             std::clamp(p_term, -closing_cap, closing_cap);
            dbg_lag = along_err_filt_;
            // 하한 0: 리더 정지/앞차 근접 시 팔로워도 완전정지(간격 유지). base*0.3 floor 면 계속 기어듦.
            limit = std::clamp(v, 0.0, base_speed_mps_ * 2.2);
            // ★롤링 머지 직렬화: 컨보이(COLUMN)에선 내 앞 순번(rank 낮은) 팔로워가 전부
            //   내 앞 2m 를 확보한 뒤에야 출발. 동시합류→클러스터 잼(run8 실측) 방지,
            //   "리더 뒤로 차례로 붙는" 순서 보장. 한 번 출발하면 latch(중간 재동결 없음).
            // ★[정리] convoy(COLUMN/obs_convoy) 팔로워 속도 = 바로 앞 로봇과의 '거리 비례' 연속제어
            //   하나로 통일. 종전의 페이싱(ff+P)·캐치업캡·슬롯도달게이트·리더추월가드·순서(적응순항)
            //   가드·기동존대기는 전부 이 한 법칙에 포함되어 제거(중복·상호간섭이 stop-go·헌팅·직각의
            //   원인이었음):
            //     v = 앞차속도 + Kp·(간격 − 목표간격)
            //   목표간격=앞차속도 매칭(정속 추종·간격 유지), 멀면 비례 가속(따라잡기), 가까우면 비례
            //   감속(추월 없음, 순서 유지), 시작엔 간격≈0→v≈0(자연 순차 합류·슬롯대기). 앞차 =
            //   rank−1(리더가 rank1 의 앞). 아래 car-following 은 <crit 물리충돌 안전망으로만 잔존.
            if (convoy_follower()) {
                double aux, auy; pathForwardDir(aux, auy);
                const auto meP = world_pos_.find(static_cast<int>(robot_id_));
                if (meP != world_pos_.end()) {
                    const double own_sp = aux * meP->second.first + auy * meP->second.second;
                    const double nowP = this->now().seconds();
                    // ★앞차 = 실제 along-track 으로 '바로 앞선' 팀원(predecessor) — 고정 rank 가
                    //   아니라 위치 기반(진짜 플래투닝). 합류/대형 중 순서가 어긋나도(예: s3 가 s2
                    //   앞) 데드락 없이 실제 앞차를 따른다. (고정 rank 앞차가 실은 뒤에 있으면
                    //   gap 음수 → s* 폭발 → 영구정지 데드락 실측: DIAMOND 벽에서 s2·s3 동결.)
                    int ahead_id = -1; double best_gap = 1e18;
                    for (const auto &kv : world_pos_) {
                        if (kv.first == static_cast<int>(robot_id_)) continue;
                        const auto st = world_seen_.find(kv.first);
                        if (st == world_seen_.end() || nowP - st->second > 2.0) continue;
                        const double gap =
                            (aux * kv.second.first + auy * kv.second.second) - own_sp;
                        if (gap > 0.05 && gap < best_gap) { best_gap = gap; ahead_id = kv.first; }
                    }
                    // ── IDM ── 앞차 있으면 그 간격/속도로, 없으면(내가 맨 앞) 자유주행(v_des).
                    const double v = std::max(0.0, static_cast<double>(current_speed_));
                    const double v_des = base_speed_mps_ * idm_vdes_mult_;
                    double s = 1.0e9, v_ah = v_des;
                    if (ahead_id >= 0) {
                        s = std::max(0.3, best_gap);
                        const auto vv = vel_.find(ahead_id);
                        v_ah = (vv != vel_.end())
                            ? std::max(0.0, aux * vv->second.first + auy * vv->second.second)
                            : 0.0;
                    }
                    const double v_rel = v - v_ah;                            // 접근(+)
                    const double s_star = idm_s0_ + std::max(0.0,
                        v * idm_time_headway_ +
                        (v * v_rel) / (2.0 * std::sqrt(idm_accel_ * idm_decel_)));
                    const double accel = idm_accel_ * (1.0
                        - std::pow(v / std::max(0.1, v_des), 4.0)
                        - (ahead_id >= 0 ? (s_star / s) * (s_star / s) : 0.0));
                    const double v_new = v + accel * 0.1;                     // dt≈100ms(sync_tick)
                    limit = std::clamp(v_new, 0.0, base_speed_mps_ * 2.2);
                }
            }
            if (!is_leader_ && obstacle_dist_ < obstacle_detect_range_) {
                limit = std::min(limit, 0.8);
            }
            // ★ 앞차 추돌 방지(car-following 안전층): 같은 차로 전방 팀원과의 간격이 추돌
            //    직전이면 그 차보다 느리게(간격 회복). 리더-상대 along 제어가 차간격을 직접
            //    규제하지 않아 column 에서 앞차 지체 시 뒷차가 닫고 들어가는 것 방지.
            if (!is_leader_) {
                double fg = 0.0, fvspd = 0.0;
                int front_id = -1;
                if (forwardTeammateGap(&fg, &fvspd, &front_id)) {
                    // 마진 확대(구 1.7/2.2/3.5): crit 1.7 은 감속거리(~0.3m) 빼면 범퍼
                    // 여유 0.1m — 측정지연·오버슛에 접촉(범퍼락 잼, run11-13 실측).
                    // ★안전망으로만(사용자: 리더 정속인데 s2 중간중간 멈춤). 종전 slow 4.5m 는
                    //   정상 종대간격(~5m)과 겹쳐 간격이 조금만 출렁여도 car-following 이 발동→
                    //   pacing 과 간섭해 stop-go. 임계를 낮춰 진짜 근접(<2.8m)일 때만 개입, 정상
                    //   간격에선 pacing(데드밴드)만 매끄럽게 속도 유지. 최종 안전은 collision_monitor.
                    const double crit = 1.8;   // 이내면 정지(hold)
                    const double hard = 2.2;   // 이내면 앞차보다 확실히 느리게
                    const double slow = 2.8;   // 이내면 비례 감속 시작
                    // ★후순번(id 큰) 차가 일시적으로 내 앞에 있는 과도기(합류 초기 동일 x
                    //   클러스터): hold 하면 상호 대기 데드락(내 hold ↔ 그 차 턴게이트,
                    //   run12 실측 s2·s3 0.4m 교착). 그 차는 정지 중이므로 저속(0.2)으로
                    //   지나가 순서를 회복(collision_monitor 최종 안전망).
                    const bool front_is_junior =
                        front_id > static_cast<int>(robot_id_);
                    // ★접촉권(<1.7m): 밀고 들어가지 말고 '뒤차(나)'가 1.5s 후진해 간격
                    //   복원(run18b 실측: 복구기동 중 콘 밖 각도로 접근→1.3m 상호잠금).
                    //   뒤에 다른 팀원 있으면 후진 대신 hold.
                    if (fg < 1.7) {
                        // ★종대(COLUMN·obs_convoy) 중엔 완전정지 대신 크롤(사용자 지시): 팔로워끼리
                        //   접촉권 상호정지·hold 재개 stop-go 스터터 방지 — 리더 궤적을 계속 따라
                        //   순서대로 붙는다. collision_monitor 가 최종 안전망. 넓은대형은 정지·후진금지.
                        limit = convoy_follower() ? std::min(limit, 0.12) : 0.0;
                        const double tnow = this->now().seconds();
                        bool behind_clear = true;
                        {
                            const auto meb2 = world_pos_.find(static_cast<int>(robot_id_));
                            if (meb2 != world_pos_.end()) {
                                const double c2 = std::cos(own_yaw_), s2v = std::sin(own_yaw_);
                                for (const auto &kv : world_pos_) {
                                    if (kv.first == static_cast<int>(robot_id_)) continue;
                                    const double rx = kv.second.first - meb2->second.first;
                                    const double ry = kv.second.second - meb2->second.second;
                                    if (c2 * rx + s2v * ry < 0.0 && std::hypot(rx, ry) < 2.5) {
                                        behind_clear = false; break;
                                    }
                                }
                            }
                        }
                        // ★후진 금지(사용자 지시): 팔로워는 접촉권에서도 뒤로 가지 않는다.
                        //   limit=0(위)로 정지 유지만 하고, 앞차(리더 포함)가 전진해 간격이
                        //   벌어지길 기다린다 → 대형은 리더 전진으로 맞춰진다.
                        (void)behind_clear;
                        RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 3000,
                            "[추돌방지] 접촉권 %.2fm — 정지 유지(후진 금지, 앞차 전진 대기)", fg);
                    } else if (fg < crit && front_is_junior) {
                        limit = std::min(limit, 0.2);
                        if (gap_hold_) {   // hold 로 goal 취소된 상태면 재개해야 움직임
                            gap_hold_ = false;
                            if (convoy_follower() && leader_trace_.poses.size() >= 4) {
                                send_convoy_path();
                            } else {
                                build_and_send_follow_path();
                            }
                        }
                        RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 2000,
                            "[추돌방지] 앞의 후순번 s%d(%.2fm) — 저속 통과(데드락 방지)",
                            front_id, fg);
                    } else if (fg < crit && convoy_follower()) {
                        // ★종대(COLUMN·obs_convoy): 앞차 근접해도 정지·goal취소(hold) 하지 말고
                        //   크롤-추종(사용자 지시: 팔로워는 리더 궤적을 그대로 따라 바로 붙는다).
                        //   full-stop+hold+재개는 stop-go 스터터의 주범(격리실험 실측).
                        limit = std::min(limit, 0.2);
                        if (gap_hold_) {
                            gap_hold_ = false;
                            if (convoy_follower() && leader_trace_.poses.size() >= 4) {
                                send_convoy_path();
                            }
                        }
                    } else if (fg < crit) {
                        limit = 0.0;                                          // 추돌 직전 — 정지
                        // ★진짜 정지: speed_limit 0 은 nav2 하한(0.01) 크리핑으로 수 분에
                        //   걸쳐 앞차에 물리 접촉(범퍼락→리더 견인 불가→웨지 폭주, run11
                        //   실측) — goal 자체를 취소해 cmd_vel 을 완전히 끊는다.
                        if (!gap_hold_ && active_fp_goal_) {
                            ignore_next_fp_result_ = true;
                            cancel_goal_safe(follow_path_client_, active_fp_goal_);
                            active_fp_goal_.reset();
                            gap_hold_ = true;
                            RCLCPP_WARN(this->get_logger(),
                                "[추돌방지] 앞차 %.2fm — FollowPath 정지(hold)", fg);
                        }
                    } else if (gap_hold_ && fg > hard + 0.2) {
                        // 간격 회복 → 재개(컨보이면 리더 궤적으로).
                        gap_hold_ = false;
                        RCLCPP_INFO(this->get_logger(),
                                    "[추돌방지] 간격 회복(%.2fm) — 재개", fg);
                        if (convoy_follower() && leader_trace_.poses.size() >= 4) {
                            send_convoy_path();
                        } else {
                            build_and_send_follow_path();
                        }
                    } else if (fg < hard) {
                        limit = std::min(limit, std::max(0.0, fvspd - 0.6));  // 앞차보다 확실히 느리게
                    } else if (fg < slow) {
                        const double s = (fg - hard) / (slow - hard);     // 0..1
                        limit = std::min(limit, fvspd + s * (limit - fvspd));
                    }
                    if (fg < slow) {
                        RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 1000,
                            "[추돌방지] 앞차 간격 %.2fm(앞차v=%.2f) → 감속 v=%.2f",
                            fg, fvspd, limit);
                    }
                } else if (gap_hold_) {
                    // 앞차가 사라짐(추월·이탈·stale) — hold 해제·재개.
                    gap_hold_ = false;
                    RCLCPP_INFO(this->get_logger(), "[추돌방지] 앞차 없음 — hold 해제·재개");
                    if (convoy_follower() && leader_trace_.poses.size() >= 4) {
                        send_convoy_path();
                    } else {
                        build_and_send_follow_path();
                    }
                }
            }
            RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 2000,
                "[속도] 종간격오차=%.2fm(필터) ff×%.2f v=%.2f (%s)",
                along_err_filt_, ff_mult, limit, pos_ok ? "pos/GNSS" : "frac");
        } else if (is_leader_ && !convoy_column_leader()) {
            const double now_s = this->now().seconds();
            double max_lag = 0.0;
            for (const auto &kv : peer_progress_) {
                if (kv.first == static_cast<int>(robot_id_)) continue;
                const auto it = peer_seen_.find(kv.first);
                if (it == peer_seen_.end() || now_s - it->second > 2.0) continue;
                max_lag = std::max(max_lag, (own_arc_ + kv.second.second) - kv.second.first);
            }
            limit = speed_from_excess(max_lag);
        } else if (leader_arc_valid_) {
            limit = speed_from_excess(own_arc_ - (leader_arc_ + my_along_m));
        }
        if (yielding_) {   // 상위 우선순위 스워브에 양보 — 감속해 먼저 지나가게(직렬화)
            limit = std::min(limit, base_speed_mps_ * 0.3);
        }
        // ★컨보이 리더 간격 governor(결정적, euclid): 페이싱(arc/필터 기반)은 pose 소스
        //   교란·fraction 혼선에 취약해 리더 러너웨이(127m 실측)를 못 막았음. 최악 팔로워의
        //   (거리-슬롯) 초과가 8m 넘으면 비례감속, 15m 넘으면 정지 — 단순·강건한 하드 상한.
        if (convoy_column_leader()) {
            const auto me_w = world_pos_.find(static_cast<int>(robot_id_));
            if (me_w != world_pos_.end()) {
                double worst = 0.0;
                const double now_ws = this->now().seconds();
                double gcy, gsy; pathForwardDir(gcy, gsy);
                for (const int64_t fid : formation_followers_) {
                    const auto it = world_pos_.find(static_cast<int>(fid));
                    const auto st = world_seen_.find(static_cast<int>(fid));
                    if (it == world_pos_.end() || st == world_seen_.end() ||
                        now_ws - st->second > 2.0) continue;
                    // 부호 있는 종방향(리더 헤딩 투영): '뒤(behind>0)'인 팔로워만 대기 대상.
                    // 앞서간(추월/이상) 팔로워를 기다리면 상호 정지 데드락.
                    const double behind =
                        -(gcy * (it->second.first - me_w->second.first) +
                          gsy * (it->second.second - me_w->second.second));
                    if (behind <= 0.0) continue;
                    const double slot_m = 1.5 *
                        static_cast<double>(fid - leader_robot_id_) *
                        formation_lateral_spacing_m_;
                    worst = std::max(worst, behind - slot_m);
                }
                // 타이트한 간격 유지: 슬롯 초과 3m(팔로워 6m 뒤) 넘으면 정지, 1m(4m 뒤)
                // 부터 비례감속. 종전 8/15m 는 너무 느슨해 팔로워가 15m 뒤에서도 리더가
                // 안 멈춰 종간격이 안 좁혀졌음(run28). 하한 0.15 로 서행 유지(trace 성장).
                // ★리더가 장애물 스레딩 중(fp_swerving_/우회)에는 완전정지 금지 — 좁은 갭
                //   한복판(벽 모서리)에서 멈추면 collision_monitor 간섭+FollowPath abort 로
                //   리더가 낀 채 뒤차까지 연쇄 정지(DIAMOND 데드락 실측). 갭을 다 빠져나온
                //   뒤(기동 종료)에만 정지해 기다린다. 스레딩 중엔 0.15 서행으로 통과 지속.
                // '기동 중' = 스워브/우회 플래그 OR 리더가 아직 라인에서 크게 벗어남(갭 안).
                //   ★스워브가 끝나도(fp_swerving_=false) 리더가 벽 가장자리(y≈-6.7)에 있으면
                //   여기서 완전정지해 낀 채 뒤차까지 연쇄정지(DIAMOND 데드락). 라인 복귀
                //   (횡편차<clear)까지는 0.15 서행으로 갭을 마저 빠져나오게 한다.
                // ★완전정지(limit=0)는 리더 궤적을 얼려 클러스터/문자열 데드락을 만든다
                //   (리더정지→trace 정지→앞 팔로워 정지→뒤 팔로워 정지, DIAMOND 복귀존 실측).
                //   리더는 항상 최소 0.15 크롤을 유지해 궤적을 늘려주고, 뭉친 팔로워는 그 전진에
                //   실려 풀린다. 유실방지는 하한 0.15(느려서 팔로워 캐치업 1.5 가 항상 우세)로 충분.
                // ★사용자 지시: 리더는 후미 대기로 크롤/정지하지 않고 계속 전진. 간격은
                //   deadband(통신 여유) 안이면 감속 0, 초과분만 slow_span 에 걸쳐 완만히 감속하되
                //   floor_frac×base 아래로는 안 내림(크롤/정지 금지). 뭉친 팔로워는 convoy_turn_ok
                //   순번대로 한 대씩 리더 뒤에 합류해 간격을 스스로 회복한다(리더 정지로 X).
                // ★장애물 통과 중(obstacle_pass_active_)에만 리더가 후미 대기로 크롤/정지하지
                //   않고 계속 전진(deadband 크게+floor 높게, 사용자 지시). 일반 종대 순항에선
                //   리더가 러너웨이하면 종대가 끊기므로 종전대로 촘촘히 페이싱(deadband 작게+
                //   floor 낮게)해 후미를 데리고 간다.
                // ★순항 중엔 리더 정속(사용자: s1 은 정속주행). 종전 순항 deadband 1.0 은 팔로워가
                //   목표간격까지 벌어질 때 리더가 기다려 감속 → 비례추종 팔로워와 커플링(스톨/서지
                //   리밋사이클). 순항 deadband 를 크게(=사실상 정속, 40m 이상 벌어질 때만 러너웨이
                //   방지) → 팔로워 거리비례 제어가 정속 리더를 안정 추종. 장애물통과 때만 촘촘 대기.
                const double deadband =
                    obstacle_pass_active_ ? column_pace_deadband_m_ : 40.0;
                const double floor =
                    obstacle_pass_active_ ? base_speed_mps_ * column_pace_floor_frac_ : 0.15;
                if (worst > deadband) {
                    const double over = worst - deadband;
                    limit = std::min(limit, std::max(floor,
                                     base_speed_mps_ *
                                     (1.0 - over / std::max(0.1, column_pace_slow_span_m_))));
                }
            }
        }
        // 차폭 충돌 방지(예측 반발장): 현재거리뿐 아니라 속도로 예측한 '최근접거리'도 봐서
        // 겹치기 전에 선제 감속. 상위 우선순위 차에만 후순위가 비킴. 정상 대형(2m)은 영향 없음.
        // ★종대(COLUMN·obs_convoy) 팔로워는 끈다(사용자 지시): 단일종대로 리더 궤적을 그대로
        //   따라야 하는데 팔로워끼리 예측반발로 상호 선제 감속·정지하면 "난리"·데드락(s3가 앞
        //   s2 때문에 정지, 시작 합류서 s2/s3 상호 감속 실측). 간격은 앞차 car-following(추돌
        //   방지)만으로 유지. 넓은 대형(DIAMOND/WEDGE)에서만 횡충돌 예측반발 유효.
        if (!convoy_follower()) {
            const auto me = world_pos_.find(static_cast<int>(robot_id_));
            const auto mev = vel_.find(static_cast<int>(robot_id_));
            if (me != world_pos_.end()) {
                const double now_w = this->now().seconds();
                const double mvx = (mev != vel_.end()) ? mev->second.first : 0.0;
                const double mvy = (mev != vel_.end()) ? mev->second.second : 0.0;
                double rf = 1.0;
                const double rcy = std::cos(own_yaw_), rsy = std::sin(own_yaw_);
                for (const auto &kv : world_pos_) {
                    if (kv.first >= static_cast<int>(robot_id_)) continue;  // 상위 우선순위만
                    const auto sit = world_seen_.find(kv.first);
                    if (sit == world_seen_.end() || now_w - sit->second > 1.0) continue;
                    const double px = me->second.first - kv.second.first;
                    const double py = me->second.second - kv.second.second;
                    // ★내 '뒤'의 피어는 제외: 전진하면 멀어지는 대상이라 반발이 무의미하고,
                    // 정지 상태끼리는 예측최근접=현재거리로 굳어 상호 동결(gridlock)됨
                    // (컬럼 합류 클러스터서 s3가 후방 s2 때문에 영구정지 실측). 후미접근
                    // 충돌은 그 차의 car-following(추돌방지)이 담당.
                    if (rcy * (-px) + rsy * (-py) < -0.3) continue;
                    const auto pv = vel_.find(kv.first);
                    const double vx = mvx - (pv != vel_.end() ? pv->second.first : 0.0);
                    const double vy = mvy - (pv != vel_.end() ? pv->second.second : 0.0);
                    // 예측 최근접 시점 t*(상대속도 기준) → 그때 거리. 현재거리와 min.
                    const double v2 = vx * vx + vy * vy;
                    const double ts = (v2 > 1e-4)
                        ? std::clamp(-(px * vx + py * vy) / v2, 0.0, predict_horizon_s_) : 0.0;
                    const double pmx = px + vx * ts, pmy = py + vy * ts;
                    const double d = std::min(std::hypot(px, py), std::hypot(pmx, pmy));
                    if (d < repulse_dist_) {
                        // safe_dist 이하면 rf=0(완전정지) → 전환 fold 직렬화(후순위가 멈춰
                        // 상위차 먼저 통과 후 접어듦). 상위차가 멀어지면 자동 회복(데드락 없음).
                        rf = std::min(rf, std::clamp(
                            (d - repulse_safe_dist_) /
                            std::max(0.1, repulse_dist_ - repulse_safe_dist_), 0.0, 1.0));
                    }
                }
                if (rf < 1.0) {
                    limit *= rf;
                    RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 1500,
                        "[예측반발] 상위 우선순위 차 예측근접 → 선제감속 x%.2f", rf);
                }
            }
        }
        // ★리더 페이싱 급정지 완화: 리더가 팔로워 지연으로 감속할 때 완전정지 대신 최소
        //   leader_pace_floor 로 부드럽게 전진(회피 후 재편성 churn·yaw 떨림 완화). 컨보이
        //   리더(COLUMN·장애물통과)는 제외 — 위 결정적 governor(하한 0.15 크롤+slot+3m 정지)가
        //   대기를 담당하므로 여기서 0.3 으로 되살리면 후미대기가 깨진다. hard_stop·웨지후진도 제외.
        if (is_leader_ && formation_enabled_ && control_mode_ == "follow_path" &&
            !hard_stop_ && wedge_reverse_ticks_ == 0 && !fp_detour_active_ &&
            !convoy_column_leader()) {
            limit = std::max(limit, leader_pace_floor_);
        }
        // 긴급정지: 정면 차폭내 장애물이 코앞인데 스워브가 못 비킴 → 정지(밀고 들어감 방지).
        // 정지 후 장애물 그대로면 끼임감지→NavigateToPose(planner, full footprint) 복구로 에스컬.
        if (hard_stop_) limit = 0.01;   // 0.0 은 nav2 에서 '제한없음'으로 해석될 수 있어 0.01
        if (limit < 0.02) limit = 0.01; // 반발 완전정지(rf=0) 등 — 0.0 '제한없음' 회피
        nav2_msgs::msg::SpeedLimit sl;
        sl.header.stamp = this->now();
        sl.percentage = false;
        sl.speed_limit = limit;
        last_speed_limit_ = limit;   // 웨지(접촉 밀착) 판정용
        speed_limit_pub_->publish(sl);
    }

    // 기준경로(ref)·내 오프셋경로(active_points_) 각 wp 까지 누적 arc(m) 사전계산.
    static void cumulative(const std::vector<std::pair<double, double>> &p,
                           std::vector<double> &c)
    {
        constexpr double kM = 111320.0;
        c.assign(p.size(), 0.0);
        for (std::size_t i = 1; i < p.size(); ++i) {
            const double dlat = (p[i].first - p[i - 1].first) * kM;
            const double dlon = (p[i].second - p[i - 1].second) *
                                kM * std::cos(p[i].first * M_PI / 180.0);
            c[i] = c[i - 1] + std::hypot(dlat, dlon);
        }
    }
    void compute_cumulative_arc(const std::vector<std::pair<double, double>> &ref)
    {
        cumulative(ref, cum_ref_);
        cumulative(active_points_, my_cum_);

        // 배리어 wp 판정: wp[0]=form-up, 내부 코너(진행방위 변화 큰 곳). 마지막(목표)은 제외.
        constexpr double kM = 111320.0;
        constexpr double kCornerRad = 0.45;   // ~26° 이상 꺾이면 코너로 보고 정렬
        is_barrier_.assign(ref.size(), 0);
        if (!ref.empty()) is_barrier_[0] = 1;   // 시작 form-up
        auto bearing = [&](std::size_t a, std::size_t b) {
            const double dn = (ref[b].first - ref[a].first) * kM;
            const double de = (ref[b].second - ref[a].second) * kM *
                              std::cos(ref[b].first * M_PI / 180.0);
            return std::atan2(dn, de);
        };
        for (std::size_t k = 1; k + 1 < ref.size(); ++k) {
            const double turn = std::abs(ang_diff(bearing(k, k + 1), bearing(k - 1, k)));
            if (turn > kCornerRad) is_barrier_[k] = 1;
        }
    }

    // ---- 동적 리더 추종 ----
    // 자기 map pose 캐싱. 리더면 /swarm/leader_pose 로도 발행(팔로워가 추종).
    void on_own_map_pose(const nav_msgs::msg::Odometry::SharedPtr m)
    {
        // ★odometry/global(EKF, GPS보정)이 살아있으면 이 콜백은 pose 를 쓰지 않는다.
        // 휠오돔(스폰오프셋)은 스키드스티어 드리프트가 누적(250m 주행 후 ~30m 오차 실측)
        // — 두 소스가 번갈아 own_map 을 쓰면 자기위치가 10Hz 로 플랩해 리더 trace 가
        // 지그재그(길이 21km!)로 오염되고 페이싱/repos 가 전부 무너진다(POC sim 실측,
        // 06-22 HIL dynamic 횡발산과 동일 병소). 휠오돔은 EKF 1s 이상 끊길 때만 폴백.
        own_yaw_rate_ = m->twist.twist.angular.z;   // 각속도(댐핑)는 항상 최신 사용
        if (this->now().seconds() - last_global_pose_t_ >= 1.0) {
            // gz odom(/sN/odom)은 스폰 기준 상대 pose. 스폰 yaw=0 이라 공유 프레임 변환은
            // 단순 오프셋: world = spawn + odom. (EKF 부재 시 폴백 전용)
            own_map_x_ = spawn_x_ + m->pose.pose.position.x;
            own_map_y_ = spawn_y_ + m->pose.pose.position.y;
            own_yaw_ = tf2::getYaw(m->pose.pose.orientation);
            own_pose_valid_ = true;
        }
        // 이하 공유 pose 발행은 own_map_*(현재 유효 소스 기준)로 항상 수행.

        geometry_msgs::msg::Pose wp;
        wp.position.x = own_map_x_;
        wp.position.y = own_map_y_;
        tf2::Quaternion q;
        q.setRPY(0.0, 0.0, own_yaw_);
        wp.orientation = tf2::toMsg(q);

        // 공유 pose 발행(근접 회피용). frame_id = robot_id.
        if (robot_pose_pub_) {
            geometry_msgs::msg::PoseStamped me;
            me.header.stamp = this->now();
            me.header.frame_id = std::to_string(robot_id_);
            me.pose = wp;
            robot_pose_pub_->publish(me);
        }
        // 리더면 /swarm/leader_pose 로도 발행(팔로워 추종 기준).
        if (is_leader_ && leader_pose_pub_) {
            geometry_msgs::msg::PoseStamped ps;
            ps.header.stamp = this->now();
            ps.header.frame_id = "map";
            ps.pose = wp;
            leader_pose_pub_->publish(ps);
        }
    }

    // 다음 웨이포인트(wlat,wlon) 방위(map ENU yaw) 계산: 리더 GPS 기준.
    void compute_formation_heading_to(double wlat, double wlon)
    {
        constexpr double kM = 111320.0;
        const double d_north = (wlat - gps_lat_) * kM;
        const double d_east = (wlon - gps_lon_) * kM *
                              std::cos(gps_lat_ * M_PI / 180.0);
        formation_heading_ = std::atan2(d_north, d_east);  // +x=east, +y=north 기준 yaw
    }
    void compute_formation_heading()
    {
        if (!pending_path_.empty()) {
            compute_formation_heading_to(pending_path_[0].first, pending_path_[0].second);
        }
    }

    // 팔로워: 리더 pose 수신 → yaw + (델타로) 리더 속도 추정.
    void on_leader_pose(const geometry_msgs::msg::PoseStamped::SharedPtr m)
    {
        leader_x_ = m->pose.position.x;
        leader_y_ = m->pose.position.y;
        // 리더 헤딩 저역통과: 리더가 웨이포인트서 잠깐 틀어도(nav2 maneuver) 팔로워가
        // 그 transient 를 따라 진동하지 않도록 부드럽게.
        const double new_yaw = tf2::getYaw(m->pose.orientation);
        if (!have_prev_leader_) {
            leader_yaw_ = new_yaw;
        } else {
            leader_yaw_ += 0.2 * ang_diff(new_yaw, leader_yaw_);
        }
        const double t = rclcpp::Time(m->header.stamp).seconds();
        if (have_prev_leader_) {
            const double dt = t - prev_leader_t_;
            if (dt > 1e-3) {
                const double v = std::hypot(leader_x_ - prev_leader_x_,
                                            leader_y_ - prev_leader_y_) / dt;
                // 저역통과(노이즈 완화) + 정지 데드밴드(jitter 로 가짜 속도 방지).
                leader_speed_ = 0.6 * leader_speed_ + 0.4 * v;
                if (leader_speed_ < 0.08) leader_speed_ = 0.0;
            }
        }
        prev_leader_x_ = leader_x_;
        prev_leader_y_ = leader_y_;
        prev_leader_t_ = t;
        have_prev_leader_ = true;
        leader_pose_valid_ = true;
        leader_stale_ticks_ = 0;
    }

    // 속도 피드포워드 추종 컨트롤러(diff-drive). 리더 속도를 따라가며(트레일링 제거)
    // 위치오차를 보정해 "리더 옆 슬롯"을 유지한다. nav2 없이 /sN/cmd_vel 직접 발행.
    void control_tick()
    {
        if (!dynamic_follow_ || !cmd_vel_pub_) {
            return;
        }
        if (!leader_pose_valid_ || !own_pose_valid_) {
            publish_stop();
            return;
        }
        if (++leader_stale_ticks_ > 20) {   // 리더 pose 1s 끊김 → 정지(stale 발산 방지)
            publish_stop();
            return;
        }
        // 슬롯 = 리더 위치 + 리더헤딩기준 횡오프셋(코너에선 리더 헤딩따라 슬롯이 회전→대형 유지).
        const int slot = formationSlotOffset();
        const double lateral = slot * formation_lateral_spacing_m_;
        const double slot_x = leader_x_ - std::sin(leader_yaw_) * lateral;
        const double slot_y = leader_y_ + std::cos(leader_yaw_) * lateral;
        const double sex = slot_x - own_map_x_;
        const double sey = slot_y - own_map_y_;
        const double dist = std::hypot(sex, sey);

        // form-up ready: 슬롯 tol 내 유지 시 ready.
        if (dist < formation_tol_m_) { ++ready_streak_; } else { ready_streak_ = 0; }
        const bool ready = (ready_streak_ >= 10);
        if (ready != formation_ready_ && formation_ready_pub_) {
            formation_ready_ = ready;
            std_msgs::msg::Bool b; b.data = ready;
            formation_ready_pub_->publish(b);
        }

        // ── 표준 unicycle pose 컨트롤러(ρ/α/β) — 슬롯 위치+리더 헤딩으로 수렴 ──
        // (블렌딩 방식의 atan2(0,0) 특이점/제자리 진동 제거. 슬롯이 뒤/옆이어도 v=k·ρ 로
        //  항상 슬롯 방향으로 이동 → 수렴. 근접 시 β 가 리더 헤딩 정렬을 담당.)
        const double rho = dist;
        const double bearing = std::atan2(sey, sex);     // 슬롯 방위
        const double alpha = ang_diff(bearing, own_yaw_); // 슬롯 점까지 헤딩오차
        const double beta = ang_diff(leader_yaw_, bearing); // 최종(리더) 헤딩까지

        // 리더 속도 피드포워드(같은방향일 때).
        const double ff = std::max(0.0, std::cos(ang_diff(leader_yaw_, own_yaw_))) * leader_speed_;

        geometry_msgs::msg::Twist cmd;
        double v = 1.0 * rho + ff;
        if (std::abs(alpha) > M_PI / 2.0) {
            v = ff;   // 슬롯이 뒤 → 먼저 회전(전진 억제)
        }
        // 슬롯 근처면 α(점추종) 비중↓ β(헤딩정렬) 비중↑ — ρ로 가중.
        const double wr = std::clamp(rho / 1.0, 0.0, 1.0);  // 0(슬롯)~1(멀리)
        double omega = (1.6 * alpha) * wr + (1.3 * ang_diff(leader_yaw_, own_yaw_)) * (1.0 - wr)
                       + 0.4 * beta * wr - 0.3 * own_yaw_rate_;
        cmd.linear.x = std::clamp(v, -0.3, 1.4);
        cmd.angular.z = std::clamp(omega, -1.5, 1.5);

        apply_collision_avoidance(cmd);
        cmd_vel_pub_->publish(cmd);
        mission_status_ = combat_robot_msgs::msg::OperationState::MISSION_MOVING;
    }

    // 로봇간 근접 회피: 전방 콘(±~64°) 안에 이웃이 avoid_radius_ 내로 들어오면 감속,
    // avoid_stop_dist_ 면 정지 + 이웃 반대쪽으로 약하게 조향. 옆 대형 이웃(≈90°)은 무시
    // 하므로 정상 편대 주행은 영향 없음. (팔로워만; 리더는 nav2 우선·우선권 부여)
    void apply_collision_avoidance(geometry_msgs::msg::Twist &cmd)
    {
        double nearest = 1e9;
        double nearest_lat = 0.0;
        // 편대원(다른 로봇)은 슬롯 간격(=spacing)만큼 떨어져 정상. 그보다 더 가까운
        // (imminent) 경우만 위협으로 본다 → 슬롯 추종을 방해하지 않음. 정적 장애물(lidar)은
        // 편대원 마스킹 후라 더 큰 반경(go-around) 사용.
        const double robot_thresh = std::max(1.2, formation_lateral_spacing_m_ - 0.5);
        for (const auto &kv : neighbor_pos_) {
            const double dx = kv.second.first - own_map_x_;
            const double dy = kv.second.second - own_map_y_;
            const double d = std::hypot(dx, dy);
            if (d >= robot_thresh || d < 1e-3) {
                continue;
            }
            const double fwd = std::cos(own_yaw_) * dx + std::sin(own_yaw_) * dy;
            const double lat = -std::sin(own_yaw_) * dx + std::cos(own_yaw_) * dy;
            // 전방 + 콘 안(|lat| < 0.9d ≈ ±64°). 옆/뒤 이웃은 위협 아님.
            if (fwd > 0.0 && std::abs(lat) < 0.9 * d && d < nearest) {
                nearest = d;
                nearest_lat = lat;
            }
        }
        // 라이다 정적 장애물도 같은 위협으로 포함(더 가까운 쪽 채택).
        if (obstacle_dist_ < nearest) {
            nearest = obstacle_dist_;
            nearest_lat = obstacle_lat_;
        }
        if (nearest >= avoid_radius_) {
            return;   // 위협 없음 → 슬롯 추종 그대로(장애물 벗어나면 자동 복귀)
        }
        // go-around: 멈추지 말고 장애물 반대쪽으로 강하게 조향하며 전진 유지 → 돌아나감.
        // (장애물 벗어나면 avoidance 미발동 → control_tick 슬롯추종이 다시 슬롯으로 복귀)
        const double closeness = std::clamp(
            (avoid_radius_ - nearest) / (avoid_radius_ - avoid_stop_dist_), 0.0, 1.0);  // 0..1
        // 장애물이 오른쪽(lat<0)이면 좌회전(+), 왼쪽이면 우회전(−). 가까울수록 강하게.
        const double steer = (nearest_lat > 0.0 ? -1.0 : 1.0) * 1.8 * closeness;
        cmd.angular.z = std::clamp(cmd.angular.z + steer, -1.6, 1.6);
        if (cmd.linear.x > 0.0) {
            cmd.linear.x *= (1.0 - 0.6 * closeness);   // 감속(최대 60%)
            // 정면 임박(거의 정중앙 & stop_dist 이내)만 완전정지, 그 외엔 전진 유지해 돌아나감.
            if (nearest < avoid_stop_dist_ && std::abs(nearest_lat) < 0.4) {
                cmd.linear.x = 0.0;
            } else {
                cmd.linear.x = std::max(cmd.linear.x, 0.25);
            }
        }
    }

    // 각도차 [-pi,pi]
    static double ang_diff(double a, double b)
    {
        return std::atan2(std::sin(a - b), std::cos(a - b));
    }

    void publish_stop()
    {
        if (cmd_vel_pub_) {
            cmd_vel_pub_->publish(geometry_msgs::msg::Twist());
        }
    }

    // 라이다 전방 콘 장애물 추출(라이다 프레임 x前/y左). sim lidar 노이즈(흩어진 1~2점)를
    // 실장애물로 오인해 와리가리하던 것 차단 위해: 좁은 콘 + 군집(클러스터) 요구.
    //  · 실장애물(box/벽)=좁은 콘에 다수 점 군집, 노이즈=드문드문 → 최근접 "≥min_cluster 군집"만 채택.
    void on_lidar(const sensor_msgs::msg::PointCloud2::SharedPtr m)
    {
        const double cy = std::cos(own_yaw_), sy = std::sin(own_yaw_);
        std::vector<std::pair<double, double>> pts;   // (r, lat) 좁은콘 후보점(거리·스워브방향)
        std::vector<std::pair<double, double>> wpts;  // (r, lat) 넓은콘(±64°) 후보점 — 실폭 측정
        double lclear = 1e9, rclear = 1e9;            // 좌/우 측방 최근접(스워브 방향 결정)
        try {
            sensor_msgs::PointCloud2ConstIterator<float> ix(*m, "x"), iy(*m, "y"), iz(*m, "z");
            for (; ix != ix.end(); ++ix, ++iy, ++iz) {
                const double x = *ix, y = *iy, z = *iz;
                if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) continue;
                if (x <= 0.15) continue;                  // 전방만
                if (z < obstacle_z_min_ || z > 1.2) continue;  // 지면·천장 제외
                const double r = std::hypot(x, y);
                if (r < lidar_self_range_) continue;      // 자기차체/코앞지면
                if (r > obstacle_detect_range_) continue; // 감지 범위 밖(E1 선행감지 12m)
                // 좌/우 측방 클리어런스(전방 넓은 콘 ±~64°, 스워브 갈 쪽이 빈지 판단).
                if (std::abs(y) < 0.9 * r) {
                    if (y > 0.3 && r < lclear) lclear = r;       // 좌측
                    else if (y < -0.3 && r < rclear) rclear = r; // 우측
                } else {
                    continue;   // 넓은 콘(±64°) 밖 → 좁은콘(⊂넓은콘)도 아님 → 둘 다 제외
                }
                // 편대원 마스킹: map 변환해 다른 로봇 위치 근처면 제외.
                // (넓은콘 점 1회 계산 → 좁은콘도 재사용. 좁은콘 ⊂ 넓은콘 이라 pts 결과 불변)
                const double mpx = own_map_x_ + cy * x - sy * y;
                const double mpy = own_map_y_ + sy * x + cy * y;
                bool is_robot = false;
                for (const auto &kv : neighbor_pos_) {
                    if (std::hypot(mpx - kv.second.first, mpy - kv.second.second) < 1.6) {
                        is_robot = true; break;
                    }
                }
                if (is_robot) continue;
                wpts.emplace_back(r, y);   // 넓은콘 실장애물점(벽 실폭 측정용)
                // 좁은 전방 콘: ±~25°(0.47r) 이되 절대폭 상한으로 묶음. 원거리(트리거 5m)서
                // 콘이 넓어져(±2.3m@5m) 코너형상·옆 팀원을 오감지하던 헛스워브 방지 — 멀리서도
                // 경로 정면(±forward_cone_half_w)만 본다.
                if (std::abs(y) > std::min(0.47 * r, forward_cone_half_w_)) continue;
                pts.emplace_back(r, y);
            }
        } catch (...) {
            return;
        }
        // 최근접부터, [r, r+0.4] 안에 min_cluster 이상 점이 모이는 첫 거리 = 실장애물.
        double best = 1e9, best_lat = 0.0, lat_lo = 0.0, lat_hi = 0.0;
        if (!pts.empty()) {
            std::sort(pts.begin(), pts.end());
            for (std::size_t i = 0; i < pts.size(); ++i) {
                const double r0 = pts[i].first;
                int cnt = 0; double latsum = 0.0, lmin = 1e9, lmax = -1e9;
                for (std::size_t j = i; j < pts.size() && pts[j].first <= r0 + 0.4; ++j) {
                    ++cnt; latsum += pts[j].second;
                    lmin = std::min(lmin, pts[j].second);
                    lmax = std::max(lmax, pts[j].second);
                }
                if (cnt >= min_cluster_pts_) {
                    best = r0; best_lat = latsum / cnt; lat_lo = lmin; lat_hi = lmax; break;
                }
            }
        }
        obstacle_dist_ = best;
        obstacle_lat_ = best_lat;
        obstacle_lat_lo_ = lat_lo;   // 장애물 우측 끝(y 최소)
        obstacle_lat_hi_ = lat_hi;   // 장애물 좌측 끝(y 최대) — 폭 적응 클리어런스용
        // ★실폭: 감지 거리대(best±) 넓은콘 점들의 횡범위 = 벽/장애물의 실제 가로폭.
        //   좁은 전방콘 폭(obstacle_lat_hi_-lo_)은 ±forward_cone_half_w 로 묶여 최대 ~2m 라
        //   벽 판별 불가 → 넓은콘 실폭으로 wide_obs(planner 우회) 판정.
        double wlo = 1e9, whi = -1e9;
        if (best < 1e8) {
            for (const auto &w : wpts) {
                if (w.first >= best - 0.5 && w.first <= best + 0.8) {
                    wlo = std::min(wlo, w.second);
                    whi = std::max(whi, w.second);
                }
            }
        }
        obstacle_wide_span_ = (whi > wlo) ? (whi - wlo) : 0.0;
        left_clear_ = lclear;
        right_clear_ = rclear;
        // 내 라이다 감지 장애물을 world 좌표로 팀원에 공유(echo 방지: 내 것만 발행).
        publish_own_obstacle();
        // 팀원이 공유한 장애물을 합산 → 내 라이다가 아직 못 본 박스도 선제 회피.
        fold_shared_obstacles();
    }

    // 자기 라이다 감지 장애물(obstacle_dist_/lat_)을 world(map) 좌표로 변환해 공유 버스에 발행.
    void publish_own_obstacle()
    {
        if (!obstacle_pub_ || !own_pose_valid_) return;
        if (obstacle_dist_ > obstacle_detect_range_) return; // 유효 감지만(없으면 미발행)
        // 거짓 전파 방지: 지속확인된 장애물만 공유(순간 블립이 팀원에 전파돼 연쇄 헛스워브 방지).
        if (obstacle_persist_count_ < 2) return;
        const double cy = std::cos(own_yaw_), sy = std::sin(own_yaw_);
        geometry_msgs::msg::PointStamped o;
        o.header.stamp = this->now();
        o.header.frame_id = std::to_string(robot_id_);
        o.point.x = own_map_x_ + cy * obstacle_dist_ - sy * obstacle_lat_;   // worldX
        o.point.y = own_map_y_ + sy * obstacle_dist_ + cy * obstacle_lat_;   // worldY
        o.point.z = std::max(0.3, 0.5 * (obstacle_lat_hi_ - obstacle_lat_lo_));  // 반경
        obstacle_pub_->publish(o);
    }

    // 팀원 장애물 보고 수신 → world 좌표로 중복병합 저장(내 것은 echo 무시).
    void on_shared_obstacle(const geometry_msgs::msg::PointStamped::SharedPtr m)
    {
        if (std::atoi(m->header.frame_id.c_str()) == static_cast<int>(robot_id_)) return;
        const double t = this->now().seconds();
        for (auto &o : shared_obstacles_) {
            if (std::hypot(o.x - m->point.x, o.y - m->point.y) < shared_merge_dist_) {
                o.x = 0.5 * (o.x + m->point.x); o.y = 0.5 * (o.y + m->point.y);
                o.radius = std::max(o.radius, m->point.z); o.stamp = t;
                return;
            }
        }
        shared_obstacles_.push_back({m->point.x, m->point.y, m->point.z, t});
        if (shared_obstacles_.size() > 20) shared_obstacles_.erase(shared_obstacles_.begin());
    }

    // 팀원이 공유한 world 장애물을 내 body-frame 으로 투영, 전방 콘 안·더 가까우면 채택.
    // → 내 라이다가 아직 못 본(가려진/먼) 박스도 팀원 정보로 미리 회피.
    void fold_shared_obstacles()
    {
        if (!own_pose_valid_) return;
        const double t = this->now().seconds();
        const double cy = std::cos(own_yaw_), sy = std::sin(own_yaw_);
        for (const auto &o : shared_obstacles_) {
            if (t - o.stamp > shared_obstacle_ttl_s_) continue;   // 만료
            const double dx = o.x - own_map_x_, dy = o.y - own_map_y_;
            const double bx = cy * dx + sy * dy;    // 전방(+)
            const double by = -sy * dx + cy * dy;   // 좌(+)
            if (bx <= 0.15) continue;               // 뒤/옆은 무시
            const double r = std::hypot(bx, by);
            if (r > obstacle_detect_range_) continue;
            if (std::abs(by) > std::min(0.47 * r, forward_cone_half_w_) + o.radius) continue;
            if (r < obstacle_dist_) {   // 더 가까운 위협 → 채택(라이다 미감지여도)
                obstacle_dist_ = r;
                obstacle_lat_ = by;
                obstacle_lat_lo_ = by - o.radius;
                obstacle_lat_hi_ = by + o.radius;
            }
        }
    }

    // ---- 리더 form-up 게이트 ----
    // ---- 하트비트 기반 라이브 팔로워 멤버십 ----
    // 하트비트 = /swarm/robot_world_pos (swarm_lidar_filter 가 전 로봇에서 상시 발행,
    // world_seen_[id] 에 마지막 수신시각). 로봇 간 거리 증감으로 mesh 링크가 끊겼다
    // 붙었다 하는 환경 전제: 배리어(form-up/재정렬)는 '지금 연결된' 팔로워만 게이트.
    bool followerAlive(int t_id, double t_now_s) const
    {
        const auto st = world_seen_.find(t_id);
        return st != world_seen_.end() && (t_now_s - st->second) <= follower_lost_s_;
    }

    // 팔로워 로스터 편입(멱등). 초기 formation_followers 목록 + 주행 중 하트비트로 새로
    // 잡힌 로봇 모두 이 경로로 등록 → ready 구독이 동적으로 생긴다(재접속 즉시 재게이트).
    void ensure_follower_registered(int t_id)
    {
        if (!is_leader_) return;
        if (t_id <= 0 || t_id == static_cast<int>(robot_id_) ||
            t_id == static_cast<int>(leader_robot_id_)) return;
        if (follower_ready_.count(t_id)) return;
        if (std::find(formation_followers_.begin(), formation_followers_.end(),
                      static_cast<int64_t>(t_id)) == formation_followers_.end()) {
            formation_followers_.push_back(t_id);
            RCLCPP_INFO(this->get_logger(),
                        "[Formation] 신규 팔로워 s%d 하트비트 감지 — 라이브 로스터 편입(현재 %zu대)",
                        t_id, formation_followers_.size());
        }
        follower_ready_[t_id] = false;
        const std::string topic = "/s" + std::to_string(t_id) + "/formation_ready";
        ready_subs_.push_back(this->create_subscription<std_msgs::msg::Bool>(
            topic, rclcpp::QoS(1).transient_local(),
            [this, t_id](std_msgs::msg::Bool::SharedPtr m){
                follower_ready_[t_id] = m->data;
                maybe_start_after_formation();
            }));
    }

    bool all_followers_ready() const
    {
        const double now_s = this->now().seconds();
        for (const int64_t id : formation_followers_) {
            if (!followerAlive(static_cast<int>(id), now_s)) {
                // 링크다운: form-up 시작 후 유예 동안은 부팅/재접속을 기다리고,
                // 그 후엔 게이트에서 제외(연결된 로봇만으로 진행 — 영원대기 방지).
                if (now_s - form_up_t0_ < formup_grace_s_) return false;
                RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 5000,
                    "[form-up] s%ld 링크다운(하트비트 %.1fs+ 무신호) — 게이트에서 제외하고 진행",
                    static_cast<long>(id), follower_lost_s_);
                continue;
            }
            const auto it = follower_ready_.find(static_cast<int>(id));
            if (it == follower_ready_.end() || !it->second) {
                return false;
            }
        }
        return true;
    }

    // 팔로워 ready 변화 시 호출. form-up 대기 중이고 전부 준비되면 그때 출발.
    void maybe_start_after_formation()
    {
        if (!waiting_for_formation_) return;
        // 전환 재정렬 중이면 전 팔로워 '재정렬 완료'를, 평소 form-up 이면 'ready'를 게이트로.
        const bool ready = reform_in_progress_ ? all_followers_reform_done()
                                               : all_followers_ready();
        if (!ready) return;
        const bool was_reform = reform_in_progress_;
        waiting_for_formation_ = false;
        reform_in_progress_ = false;
        // 전환 재정렬 후 재개: 팔로워는 *이미 새 대형 슬롯에 정렬*돼 있음 → form-up 배리어
        // 재실행은 불필요(25s 낭비 → 완주 실패). 리더가 즉시 순항 재개하면 팔로워는
        // leader_arc>0 보고 자동으로 form-up 탈출해 따라옴.
        resume_after_reform_ = was_reform;
        RCLCPP_INFO(this->get_logger(),
                    "[Formation] 전 팔로워 %s 완료 → 미션 출발",
                    was_reform ? "재정렬" : "form-up");
        start_mission(held_path_);
        held_path_.clear();
    }

    // 전 팔로워 재정렬 완료 여부(리더 재개 게이트). stale(>3s) 은 미완료로 간주(데드락은 팔로워
    // 자체 타임아웃이 풀어줌).
    bool all_followers_reform_done() const
    {
        const double now_s = this->now().seconds();
        for (const int64_t id : formation_followers_) {
            // ★링크다운(하트비트 자체가 끊김) → 재정렬 게이트에서 제외. 연결된 팔로워만
            //   기다린다(주행 중 이탈 로봇이 대형전환을 영원히 막는 것 방지). 살아있는데
            //   reform 보고만 stale(>3s)이면 기존대로 미완료(블록) 유지.
            if (!followerAlive(static_cast<int>(id), now_s)) {
                RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 5000,
                    "[재정렬] s%ld 링크다운 — 재정렬 게이트에서 제외", static_cast<long>(id));
                continue;
            }
            const auto it = reform_done_peers_.find(static_cast<int>(id));
            const auto st = reform_seen_.find(static_cast<int>(id));
            if (it == reform_done_peers_.end() || it->second < 0.5) return false;
            if (st == reform_seen_.end() || now_s - st->second > 3.0) return false;
        }
        return true;
    }

    // ---- 제곱거리 최적 슬롯 배정 ----
    // 모든 (fresh) 팔로워를 슬롯 rank 1..N 에 제곱거리 합 최소로 배정(순열 전수, ≤5대).
    // 제곱거리여야 직선경로 비교차 보장(CAPT). 동일 입력→동일 결과(분산 일관). latch 됨.
    void computeAssignment()
    {
        double Lx, Ly, h;
        if (!leaderFrame(&Lx, &Ly, &h)) return;   // 리더프레임 미가용 → static fallback 유지
        const double now_s = this->now().seconds();
        std::vector<int> fids;
        for (const auto &kv : world_pos_) {
            if (kv.first == static_cast<int>(leader_robot_id_)) continue;
            const auto st = world_seen_.find(kv.first);
            if (st == world_seen_.end() || now_s - st->second > 2.0) continue;  // stale 제외
            fids.push_back(kv.first);
        }
        const int n = static_cast<int>(fids.size());
        if (n == 0) return;
        std::sort(fids.begin(), fids.end());
        // 슬롯 world 좌표(rank 1..n).
        const uint8_t geom = geometryForMode(formation_type_, formation_number_);
        const double ch = std::cos(h), sh = std::sin(h);
        std::vector<std::pair<double, double>> slot(n);
        for (int r = 1; r <= n; ++r) {
            const FormationSlot s = formationSlotFor(geom, r);
            const double cx = s.cross * formation_lateral_spacing_m_;
            const double ax = s.along * formation_lateral_spacing_m_;
            slot[r - 1] = {Lx + ch * ax - sh * cx, Ly + sh * ax + ch * cx};
        }
        // 순열 전수 → 제곱거리 합 최소 배정.
        std::vector<int> perm(n), best(n);
        for (int i = 0; i < n; ++i) perm[i] = i;
        best = perm; double bestc = 1e18;
        do {
            double c = 0.0;
            for (int i = 0; i < n; ++i) {
                const auto &fp = world_pos_[fids[i]];
                const double dx = fp.first - slot[perm[i]].first;
                const double dy = fp.second - slot[perm[i]].second;
                c += dx * dx + dy * dy;
            }
            if (c < bestc) { bestc = c; best = perm; }
        } while (std::next_permutation(perm.begin(), perm.end()));
        assignment_.clear();
        assignment_[static_cast<int>(leader_robot_id_)] = 0;
        for (int i = 0; i < n; ++i) assignment_[fids[i]] = best[i] + 1;
        assignment_valid_ = true;
        const auto me = assignment_.find(static_cast<int>(robot_id_));
        RCLCPP_INFO(this->get_logger(), "[배정] 제곱거리 최적 슬롯 배정 완료 — 내 rank=%d (팔로워 %d대)",
                    me != assignment_.end() ? me->second : -1, n);
    }
    // 슬롯 rank = id 순 고정(사용자 지정: s1→s2→s3 순으로 정렬). 즉 column 순서는 항상
    // 낮은 id가 앞. "최적화"는 슬롯 재배정이 아니라 그 id-슬롯으로 가는 동선/타이밍에 둔다.
    // (헝가리안 computeAssignment 는 거리최적이라 id순서를 뒤집어 사용자 의도와 반대 → 미사용.)
    int assignedRank() const
    {
        return static_cast<int>(robot_id_) - static_cast<int>(leader_robot_id_);
    }
    int assignedRankOf(int id) const
    {
        return id - static_cast<int>(leader_robot_id_);
    }

    // 임의 rank 의 슬롯 world 좌표 + 도착 헤딩. 리더 world 위치 + 슬롯오프셋(리더경로 접선 회전).
    // 리더가 정지해도 leader_ref_path_ 접선은 안정적이라 헤딩 결정적. 각 로봇이 동일 입력
    // (공유 world_pos_ + 공유 leader_ref_path_)으로 임의 rank 슬롯을 계산 → 직렬화 교차판단 일관.
    bool compute_slot_world(int rank, double *sx, double *sy, double *syaw) const
    {
        const auto &P = leader_ref_path_.poses;
        if (P.size() < 2) return false;
        double Lx, Ly;
        {
            const auto lw = world_pos_.find(static_cast<int>(leader_robot_id_));
            if (lw == world_pos_.end()) return false;
            Lx = lw->second.first; Ly = lw->second.second;   // 리더 현재(live) 위치 기준
        }
        std::size_t imin = 0; double dmin = 1e18;
        for (std::size_t i = 0; i < P.size(); ++i) {
            const double dx = P[i].pose.position.x - Lx, dy = P[i].pose.position.y - Ly;
            const double d = dx * dx + dy * dy;
            if (d < dmin) { dmin = d; imin = i; }
        }
        const std::size_t j = std::min(imin + 4, P.size() - 1);
        double hx = P[j].pose.position.x - P[imin].pose.position.x;
        double hy = P[j].pose.position.y - P[imin].pose.position.y;
        if (std::hypot(hx, hy) < 1e-3 && imin > 0) {
            hx = P[imin].pose.position.x - P[imin - 1].pose.position.x;
            hy = P[imin].pose.position.y - P[imin - 1].pose.position.y;
        }
        const double h = std::atan2(hy, hx);
        const double ch = std::cos(h), sh = std::sin(h);
        const uint8_t geom = geometryForMode(formation_type_, formation_number_);
        const FormationSlot slot = formationSlotFor(geom, rank);
        // 전체 슬롯(cross 횡 + along 종)으로 정확히 배치 — 각 로봇이 '서로 다른' 슬롯에 가야
        // (예: column 은 같은 x=0 라인이라 앞뒤 along 간격이 없으면 같은 점에 몰려 충돌).
        const double cx = slot.cross * formation_lateral_spacing_m_;   // 좌(+)
        const double ax = slot.along * formation_lateral_spacing_m_;   // 전(+; 보통 음수=뒤)
        *sx = Lx + ch * ax + (-sh) * cx;
        *sy = Ly + sh * ax + ( ch) * cx;
        *syaw = h;
        return true;
    }
    // anchor(또는 live) 리더프레임 (Lx,Ly,heading) 추출. compute_slot_world 와 동일 규칙.
    bool slotFrame(double *Lx, double *Ly, double *h) const
    {
        const auto &P = leader_ref_path_.poses;
        if (P.size() < 2) return false;
        // 리더 현재(live) world 위치 기준 — 슬롯/재배치를 현 위치에서 계산(출발점 latch 아님).
        // 리더가 form-up 중 제자리 회전만 하므로(드리프트 없음) 안정적이고, 출발 후엔 live 추적.
        const auto lw = world_pos_.find(static_cast<int>(leader_robot_id_));
        if (lw == world_pos_.end()) return false;
        *Lx = lw->second.first; *Ly = lw->second.second;
        std::size_t imin = 0; double dmin = 1e18;
        for (std::size_t i = 0; i < P.size(); ++i) {
            const double dx = P[i].pose.position.x - *Lx, dy = P[i].pose.position.y - *Ly;
            const double d = dx * dx + dy * dy;
            if (d < dmin) { dmin = d; imin = i; }
        }
        const std::size_t j = std::min(imin + 4, P.size() - 1);
        double hx = P[j].pose.position.x - P[imin].pose.position.x;
        double hy = P[j].pose.position.y - P[imin].pose.position.y;
        if (std::hypot(hx, hy) < 1e-3 && imin > 0) {
            hx = P[imin].pose.position.x - P[imin - 1].pose.position.x;
            hy = P[imin].pose.position.y - P[imin - 1].pose.position.y;
        }
        *h = std::atan2(hy, hx);
        return true;
    }
    // 2단계 staging: 현재 횡오프셋 유지한 채 목표 깊이(along)로. column 뒤 슬롯(s3)이
    // 앞 주차차(s2)·리더를 지나치지 않게 — 먼저 깊이로 빠진 뒤 중앙선 슬라이드.
    bool compute_stage_slot(double *sx, double *sy, double *syaw) const
    {
        double Lx, Ly, h;
        if (!own_pose_valid_ || !slotFrame(&Lx, &Ly, &h)) return false;
        const double ch = std::cos(h), sh = std::sin(h);
        const double ax = mySlot().along * formation_lateral_spacing_m_;   // 목표 깊이
        // staging 횡 = abreast 슬롯(고정). live 현재 횡을 쓰면 로봇이 안쪽으로 조금 틀 때
        // staging 도 따라 안쪽 이동 → 중앙선으로 나선 진입(앞차 스침). 고정해야 직진 후진.
        const double cross_stage =
            formationSlotFor(FORMATION_LINE_ABREAST, assignedRank()).cross *
            formation_lateral_spacing_m_;
        *sx = Lx + ch * ax + (-sh) * cross_stage;
        *sy = Ly + sh * ax + ( ch) * cross_stage;
        *syaw = h;
        return true;
    }
    // 현재 깊이(along)가 목표 깊이에 도달했는가(2단계: 깊이 먼저 → 중앙선).
    bool atTargetDepth() const
    {
        double Lx, Ly, h;
        if (!own_pose_valid_ || !slotFrame(&Lx, &Ly, &h)) return true;
        const double along_c = (own_map_x_ - Lx) * std::cos(h) + (own_map_y_ - Ly) * std::sin(h);
        const double ax = mySlot().along * formation_lateral_spacing_m_;
        return std::abs(along_c - ax) < 0.8;
    }
    // 내 새 슬롯(배정 rank).
    bool compute_reform_slot(double *sx, double *sy, double *syaw) const
    {
        return compute_slot_world(assignedRank(), sx, sy, syaw);
    }

    // 두 선분(a0→a1, b0→b1) 의 최단거리. 시간기반 다음위치 예측의 기하 프록시(두 로봇이 각자
    // 슬롯으로 직선 이동 시 가장 가까워지는 거리). thresh 미만이면 경로가 '겹친다'고 판단.
    static double segMinDist(double ax0,double ay0,double ax1,double ay1,
                             double bx0,double by0,double bx1,double by1)
    {
        // 두 선분 최단거리: 샘플링(11점)으로 충분(슬롯 이동거리 짧음).
        double best = 1e18;
        for (int i = 0; i <= 10; ++i) {
            const double s = i / 10.0;
            const double px = ax0 + (ax1 - ax0) * s, py = ay0 + (ay1 - ay0) * s;
            for (int k = 0; k <= 10; ++k) {
                const double t = k / 10.0;
                const double qx = bx0 + (bx1 - bx0) * t, qy = by0 + (by1 - by0) * t;
                best = std::min(best, std::hypot(px - qx, py - qy));
            }
        }
        return best;
    }
    // 주어진 rank 의 3단계 staging 횡(off-centerline 측면). 충돌-인지 직렬화용 — 두 차의
    // staging 측면이 반대(부호 반대)면 재배치 경로가 좌우로 갈라져 안 겹침 → 동시 진행 가능.
    // (column 처럼 중앙선 슬롯은 직선프록시가 거짓 교차 판정 → 측면부호로 보정.)
    double stagingCrossFor(int rank) const
    {
        const uint8_t geom = geometryForMode(formation_type_, formation_number_);
        const double cross_t = formationSlotFor(geom, rank).cross * formation_lateral_spacing_m_;
        const double clear = formation_lateral_spacing_m_;
        if (std::abs(cross_t) >= clear - 0.1) return cross_t;   // 이미 측면 슬롯
        return formationSlotFor(FORMATION_LINE_ABREAST, rank).cross *
               formation_lateral_spacing_m_;                    // 중앙선 슬롯 → abreast 측면 staging
    }
    // form-up 직렬화: 내 슬롯경로가 '더 높은 우선순위' 팔로워의 슬롯경로와 차폭 클리어런스
    // 안에서 겹칠 것 같으면 그 로봇이 슬롯 도착할 때까지 대기. 안 겹치면 동시 진행(빠름).
    //  우선순위 = ① 더 깊은 슬롯(더 음수 along) 먼저 — column 등에서 뒤쪽부터 채워야
    //              나중 차가 먼저 도착한(앞쪽) 차를 통과하지 않음. ② 동률이면 낮은 id 먼저.
    //  보수적: 상대 위치를 못 보거나(stale) 슬롯계산 실패면 '대기'(blind 진행=충돌).
    bool formup_can_proceed()
    {
        if (!own_pose_valid_) return false;
        double msx, msy, msyaw;
        if (!compute_reform_slot(&msx, &msy, &msyaw)) return false;
        const int my_id = static_cast<int>(robot_id_);
        const int lead = static_cast<int>(leader_robot_id_);
        const double now_s = this->now().seconds();
        // 충돌-인지 직렬화: 더 높은 우선순위(낮은 id) 팔로워와 *재배치 경로가 실제로 겹칠
        // 때만* 그 차 먼저(직렬). 안 겹치면 동시 재배치 → reform 시간 ~절반(병렬화).
        //  · 3단계 staging 이 좌우 반대측으로 빠지는 대형(column 등)은 직선프록시도 보통 비교차.
        //  · 안전망: 동시 진행 중 근접하면 drive_reposition 의 우선순위-하드스톱이 후순위만 정지.
        for (const auto &kv : world_pos_) {
            const int j = kv.first;
            if (j == lead || j == my_id) continue;        // 리더·자기 제외
            if (j >= my_id) continue;                     // 나보다 높은 id → 무시(내가 먼저)
            const auto st = world_seen_.find(j);
            if (st == world_seen_.end() || now_s - st->second > 1.5) return false;  // stale → 대기
            const int jrank = assignedRankOf(j);          // 배정된 slot rank
            double jsx, jsy, jsyaw;
            if (!compute_slot_world(jrank, &jsx, &jsy, &jsyaw)) return false;
            const double jx = kv.second.first, jy = kv.second.second;
            if (std::hypot(jx - jsx, jy - jsy) < formation_tol_m_) continue;  // j 도착 → 무관
            // staging 측면이 반대면(column 좌우 교대 등) 재배치 경로가 갈라져 비교차 → 병렬.
            const double msc = stagingCrossFor(assignedRank());
            const double jsc = stagingCrossFor(jrank);
            if (msc * jsc < -0.1) continue;               // 반대 측면 → 충돌 없음 → 동시 진행
            // 같은 측면(또는 모호) → 직선프록시로 교차 확인. 겹치면 j 먼저(직렬).
            const double d = segMinDist(own_map_x_, own_map_y_, msx, msy, jx, jy, jsx, jsy);
            if (d < formup_clearance_m_) return false;    // 경로 교차 → j 먼저
        }
        return true;
    }

    void send_formup_goal(double x, double y, double yaw)
    {
        if (!nav_client_->wait_for_action_server(1s)) return;
        NavigateToPose::Goal goal;
        goal.pose.header.frame_id = map_frame_;
        goal.pose.header.stamp = this->now();
        goal.pose.pose.position.x = x;
        goal.pose.pose.position.y = y;
        goal.pose.pose.orientation.z = std::sin(yaw * 0.5);
        goal.pose.pose.orientation.w = std::cos(yaw * 0.5);
        auto opts = rclcpp_action::Client<NavigateToPose>::SendGoalOptions();
        opts.goal_response_callback =
            [this](GoalHandleNav::SharedPtr gh){ active_formup_goal_ = gh; };
        opts.result_callback =
            [this](const GoalHandleNav::WrappedResult &r){
                active_formup_goal_.reset();
                formup_nav_inflight_ = false;
                if (r.code != rclcpp_action::ResultCode::SUCCEEDED) {
                    formup_retry_t_ = this->now().seconds();   // 실패 → 잠시 후 재시도
                }
            };
        nav_client_->async_send_goal(goal, opts);
        formup_nav_inflight_ = true;
    }

    // 진행방향 전방, 같은 차로(횡 ±1.1m) 가장 가까운 팀원과의 중심간격 + 그 차 속도.
    // 반환 true=전방 같은차로에 차 있음. 순항 중 추돌 방지(adaptive cruise)용.
    bool forwardTeammateGap(double *gap, double *front_speed,
                            int *front_id = nullptr) const
    {
        const auto ow = world_pos_.find(static_cast<int>(robot_id_));
        if (ow == world_pos_.end() || !own_pose_valid_) return false;
        // ★전방축 = *경로축*(pathForwardDir), 자기 순간 헤딩(own_yaw_)이 아니다. own_yaw_ 는
        //   팔로워가 합류로 회전할 때 옆/뒤(정지 포함) 팀원을 '전방 같은차로'로 오탐 → 그 유령
        //   앞차(v=0)에 car-following 이 속도를 0 으로 강제 → along-track 페이싱(뒤처짐→가속)을
        //   덮어써 팔로워가 얼어붙던 근본버그(격리실험서 단일 팔로워 정지 실측). 경로축은 주행
        //   내내 안정(+x)이라 '진짜 경로상 앞'만 잡는다. along>0.3 으로 '옆(≈0)' 팀원 배제.
        double ux, uy;
        pathForwardDir(ux, uy);
        const double px = -uy, py = ux;
        const double now_s = this->now().seconds();
        double best = 1e18, fv = 0.0; bool found = false;
        for (const auto &kv : world_pos_) {
            if (kv.first == static_cast<int>(robot_id_)) continue;
            const auto st = world_seen_.find(kv.first);
            if (st == world_seen_.end() || now_s - st->second > 1.5) continue;  // stale 제외
            const double rx = kv.second.first - ow->second.first;
            const double ry = kv.second.second - ow->second.second;
            const double along = rx * ux + ry * uy;          // +면 전방(경로축)
            const double cross = rx * px + ry * py;
            const double dist = std::hypot(rx, ry);          // 실제 중심거리(along 투영 아님)
            if (along > 0.3 && std::abs(cross) < 1.6 && dist < best) {  // 경로상 앞 같은차로
                best = dist; found = true;
                const auto pv = vel_.find(kv.first);
                fv = (pv != vel_.end())
                     ? std::hypot(pv->second.first, pv->second.second) : 0.0;
                if (front_id) *front_id = kv.first;
            }
        }
        if (found) { *gap = best; *front_speed = fv; }
        return found;
    }

    // 가장 가까운 팀원과의 거리(world_pos_, stale>1.5s 제외). 못 보면 +inf.
    double nearestPeerDist() const
    {
        if (!own_pose_valid_) return 1e18;
        const double now_s = this->now().seconds();
        double best = 1e18;
        for (const auto &kv : world_pos_) {
            if (kv.first == static_cast<int>(robot_id_)) continue;
            const auto st = world_seen_.find(kv.first);
            if (st == world_seen_.end() || now_s - st->second > 1.5) continue;
            best = std::min(best,
                std::hypot(own_map_x_ - kv.second.first, own_map_y_ - kv.second.second));
        }
        return best;
    }

    // 더 높은 우선순위(낮은 id) 팀원 + 리더 중 가장 가까운 거리. 재배치 하드스톱용 —
    // 대칭 정지(둘이 마주보면 교착) 대신 *후순위(높은 id)만 양보*. 동순위/후순위는 무시하고
    // 내가 진행 → 병렬 재배치서도 데드락 없음(리더는 항상 회피).
    double nearestHigherPriorityPeerDist() const
    {
        if (!own_pose_valid_) return 1e18;
        const double now_s = this->now().seconds();
        const int my_id = static_cast<int>(robot_id_);
        const int lead = static_cast<int>(leader_robot_id_);
        double best = 1e18;
        for (const auto &kv : world_pos_) {
            const int j = kv.first;
            if (j == my_id) continue;
            if (j != lead && j >= my_id) continue;   // 후순위(높은 id) 무시; 리더는 항상 회피
            const auto st = world_seen_.find(j);
            if (st == world_seen_.end() || now_s - st->second > 1.5) continue;
            best = std::min(best,
                std::hypot(own_map_x_ - kv.second.first, own_map_y_ - kv.second.second));
        }
        return best;
    }

    // 리더 자기 경로(fp_last_path_) 초기 진행방위. 리더는 leader_ref_path_ 를 구독 안 하므로
    // (자기가 발행) leaderFrame 대신 자기 FollowPath 경로로 헤딩 산출. form-up 회전 목표용.
    bool leaderOwnPathHeading(double *h) const
    {
        const auto &P = fp_last_path_.poses;
        if (P.size() < 2 || !own_pose_valid_) return false;
        std::size_t imin = 0; double dmin = 1e18;
        for (std::size_t i = 0; i < P.size(); ++i) {
            const double dx = P[i].pose.position.x - own_map_x_;
            const double dy = P[i].pose.position.y - own_map_y_;
            const double d = dx * dx + dy * dy;
            if (d < dmin) { dmin = d; imin = i; }
        }
        const std::size_t j = std::min(imin + 4, P.size() - 1);
        const double hx = P[j].pose.position.x - P[imin].pose.position.x;
        const double hy = P[j].pose.position.y - P[imin].pose.position.y;
        if (std::hypot(hx, hy) < 1e-3) return false;
        *h = std::atan2(hy, hx);
        return true;
    }

    // 리더 world 프레임(위치 + 진행방위). compute_slot_world 와 동일 추출.
    bool leaderFrame(double *Lx, double *Ly, double *h) const
    {
        const auto lw = world_pos_.find(static_cast<int>(leader_robot_id_));
        if (lw == world_pos_.end()) return false;
        const auto &P = leader_ref_path_.poses;
        if (P.size() < 2) return false;
        *Lx = lw->second.first; *Ly = lw->second.second;
        std::size_t imin = 0; double dmin = 1e18;
        for (std::size_t i = 0; i < P.size(); ++i) {
            const double dx = P[i].pose.position.x - *Lx, dy = P[i].pose.position.y - *Ly;
            const double d = dx * dx + dy * dy;
            if (d < dmin) { dmin = d; imin = i; }
        }
        const std::size_t j = std::min(imin + 4, P.size() - 1);
        double hx = P[j].pose.position.x - P[imin].pose.position.x;
        double hy = P[j].pose.position.y - P[imin].pose.position.y;
        if (std::hypot(hx, hy) < 1e-3 && imin > 0) {
            hx = P[imin].pose.position.x - P[imin - 1].pose.position.x;
            hy = P[imin].pose.position.y - P[imin - 1].pose.position.y;
        }
        *h = std::atan2(hy, hx);
        return true;
    }

    // 부드러운 곡선 추종 primitive: (tx,ty) 로 '이동하면서 동시에 회전'(전/후진 짧은쪽).
    // 제자리회전-후-직진(뻣뻣) 대신 v∝cos(heading_err) 로 진행축이 어긋나면 감속·더 꺾어
    // 자연스런 호를 그림. 위치 도달 후 align_heading 이면 제자리 회전으로 헤딩 정렬.
    bool driveTo(double tx, double ty, bool align_heading, double final_heading)
    {
        auto wrap = [](double a){
            while (a > M_PI) a -= 2.0 * M_PI;
            while (a < -M_PI) a += 2.0 * M_PI; return a; };
        const double dx = tx - own_map_x_, dy = ty - own_map_y_;
        const double dist = std::hypot(dx, dy);
        const double pos_tol = std::min(0.35, formation_tol_m_ * 0.4);
        geometry_msgs::msg::Twist cmd;
        if (dist > pos_tol) {
            const double bearing = std::atan2(dy, dx);
            const double ang_fwd = wrap(bearing - own_yaw_);
            const bool reverse = std::abs(ang_fwd) > M_PI / 2.0;   // 목표가 뒤 → 후진
            const double travel_h = reverse ? wrap(bearing + M_PI) : bearing;
            const double herr = wrap(travel_h - own_yaw_);         // 진행축 정렬 오차
            // 이동+회전 동시: 어긋날수록 감속(0.2배 하한)해 호를 그림. 완전정지 안 함.
            const double scale = std::max(0.2, std::cos(herr));
            const double v = std::clamp(0.6 * dist, 0.12, base_speed_mps_) * scale;
            cmd.linear.x = reverse ? -v : v;
            // 곡선 조향. 후진 시엔 조향 부호 반전(후진 기하).
            cmd.angular.z = std::clamp((reverse ? -1.6 : 1.6) * herr, -0.9, 0.9);
            repos_cmd_pub_->publish(cmd);
            return false;
        }
        if (align_heading) {
            const double yaw_err = wrap(final_heading - own_yaw_);
            if (std::abs(yaw_err) > 0.12) {
                cmd.angular.z = std::clamp(1.8 * yaw_err, -0.7, 0.7);
                repos_cmd_pub_->publish(cmd);
                return false;
            }
        }
        repos_cmd_pub_->publish(cmd);   // 0 — 도달
        return true;
    }

    // form-up 정렬 컨트롤러(직접 cmd_vel). 사용자 지정 + 리더 충돌회피 2단계 경로:
    //   Phase1: 현재 횡오프셋 유지한 채 목표 깊이(along)로 후진 — 리더 근처 안 지나고 멀어짐.
    //   Phase2: 목표 깊이에서 중앙선/슬롯 횡으로 슬라이드 — 리더서 |along|만큼 떨어진 안전구역.
    //   Phase3: 제자리 회전으로 대형 헤딩 정렬.
    // 충돌안전: 호출 전 직렬화(formup_can_proceed) + 근접 하드스톱.
    // 반환: 슬롯 '위치' 도달(ready) 여부(사용자: '위치만' 게이트).
    bool drive_reposition(double sx, double sy, double syaw)
    {
        if (!repos_cmd_pub_ || !own_pose_valid_) return false;
        geometry_msgs::msg::Twist zero;
        // 근접 하드스톱(우선순위-인지): 더 높은 우선순위 차/리더에만 양보 정지 → 병렬 재배치서
        // 대칭 교착 없음. planner 가 팀원을 못 봐도 물리접촉 직전이면 정지.
        if (nearestHigherPriorityPeerDist() < formup_hardstop_m_) {
            repos_cmd_pub_->publish(zero);
            return false;
        }
        double Lx, Ly, h;
        if (!slotFrame(&Lx, &Ly, &h)) { repos_cmd_pub_->publish(zero); return false; }
        const double ch = std::cos(h), sh = std::sin(h);
        const FormationSlot slot = mySlot();
        const double cross_t = slot.cross * formation_lateral_spacing_m_;   // 목표 횡
        const double along_t = slot.along * formation_lateral_spacing_m_;   // 목표 깊이
        // 내 현재 리더-프레임 오프셋.
        const double rx = own_map_x_ - Lx, ry = own_map_y_ - Ly;
        const double along_c = rx * ch + ry * sh;
        const double cross_c = rx * (-sh) + ry * ch;
        // staging 횡 = 리더 중앙선에서 충분히 벗어난 측면. 목표가 이미 옆(|cross_t|≥간격)이면
        // 목표 측, 아니면(중앙선 슬롯) 내 abreast 측면(±간격). 이 측면에서 깊이를 바꿔야
        // 리더 중앙선을 안 통과(form-up·column↔wedge·column↔diamond 전부 안전).
        const double clear = formation_lateral_spacing_m_;   // 2m
        double cross_stage;
        if (std::abs(cross_t) >= clear - 0.1) {
            cross_stage = cross_t;
        } else {
            const double ab = formationSlotFor(FORMATION_LINE_ABREAST, assignedRank()).cross
                              * formation_lateral_spacing_m_;
            cross_stage = ab;   // ±간격(중앙선 벗어남)
        }
        // [진단] drive_reposition 런타임 값 — 왜 팔로워가 슬롯 대신 리더로 가는지 추적.
        RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 1500,
            "[repos] L=(%.2f,%.2f) h=%.0f own=(%.2f,%.2f) cross_c=%.2f along_c=%.2f "
            "stage=%.2f cross_t=%.2f along_t=%.2f xcross=%d bk=%d p1=%d st=%d",
            Lx, Ly, h * 180.0 / M_PI, own_map_x_, own_map_y_, cross_c, along_c,
            cross_stage, cross_t, along_t,
            static_cast<int>(cross_c * cross_stage < 0.0),
            static_cast<int>(formup_backed_),
            static_cast<int>(formup_phase1_done_), static_cast<int>(formup_staged_));
        auto world = [&](double cr, double al, double *x, double *y){
            *x = Lx + ch * al + (-sh) * cr; *y = Ly + sh * al + ( ch) * cr; };
        // ★ 리더 중앙선 횡단 필요 판정: 현재 횡과 staging 횡의 부호가 반대면 리더 앞을
        //   가로질러야 함(리더가 방향 반전 시 팔로워가 슬롯 반대편에 놓임). 이때 리더 깊이
        //   (along≈0)에서 횡단하면 근접 하드스톱(formup_hardstop_m_)에 걸려 리더 옆에서
        //   영구 프리즈 → form-up 데드락. 안전 깊이(리더 뒤 clearance)로 먼저 후퇴 후 횡단.
        const bool must_cross = (cross_c * cross_stage < 0.0);
        // 횡단 깊이 = 리더 뒤 clearance(하드스톱보다 큼). 이미 더 뒤면 현재 깊이 유지.
        const double along_stage = must_cross ? std::min(along_c, -(clear + 0.3)) : along_c;
        // Phase0: 횡단 필요 시, 현재 측면 유지한 채 안전 깊이(리더 뒤)로 후퇴 → 리더 회피.
        if (must_cross && !formup_backed_) {
            if (std::abs(along_c - along_stage) < 0.5) formup_backed_ = true;
            else { double tx, ty; world(cross_c, along_stage, &tx, &ty);
                   driveTo(tx, ty, false, 0.0); return false; }
        }
        // Phase1: (안전) 깊이에서 staging 측면으로 빠짐(리더 중앙선에서 이탈).
        if (!formup_phase1_done_) {
            if (std::abs(cross_c - cross_stage) < 0.5) formup_phase1_done_ = true;
            else { double tx, ty; world(cross_stage, along_stage, &tx, &ty);
                   driveTo(tx, ty, false, 0.0); return false; }
        }
        // Phase2: staging 측면에서 목표 깊이로 이동(리더 회피 — 옆으로 비껴 통과).
        if (!formup_staged_) {
            if (std::abs(along_c - along_t) < 0.6) formup_staged_ = true;
            else { double tx, ty; world(cross_stage, along_t, &tx, &ty);
                   driveTo(tx, ty, false, 0.0); return false; }
        }
        // Phase3: 최종 슬롯(목표 횡) + 헤딩 정렬.
        double fx, fy; world(cross_t, along_t, &fx, &fy);
        return driveTo(fx, fy, true, syaw);
    }

    void send_reform_goal(double x, double y, double yaw)
    {
        if (!nav_client_->wait_for_action_server(1s)) return;
        NavigateToPose::Goal goal;
        goal.pose.header.frame_id = map_frame_;
        goal.pose.header.stamp = this->now();
        goal.pose.pose.position.x = x;
        goal.pose.pose.position.y = y;
        goal.pose.pose.orientation.z = std::sin(yaw * 0.5);
        goal.pose.pose.orientation.w = std::cos(yaw * 0.5);
        auto opts = rclcpp_action::Client<NavigateToPose>::SendGoalOptions();
        opts.goal_response_callback =
            [this](GoalHandleNav::SharedPtr gh){ active_reform_goal_ = gh; };
        opts.result_callback =
            [this](const GoalHandleNav::WrappedResult &r){ reform_result_callback(r); };
        nav_client_->async_send_goal(goal, opts);
        reform_nav_inflight_ = true;
    }

    void reform_result_callback(const GoalHandleNav::WrappedResult &result)
    {
        active_reform_goal_.reset();
        reform_nav_inflight_ = false;
        if (!reforming_) return;
        if (result.code == rclcpp_action::ResultCode::SUCCEEDED) {
            reform_done_ = true;
            RCLCPP_INFO(this->get_logger(), "[Formation] 새 슬롯 도착 — 재정렬 완료");
        } else {
            reform_retry_t_ = this->now().seconds();   // 잠시 후 재시도
        }
    }

    // 매 sync_tick 호출. reform_ready 발행 + 재정렬 중이면 직렬 NavigateToPose. 반환 true =
    // 재정렬이 이 틱의 속도/제어를 점유(정지 유지).
    bool reform_tick()
    {
        if (reform_ready_pub_) {   // 리더·재정렬불필요·도착=done(1), 재정렬중=0
            geometry_msgs::msg::PointStamped r;
            r.header.stamp = this->now();
            r.header.frame_id = std::to_string(robot_id_);
            r.point.x = (is_leader_ || !reforming_ || reform_done_) ? 1.0 : 0.0;
            reform_ready_pub_->publish(r);
        }
        if (!reforming_) return false;
        const double now_s = this->now().seconds();
        if (now_s - reform_t0_ > reform_timeout_s_) {   // 데드락 방지
            reform_done_ = true;
            RCLCPP_WARN(this->get_logger(), "[Formation] 재정렬 타임아웃 — done 강제");
            if (active_reform_goal_) {
                cancel_goal_safe(nav_client_, active_reform_goal_); active_reform_goal_.reset();
            }
            reform_nav_inflight_ = false;
            return true;
        }
        geometry_msgs::msg::Twist zero;
        if (reform_done_) {   // 도착 — 정지 유지(cmd_vel_nav)
            if (repos_cmd_pub_) repos_cmd_pub_->publish(zero);
            return true;
        }
        // 직렬화: 예측 경로교차 시에만 상위 우선순위 먼저(form-up과 동일 conflict-aware).
        // column→wedge 처럼 좌우 반대편이면 충돌 안 해 동시 재배치 → 빠름(타임아웃 회피).
        if (!formup_can_proceed()) {
            RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 1500,
                "[reform진단] 직렬화 대기(상위 우선순위 경로교차)");
            if (repos_cmd_pub_) repos_cmd_pub_->publish(zero);
            return true;
        }
        double sx, sy, syaw;
        if (!compute_reform_slot(&sx, &sy, &syaw)) {   // 리더위치/경로 대기
            RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 1500,
                "[reform진단] compute_reform_slot 실패(리더위치/경로 대기)");
            if (repos_cmd_pub_) repos_cmd_pub_->publish(zero);
            return true;
        }
        // form-up 과 동일한 cmd_vel_nav 직접제어 재배치(2단계 staging·후진·제자리회전).
        RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 1500,
            "[reform진단] 재배치 호출 슬롯(%.1f,%.1f) 내위치(%.1f,%.1f) P1done=%d staged=%d",
            sx, sy, own_map_x_, own_map_y_, formup_phase1_done_, formup_staged_);
        if (drive_reposition(sx, sy, syaw)) reform_done_ = true;
        return true;
    }

    std::size_t current_waypoint_count() const
    {
        return active_ ? active_points_.size() : pending_path_.size();
    }

    uint16_t clamp_waypoint_count(std::size_t value) const
    {
        constexpr std::size_t max_u16 = 65535u;
        return static_cast<uint16_t>(std::min(value, max_u16));
    }

    float mission_progress_ratio() const
    {
        const std::size_t total = current_waypoint_count();
        if (total == 0) {
            return 0.0f;
        }
        const std::size_t completed = std::min(active_index_, total);
        return static_cast<float>(completed) / static_cast<float>(total);
    }

    // robot_server gates tablet mode changes on operation_state == IDLE. Report the
    // real state derived from the mission lifecycle: IDLE while idle/ready (so the
    // operator can start a mode), MOVE while a path is executing/paused, ERROR on fault.
    static uint8_t operation_state_from_mission(uint8_t mission_status)
    {
        using OperationState = combat_robot_msgs::msg::OperationState;
        switch (mission_status) {
            case OperationState::MISSION_MOVING:
            case OperationState::MISSION_PAUSED:
            case OperationState::MISSION_REACHED:
            case OperationState::MISSION_SURVEILLING:
                return OperationState::MOVE;
            case OperationState::MISSION_ERROR:
                return OperationState::ERROR;
            case OperationState::MISSION_NONE:
            case OperationState::MISSION_READY:
            default:
                return OperationState::IDLE;
        }
    }

    void publish_mission_status()
    {
        if (!mission_state_pub_) {
            return;
        }

        combat_robot_msgs::msg::OperationState msg;
        msg.state = operation_state_from_mission(mission_status_);
        // mission_control does not own the operation mode (RECON/PROTECT/...);
        // robot_server is the authority for active_mode_id, so report neutral here.
        msg.active_mode_id = combat_robot_msgs::msg::OperationState::ACTIVE_MODE_IDLE;
        msg.mission_status = mission_status_;
        msg.current_waypoint_index = clamp_waypoint_count(active_index_);
        msg.total_waypoints = clamp_waypoint_count(current_waypoint_count());
        msg.progress_ratio = mission_progress_ratio();
        msg.distance_to_next_wp_m = distance_to_next_wp_m_;
        msg.distance_to_goal_m = distance_to_goal_m_;
        msg.gps_lat = gps_lat_;
        msg.gps_lon = gps_lon_;
        msg.gps_heading = gps_heading_;
        msg.current_speed_mps = current_speed_;
        msg.error_code = mission_error_code_;
        mission_state_pub_->publish(msg);

        // FollowPath 경로 시각화 주기 재발행(rviz Path display 가 항상 최신을 받게).
        if (active_ && control_mode_ == "follow_path" && fp_path_pub_ &&
            !fp_last_path_.poses.empty()) {
            fp_last_path_.header.stamp = this->now();
            fp_path_pub_->publish(fp_last_path_);
        }
    }

    void update_mission_status(uint8_t status, uint8_t error_code = MISSION_ERROR_NONE)
    {
        mission_status_ = status;
        mission_error_code_ = error_code;
        publish_mission_status();
    }

    // ---------------- 태블릿 SwarmPathCommand ----------------
    void path_command_callback(const combat_robot_msgs::msg::SwarmPathCommand::SharedPtr msg)
    {
        if (dynamic_follow_) {
            // Dynamic follower ignores waypoint commands; it tracks the leader's
            // live pose (follow_tick). The leader still executes the path.
            return;
        }
        switch (msg->command) {
            case CMD_LOAD_PATH: {
                if (msg->path_json.empty()) {
                    RCLCPP_WARN(this->get_logger(), "LOAD_PATH payload 비어있음");
                    update_mission_status(
                        combat_robot_msgs::msg::OperationState::MISSION_ERROR,
                        MISSION_ERROR_INVALID_PATH_PAYLOAD);
                    return;
                }
                auto pts = parsePathJson(msg->path_json);
                if (pts.empty()) {
                    RCLCPP_WARN(this->get_logger(), "LOAD_PATH path_json 에서 waypoint 추출 실패");
                    update_mission_status(
                        combat_robot_msgs::msg::OperationState::MISSION_ERROR,
                        MISSION_ERROR_INVALID_PATH_PAYLOAD);
                    return;
                }
                pending_path_ = std::move(pts);
                start_consumed_ = false;   // 새 경로 → 다음 START 1회 처리
                // 새 경로 = 새 앵커 필요 → 이전 미션의 stale 앵커 무효화(안 그러면
                // 팔로워가 옛 리더위치로 form-up 가버림).
                anchor_valid_ = false;
                awaiting_anchor_ = false;
                if (!active_) {
                    active_index_ = 0;
                    distance_to_next_wp_m_ = 0.0f;
                    distance_to_goal_m_ = 0.0f;
                    update_mission_status(
                        combat_robot_msgs::msg::OperationState::MISSION_READY);
                } else {
                    publish_mission_status();
                }
                RCLCPP_INFO(this->get_logger(),
                            "[Tablet] LOAD_PATH 캐싱: %zu wps", pending_path_.size());
                break;
            }
            case CMD_START:
                if (start_consumed_) {
                    return;  // 같은 경로의 중복 START(-t 반복) 무시
                }
                start_consumed_ = true;
                if (pending_path_.empty()) {
                    RCLCPP_WARN(this->get_logger(), "[Tablet] START 받았으나 경로 없음");
                    update_mission_status(
                        combat_robot_msgs::msg::OperationState::MISSION_ERROR,
                        MISSION_ERROR_PATH_NOT_LOADED);
                    return;
                }
                // 주행 중 새 START → 현재 미션 정지 후 다시 form-up.
                if (active_) {
                    RCLCPP_INFO(this->get_logger(),
                                "[Formation] 새 명령 — 현재 미션 정지, 대형 재정렬 후 출발");
                    cancel_mission();
                }
                // 리더 현재 위치를 앵커로 경로 앞에 붙여 "리더 위치에서 정렬 후 목표로".
                begin_mission_with_anchor();
                break;
            case CMD_STOP:
                pending_path_.clear();
                cancel_mission();
                RCLCPP_INFO(this->get_logger(), "[Tablet] STOP");
                break;
            case CMD_PAUSE:
                if (!active_) {
                    RCLCPP_WARN(this->get_logger(), "[Tablet] PAUSE 받았으나 주행 중 아님");
                    return;
                }
                if (paused_) {
                    RCLCPP_WARN(this->get_logger(), "[Tablet] PAUSE 받았으나 이미 일시정지 상태");
                    return;
                }
                paused_ = true;
                // 현재 goal 을 취소 (active_index_ 는 유지 → RESUME 시 동일 wp 부터 재개).
                // goal 이 아직 accept 전이면 goal_response_callback 에서 즉시 cancel 처리됨.
                if (active_goal_) {
                    cancel_goal_safe(nav_client_, active_goal_);
                }
                update_mission_status(
                    combat_robot_msgs::msg::OperationState::MISSION_PAUSED);
                RCLCPP_INFO(this->get_logger(),
                            "[Tablet] PAUSE — wp[%zu/%zu] 에서 정지",
                            active_index_ + 1, active_points_.size());
                break;
            case CMD_RESUME:
                if (!active_) {
                    RCLCPP_WARN(this->get_logger(), "[Tablet] RESUME 받았으나 미션 없음");
                    return;
                }
                if (!paused_) {
                    RCLCPP_WARN(this->get_logger(), "[Tablet] RESUME 받았으나 일시정지 상태 아님");
                    return;
                }
                paused_ = false;
                update_mission_status(
                    combat_robot_msgs::msg::OperationState::MISSION_MOVING);
                RCLCPP_INFO(this->get_logger(),
                            "[Tablet] RESUME — wp[%zu/%zu] 부터 재개",
                            active_index_ + 1, active_points_.size());
                execute_next_waypoint();
                break;
            default:
                RCLCPP_WARN(this->get_logger(), "unknown path command: %u", msg->command);
                update_mission_status(
                    combat_robot_msgs::msg::OperationState::MISSION_ERROR,
                    MISSION_ERROR_INVALID_PATH_COMMAND);
        }
    }

    // ---------------- /mission_input 직접 입력 ----------------
    void mission_callback(const combat_robot_msgs::msg::WaypointList::SharedPtr msg)
    {
        if (dynamic_follow_) {
            return;  // follower tracks the leader, not direct waypoints
        }
        if (msg->waypoints.empty()) {
            RCLCPP_WARN(this->get_logger(), "/mission_input: 빈 waypoint");
            return;
        }
        std::vector<std::pair<double, double>> pts;
        pts.reserve(msg->waypoints.size());
        for (const auto &wp : msg->waypoints) {
            pts.emplace_back(wp.way_lat, wp.way_lon);
        }
        RCLCPP_INFO(this->get_logger(),
                    "/mission_input 수신: mission_id=%d, %zu wps",
                    msg->mission_id, pts.size());
        start_mission(pts);
    }

    // ---------------- 공통 진입점 ----------------
    // 경로 앞에 리더 앵커(GPS) 한 점을 붙인다 → 오프셋 적용 시 첫 목표가
    // (리더위치 + 자기슬롯) = 리더 위치에서의 form-up 슬롯이 된다.
    std::vector<std::pair<double, double>>
    prepend_anchor(const std::vector<std::pair<double, double>> &path) const
    {
        std::vector<std::pair<double, double>> out;
        out.reserve(path.size() + 1);
        out.emplace_back(anchor_lat_, anchor_lon_);
        for (const auto &p : path) out.push_back(p);
        return out;
    }

    // 리더 앵커 수신(팔로워). 대기 중이었다면 [앵커]+경로로 출발.
    void on_anchor(const geometry_msgs::msg::PointStamped::SharedPtr m)
    {
        anchor_lat_ = m->point.x;
        anchor_lon_ = m->point.y;
        anchor_valid_ = true;
        if (awaiting_anchor_) {
            awaiting_anchor_ = false;
            RCLCPP_INFO(this->get_logger(), "[Formation] 리더 앵커 수신 — form-up 출발");
            start_mission(prepend_anchor(pending_path_));
        }
    }

    // 스웜 전원이 wp 인덱스 k 이상 도달했나(배리어 통과 조건).
    bool all_reached(int k) const
    {
        if (reached_index_ < k) return false;
        for (const auto &kv : peer_progress_) {
            if (kv.first == static_cast<int>(robot_id_)) continue;
            const auto it = swarm_reached_.find(kv.first);
            if (it == swarm_reached_.end() || it->second < k) return false;
        }
        return true;
    }

    // 다른 로봇의 도달 인덱스 수신. 내가 배리어 대기 중이고 전원 도착했으면 출발.
    void on_ready(const geometry_msgs::msg::PointStamped::SharedPtr m)
    {
        const int id = std::atoi(m->header.frame_id.c_str());
        swarm_reached_[id] = static_cast<int>(std::lround(m->point.x));
        if (barrier_waiting_ && all_reached(reached_index_)) {
            barrier_waiting_ = false;
            RCLCPP_INFO(this->get_logger(),
                        "[Formation] 전원 정렬 완료(wp%d) — 함께 출발", reached_index_);
            ++active_index_;
            execute_next_waypoint();
        }
    }

    // 미션 출발(앵커 기반). 리더: 자기 위치를 GPS 앵커로 방송 후 [앵커]+경로로 출발.
    // 팔로워: 리더 앵커 대기 후 [앵커]+경로로 출발(팔로워 첫 목표=리더위치+슬롯=form-up).
    // 거버너가 팔로워 정렬될 때까지 리더를 사실상 대기시킨다.
    void begin_mission_with_anchor()
    {
        if (!formation_enabled_) {
            start_mission(pending_path_);
            return;
        }
        if (is_leader_) {
            if (!own_pose_valid_ || !to_ll_client_ ||
                !to_ll_client_->wait_for_service(1s)) {
                RCLCPP_WARN(this->get_logger(),
                            "리더 pose/toLL 미확보 — 앵커없이 출발");
                start_mission(pending_path_);
                return;
            }
            auto req = std::make_shared<robot_localization::srv::ToLL::Request>();
            req->map_point.x = own_map_x_;
            req->map_point.y = own_map_y_;
            req->map_point.z = 0.0;
            to_ll_client_->async_send_request(
                req,
                [this](rclcpp::Client<robot_localization::srv::ToLL>::SharedFuture fut) {
                    auto resp = fut.get();
                    anchor_lat_ = resp->ll_point.latitude;
                    anchor_lon_ = resp->ll_point.longitude;
                    anchor_valid_ = true;
                    geometry_msgs::msg::PointStamped a;
                    a.header.stamp = this->now();
                    a.header.frame_id = std::to_string(leader_robot_id_);
                    a.point.x = anchor_lat_;
                    a.point.y = anchor_lon_;
                    if (anchor_pub_) anchor_pub_->publish(a);
                    RCLCPP_INFO(this->get_logger(),
                                "[Formation] 리더 앵커=(%.7f, %.7f) 방송 — 대형 정렬 후 출발",
                                anchor_lat_, anchor_lon_);
                    start_mission(prepend_anchor(pending_path_));
                });
        } else {
            if (anchor_valid_) {
                start_mission(prepend_anchor(pending_path_));
            } else {
                awaiting_anchor_ = true;
                anchor_timeout_timer_ = this->create_wall_timer(5s, [this]() {
                    anchor_timeout_timer_->cancel();
                    if (awaiting_anchor_) {
                        awaiting_anchor_ = false;
                        RCLCPP_WARN(this->get_logger(),
                                    "리더 앵커 타임아웃 — 앵커없이 출발");
                        start_mission(pending_path_);
                    }
                });
                update_mission_status(
                    combat_robot_msgs::msg::OperationState::MISSION_READY);
            }
        }
    }

    void start_mission(const std::vector<std::pair<double, double>> &pts)
    {
        if (active_) {
            RCLCPP_WARN(this->get_logger(), "이미 미션 실행 중 — 새 요청 거절");
            return;
        }
        // nav2 가 아직 안 떴으면(느린 기동/리소스 경합) 영구실패 대신 재시도 — START 가
        // 일찍 와도 nav2 준비되면 자동 출발. (한 로봇만 못 떠서 멈추는 것 방지.)
        const bool fp_mode = (control_mode_ == "follow_path");
        const bool server_ready = fp_mode
            ? follow_path_client_->wait_for_action_server(2s)
            : nav_client_->wait_for_action_server(2s);
        if (!server_ready) {
            RCLCPP_WARN(this->get_logger(),
                        "Nav2 navigate_to_pose 아직 준비 안됨 — 2s 후 재시도");
            pending_start_path_ = pts;
            if (!retry_start_timer_) {
                retry_start_timer_ = this->create_wall_timer(2s, [this]() {
                    if (active_ || pending_start_path_.empty()) {
                        return;
                    }
                    const bool ready = (control_mode_ == "follow_path")
                        ? follow_path_client_->wait_for_action_server(std::chrono::milliseconds(200))
                        : nav_client_->wait_for_action_server(std::chrono::milliseconds(200));
                    if (ready) {
                        RCLCPP_INFO(this->get_logger(),
                                    "Nav2 준비 완료 — 보류된 미션 출발");
                        auto p = pending_start_path_;
                        pending_start_path_.clear();
                        start_mission(p);
                    }
                });
            }
            update_mission_status(
                combat_robot_msgs::msg::OperationState::MISSION_READY);
            return;
        }
        // 제곱거리 최적 슬롯 배정(현재 위치 기준). world_pos_ 미가용이면 static fallback,
        // form-up 중 lazy 재계산. 새 미션 = 새 배정.
        assignment_valid_ = false;
        computeAssignment();
        // 기준경로를 촘촘히 보간(출발 스크럽 드리프트가 명목 직선으로 빨리 복귀하도록).
        const auto dense_pts = densify_path(pts, formation_densify_m_);
        // 기준경로 → 내 편대 슬롯 경로로 변환(리더/비활성이면 그대로).
        const auto my_path = applyFormationOffset(dense_pts);
        if (formationSlotOffset() != 0 && formation_enabled_) {
            RCLCPP_INFO(this->get_logger(),
                        "편대 오프셋 적용: slot=%d lateral=%.2fm (%zu wps)",
                        formationSlotOffset(),
                        formationSlotOffset() * formation_lateral_spacing_m_,
                        my_path.size());
        }
        active_points_ = my_path;
        active_index_ = 0;
        active_ = true;
        paused_ = false;
        // 컨보이 상태 리셋: 새 미션 = 새 리더 궤적(리더는 breadcrumb 새로, 팔로워는
        // 이전 스테이지의 stale trace 폐기 — 리더가 1s 내 재방송).
        own_trace_.poses.clear();
        leader_trace_.poses.clear();
        convoy_sent_len_ = 0;
        convoy_turn_latched_ = false;   // 롤링 머지 출발순서 재평가
        gap_hold_ = false;
        wedge_reverse_ticks_ = 0;
        wedge_count_ = 0;
        // 출발 form-up: 전원 슬롯 정렬 후 동시 출발(특히 단종은 abreast 스폰→라인 정렬).
        // 단, 전환 재정렬 후 재개(resume_after_reform_)면 이미 대형이라 배리어 skip → 즉시 순항.
        // ★COLUMN(컨보이)은 롤링 머지(사용자 지시): 정지 정렬 없이 리더가 즉시 출발하고,
        //   팔로워는 정지 대기하다 리더가 자기 슬롯거리만큼 전진하면 뒤로 차례로 합류.
        //   (속도 governor 가 슬롯보다 앞이면 v=0 으로 잡아주고, 컨보이 경로는 trace 가
        //    슬롯거리+2m 이상 자라야 발급되므로 배리어/선배치 없이 순차 출발이 성립.)
        const bool rolling_merge_column =
            control_mode_ == "follow_path" && formation_enabled_ &&
            geometryForMode(formation_type_, formation_number_) == FORMATION_COLUMN;
        if (rolling_merge_column) {
            forming_up_ = false;
            RCLCPP_INFO(this->get_logger(),
                        "[컨보이] 롤링 머지 — 정지정렬 생략: 리더 즉시 출발, 팔로워 순차 합류");
        } else if (control_mode_ == "follow_path" && formation_enabled_ &&
                   !resume_after_reform_) {
            forming_up_ = true;
            form_up_t0_ = this->now().seconds();
            // ★ 새 form-up 시작: 리더의 팔로워 ready 상태를 모두 리셋 → 이전 미션의 stale
            //   ready(true)를 보고 즉시 출발하는 것 방지. 팔로워가 이번 슬롯에 실제 도착해
            //   fresh ready 를 보낼 때까지 진짜 대기. (조기출발→주행 중 캐치업으로 직선 흐트러짐 수정)
            for (auto &kv : follower_ready_) kv.second = false;
        }
        resume_after_reform_ = false;   // one-shot
        distance_to_next_wp_m_ = 0.0f;
        distance_to_goal_m_ = 0.0f;
        reached_index_ = -1;           // 배리어 상태 리셋
        barrier_waiting_ = false;
        swarm_reached_.clear();
        compute_cumulative_arc(dense_pts);   // 속도동기화용 arc 테이블 + 배리어(코너) 판정
        update_mission_status(
            combat_robot_msgs::msg::OperationState::MISSION_MOVING);
        if (fp_mode) {
            fp_map_points_.clear();
            fp_convert_index_ = 0;
            // ★[정리/platooning] convoy(COLUMN) 팔로워는 선행차 경로를 '연속' 추종해야 한다(경로
            //   완주로 정지하면 안 됨). 그래서 리더 기준경로(fp_original_length_ 진행추적 포함)를
            //   빌드하되, 진입은 대각으로 — build_follower_path_from_leader 가 자기 위치에서 라인으로
            //   대각 커넥터를 prepend 하므로 직각 횡이동 대신 lane-change 합류가 된다.
            if (is_leader_ || !formation_enabled_ || convoy_follower()) {
                // 리더/convoy: 자기 GPS→map 변환(체인) → 기준경로 빌드(진행추적 원본길이 확보).
                convert_next_fp_point();
            } else {
                // ★ [군집 init] 모든 팔로워: 주행경로를 먼저 만들지 않고, 리더 기준 절대슬롯에
                //   먼저 선배치(form-up)한다. 리더가 출발할 때 비로소 offset 주행경로를 빌드.
                //   (기존엔 후방슬롯만 선배치, abreast 는 offset-path 를 따라 슬롯 진입 →
                //    leader_ref_path 의존 + form-up 교착(s2 stuck)의 원인이었음. 이제 통일:
                //    slot 생성 → nav2 체인(cmd_vel_nav 직접제어, skid-steer 스크럽 회피)으로 진입 →
                //    전원 도착 후 리더 출발 → 그때 주행경로 생성.)
                //   leader_ref_path 는 리더가 START 시 transient_local 로 방송 → 출발 전 도착.
                formup_reposition_ = true;
                RCLCPP_INFO(this->get_logger(),
                            "[form-up] 슬롯 선배치 모드 (cross=%.1f along=%.1f)",
                            mySlot().cross, mySlot().along);
            }
        } else {
            execute_next_waypoint();
        }
    }

    // 팔로워: 리더 dense 기준경로에 cross(횡)+along(종) 오프셋만 적용 → 전원 리더와 동일 곡선
    // 기반(컬럼 cross=0=완전 동일). map-space 에서 처리(로봇별 GPS 변환차 제거).
    void build_follower_path_from_leader()
    {
        const auto &lp = leader_ref_path_.poses;
        if (lp.size() < 2) {
            RCLCPP_WARN(this->get_logger(), "리더경로 점부족 — GPS 변환 폴백");
            fp_map_points_.clear(); fp_convert_index_ = 0; convert_next_fp_point();
            return;
        }
        const FormationSlot slot = mySlot();
        const double cross_m = slot.cross * formation_lateral_spacing_m_;
        const double along_trunc = std::abs(slot.along * formation_lateral_spacing_m_); // 끝에서 자를 길이
        const int n = static_cast<int>(lp.size());
        // cross(횡) 수직 시프트만 적용 → 곡선 자체는 리더와 동일(컬럼 cross=0=완전 동일).
        // along(종 stagger)은 점별 시프트(코너 왜곡)가 아니라 '끝에서 잘라내기'로 → 곡선 보존,
        // 팔로워는 리더 골보다 along 만큼 앞서 끝남(뒤따르는 대형).
        std::vector<std::pair<double, double>> shifted;
        shifted.reserve(n);
        for (int i = 0; i < n; ++i) {
            const int a = std::max(0, i - 1), b = std::min(n - 1, i + 1);
            double tx = lp[b].pose.position.x - lp[a].pose.position.x;
            double ty = lp[b].pose.position.y - lp[a].pose.position.y;
            const double tl = std::hypot(tx, ty);
            if (tl > 1e-6) { tx /= tl; ty /= tl; }
            shifted.emplace_back(lp[i].pose.position.x + (-ty) * cross_m,
                                 lp[i].pose.position.y + (tx) * cross_m);
        }
        // along: 곡선을 arc 따라 |along| 뒤로 시프트 = 앞에 외삽 추가 + 끝에서 제거.
        // → 곡선 형상 동일(컬럼 완전 일치), 일정 간격 stagger, form-up 충돌 없음.
        fp_map_points_.clear();
        if (along_trunc > 0.1 && shifted.size() >= 2) {
            double tx = shifted[1].first - shifted[0].first;
            double ty = shifted[1].second - shifted[0].second;
            const double tl = std::hypot(tx, ty);
            if (tl > 1e-6) { tx /= tl; ty /= tl; }
            const int npre = static_cast<int>(std::ceil(along_trunc / 0.5));
            for (int k = npre; k >= 1; --k) {   // 시작 앞에 외삽
                fp_map_points_.emplace_back(shifted[0].first - tx * 0.5 * k,
                                            shifted[0].second - ty * 0.5 * k);
            }
            std::size_t keep = shifted.size();   // 끝에서 |along| 제거
            double acc = 0.0;
            for (std::size_t i = shifted.size() - 1; i > 1; --i) {
                acc += std::hypot(shifted[i].first - shifted[i-1].first,
                                  shifted[i].second - shifted[i-1].second);
                if (acc >= along_trunc) { keep = i; break; }
            }
            fp_map_points_.insert(fp_map_points_.end(),
                                  shifted.begin(), shifted.begin() + keep);
        } else {
            fp_map_points_ = shifted;   // 리더(또는 along=0)
        }
        RCLCPP_INFO(this->get_logger(),
                    "리더경로 기반 팔로워 경로 생성: %zu점 (cross=%.1f, arc시프트 %.1fm)",
                    fp_map_points_.size(), cross_m, along_trunc);
        last_slot_cross_ = cross_m;   // 횡변경 감지 기준 갱신(전환 시 재배치 판단)
        build_and_send_follow_path();   // 보간+코너검출+전송(이미 둥근 경로라 재라운딩 ~no-op)
    }

    // ===== FollowPath 모드 =====
    // 오프셋경로(active_points_, GPS)를 /fromLL 로 한 점씩 map 으로 변환(체인) → 전부 모이면
    // nav_msgs/Path 를 만들어 FollowPath 액션으로 보낸다(직접 추종, planner 우회).
    void convert_next_fp_point()
    {
        if (!active_) return;
        if (fp_convert_index_ >= active_points_.size()) {
            build_and_send_follow_path();
            return;
        }
        if (!from_ll_client_->wait_for_service(3s)) {
            RCLCPP_ERROR(this->get_logger(), "/fromLL 서비스 없음 (FollowPath 변환 실패)");
            active_ = false;
            update_mission_status(combat_robot_msgs::msg::OperationState::MISSION_ERROR);
            return;
        }
        const auto [lat, lon] = active_points_[fp_convert_index_];
        auto req = std::make_shared<robot_localization::srv::FromLL::Request>();
        req->ll_point.latitude = lat;
        req->ll_point.longitude = lon;
        req->ll_point.altitude = 0.0;
        from_ll_client_->async_send_request(req,
            [this](rclcpp::Client<robot_localization::srv::FromLL>::SharedFuture future){
                if (!active_) return;
                auto resp = future.get();
                fp_map_points_.emplace_back(resp->map_point.x, resp->map_point.y);
                ++fp_convert_index_;
                convert_next_fp_point();
            });
    }

    // 급코너(직각 등)를 반경 R 베지어 필렛으로 둥글게 — 로봇이 제자리회전 없이 부드럽게 통과.
    // 각 꼭짓점에서 양 세그먼트로 R 만큼 물러나 그 사이를 2차 베지어(꼭짓점=제어점)로 호 생성.
    std::vector<std::pair<double, double>>
    round_corners(const std::vector<std::pair<double, double>> &pts, double R) const
    {
        if (pts.size() < 3 || R < 0.1) return pts;
        std::vector<std::pair<double, double>> out;
        out.push_back(pts.front());
        for (std::size_t i = 1; i + 1 < pts.size(); ++i) {
            double ix = pts[i].first - pts[i-1].first, iy = pts[i].second - pts[i-1].second;
            double ox = pts[i+1].first - pts[i].first, oy = pts[i+1].second - pts[i].second;
            const double li = std::hypot(ix, iy), lo = std::hypot(ox, oy);
            if (li < 1e-3 || lo < 1e-3) { out.push_back(pts[i]); continue; }
            ix/=li; iy/=li; ox/=lo; oy/=lo;
            const double turn = std::abs(ang_diff(std::atan2(oy, ox), std::atan2(iy, ix)));
            if (turn < 0.3) { out.push_back(pts[i]); continue; }   // 완만 → 그대로
            const double cut = std::min({R, 0.45 * li, 0.45 * lo});
            const double ax = pts[i].first - ix*cut, ay = pts[i].second - iy*cut;
            const double bx = pts[i].first + ox*cut, by = pts[i].second + oy*cut;
            const int seg = std::max(5, static_cast<int>(cut * 4));
            for (int k = 0; k <= seg; ++k) {
                const double t = static_cast<double>(k) / seg, u = 1.0 - t;
                out.emplace_back(u*u*ax + 2*u*t*pts[i].first + t*t*bx,
                                 u*u*ay + 2*u*t*pts[i].second + t*t*by);
            }
        }
        out.push_back(pts.back());
        return out;
    }

    void build_and_send_follow_path()
    {
        if (!active_ || fp_map_points_.size() < 2) {
            RCLCPP_WARN(this->get_logger(), "FollowPath: 경로점 부족(%zu)", fp_map_points_.size());
            active_ = false;
            update_mission_status(combat_robot_msgs::msg::OperationState::MISSION_ERROR);
            return;
        }
        // 급코너 둥글게(필렛) — 제자리회전 제거, 부드러운 코너 주행. corner_arcs_ 는 원본 기준
        // 으로 먼저 잡고(아래), 라운딩된 점으로 dense path 생성.
        fp_map_points_ = round_corners(fp_map_points_, corner_radius_m_);
        // map 프레임에서 0.5m 간격으로 보간 → 부드러운 추종 경로. 길이도 계산(진행도/속도동기용).
        nav_msgs::msg::Path path;
        path.header.frame_id = map_frame_;
        path.header.stamp = this->now();
        fp_path_length_ = 0.0;
        auto add_pose = [&](double x, double y, double yaw){
            geometry_msgs::msg::PoseStamped ps;
            ps.header = path.header;
            ps.pose.position.x = x; ps.pose.position.y = y;
            ps.pose.orientation.z = std::sin(yaw * 0.5);
            ps.pose.orientation.w = std::cos(yaw * 0.5);
            path.poses.push_back(ps);
        };
        std::vector<double> vertex_arc(fp_map_points_.size(), 0.0);   // 각 꼭짓점 누적 arc
        for (std::size_t i = 1; i < fp_map_points_.size(); ++i) {
            const double x0 = fp_map_points_[i-1].first, y0 = fp_map_points_[i-1].second;
            const double x1 = fp_map_points_[i].first,   y1 = fp_map_points_[i].second;
            const double seg = std::hypot(x1-x0, y1-y0);
            const double yaw = std::atan2(y1-y0, x1-x0);
            const int n = std::max(1, static_cast<int>(seg / 0.5));
            for (int k = 0; k < n; ++k) {
                const double f = static_cast<double>(k) / n;
                add_pose(x0 + (x1-x0)*f, y0 + (y1-y0)*f, yaw);
            }
            fp_path_length_ += seg;
            vertex_arc[i] = fp_path_length_;
            if (i + 1 == fp_map_points_.size()) add_pose(x1, y1, yaw);  // 마지막 점
        }
        // 코너 arc 위치 기록 → 시간동기 코너 동시탈출용. 라운딩으로 코너가 여러 점에 퍼지므로
        // ~3m 윈도우 누적 방위변화로 감지(둥근 코너 전체를 1개로). 4m 이내 중복은 합침.
        corner_arcs_.clear();
        auto bearing = [&](std::size_t a, std::size_t b){
            return std::atan2(fp_map_points_[b].second - fp_map_points_[a].second,
                              fp_map_points_[b].first - fp_map_points_[a].first);
        };
        double last_corner = -1e9;
        for (std::size_t i = 1; i + 1 < fp_map_points_.size(); ++i) {
            double acc = 0.0, cum = 0.0;
            for (std::size_t j = i; j + 1 < fp_map_points_.size() && cum < 3.0; ++j) {
                acc += ang_diff(bearing(j, j + 1), bearing(j - 1, j));
                cum += std::hypot(fp_map_points_[j+1].first - fp_map_points_[j].first,
                                  fp_map_points_[j+1].second - fp_map_points_[j].second);
            }
            if (std::abs(acc) > 0.45 && vertex_arc[i] - last_corner > 4.0) {
                corner_arcs_.push_back(vertex_arc[i]);
                last_corner = vertex_arc[i];
            }
        }
        RCLCPP_INFO(this->get_logger(), "코너 %zu개 감지(라운딩 R=%.1fm)",
                    corner_arcs_.size(), corner_radius_m_);
        // 원본 미션 길이 기록(우회/재개에도 불변) — 진행도(fraction) 분모로 써서
        // 우회로 경로가 짧아져도 모든 로봇이 같은 스케일로 동기화되게 한다.
        fp_original_length_ = fp_path_length_;
        fp_start_time_ = this->now();   // 출발 grace 기준(스폰 settling 중 지면오감지 억제)
        fp_base_line_ = path;           // 컨보이 합성용 원본 라인(스워브/컨보이 덮어쓰기와 무관)
        // ★MPPI 재배치: 이미 경로를 한참 따라온 상태(우회 성공 후 rejoin, 안전망 재전송)에서
        //   원본 전체경로(맨 앞부터)를 새 goal 로 보내면, MPPI PathHandler 의 근접점 탐색이
        //   경로 시작(x≈0) 근처만 봐서 멀리 있는 로봇(x≈118)을 못 찾음 → "0 poses" abort 무한
        //   루프(벽 통과 후 정지 실측). 로봇 현위치 근처 pose 부터로 잘라 보내면 MPPI 가 즉시
        //   재배치. fp_original_length_(진행도 분모)·fp_base_line_ 는 전체 유지 → 진행도/컨보이
        //   불변(fp_dist_to_goal_ 는 잔여거리라 arc 정확). 초기 발행(로봇=시작점)엔 near≈0=무영향.
        if (own_pose_valid_ && path.poses.size() > 6) {
            std::size_t near = 0; double bd = 1e18;
            for (std::size_t i = 0; i < path.poses.size(); ++i) {
                const double dx = path.poses[i].pose.position.x - own_map_x_;
                const double dy = path.poses[i].pose.position.y - own_map_y_;
                const double d2 = dx*dx + dy*dy;
                if (d2 < bd) { bd = d2; near = i; }
            }
            if (near > 4) {   // 로봇이 경로 시작에서 >2m 진행 → 현위치 근처부터 전송
                const std::size_t from = near - 2;   // 1m 뒤부터(MPPI 근접점/프루닝 여유)
                nav_msgs::msg::Path trimmed;
                trimmed.header = path.header;
                trimmed.poses.assign(path.poses.begin() + from, path.poses.end());
                RCLCPP_INFO(this->get_logger(),
                            "재개 트림: near=%zu → %zu점 전송(원본 %zu점, MPPI 재배치)",
                            near, trimmed.poses.size(), path.poses.size());
                send_fp_path(trimmed);
                return;
            }
        }
        send_fp_path(path);
    }

    // 주어진 밀집 Path 를 FollowPath 로 전송(보관/시각화/길이계산 공통). build(보간) 과
    // 우회 후 재개(suffix 직접)에서 공유. fp_original_length_ 는 호출 전에 세팅됨.
    void send_fp_path(nav_msgs::msg::Path path)
    {
        if (path.poses.size() < 2) {
            active_ = false;
            update_mission_status(combat_robot_msgs::msg::OperationState::MISSION_ERROR);
            return;
        }
        double len = 0.0;
        for (std::size_t i = 1; i < path.poses.size(); ++i) {
            len += std::hypot(
                path.poses[i].pose.position.x - path.poses[i-1].pose.position.x,
                path.poses[i].pose.position.y - path.poses[i-1].pose.position.y);
        }
        fp_path_length_ = len;
        fp_dist_to_goal_ = len;
        fp_last_path_ = path;
        if (fp_path_pub_) fp_path_pub_->publish(path);
        RCLCPP_INFO(this->get_logger(),
                    "FollowPath 추종 시작: %zu 점, 길이 %.1fm (원본 %.1fm)",
                    path.poses.size(), len, fp_original_length_);

        // ★재전송(안전망/ABORTED재개)로 아직 살아있는 goal 을 preempt 하면, 옛 goal 이
        //   ABORTED 로 돌아온다. 그걸 '실제 abort'로 오인하면 또 재전송→또 preempt→…churn
        //   이 되어 MPPI 가 매번 재초기화(제어 트랜지언트=회피 후 yaw 떨림·비직선 추종 실측).
        //   preempt 상황이면 그 1회 결과를 무시(swerve/detour 가 이미 쓰는 패턴과 동일).
        if (active_fp_goal_) ignore_next_fp_result_ = true;
        FollowPath::Goal goal;
        goal.path = path;
        goal.controller_id = "FollowPath";
        goal.goal_checker_id = "general_goal_checker";
        auto opts = rclcpp_action::Client<FollowPath>::SendGoalOptions();
        opts.goal_response_callback =
            [this](GoalHandleFP::SharedPtr gh){ active_fp_goal_ = gh; };
        opts.feedback_callback =
            [this](GoalHandleFP::SharedPtr,
                   const std::shared_ptr<const FollowPath::Feedback> fb){
                fp_dist_to_goal_ = fb->distance_to_goal;
            };
        opts.result_callback =
            std::bind(&SwarmPathExecutorNode::fp_result_callback, this, _1);
        follow_path_client_->async_send_goal(goal, opts);
        // 리더-경로-공유: 리더가 경로 보낼 때마다(기준+스워브) 방송 → 팔로워 동일 곡선 추종.
        if (is_leader_ && leader_ref_path_pub_) {
            leader_ref_path_pub_->publish(path);
        }
    }

    // 단종(컬럼) 팔로워인가 — cross≈0(같은 라인) → 리더 경로(스워브 포함)를 그대로 추종.
    bool is_column_follower() const
    {
        return !is_leader_ && formation_enabled_ &&
               std::abs(mySlot().cross) < 0.1 &&
               std::abs(mySlot().along) > 0.1;
    }

    void fp_result_callback(const GoalHandleFP::WrappedResult &result)
    {
        // terminal 결과 = 그 handle 은 끝 — 어떤 분기로 나가든 리셋해야 goal 부재
        // 안전망이 stale handle 에 눈멀지 않는다(run24: ignore 로 결과 삼키며 미리셋
        // → '활성+goal있음' 착시 40분 방치 실측). 단 preempt 로 이미 새 goal 이
        // 들어온 경우를 지우지 않게 goal_id 일치 시에만.
        if (active_fp_goal_ && active_fp_goal_->get_goal_id() == result.goal_id) {
            active_fp_goal_.reset();
        }
        if (!active_) return;
        // 스워브/우회 위해 옛 goal 을 취소했을 때 그 terminal(보통 ABORTED/CANCELED) 1회 무시
        // — 안 그러면 ABORTED 가 NavigateToPose 폴백을 발동시켜 방금 보낸 스워브를 덮어씀.
        if (ignore_next_fp_result_) {
            ignore_next_fp_result_ = false;
            return;
        }
        if (result.code == rclcpp_action::ResultCode::SUCCEEDED) {
            // 컨보이 팔로워: 경로 끝 = 슬롯(리더 뒤)이라, 리더가 아직 미션 중이면 이건
            // '슬롯 도달'이지 미션 완료가 아님 — 대기하고 trace 성장 시 convoy_tick 이
            // 재전송. 미션 끝 판정 = trace 끝이 원본 라인 종점 부근(<3m).
            if (convoy_follower() && !fp_base_line_.poses.empty() &&
                !leader_trace_.poses.empty()) {
                const auto &te = leader_trace_.poses.back().pose.position;
                const auto &ge = fp_base_line_.poses.back().pose.position;
                if (std::hypot(te.x - ge.x, te.y - ge.y) > 3.0) {
                    RCLCPP_INFO(this->get_logger(),
                                "[컨보이] 슬롯 도달 — 리더 전진 대기(미션 계속)");
                    return;   // active_ 유지 → convoy_tick 이 이어감
                }
            }
            RCLCPP_INFO(this->get_logger(), "FollowPath 완료 — 미션 도착");
            active_ = false;
            detour_fail_streak_ = 0;
            fp_dist_to_goal_ = 0.0;
            update_mission_status(combat_robot_msgs::msg::OperationState::MISSION_REACHED);
        } else if (result.code == rclcpp_action::ResultCode::ABORTED) {
            if (!fp_detour_active_) {
                // ★ abort 원인 구분: RPP progress_checker 는 30s 간 0.5m 미이동이면 abort 하는데,
                //   우리는 간격유지·form-up 대기·추돌방지·예측반발로 speed_limit≈0(의도적 정지)을
                //   자주 건다. 그걸 '끼임'으로 오인해 우회(재계획)하면 "앞에 아무것도 없는데
                //   경로 재생성/끝점점프"가 됨. → *실제 lidar 전방장애물이 코앞일 때만* 우회.
                if (obstacle_dist_ < detour_trigger_dist_ && !convoy_follower()) {
                    RCLCPP_WARN(this->get_logger(),
                                "FollowPath ABORTED(실장애물 %.1fm) — 로컬 우회 시도", obstacle_dist_);
                    trigger_detour(escalated_rejoin_index());
                } else {
                    // 장애물 없음 = 우리 의도적 정지를 progress_checker 가 오인한 것 → 우회 금지.
                    // FollowPath 만 재전송해 같은 라인 그대로 이어감(정지 풀리면 정상 주행).
                    // ★ 쿨다운 필수: 재전송이 직전 goal 을 즉시 preempt→그게 ABORTED→또 재전송으로
                    //   초당 수백번 self-루프(로봇 정지)했음. 멀쩡히 달리던 FollowPath 를 안 끊도록
                    //   최소 간격(3s)을 둬 한 번 보낸 goal 이 실제로 주행할 시간을 보장.
                    const double now_s = this->now().seconds();
                    if (now_s - last_fp_resend_t_ > 3.0) {
                        last_fp_resend_t_ = now_s;
                        RCLCPP_INFO(this->get_logger(),
                            "FollowPath ABORTED(전방장애물 없음 — 간격/대기 정지 오인) → 재전송 재개");
                        // 컨보이 팔로워는 라인이 아니라 리더 궤적 경로로 재개.
                        if (convoy_follower() && leader_trace_.poses.size() >= 4) {
                            send_convoy_path();
                        } else {
                            build_and_send_follow_path();
                        }
                    }
                    // else: 최근 재전송함 → 이번 abort 무시(자기 preempt 루프 차단)
                }
            }
        }
        // CANCELED 는 STOP/PAUSE/우회 경로에서 처리되므로 무시.
    }

    // 5Hz: body-frame lidar 전방 콘 최근접 장애물(obstacle_dist_, on_lidar 가 계산)로 선제 우회.
    //  · costmap/IsPathValid 가 아니라 raw lidar 를 직접 봐서 EKF·map정합·로봇별 편차에 면역.
    //  · on_lidar 가 이미 지면(z<−0.4)·팀원(마스킹) 제외 → 실장애물만. (이게 클러터 거짓양성과
    //    로봇별 미감지를 동시에 해결. costmap 기반은 sim localization 편차로 신뢰불가였음.)
    // 우선순위 양보 평가(항상): 더 높은 우선순위(작은 id=리더 우선)가 종방향 근접 위치서
    // 스워브 중이면 yielding_=true → sync_tick 이 나를 감속. 자기 장애물 유무와 무관하게
    // 평가해야 함 — 옆 차가 내 라인 쪽으로 스워브하는데 내가 마침 장애물이 없으면 양보 못하던
    // 문제(동시아닌 회피서 충돌) 해결.
    void update_yield()
    {
        yielding_ = false;
        if (control_mode_ != "follow_path" || !active_ || fp_original_length_ < 1e-3) return;
        const double now_s = this->now().seconds();
        for (const auto &kv : peer_swerve_) {
            if (kv.first >= static_cast<int>(robot_id_)) continue;       // 상위 우선순위만
            const auto it = peer_swerve_seen_.find(kv.first);
            if (it == peer_swerve_seen_.end() || now_s - it->second > 1.0) continue;
            if (kv.second.first > 0.5 &&
                std::abs(kv.second.second - own_arc_) * fp_original_length_ < 7.0) {
                yielding_ = true; break;
            }
        }
    }

    // 끼임 감지: 명령은 주행인데 실제 위치(own_map)가 N초간 거의 안 움직이면(스워브가 다른
    // 장애물에 끼이는 등) NavigateToPose 로 복구 — planner 가 costmap 으로 양쪽 장애물 우회.
    void check_stuck()
    {
        // form-up·reform 중에는 슬롯 재배치로 의도적 정지/대기(직렬화·phase 전환)가 있어
        // 끼임 판정 금지(오탐 → NavigateToPose 복구가 cmd_vel_nav 재배치와 충돌). 종료 시 재기준.
        if (!active_ || fp_detour_active_ || forming_up_ || reforming_ ||
            control_mode_ != "follow_path" || !own_pose_valid_) {
            stuck_ref_set_ = false;
            return;
        }
        const double t = this->now().seconds();
        const double moved = stuck_ref_set_
            ? std::hypot(own_map_x_ - stuck_ref_x_, own_map_y_ - stuck_ref_y_) : 1e9;
        if (!stuck_ref_set_ || moved > 0.4) {
            stuck_ref_x_ = own_map_x_; stuck_ref_y_ = own_map_y_;
            stuck_ref_t_ = t; stuck_ref_set_ = true;
            return;
        }
        if (t - stuck_ref_t_ > stuck_timeout_s_) {   // N초간 0.4m 미만
            // 실제 lidar 전방장애물이 코앞일 때만 '끼임'으로 보고 우회. 장애물 없으면 우리가
            // 간격유지/대기로 의도적으로 멈춘 것 → 우회 금지(헛 재계획 방지), 기준만 리셋.
            if (obstacle_dist_ >= detour_trigger_dist_) {
                // ★웨지(접촉 밀착) 감지: 명령속도는 높은데(의도적 정지 아님) 안 움직이고
                //   라이다도 무감지 = 장애물이 차체에 밀착(라이다 최소거리/self필터 블라인드,
                //   run10 s3 박스 그라인딩 5분 실측). 2s 후진으로 빼낸 뒤 경로 재전송.
                //   재발동 폭주 방지: 후진 진행 중 금지 + 쿨다운 10s(run11 리더 110회 실측).
                // 뒤 2.5m 내 팀원이 있으면 후진 금지(뒤차를 밀어 범퍼락 유발 — run13 실측
                // 리더가 s2 를 후진으로 밀착시켜 상호 견인불능).
                bool teammate_behind = false;
                {
                    const auto meb = world_pos_.find(static_cast<int>(robot_id_));
                    if (meb != world_pos_.end()) {
                        const double bcy = std::cos(own_yaw_), bsy = std::sin(own_yaw_);
                        for (const auto &kv : world_pos_) {
                            if (kv.first == static_cast<int>(robot_id_)) continue;
                            const double rx = kv.second.first - meb->second.first;
                            const double ry = kv.second.second - meb->second.second;
                            if (bcy * rx + bsy * ry < 0.0 && std::hypot(rx, ry) < 2.5) {
                                teammate_behind = true; break;
                            }
                        }
                    }
                }
                // 판정 = '실제 최종 명령'(cmd_vel) 기준: goal 취소로 컨트롤러가 안 내는
                // 정상 정지에선 last_cmd 가 0/stale → 오발 없음. 미션당 5회 상한.
                const bool cmd_active = (t - last_cmd_t_ < 0.6) && last_cmd_speed_ > 0.3;
                // ★후진 금지(사용자 지시): 팔로워는 절대 뒤로 가지 않는다 — 대형은 리더가
                //   앞으로 전진하며 맞춘다. 팔로워가 막히면 정지 대기(리더 대기 로직이 공간
                //   확보) 후 전진 재개. 리더만 최후의 접촉 교착 해소용 후진 허용.
                if (is_leader_ && cmd_active && wedge_count_ < 5 && wedge_reverse_ticks_ == 0 &&
                    !teammate_behind && t - last_wedge_t_ > 10.0) {
                    RCLCPP_WARN(this->get_logger(),
                        "[웨지] 명령 %.1fm/s 인데 %.0fs 정지·라이다 무감지 — 접촉 의심, 2s 후진",
                        last_speed_limit_, t - stuck_ref_t_);
                    last_wedge_t_ = t;
                    ++wedge_count_;
                    wedge_reverse_ticks_ = 20;
                    wedge_rev_x_ = own_map_x_; wedge_rev_y_ = own_map_y_;
                    if (active_fp_goal_) {
                        ignore_next_fp_result_ = true;
                        cancel_goal_safe(follow_path_client_, active_fp_goal_);
                        active_fp_goal_.reset();
                    }
                }
                stuck_ref_set_ = false;
                return;
            }
            RCLCPP_WARN(this->get_logger(),
                        "끼임 감지(%.1fs 정지, 장애물 %.1fm) — NavigateToPose 복구(planner 우회)",
                        t - stuck_ref_t_, obstacle_dist_);
            stuck_ref_set_ = false;
            fp_swerving_ = false;
            trigger_detour(escalated_rejoin_index());
        }
    }

    // 차량 라이트 상태표시: 주행=하이빔(3), form-up/대형전환=하향등 점멸(2↔1), 대기=하향등(2).
    //   can_reader(--control)가 /sN/headlight 를 구독해 CAN 0x201 light 바이트로 반영.
    void light_tick()
    {
        if (!light_pub_) return;
        std_msgs::msg::UInt8 m;
        if (forming_up_ || reform_in_progress_) {
            blink_toggle_ = !blink_toggle_;
            m.data = blink_toggle_ ? 2 : 1;      // 하향등 점멸 (2 low ↔ 1 off)
        } else if (active_ && control_mode_ == "follow_path") {
            m.data = 3;                          // 하이빔 주행
        } else {
            m.data = 2;                          // 하향등 (대기)
        }
        light_pub_->publish(m);
    }

    // 디버그 마커: 감지 장애물(빨강 구) + 로봇 상태(텍스트). rviz MarkerArray /sN/swarm_debug.
    void publish_debug_markers()
    {
        if (!dbg_marker_pub_ || !own_pose_valid_) return;
        visualization_msgs::msg::MarkerArray ma;
        const double cy = std::cos(own_yaw_), sy = std::sin(own_yaw_);
        visualization_msgs::msg::Marker obs;
        obs.header.frame_id = map_frame_; obs.header.stamp = this->now();
        obs.ns = "obstacle"; obs.id = 0;
        obs.type = visualization_msgs::msg::Marker::SPHERE;
        obs.pose.orientation.w = 1.0;
        if (active_ && obstacle_dist_ < obstacle_detect_range_) {
            obs.action = visualization_msgs::msg::Marker::ADD;
            obs.pose.position.x = own_map_x_ + cy * obstacle_dist_ - sy * obstacle_lat_;
            obs.pose.position.y = own_map_y_ + sy * obstacle_dist_ + cy * obstacle_lat_;
            obs.pose.position.z = 0.5;
            obs.scale.x = obs.scale.y = obs.scale.z = 0.6;
            obs.color.r = 1.0; obs.color.a = 0.85;
        } else {
            obs.action = visualization_msgs::msg::Marker::DELETE;
        }
        ma.markers.push_back(obs);
        visualization_msgs::msg::Marker txt;
        txt.header = obs.header; txt.ns = "state"; txt.id = 0;
        txt.type = visualization_msgs::msg::Marker::TEXT_VIEW_FACING;
        txt.action = visualization_msgs::msg::Marker::ADD;
        txt.pose.position.x = own_map_x_; txt.pose.position.y = own_map_y_; txt.pose.position.z = 1.6;
        txt.pose.orientation.w = 1.0; txt.scale.z = 0.5;
        const std::string st = fp_detour_active_ ? "DETOUR" : fp_swerving_ ? "SWERVE"
                             : yielding_ ? "YIELD" : "OK";
        txt.text = "s" + std::to_string(robot_id_) + ":" + st;
        txt.color.r = (st == "OK") ? 0.2 : 1.0; txt.color.g = (st == "OK") ? 1.0 : 0.5;
        txt.color.b = 0.2; txt.color.a = 1.0;
        ma.markers.push_back(txt);

        // ★ form-up 목표 슬롯 마커(팔로워): 도착판정 반경 원기둥 + "sN slot" 텍스트.
        //   ready(디바운스 충족)=초록, 이동 중=노랑. 사용자가 슬롯 위치와 팔로워 도착을 확인.
        if (slot_valid_ && !is_leader_) {
            const bool rdy = (formup_ready_streak_ >= 10);
            visualization_msgs::msg::Marker slot;
            slot.header = obs.header; slot.ns = "slot"; slot.id = robot_id_;
            slot.type = visualization_msgs::msg::Marker::CYLINDER;
            slot.action = visualization_msgs::msg::Marker::ADD;
            slot.pose.position.x = slot_x_; slot.pose.position.y = slot_y_;
            slot.pose.position.z = 0.1; slot.pose.orientation.w = 1.0;
            slot.scale.x = slot.scale.y = 2.0 * formup_arrive_tol_m_; slot.scale.z = 0.2;
            slot.color.r = rdy ? 0.1 : 1.0; slot.color.g = rdy ? 1.0 : 0.85;
            slot.color.b = 0.1; slot.color.a = 0.55;
            ma.markers.push_back(slot);
            visualization_msgs::msg::Marker st2;
            st2.header = obs.header; st2.ns = "slot_txt"; st2.id = robot_id_;
            st2.type = visualization_msgs::msg::Marker::TEXT_VIEW_FACING;
            st2.action = visualization_msgs::msg::Marker::ADD;
            st2.pose.position.x = slot_x_; st2.pose.position.y = slot_y_; st2.pose.position.z = 0.8;
            st2.pose.orientation.w = 1.0; st2.scale.z = 0.4;
            st2.text = "s" + std::to_string(robot_id_) + " slot";
            st2.color.r = 1.0; st2.color.g = 1.0; st2.color.b = 1.0; st2.color.a = 1.0;
            ma.markers.push_back(st2);
            // ★ 슬롯까지 가는 경로 라인(현재위치→슬롯). drive_reposition 은 nav2 /plan 이
            //   없으므로 이 라인으로 '슬롯까지 경로'를 표시. ready=초록, 이동중=노랑.
            visualization_msgs::msg::Marker ln;
            ln.header = obs.header; ln.ns = "slot_path"; ln.id = robot_id_;
            ln.type = visualization_msgs::msg::Marker::LINE_STRIP;
            ln.action = visualization_msgs::msg::Marker::ADD;
            ln.pose.orientation.w = 1.0; ln.scale.x = 0.08;   // 선 두께
            ln.color.r = rdy ? 0.1 : 1.0; ln.color.g = rdy ? 1.0 : 0.85;
            ln.color.b = 0.1; ln.color.a = 0.9;
            geometry_msgs::msg::Point p0, p1;
            p0.x = own_map_x_; p0.y = own_map_y_; p0.z = 0.1;
            p1.x = slot_x_; p1.y = slot_y_; p1.z = 0.1;
            ln.points.push_back(p0); ln.points.push_back(p1);
            ma.markers.push_back(ln);
        }
        dbg_marker_pub_->publish(ma);
    }

    void obstacle_check_tick()
    {
        if (!active_ || control_mode_ != "follow_path") { yielding_ = false; return; }
        check_stuck();    // 끼임 복구(항상 평가)
        update_yield();   // 양보는 항상 평가(자기 장애물 없어도)
        publish_debug_markers();   // rviz 디버그(감지장애물·상태)
        if (fp_detour_active_ || !active_fp_goal_ || !own_pose_valid_ ||
            fp_last_path_.poses.size() < 2) {
            return;
        }
        // 스워브 중 장애물이 전방콘서 벗어나면 스워브 상태 해제. (단 detection 은 계속 돌아서
        // 스워브 경로상에 '새 장애물'이 나타나면 쿨다운 뒤 재스워브 — 못 피하던 문제 해결.)
        if (fp_swerving_ && obstacle_dist_ >= detour_trigger_dist_) {
            fp_swerving_ = false;
        }
        // 출발 grace: 스폰 직후 settling 피치로 지면이 잠깐 장애물로 잡히는 초반 거짓우회 억제.
        if ((this->now() - fp_start_time_).seconds() < startup_grace_s_) {
            obstacle_persist_count_ = 0;
            return;
        }
        const double d = obstacle_dist_;
        // 긴급정지(밀고 들어감 방지): 정면·차폭내 장애물이 hard_stop_dist 이내 = 스워브가 못
        // 비킨 상황 → sync_tick 에서 정지. executor 견고감지(클러스터+콘+마스킹)라 sim 오탐 없음.
        hard_stop_ = (d < hard_stop_dist_ &&
                      std::abs(obstacle_lat_) < swerve_footprint_half_w_ + 0.2);
        if (hard_stop_) {
            RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 1000,
                "[긴급정지] 정면 장애물 %.2fm(횡%.2f) — 스워브 불가, 정지", d, obstacle_lat_);
        }
        // 지속성: 순간 노이즈 무시 — N틱 연속 trigger 거리 내일 때만 우회.
        // 넓은 장애물(스워브 불가)은 감지범위(12m) 전체에서 지속성 카운트(조기 우회).
        // ★obs_w = 넓은콘 실측 횡폭(좁은콘 hi-lo 아님) → 벽도 제대로 wide 로 잡힘.
        const double obs_w = obstacle_wide_span_;
        const bool wide_obs = (obs_w > wide_obstacle_span_m_) && (d < obstacle_detect_range_);
        if (d < detour_trigger_dist_ || wide_obs) {
            ++obstacle_persist_count_;
        } else {
            obstacle_persist_count_ = 0;
        }
        RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 2000,
            "[우회진단] 전방장애물(lidar) dist=%.2fm 실폭=%.1fm(wide=%d) trigger<%.1f "
            "persist=%d/%d swerving=%d yield=%d",
            (d > 1e8 ? -1.0 : d), obs_w, wide_obs ? 1 : 0, detour_trigger_dist_,
            obstacle_persist_count_, obstacle_persist_ticks_, fp_swerving_, yielding_);
        // COLUMN 컨보이 팔로워: 자체 스워브/planner 우회 금지 — 리더 궤적이 회피를 담당.
        // (위의 hard_stop 긴급정지 + collision_monitor + check_stuck 복구는 그대로 유지)
        // if (convoy_follower()) { obstacle_persist_count_ = 0; return; }
        if (obstacle_persist_count_ < obstacle_persist_ticks_) return;
        if (yielding_) {   // 양보 중엔 새 스워브 안 함(상위 우선순위 먼저 통과)
            RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 1500,
                "[양보] 상위 우선순위 차 스워브 중 — 스워브 보류·감속");
            return;
        }
        // 스워브 직후 쿨다운 — 방금 만든 스워브가 효과 보일 시간(같은 장애물에 thrash 방지).
        if ((this->now().seconds() - last_swerve_time_) < swerve_cooldown_s_) return;
        // 이미 스워브 중이면 같은 장애물을 또 피하려 경로를 재구축하지 않는다(연쇄 스워브 방지).
        // 정상 스워브는 차체가 옆으로 빠지며 장애물이 콘 밖으로 나가 자연 해소됨. 재스워브는
        // 스워브가 안 먹혀 장애물이 '여전히 코앞·중앙(차폭 내)'일 때만 — 그땐 더 넓게 다시.
        if (fp_swerving_) {
            const bool still_threat = (d < 2.8) &&
                (std::abs(obstacle_lat_) < swerve_footprint_half_w_ + 0.2);
            if (!still_threat) { obstacle_persist_count_ = 0; return; }
        }
        obstacle_persist_count_ = 0;
        // ★큰(넓은) 장애물: 스워브(횡 최대 2.8m)로 못 비낀다 — 감지 즉시(최대 12m 전)
        //   planner 우회로 크게 돈다(global costmap 이 장애물 전모를 알고 있음). rejoin 은
        //   장애물 너머(전방거리+여유)로. (사용자 지시: 큰 장애물은 훨씬 전부터 회피기동)
        if (wide_obs && !fp_swerving_) {
            const double mult = 1.0 + std::min(detour_fail_streak_, 5);
            const std::size_t near = nearest_path_index();
            const std::size_t rejoin = std::min(
                near + static_cast<std::size_t>(std::ceil((d + 6.0 * mult) / 0.5)),
                fp_last_path_.poses.empty() ? std::size_t(0) : fp_last_path_.poses.size() - 1);
            RCLCPP_WARN(this->get_logger(),
                        "넓은 장애물(폭 %.1fm, 전방 %.1fm) — 조기 planner 우회(rejoin +%.0fm)",
                        obs_w, d, d + 6.0 * mult);
            trigger_detour(rejoin);
            return;
        }
        // 부드러운 스워브(넓고 얕게)로 회피 — 멀리서부터 횡이탈, 장애물 지나 라인 복귀.
        trigger_swerve(d, obstacle_lat_);
    }

    // 장애물(전방 obs_dist, 횡 obs_lat)을 부드러운 횡오프셋 경로로 회피.
    //  · NavigateToPose dogleg(정지·회전·타이트) 대신, 현재 라인을 따라 멀리서부터 완만히
    //    횡이탈→장애물 옆 유지→완만히 복귀하는 경로를 만들어 FollowPath 로 추종(무정지·무회전).
    //  · 스워브 방향 = 장애물 반대쪽. 경로 자체에 복귀 램프 포함 → 별도 rejoin/재개 불필요.
    void trigger_swerve(double obs_dist, double obs_lat)
    {
        if (fp_last_path_.poses.size() < 2 || !own_pose_valid_) return;
        const std::size_t near = nearest_path_index();
        const std::size_t n = fp_last_path_.poses.size();
        // 스워브 방향 = 장애물-인지: 선호(슬롯 바깥쪽/중앙은 장애물 반대)하되, 그 쪽이 막혔으면
        // 빈 쪽으로, 양쪽 다 막혔으면 단순 스워브 불가 → NavigateToPose(planner, 모든 장애물·
        // footprint 인지 우회) 폴백. (+1=좌, −1=우; left_clear_/right_clear_ 는 on_lidar 측방거리)
        const int slot = formationSlotOffset();
        const double pref = (slot > 0) ? 1.0 : (slot < 0) ? -1.0
                                                          : (obs_lat >= 0.0 ? -1.0 : 1.0);
        const double need = swerve_clearance_m_ + 0.6;   // 그 쪽이 이만큼 비어야 안전(footprint 포함)
        const bool left_ok = left_clear_ > need;
        const bool right_ok = right_clear_ > need;
        double side;
        if ((pref > 0 && left_ok) || (pref < 0 && right_ok)) {
            side = pref;                                  // 선호 쪽 비었음
        } else if (left_ok) {
            side = 1.0;
        } else if (right_ok) {
            side = -1.0;
        } else {
            RCLCPP_WARN(this->get_logger(),
                        "양쪽 막힘(L=%.1f R=%.1f) — 스워브 불가, planner 우회 폴백",
                        left_clear_, right_clear_);
            const std::size_t rj = std::min(
                near + static_cast<std::size_t>(std::ceil(detour_skip_m_ / 0.5)), n - 1);
            trigger_detour(rj);
            return;
        }
        // 폭 적응 클리어런스: 스워브 쪽 장애물 끝 + swept 차폭반 + 여유. 좁은 장애물엔 작게(과한
        // 이탈 방지), 넓은 벽엔 크게. 하한 = swept 반폭+여유 → 점/소형장애물도 차체 측면이 클리어.
        // (스워브 중 차체가 비스듬해져 모서리가 더 쓸리므로 평폭 0.43 이 아닌 swept 0.7 사용)
        const double obs_edge = (side > 0) ? obstacle_lat_hi_ : -obstacle_lat_lo_;
        const double clear = std::clamp(obs_edge + swerve_footprint_half_w_ + 0.4,
                                        swerve_footprint_half_w_ + 0.6, 2.8);
        // 장애물 3m 앞에서 full clear 도달(구 1m — "코앞 회피"의 원인). 조기감지(12m)+
        // 트리거(8m)와 합쳐 멀리서부터 완만히 빠진다.
        const double up_end = std::max(2.5, obs_dist - 3.0);
        const double down_start = obs_dist + swerve_hold_past_m_; // 장애물 지나 유지 후
        const double down_end = down_start + swerve_transition_m_;// 완만히 복귀

        nav_msgs::msg::Path sp;
        sp.header = fp_last_path_.header;
        sp.header.stamp = this->now();
        for (std::size_t i = near; i < n; ++i) {
            geometry_msgs::msg::PoseStamped ps = fp_last_path_.poses[i];
            const double s = static_cast<double>(i - near) * 0.5;   // near 부터 호 거리
            double off = 0.0;
            if (s < up_end) {
                off = clear * 0.5 * (1.0 - std::cos(M_PI * std::clamp(s / up_end, 0.0, 1.0)));
            } else if (s < down_start) {
                off = clear;
            } else if (s < down_end) {
                off = clear * 0.5 *
                      (1.0 + std::cos(M_PI * std::clamp((s - down_start) /
                                      swerve_transition_m_, 0.0, 1.0)));
            } else {
                off = 0.0;
            }
            if (off > 1e-3) {
                // 경로 진행방향 수직으로 오프셋. 방향 yaw = 다음 점 향함.
                const std::size_t k = std::min(i + 1, n - 1);
                const double yaw = std::atan2(
                    fp_last_path_.poses[k].pose.position.y - fp_last_path_.poses[i].pose.position.y,
                    fp_last_path_.poses[k].pose.position.x - fp_last_path_.poses[i].pose.position.x);
                ps.pose.position.x += -std::sin(yaw) * side * off;
                ps.pose.position.y +=  std::cos(yaw) * side * off;
            }
            sp.poses.push_back(ps);
        }
        RCLCPP_WARN(this->get_logger(),
                    "스워브 회피: 장애물 %.1fm %s쪽 → %.1fm 횡이탈(램프 %.1fm)",
                    obs_dist, (side < 0 ? "왼" : "오"), clear, swerve_transition_m_);
        fp_swerving_ = true;
        last_swerve_time_ = this->now().seconds();   // 재스워브 쿨다운 기준
        if (active_fp_goal_) {
            ignore_next_fp_result_ = true;   // 옛 goal 취소의 ABORTED 가 폴백 발동 안 하게
            cancel_goal_safe(follow_path_client_, active_fp_goal_);
            active_fp_goal_.reset();
        }
        send_fp_path(sp);   // fp_original_length_ 불변 → 진행도 동기화 유지
    }

    // own_map 위치에서 가장 가까운 fp_last_path_ pose 인덱스.
    std::size_t nearest_path_index() const
    {
        std::size_t near = 0;
        double best = 1e18;
        for (std::size_t i = 0; i < fp_last_path_.poses.size(); ++i) {
            const double dx = fp_last_path_.poses[i].pose.position.x - own_map_x_;
            const double dy = fp_last_path_.poses[i].pose.position.y - own_map_y_;
            const double d = dx * dx + dy * dy;
            if (d < best) { best = d; near = i; }
        }
        return near;
    }

    // ---- COLUMN 컨보이 구현 ----
    // 이 로봇이 '컨보이 팔로워'인가: COLUMN 대형의 FollowPath 팔로워.
    bool convoy_follower() const
    {
        return !is_leader_ && formation_enabled_ && control_mode_ == "follow_path" &&
               (geometryForMode(formation_type_, formation_number_) == FORMATION_COLUMN ||
                obs_convoy_);   // ★obs_convoy: 장애물 통과 구간 임시 종대(리더궤적 추종)
    }

    // 팔로워(비-COLUMN): 리더 우회 통과를 "내 진행분율(own_arc_) vs 리더 궤적의
    // 우회 far-edge" 로 게이팅한다. 리더의 순간 swerve 상태가 아니라 내가 실제로
    // 그 갭을 지났는지로 판정 → 리더가 먼저 통과해도(먼 후미) 나는 갭을 다 지날
    // 때까지 리더궤적을 계속 추종하고, 다 지난 뒤에만 대형 슬롯으로 복원한다.
    void update_obstacle_convoy()
    {
        if (is_leader_ || !formation_enabled_ || control_mode_ != "follow_path" ||
            geometryForMode(formation_type_, formation_number_) == FORMATION_COLUMN) {
            obs_convoy_ = false;
            return;   // 리더/비활성/COLUMN(이미 상시 컨보이)은 대상 아님
        }

        std::vector<std::pair<double, double>> trace, ref;
        trace.reserve(leader_trace_.poses.size());
        for (const auto &ps : leader_trace_.poses) {
            trace.push_back({ps.pose.position.x, ps.pose.position.y});
        }
        ref.reserve(leader_ref_path_.poses.size());
        for (const auto &ps : leader_ref_path_.poses) {
            ref.push_back({ps.pose.position.x, ps.pose.position.y});
        }

        // 내 진행 = 내 월드좌표를 리더 기준경로 축에 투영한 along-track 거리(m). ★경로무관
        // ·단조(own_arc_ 은 추종경로 dist-to-goal 기반이라 코히어 진입/이탈로 경로가 바뀌면
        // 값이 튀어 자기강화 플래핑을 유발). 리더 궤적의 우회 far-edge 도 같은 축의 m.
        const double own_s =
            combat_robot_nav2::alongTrack({own_map_x_, own_map_y_}, ref);
        const double far_s =
            combat_robot_nav2::detourFarAlongTrack(trace, ref, detour_lat_thresh_m_);
        const bool has_detour = far_s > -1e30;

        if (!obs_convoy_) {
            // 진입: ①리더 조기 브로드캐스트(leader_obstacle_pass_active_) 또는 ②내 앞 우회
            //   far-edge. ★①이 조기종대의 핵심 — 리더가 스워브하기 전, 개방공간에서 미리
            //   종대 추종을 시작해 넓은 대형이 갭 앞에서 funnel/데드락하지 않게 한다.
            const bool forced_early = leader_obstacle_pass_active_;
            if ((forced_early || (has_detour && far_s > own_s)) &&
                trace.size() >= 4 && !reforming_) {
                obs_convoy_ = true;
                // 이탈 목표: far_s 가 이미 보이면 그 값, 아직이면 큰 값으로 잠정 래치(스워브가
                //   생기면 아래에서 실제 far_s 로 당겨짐). override 가 켜진 동안엔 어차피 유지.
                detour_exit_s_ = has_detour ? far_s : (own_s + 1.0e4);
                convoy_turn_latched_ = false;   // 롤링 머지 순번 재무장(한 대씩 순차 합류)
                RCLCPP_INFO(this->get_logger(),
                    "[코히어통과] %s → 리더궤적 추종(내 s=%.1fm far=%.1fm)",
                    forced_early ? "리더 장애물통과 조기발동" : "내 앞 우회 far-edge",
                    own_s, far_s);
                send_convoy_path();   // 즉시 리더궤적 추종 시작(자체 회피 안 함)
            }
        } else {
            // 래치 목표 갱신: 스워브로 far_s 가 생기면 실제 우회 far-edge 로 당김(잠정 큰 값→실제).
            if (has_detour && (detour_exit_s_ > own_s + 1.0e3 || far_s > detour_exit_s_)) {
                detour_exit_s_ = far_s;
            }
            // 이탈: ★리더가 통과 종료를 알렸고(override off) + 내가 far-edge 를 마진만큼 실제로
            //   지남. override 활성 중엔 유지 → 후미가 갭 한복판에서 조기복원되지 않는다(개별
            //   판정이라 리더 중앙해제로 후미를 버리던 버그 없음).
            if (!leader_obstacle_pass_active_ &&
                own_s >= detour_exit_s_ + detour_exit_margin_m_) {
                obs_convoy_ = false;
                RCLCPP_INFO(this->get_logger(),
                    "[코히어통과] 우회 통과 완료(내 s=%.1fm >= far-edge %.1fm+%.1f) → 대형 복원",
                    own_s, detour_exit_s_, detour_exit_margin_m_);
                if (leader_ref_valid_) { build_follower_path_from_leader(); }
            }
        }
    }

    // 컨보이 리더인가(COLUMN, 리더): 레거시 페이싱 대신 결정적 간격 governor 사용.
    // 안정적 전진방향(경로 start→end 단위벡터). ★own_yaw_ 는 리더가 갭을 스레딩할 때
    //   −y 를 가리켜, 팔로워의 '뒤짐(behind)' 종방향 투영이 뒤집혀(따라오는 팔로워를
    //   '안 뒤짐'으로 오판) obstacle_pass 조기해제·governor 오작동을 유발. 경로축은 미션
    //   내내 안정(+x). fp_base_line_(원본 라인) 없으면 own_yaw_ 폴백.
    bool pathForwardDir(double &t_ux, double &t_uy) const
    {
        const auto &P = fp_base_line_.poses;
        if (P.size() >= 2) {
            const double dx = P.back().pose.position.x - P.front().pose.position.x;
            const double dy = P.back().pose.position.y - P.front().pose.position.y;
            const double len = std::hypot(dx, dy);
            if (len > 1e-6) { t_ux = dx / len; t_uy = dy / len; return true; }
        }
        t_ux = std::cos(own_yaw_); t_uy = std::sin(own_yaw_);
        return false;
    }

    // 리더가 기준선(fp_base_line_)에서 벗어난 횡거리(m). 회피로 갭에 들어가 있으면 크고,
    // 갭을 다 빠져나와 라인 복귀하면 ~0. governor 완전정지를 "라인 복귀 후"로 게이팅하는 데 씀.
    double leaderLateralOffset() const
    {
        const auto &P = fp_base_line_.poses;
        if (P.size() < 2) { return 0.0; }
        const auto &a = P.front().pose.position;
        const auto &b = P.back().pose.position;
        return combat_robot_nav2::pointSegDist(
            {own_map_x_, own_map_y_}, {a.x, a.y}, {b.x, b.y});
    }

    bool convoy_column_leader() const
    {
        // COLUMN 상시, 또는 임의 대형에서 장애물 통과 중(사용자 R2: 회피 시 종대로 통과)
        // → 리더는 결정적 간격 governor 로 후미를 확실히 기다리며 좁은 갭을 매끄럽게 통과.
        return is_leader_ && formation_enabled_ && control_mode_ == "follow_path" &&
               (geometryForMode(formation_type_, formation_number_) == FORMATION_COLUMN ||
                obstacle_pass_active_);
    }

    // 롤링 머지 출발 순서: 컬럼 순번 = id 순 고정(assignedRank). 내 앞 순번(리더 제외,
    // id 가 나보다 낮은 팔로워) 전원이 내 진행방향 기준 2m 이상 앞设을 때 true.
    // 공유 world_pos_ 기반(fresh 2s), 첫 통과 시 latch(중간 재동결 없음).
    // ★슬롯 도달 게이트(사용자 지시): 팔로워는 자기 종대 슬롯(리더 뒤 rank×간격, 리더와 함께
    //   전진)이 자기 위치(along-track)에 도달할 때까지 제자리 대기하다, 슬롯이 오면 falling-in.
    //   → 리더가 앞으로 가며 슬롯을 끌고 오고, 각 팔로워는 자기 슬롯이 옆을 지날 때 한 대씩 출발.
    //   슬롯 도착 순서 = rank(가까운 슬롯이 먼저 도달) → s2 먼저, s3 나중 → 간격 두고 순차 합류
    //   (동시 진입 s2·s3 충돌·상호정지 방지). 1회 latch(도착하면 계속 추종).
    bool convoy_turn_ok()
    {
        if (convoy_turn_latched_) return true;
        const int myrank = assignedRank();
        if (myrank <= 0) { convoy_turn_latched_ = true; return true; }   // 리더 자신
        const auto me = world_pos_.find(static_cast<int>(robot_id_));
        const auto ld = world_pos_.find(static_cast<int>(leader_robot_id_));
        if (me == world_pos_.end() || ld == world_pos_.end()) return false;
        double ux, uy;
        pathForwardDir(ux, uy);
        const double leader_s = ux * ld->second.first + uy * ld->second.second;
        const double own_s = ux * me->second.first + uy * me->second.second;
        // 내 슬롯 = 리더 뒤 (1.5×rank×spacing) m. formationSlotFor(COLUMN).along = -1.5×rank.
        const double slot_gap = 1.5 * static_cast<double>(myrank) *
                                formation_lateral_spacing_m_;
        const double slot_s = leader_s - slot_gap;
        if (slot_s >= own_s - 0.3) {   // 슬롯이 내 위치에 도달 → falling-in 출발
            convoy_turn_latched_ = true;
            RCLCPP_INFO(this->get_logger(),
                "[컨보이] 슬롯 도착(slot_s=%.1f ≥ own_s=%.1f, 순번 %d) — 리더 뒤 합류 출발",
                slot_s, own_s, myrank);
            return true;
        }
        return false;
    }

    // 리더: 실주행 위치를 0.5m 간격 breadcrumb 으로 기록, 1Hz 스로틀 발행(latched).
    void record_leader_trace()
    {
        if (!is_leader_ || !leader_trace_pub_ || !active_ || forming_up_ ||
            control_mode_ != "follow_path" || !own_pose_valid_) {
            return;
        }
        bool append = own_trace_.poses.empty();
        if (!append) {
            const auto &lp = own_trace_.poses.back().pose.position;
            append = std::hypot(own_map_x_ - lp.x, own_map_y_ - lp.y) >= 0.5;
        }
        if (append) {
            geometry_msgs::msg::PoseStamped ps;
            ps.header.frame_id = map_frame_;
            ps.header.stamp = this->now();
            ps.pose.position.x = own_map_x_;
            ps.pose.position.y = own_map_y_;
            ps.pose.orientation.z = std::sin(own_yaw_ * 0.5);
            ps.pose.orientation.w = std::cos(own_yaw_ * 0.5);
            own_trace_.header.frame_id = map_frame_;
            own_trace_.poses.push_back(ps);
        }
        const double now_s = this->now().seconds();
        if (append && now_s - last_trace_pub_t_ > 1.0) {
            last_trace_pub_t_ = now_s;
            own_trace_.header.stamp = this->now();
            leader_trace_pub_->publish(own_trace_);
        }
    }

    // 팔로워(COLUMN): 리더 궤적이 자라면 3s/4m 스로틀로 추종 경로를 재합성해 재전송.
    void convoy_tick()
    {
        if (!convoy_follower() || !active_ || forming_up_ || reforming_ ||
            fp_detour_active_ || gap_hold_ || wedge_reverse_ticks_ > 0 ||
            !own_pose_valid_) {
            return;   // hold/후진 중 재전송 금지(새 goal 이 hold 를 깨는 것 방지)
        }
        if (leader_trace_.poses.size() < 4) return;   // 리더가 아직 별로 안 움직임
        const double now_s = this->now().seconds();
        if (now_s - last_convoy_send_t_ < 3.0) return;
        // ★재전송은 trace 8m(16점) 성장마다만 — 경로가 리더 전체 궤적이라 자주 바꿀 필요
        //   없고, 잦은 goal 교체는 RPP 를 계속 재시작시켜 팔로워를 실제로 못 달리게 함
        //   (run28: 3s 재전송→odom_v=0). 종간격은 속도 governor 가 실시간 조절하므로
        //   경로는 드물게 갱신해도 됨. goal 소진(끝 도달) 임박 시엔 즉시 갱신.
        const bool grew_8m = leader_trace_.poses.size() >= convoy_sent_len_ + 16;
        bool exhausted = false;
        if (!active_fp_goal_ && !leader_trace_.poses.empty()) {
            const auto &te = leader_trace_.poses.back().pose.position;
            const double dist = std::hypot(te.x - own_map_x_, te.y - own_map_y_);
            const double slot_m = std::abs(mySlot().along) * formation_lateral_spacing_m_;
            exhausted = dist > slot_m + 1.0;   // 경로 끝(리더)까지 가야 하는데 goal 없음
        }
        if (!grew_8m && !exhausted) return;
        send_convoy_path();
    }

    // 컨보이 경로 = 리더 trace(내 최근접점부터) + trace 끝 이후의 원본 라인 잔여.
    // 라인 잔여를 붙여 경로 끝 = 미션 goal 을 유지(트레이스 끝=리더 현재위치서
    // goal_checker 가 조기 SUCCEED 하는 것 방지). 팔로워는 간격유지로 리더 뒤에
    // 머물러 미탐사 라인 구간을 실제로 밟지 않는다(다음 갱신이 trace 로 대체).
    void send_convoy_path()
    {
        if (leader_trace_.poses.size() < 2) return;
        nav_msgs::msg::Path path;
        path.header.frame_id = map_frame_;
        path.header.stamp = this->now();
        // 내 위치 최근접 trace 인덱스부터 suffix (프레임명은 datum 공유라 좌표 동일 — 재라벨만)
        std::size_t i0 = 0; double best = 1e18;
        for (std::size_t i = 0; i < leader_trace_.poses.size(); ++i) {
            const auto &pp = leader_trace_.poses[i].pose.position;
            const double d = std::hypot(pp.x - own_map_x_, pp.y - own_map_y_);
            if (d < best) { best = d; i0 = i; }
        }
        // ★합류점 = 최근접이 아니라 '횡오프셋 비례로 앞선' trace 점(사용자 지시: 슬롯까지 너무
        //   직선적으로 말고 스무스하게). 최근접 합류는 옆으로 붙었다가 급회전(확 직진→꺾기)해
        //   s3 와 충돌. 앞선 합류점으로 잇는 대각 커넥터 = lane-change 처럼 부드럽게 리더 궤적에
        //   얹힘. join_ahead = 횡오프셋의 ~2.5배(최소 2m) — 옆으로 많이 벗어날수록 더 멀리서
        //   완만히 합류. lookahead(1.5)와 맞물려 진입 곡률 부드럽게.
        double lat_off = 0.0;
        {
            double ux, uy; pathForwardDir(ux, uy);
            const auto &np = leader_trace_.poses[i0].pose.position;
            lat_off = std::abs(-uy * (own_map_x_ - np.x) + ux * (own_map_y_ - np.y));
        }
        const double join_ahead = std::max(2.0, 2.5 * lat_off);
        std::size_t i1 = i0; double acc = 0.0;
        for (std::size_t i = i0; (i + 1) < leader_trace_.poses.size(); ++i) {
            const auto &a = leader_trace_.poses[i].pose.position;
            const auto &b = leader_trace_.poses[i + 1].pose.position;
            acc += std::hypot(b.x - a.x, b.y - a.y);
            i1 = i + 1;
            if (acc >= join_ahead) break;
        }
        for (std::size_t i = i1; i < leader_trace_.poses.size(); ++i) {
            auto ps = leader_trace_.poses[i];
            ps.header.frame_id = map_frame_;
            path.poses.push_back(ps);
        }
        // ★경로 = 리더 궤적 전체(최근접~trace끝). 종전엔 슬롯에서 절단했는데, 리더
        // 정지 시 경로가 3~5m 로 짧아지고 매 tick 재전송돼 RPP 가 goal 을 계속 재시작
        // → 팔로워가 speed_limit 을 받고도 실제 정지(odom_v=0, run28 실측). 절단을 없애
        // 긴 안정 경로를 유지하고, 추월/크리핑 방지는 팔로워 속도 governor(dist<slot이면
        // 감속/정지)에 맡긴다. 경로 끝(리더 위치) 도달 SUCCEED 는 fp result 의 컨보이
        // 대기 처리가 막는다.
        if (path.poses.size() < 2) {
            return;   // 리더가 거의 안 움직임 — 유지(정지)
        }
        // ★경로가 내 위치에서 시작해야 함: trace 는 리더 출발점부터라 뒤처진 팔로워(슬롯
        // -Nm)에겐 첫 pose 가 수 m 앞 — RPP 가 로컬 costmap(±5m) 밖 경로를 프루닝해
        // "Resulting plan has 0 poses" abort 루프(실측: s4 7m 갭 출발불가). 내 위치→경로
        // 시작을 0.5m 간격 직선 커넥터로 prepend.
        {
            const auto &f = path.poses.front().pose.position;
            const double gap = std::hypot(f.x - own_map_x_, f.y - own_map_y_);
            if (gap > 0.6) {
                const double yaw = std::atan2(f.y - own_map_y_, f.x - own_map_x_);
                const int n = std::max(1, static_cast<int>(gap / 0.5));
                std::vector<geometry_msgs::msg::PoseStamped> pre;
                for (int k = 0; k < n; ++k) {
                    const double t = static_cast<double>(k) / n;
                    geometry_msgs::msg::PoseStamped ps;
                    ps.header = path.header;
                    ps.pose.position.x = own_map_x_ + (f.x - own_map_x_) * t;
                    ps.pose.position.y = own_map_y_ + (f.y - own_map_y_) * t;
                    ps.pose.orientation.z = std::sin(yaw * 0.5);
                    ps.pose.orientation.w = std::cos(yaw * 0.5);
                    pre.push_back(ps);
                }
                path.poses.insert(path.poses.begin(), pre.begin(), pre.end());
            }
        }
        convoy_sent_len_ = leader_trace_.poses.size();
        last_convoy_send_t_ = this->now().seconds();
        last_fp_resend_t_ = last_convoy_send_t_;   // ABORTED 재전송 쿨다운과 단일화(인터리브 폭주 방지)
        RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 5000,
            "[컨보이] 리더 궤적 추종 경로 갱신: trace %zu점(내앞 %zu) + 라인잔여 → 총 %zu점",
            leader_trace_.poses.size(), leader_trace_.poses.size() - i0, path.poses.size());
        if (active_fp_goal_) {
            ignore_next_fp_result_ = true;   // 옛 goal 취소 ABORTED 가 폴백 발동 안 하게
            cancel_goal_safe(follow_path_client_, active_fp_goal_);
            active_fp_goal_.reset();
        }
        fp_swerving_ = false;
        send_fp_path(path);   // fp_original_length_ 불변 → 진행도 스케일 유지
    }

    // 우회 rejoin 인덱스: 라인상 detour_skip_m_ 앞. 연속 실패 시 skip 을 배수 연장 —
    // rejoin 이 장애물 내부라 planner 가 만들 수 없으면(0-pose plan) 같은 점 재시도가
    // 아니라 더 먼 복귀점을 시도해 장애물 클러스터를 넘긴다(최대 6배).
    std::size_t escalated_rejoin_index()
    {
        const double mult = 1.0 + std::min(detour_fail_streak_, 5);
        const std::size_t near = nearest_path_index();
        if (detour_fail_streak_ > 0) {
            RCLCPP_WARN(this->get_logger(),
                        "우회 연속실패 %d회 — rejoin 전방 %.0fm 로 연장",
                        detour_fail_streak_, detour_skip_m_ * mult);
        }
        return std::min(
            near + static_cast<std::size_t>(std::ceil(detour_skip_m_ * mult / 0.5)),
            fp_last_path_.poses.empty() ? std::size_t(0) : fp_last_path_.poses.size() - 1);
    }

    // 밀집 경로(fp_last_path_)상 rejoin_idx 점으로 NavigateToPose 우회(장애물 너머 라인 복귀점).
    // planner 가 장애물을 돌아 경로를 만들고, 도착하면 그 점부터 남은 대칭 라인을 FollowPath 재개.
    void trigger_detour(std::size_t rejoin_idx)
    {
        if (fp_last_path_.poses.size() < 2 || rejoin_idx >= fp_last_path_.poses.size() ||
            !own_pose_valid_) {
            build_and_send_follow_path();   // 우회 불가 → 재전송 폴백
            return;
        }
        fp_rejoin_index_ = rejoin_idx;
        const double rx = fp_last_path_.poses[rejoin_idx].pose.position.x;
        const double ry = fp_last_path_.poses[rejoin_idx].pose.position.y;
        // rejoin 에서 라인 진행방향(다음 점 향함) → 도착 헤딩.
        const std::size_t hn = std::min(rejoin_idx + 1, fp_last_path_.poses.size() - 1);
        const double yaw = std::atan2(fp_last_path_.poses[hn].pose.position.y - ry,
                                      fp_last_path_.poses[hn].pose.position.x - rx);

        // FollowPath 취소 후 우회 모드 진입.
        if (active_fp_goal_) {
            cancel_goal_safe(follow_path_client_, active_fp_goal_);
            active_fp_goal_.reset();
        }
        fp_detour_active_ = true;
        detour_start_t_ = this->now().seconds();   // 행 워치독 기준
        RCLCPP_INFO(this->get_logger(),
                    "로컬 우회: rejoin=(%.1f,%.1f) 밀집idx=%zu/%zu",
                    rx, ry, fp_rejoin_index_, fp_last_path_.poses.size());

        if (!nav_client_->wait_for_action_server(1s)) {
            RCLCPP_WARN(this->get_logger(), "우회 실패 — navigate_to_pose 미준비");
            fp_detour_active_ = false;
            build_and_send_follow_path();
            return;
        }
        NavigateToPose::Goal goal;
        goal.pose.header.frame_id = map_frame_;
        goal.pose.header.stamp = this->now();
        goal.pose.pose.position.x = rx;
        goal.pose.pose.position.y = ry;
        goal.pose.pose.orientation.z = std::sin(yaw * 0.5);
        goal.pose.pose.orientation.w = std::cos(yaw * 0.5);
        auto opts = rclcpp_action::Client<NavigateToPose>::SendGoalOptions();
        opts.goal_response_callback =
            [this](GoalHandleNav::SharedPtr gh){ active_detour_goal_ = gh; };
        opts.result_callback =
            [this](const GoalHandleNav::WrappedResult &r){ detour_result_callback(r); };
        nav_client_->async_send_goal(goal, opts);
    }

    void detour_result_callback(const GoalHandleNav::WrappedResult &result)
    {
        active_detour_goal_.reset();
        if (!active_ || !fp_detour_active_) return;
        fp_detour_active_ = false;
        if (result.code == rclcpp_action::ResultCode::SUCCEEDED) {
            detour_fail_streak_ = 0;
            // rejoin 도착 → 밀집 경로의 rejoin 이후 suffix 만 남겨 FollowPath 재개(라인 복귀).
            // fp_original_length_ 는 불변이라 진행도 fraction 은 원본 스케일 유지.
            nav_msgs::msg::Path rem;
            rem.header = fp_last_path_.header;
            rem.header.stamp = this->now();
            if (fp_rejoin_index_ < fp_last_path_.poses.size()) {
                rem.poses.assign(fp_last_path_.poses.begin() + fp_rejoin_index_,
                                 fp_last_path_.poses.end());
            }
            RCLCPP_INFO(this->get_logger(),
                        "우회 완료 — 라인 복귀(남은 %zu점) FollowPath 재개",
                        rem.poses.size());
            // ★컨보이 팔로워: 잔여가 짧아도 미션 종료가 아니다 — 컨보이 경로는 슬롯에서
            //   절단된 '리더까지의 구간'일 뿐(run15 실측: 우회 복귀 잔여 1점 → active_=false
            //   로 미션 사망 → 영구 정지 + 후순번 추월). trace 로 재개하거나 슬롯 대기.
            if (convoy_follower() && !fp_base_line_.poses.empty() &&
                !leader_trace_.poses.empty()) {
                const auto &te = leader_trace_.poses.back().pose.position;
                const auto &ge = fp_base_line_.poses.back().pose.position;
                if (std::hypot(te.x - ge.x, te.y - ge.y) > 3.0) {
                    if (leader_trace_.poses.size() >= 4) {
                        send_convoy_path();
                    } else {
                        RCLCPP_INFO(this->get_logger(),
                                    "[컨보이] 우회 복귀 — 슬롯 대기(미션 계속)");
                    }
                    return;
                }
            }
            if (rem.poses.size() >= 2) {
                send_fp_path(rem);
            } else {
                active_ = false;
                update_mission_status(
                    combat_robot_msgs::msg::OperationState::MISSION_REACHED);
            }
        } else {
            // 우회 실패(여전히 막힘 등) → 잠시 후 재시도(다음 ABORT/IsPathValid 에 위임).
            // 실패 누적 → 다음 rejoin 은 escalated_rejoin_index() 가 더 멀리 잡음.
            ++detour_fail_streak_;
            RCLCPP_WARN(this->get_logger(), "우회 실패(code=%d, 연속 %d회) — FollowPath 재전송",
                        static_cast<int>(result.code), detour_fail_streak_);
            // ★3연속 즉시실패 = 시작 pose 가 장애물/인플레이션 안(planner 는 goal 이
            //   아니라 start 에서 실패 — rejoin 연장 무효, run22 리더 10연속 실측).
            //   collision_monitor 가 cmd 를 눌러 웨지(cmd 기반)도 발동 불가 → 강제
            //   이탈 후진 0.6m(뒤 1.2m 여유만 확인) 후 재시도.
            // ★후진 금지(사용자 지시): 팔로워는 우회 실패 시에도 후진하지 않고 경로 재시도만.
            //   리더만 최후의 시작-pose 충돌 이탈 후진 허용.
            if (is_leader_ && detour_fail_streak_ >= 3 && wedge_reverse_ticks_ == 0) {
                double nearest_behind = 1e9;
                const auto meb = world_pos_.find(static_cast<int>(robot_id_));
                if (meb != world_pos_.end()) {
                    const double c = std::cos(own_yaw_), s = std::sin(own_yaw_);
                    for (const auto &kv : world_pos_) {
                        if (kv.first == static_cast<int>(robot_id_)) continue;
                        const double rx = kv.second.first - meb->second.first;
                        const double ry = kv.second.second - meb->second.second;
                        if (c * rx + s * ry < 0.0) {
                            nearest_behind = std::min(nearest_behind, std::hypot(rx, ry));
                        }
                    }
                }
                if (nearest_behind > 1.2) {
                    RCLCPP_WARN(this->get_logger(),
                        "[이탈] 우회 %d연속 실패(시작 pose 충돌 의심) — 강제 후진 이탈",
                        detour_fail_streak_);
                    // 0.6m 로는 인플레이션(0.85~2.0m)을 못 벗어나 이탈-실패 사이클이
                    // 수십회 반복(run23: 67회) — 4s(1.2m) 후진으로 확실히 벗어난다.
                    wedge_reverse_ticks_ = 40;
                    wedge_rev_x_ = own_map_x_; wedge_rev_y_ = own_map_y_;
                    return;   // 후진 블록이 종료 후 경로 재전송
                }
            }
            build_and_send_follow_path();
        }
    }

    // 안전 goal 취소: 이미 terminal(ABORTED/SUCCEEDED) 이라 클라이언트가 더는 모르는 handle 을
    // async_cancel_goal 하면 rclcpp_action 이 UnknownGoalHandleError 를 던져 노드가 죽음
    // (STOP 시 s2 executor 크래시 확인). try-catch 로 무시 — 이미 끝난 goal 은 취소할 게 없음.
    template <typename ClientPtr, typename GoalHandlePtr>
    void cancel_goal_safe(const ClientPtr &client, const GoalHandlePtr &goal)
    {
        if (!goal) return;
        try {
            client->async_cancel_goal(goal);
        } catch (const std::exception &e) {
            RCLCPP_DEBUG(this->get_logger(), "goal 취소 무시(이미 종료): %s", e.what());
        }
    }

    void cancel_mission()
    {
        cancel_goal_safe(nav_client_, active_goal_);
        cancel_goal_safe(follow_path_client_, active_fp_goal_);
        cancel_goal_safe(nav_client_, active_detour_goal_);
        cancel_goal_safe(nav_client_, active_formup_goal_);
        active_ = false;
        paused_ = false;
        active_goal_.reset();
        active_fp_goal_.reset();
        active_detour_goal_.reset();
        active_formup_goal_.reset();
        formup_reposition_ = false;
        formup_nav_inflight_ = false;
        formup_anchor_valid_ = false;
        formup_backed_ = false; formup_staged_ = false; formup_phase1_done_ = false;
        leader_formup_cancelled_ = false;
        assignment_valid_ = false;
        fp_detour_active_ = false;
        fp_check_inflight_ = false;
        fp_dist_to_goal_ = 0.0;
        active_points_.clear();
        active_index_ = 0;
        has_base_goal_ = false;   // 횡잠금 중지
        distance_to_next_wp_m_ = 0.0f;
        distance_to_goal_m_ = 0.0f;
        update_mission_status(
            combat_robot_msgs::msg::OperationState::MISSION_NONE);
    }

    void execute_next_waypoint()
    {
        if (active_index_ >= active_points_.size()) {
            RCLCPP_INFO(this->get_logger(), "모든 waypoint 완료 (%zu)", active_points_.size());
            active_ = false;
            distance_to_next_wp_m_ = 0.0f;
            distance_to_goal_m_ = 0.0f;
            update_mission_status(
                combat_robot_msgs::msg::OperationState::MISSION_REACHED);
            return;
        }
        if (!from_ll_client_->wait_for_service(3s)) {
            RCLCPP_ERROR(this->get_logger(),
                         "/fromLL 서비스 없음 (navsat_transform_node 확인)");
            active_ = false;
            distance_to_next_wp_m_ = 0.0f;
            distance_to_goal_m_ = 0.0f;
            update_mission_status(
                combat_robot_msgs::msg::OperationState::MISSION_ERROR);
            return;
        }
        const auto [lat, lon] = active_points_[active_index_];
        auto req = std::make_shared<robot_localization::srv::FromLL::Request>();
        req->ll_point.latitude = lat;
        req->ll_point.longitude = lon;
        req->ll_point.altitude = 0.0;
        from_ll_client_->async_send_request(req,
            std::bind(&SwarmPathExecutorNode::from_ll_response_callback, this, _1));
    }

    void from_ll_response_callback(rclcpp::Client<robot_localization::srv::FromLL>::SharedFuture future)
    {
        if (!active_) {
            RCLCPP_INFO(this->get_logger(),
                        "STOP 이후 늦은 /fromLL 응답 도착 — 무시");
            return;
        }
        if (paused_) {
            // PAUSE 가 /fromLL 대기 중 도착 — goal 전송 보류. RESUME 시 재요청됨.
            RCLCPP_INFO(this->get_logger(),
                        "PAUSE 중 /fromLL 응답 도착 — goal 전송 보류 (RESUME 대기)");
            return;
        }
        auto response = future.get();
        const double map_x = response->map_point.x;
        const double map_y = response->map_point.y;

        RCLCPP_INFO(this->get_logger(),
                    "wp[%zu/%zu] (lat=%.7f, lon=%.7f) → map(%.2f, %.2f) 주행",
                    active_index_ + 1, active_points_.size(),
                    active_points_[active_index_].first,
                    active_points_[active_index_].second,
                    map_x, map_y);

        base_goal_x_ = map_x;          // 원본 목표(횡잠금 보정의 기준)
        base_goal_y_ = map_y;
        has_base_goal_ = true;
        send_nav_goal(map_x, map_y);
    }

    // NavigateToPose 목표 전송(공통). 횡잠금 보정 시 보정된 좌표로 재전송에도 사용.
    void send_nav_goal(double x, double y)
    {
        auto goal = NavigateToPose::Goal();
        goal.pose.header.frame_id = map_frame_;
        goal.pose.header.stamp = this->now();
        goal.pose.pose.position.x = x;
        goal.pose.pose.position.y = y;
        goal.pose.pose.orientation.w = 1.0;

        const int seq = ++goal_seq_;
        auto opts = rclcpp_action::Client<NavigateToPose>::SendGoalOptions();
        opts.goal_response_callback =
            std::bind(&SwarmPathExecutorNode::goal_response_callback, this, _1);
        opts.feedback_callback =
            std::bind(&SwarmPathExecutorNode::feedback_callback, this, _1, _2);
        // 횡잠금 재전송으로 preempt된 옛 goal 의 result(취소/abort)는 무시.
        opts.result_callback =
            [this, seq](const GoalHandleNav::WrappedResult &r){
                if (seq != goal_seq_) return;
                result_callback(r);
            };
        nav_client_->async_send_goal(goal, opts);
        sent_goal_x_ = x;
        sent_goal_y_ = y;
    }

    // 리더-상대 횡잠금: 종(along)은 미션 목표대로 두고, 횡(cross)만 리더 수직위치+오프셋
    // 이 되도록 현재 목표를 옆으로 보정해 재전송. RPP cross-track 오차를 피드백 상쇄 →
    // 모든 대형(앞서가는 것 포함)에서 대칭. 정상대형 주행 중에만 동작.
    void lateral_lock_tick()
    {
        if (!active_ || paused_ || !has_base_goal_ || barrier_waiting_ ||
            reached_index_ < 0 || !leader_world_valid_ || !lock_own_valid_ ||
            !formation_enabled_) {
            return;
        }
        const FormationSlot slot = mySlot();
        if (std::abs(slot.cross) < 1e-6) {
            return;   // 횡오프셋 0(일렬 등) → 횡잠금 불필요
        }
        // 진행방향 = 내 위치→현재목표. 수직(좌) p. (own/leader 모두 동일 소스 사용)
        const double dx = base_goal_x_ - lock_own_x_;
        const double dy = base_goal_y_ - lock_own_y_;
        const double L = std::hypot(dx, dy);
        if (L < 0.5) return;                 // 목표 근처면 보정 안함(jitter 방지)
        const double ux = dx / L, uy = dy / L;
        const double px = -uy, py = ux;       // 좌측 수직 단위
        // 내가 (리더 + cross*spacing*p) 에 있어야 함. 횡오차(=p성분):
        const double cross_m = slot.cross * formation_lateral_spacing_m_;
        const double lateral_err =
            (leader_world_x_ - lock_own_x_) * px + (leader_world_y_ - lock_own_y_) * py + cross_m;
        // 피드백(보수적 게인 + 클램프 — 고게인은 발산). 잔차 ~c/(1+Kp).
        double shift = lateral_lock_gain_ * lateral_err;
        shift = std::clamp(shift, -1.2, 1.2);   // 폭주 방지
        const double gx = base_goal_x_ + shift * px;
        const double gy = base_goal_y_ + shift * py;
        // 변화가 의미있을 때만 재전송(과도한 preempt 방지).
        if (std::hypot(gx - sent_goal_x_, gy - sent_goal_y_) > 0.15) {
            RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 2000,
                "[lat-lock] err=%.2f shift=%.2f own=(%.2f,%.2f) lead=(%.2f,%.2f) "
                "goal %.2f,%.2f -> %.2f,%.2f",
                lateral_err, shift, lock_own_x_, lock_own_y_,
                leader_world_x_, leader_world_y_, base_goal_x_, base_goal_y_, gx, gy);
            send_nav_goal(gx, gy);
        }
    }

    void feedback_callback(
        GoalHandleNav::SharedPtr,
        const std::shared_ptr<const NavigateToPose::Feedback> feedback)
    {
        if (!active_ || paused_ || !feedback) {
            return;
        }
        if (std::isfinite(feedback->distance_remaining)) {
            const float remaining = std::max(0.0f, static_cast<float>(feedback->distance_remaining));
            distance_to_next_wp_m_ = remaining;
            distance_to_goal_m_ = remaining;
        }
        publish_mission_status();
    }

    void goal_response_callback(const GoalHandleNav::SharedPtr &gh)
    {
        if (!gh) {
            RCLCPP_ERROR(this->get_logger(), "Nav2 가 goal 을 거절함");
            active_ = false;
            active_goal_.reset();
            update_mission_status(
                combat_robot_msgs::msg::OperationState::MISSION_ERROR);
            return;
        }
        if (!active_) {
            // STOP 이 goal 전송과 accept 사이에 도착 — 즉시 cancel
            RCLCPP_INFO(this->get_logger(),
                        "STOP 이후 goal 이 accept 됨 — 즉시 cancel");
            cancel_goal_safe(nav_client_, gh);
            return;
        }
        if (paused_) {
            // PAUSE 가 goal 전송과 accept 사이에 도착 — 즉시 cancel (active_index_ 유지)
            RCLCPP_INFO(this->get_logger(),
                        "PAUSE 중 goal 이 accept 됨 — 즉시 cancel");
            cancel_goal_safe(nav_client_, gh);
            return;
        }
        active_goal_ = gh;
        update_mission_status(
            combat_robot_msgs::msg::OperationState::MISSION_MOVING);
    }

    void result_callback(const GoalHandleNav::WrappedResult &result)
    {
        active_goal_.reset();
        if (!active_) {
            // STOP 으로 이미 종료된 상태에서 늦게 도착한 result — 무시
            RCLCPP_INFO(this->get_logger(),
                        "STOP 이후 늦은 nav result 도착 (code=%d) — 무시",
                        static_cast<int>(result.code));
            return;
        }
        if (paused_) {
            // PAUSE 로 인한 cancel 결과 — 미션 실패로 처리하지 않음. active_index_ 유지.
            RCLCPP_INFO(this->get_logger(),
                        "PAUSE 로 wp[%zu] goal cancel 됨 (code=%d) — RESUME 대기",
                        active_index_ + 1, static_cast<int>(result.code));
            update_mission_status(
                combat_robot_msgs::msg::OperationState::MISSION_PAUSED);
            return;
        }
        if (result.code == rclcpp_action::ResultCode::SUCCEEDED) {
            RCLCPP_INFO(this->get_logger(), "wp[%zu] 도착", active_index_ + 1);
            wp_retry_ = 0;
            reached_index_ = static_cast<int>(active_index_);
            distance_to_next_wp_m_ = 0.0f;
            distance_to_goal_m_ = 0.0f;
            // 배리어 wp(시작 form-up + 코너) 도착 → 전원 도착 전엔 출발 보류(정지 대기).
            const bool barrier = formation_enabled_ &&
                active_index_ < is_barrier_.size() && is_barrier_[active_index_] != 0;
            if (barrier && !all_reached(reached_index_)) {
                barrier_waiting_ = true;
                RCLCPP_INFO(this->get_logger(),
                            "[Formation] wp%d 도착 — 전원 정렬 대기(코너/시작)", reached_index_);
                update_mission_status(
                    combat_robot_msgs::msg::OperationState::MISSION_MOVING);
                return;   // nav2 goal 없음 → 정지하고 대기
            }
            ++active_index_;
            update_mission_status(
                combat_robot_msgs::msg::OperationState::MISSION_MOVING);
            execute_next_waypoint();
        } else if (wp_retry_ < kMaxWpRetry) {
            // nav2 abort/취소(예: 일시적으로 다른 로봇이 앞을 막음)는 영구실패로 보지
            // 않고 잠시 후 같은 wp 재시도 — 막은 로봇이 비키면 통과(편대 견고성).
            ++wp_retry_;
            RCLCPP_WARN(this->get_logger(),
                        "wp[%zu] 실패(code=%d) — %1.1fs 후 재시도 %d/%d",
                        active_index_ + 1, static_cast<int>(result.code),
                        1.5, wp_retry_, kMaxWpRetry);
            wp_retry_timer_ = this->create_wall_timer(
                std::chrono::milliseconds(1500), [this]() {
                    wp_retry_timer_->cancel();
                    if (active_ && !paused_) {
                        execute_next_waypoint();   // active_index_ 그대로 = 같은 wp 재시도
                    }
                });
            update_mission_status(
                combat_robot_msgs::msg::OperationState::MISSION_MOVING);
        } else {
            RCLCPP_ERROR(this->get_logger(),
                         "wp[%zu] 최대 재시도(%d) 초과 — 미션 중단",
                         active_index_ + 1, kMaxWpRetry);
            active_ = false;
            wp_retry_ = 0;
            distance_to_next_wp_m_ = 0.0f;
            distance_to_goal_m_ = 0.0f;
            update_mission_status(
                combat_robot_msgs::msg::OperationState::MISSION_ERROR);
        }
    }
};

int main(int argc, char **argv)
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<SwarmPathExecutorNode>());
    rclcpp::shutdown();
    return 0;
}
