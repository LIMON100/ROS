#include "combat_robot_operation_system.hpp"

namespace combat_robot_system {

void CombatRobotOperationSystem::initParameters()
{
    // ── Deployment ──────────────────────────────────────────────────────────
    deployment_mode_ = this->declare_parameter<std::string>("deployment_mode", "production");
    std::transform(
        deployment_mode_.begin(),
        deployment_mode_.end(),
        deployment_mode_.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    demo_deployment_enabled_ = deployment_mode_ == "demo";

    // ── Hardware watchdog toggles ───────────────────────────────────────────
    check_detector_status_ = this->declare_parameter("checks.detector_status", true);
    check_gun_status_      = this->declare_parameter("checks.gun_status", true);
    check_pantilt_status_  = this->declare_parameter("checks.pantilt_status", true);

    // ── Hardware watchdog thresholds (seconds since last update) ────────────
    detector_status_timeout_sec_ =
        this->declare_parameter("checks.detector_status_timeout_sec", 3.0);
    gun_status_timeout_sec_ =
        this->declare_parameter("checks.gun_status_timeout_sec", 5.0);
    pantilt_status_timeout_sec_ =
        this->declare_parameter("checks.pantilt_status_timeout_sec", 3.0);

    // ── Optics ──────────────────────────────────────────────────────────────
    range_deg_       = static_cast<float>(this->declare_parameter("optics.camera_fov_deg", 65.4));
    aspect_ratio_w_  = static_cast<int>(this->declare_parameter("optics.aspect_ratio_w", 16));
    aspect_ratio_h_  = static_cast<int>(this->declare_parameter("optics.aspect_ratio_h", 9));
    fps_             = static_cast<int>(this->declare_parameter("optics.fps", 30));

    // Derived optics
    h_trans_ = static_cast<float>(0.5 / std::tan(range_deg_ / 2.0 * pi / 180.0));
    v_trans_ = (aspect_ratio_h_ > 0)
        ? h_trans_ * static_cast<float>(aspect_ratio_w_) / static_cast<float>(aspect_ratio_h_)
        : h_trans_;

    // ── Pan/Tilt mechanical limits and speed scaling ────────────────────────
    min_pan_deg_         = static_cast<float>(this->declare_parameter("pan_tilt.min_pan_deg", -180.0));
    max_pan_deg_         = static_cast<float>(this->declare_parameter("pan_tilt.max_pan_deg",  180.0));
    min_tilt_deg_        = static_cast<float>(this->declare_parameter("pan_tilt.min_tilt_deg", -75.0));
    max_tilt_deg_        = static_cast<float>(this->declare_parameter("pan_tilt.max_tilt_deg",  45.0));
    pan_speed_divider_   = static_cast<float>(this->declare_parameter("pan_tilt.pan_speed_divider",  10.0));
    tilt_speed_divider_  = static_cast<float>(this->declare_parameter("pan_tilt.tilt_speed_divider", 5.0));
    tilt_aspect_factor_  = static_cast<float>(this->declare_parameter("pan_tilt.tilt_aspect_factor", 0.75));
    pan_tilt_min_speed_  = static_cast<float>(this->declare_parameter("pan_tilt.min_speed", 3.0));

    // ── Prediction / feed-forward ───────────────────────────────────────────
    m_enable_prediction_  = this->declare_parameter("prediction.enable", false);
    m_pan_tilt_delay_     = this->declare_parameter("prediction.pan_tilt_delay", 0.1);
    m_ff_gain_            = this->declare_parameter("prediction.ff_gain", 0.0);
    m_velocity_lpf_alpha_ = this->declare_parameter("prediction.velocity_lpf_alpha", 0.5);

    // ── Ballistic correction ────────────────────────────────────────────────
    m_gravity_correction_ = this->declare_parameter("correction.gravity", 0.0);
    m_wind_correction_x_  = this->declare_parameter("correction.wind_x", 0.0);
    m_wind_correction_y_  = this->declare_parameter("correction.wind_y", 0.0);

    // ── Scanning timeout shape ──────────────────────────────────────────────
    scan_base_timeout_sec_      = this->declare_parameter("scanning.base_timeout_sec", 3.0);
    scan_timeout_speed_divider_ = this->declare_parameter("scanning.timeout_speed_divider", 2.0);
    scan_pan_range_deg_  = static_cast<float>(
        this->declare_parameter("scanning.pan_range_deg", 40.0));
    scan_tilt_range_deg_ = static_cast<float>(
        this->declare_parameter("scanning.tilt_range_deg", 0.0));
    scan_tilt_step_deg_  = static_cast<float>(
        this->declare_parameter("scanning.tilt_step_deg", 15.0));
    scan_general_tilt_deg_ = static_cast<float>(
        this->declare_parameter("scanning.general_tilt_deg", 0.0));
    scan_drone_tilt_deg_ = static_cast<float>(
        this->declare_parameter("scanning.drone_tilt_deg", -30.0));

    // ── Tracking lock-lost debounce ─────────────────────────────────────────
    // Production PROTECT modes saw SURVEILLANCE↔TRACKING flap every frame
    // when the detector's is_locked toggled on a single bad frame. Hold
    // TRACKING for this long after the last positive lock before reverting.
    tracking_lock_lost_grace_sec_ =
        this->declare_parameter("tracking.lock_lost_grace_sec", 1.0);
    tracking_min_lock_held_sec_ =
        this->declare_parameter("tracking.min_lock_held_sec", 0.2);

    // ── PROTECT engagement queue ────────────────────────────────────────────
    // Multi-target rotation in PROTECT modes. SURVEILLANCE enqueues every
    // newly-locked track_id (up to target_count). TRACKING fires the gun
    // continuously while the active target stays locked AND
    // attack_permission==APPROVE; if permission is revoked the active
    // target is dropped from the queue and the FSM moves on. No per-target
    // fire-duration cap — fire is bounded by how long the target stays in
    // FOV and how long permission stays APPROVE.
    const auto declared_protect_target_count =
        this->declare_parameter("protect.target_count", 3L);
    protect_target_count_ =
        std::max<int>(1, static_cast<int>(declared_protect_target_count));
    // Hold off SURVEILLANCE→TRACKING by this many seconds (or until the
    // queue hits target_count) so a sweep can accumulate multiple
    // track_ids before engagement starts. Default 0 — transition on the
    // first lock so the pan/tilt stops scanning and starts aiming
    // immediately; raise to e.g. 5.0 for the wall-demo multi-queue case.
    protect_scan_duration_sec_ =
        this->declare_parameter("protect.scan_duration_sec", 0.0);

    // ── Aim point shaping ───────────────────────────────────────────────────
    // human_height_ratio: aim point as fraction from bbox TOP toward bottom.
    // 0.3 ≈ head/face, 0.5 = bbox center, 0.7 ≈ lower torso.
    aim_human_height_ratio_ =
        this->declare_parameter("aim.human_height_ratio", 0.3);

    // ── Demo sequence ───────────────────────────────────────────────────────
    demo_forward_distance_m_ = this->declare_parameter("demo.forward_distance_m", 0.3);
    demo_reverse_distance_m_ = this->declare_parameter("demo.reverse_distance_m", 0.3);
    demo_drive_speed_mps_    = this->declare_parameter("demo.drive_speed_mps", 0.2);
    demo_fire_duration_sec_  = this->declare_parameter("demo.fire_duration_sec", 3.0);
    demo_fire_duration_multi_sec_ = this->declare_parameter("demo.fire_duration_multi_sec", 3.0);
    demo_scan_duration_sec_  = this->declare_parameter("demo.scan_duration_sec", 3.0);
    demo_engage_target_timeout_sec_ = this->declare_parameter("demo.engage_target_timeout_sec", 5.0);
    demo_total_duration_sec_ = this->declare_parameter("demo.total_duration_sec", 0.0);
    const auto declared_demo_target_count = this->declare_parameter("demo.target_count", 3L);
    demo_target_count_ = std::max<int>(1, static_cast<int>(declared_demo_target_count));
    demo_scan_default_pan_deg_  = static_cast<float>(
        this->declare_parameter("demo.scan_default_pan_deg", 0.0));
    demo_scan_default_tilt_deg_ = static_cast<float>(
        this->declare_parameter("demo.scan_default_tilt_deg", 0.0));

    // ── Dummy leader state (overlay-driven; absent in production base) ─────
    dummy_leader_state_.enabled = this->declare_parameter("dummy_leader_state.enabled", false);
    dummy_leader_state_.gps_lat = this->declare_parameter("dummy_leader_state.gps_lat", 37.402152);
    dummy_leader_state_.gps_lon = this->declare_parameter("dummy_leader_state.gps_lon", 127.108517);
    dummy_leader_state_.gps_heading = static_cast<float>(
        this->declare_parameter("dummy_leader_state.gps_heading", 78.0));
    dummy_leader_state_.current_speed_mps = static_cast<float>(
        this->declare_parameter("dummy_leader_state.current_speed_mps", 1.6));
    dummy_leader_state_.current_waypoint_index = static_cast<uint16_t>(
        this->declare_parameter("dummy_leader_state.current_waypoint_index", 3));
    dummy_leader_state_.total_waypoints = static_cast<uint16_t>(
        this->declare_parameter("dummy_leader_state.total_waypoints", 8));
    dummy_leader_state_.progress_ratio = static_cast<float>(
        this->declare_parameter("dummy_leader_state.progress_ratio", 0.38));
    dummy_leader_state_.distance_to_next_wp_m = static_cast<float>(
        this->declare_parameter("dummy_leader_state.distance_to_next_wp_m", 12.5));
    dummy_leader_state_.distance_to_goal_m = static_cast<float>(
        this->declare_parameter("dummy_leader_state.distance_to_goal_m", 94.0));
}

CombatRobotOperationSystem::CombatRobotOperationSystem(
    const rclcpp::NodeOptions& options)
    : Node("combat_robot_operation_system_node", options)
{
    initParameters();

    m_pub_pan_tilt_control_command =
        this->create_publisher<PanTiltControlCommand>("/pan_tilt_control_command", 10);
    m_pub_drive_command_ = this->create_publisher<DriveCommand>("/drive_command", 1);

    m_sub_controller_state = this->create_subscription<PanTiltState>(
        "/pan_tilt_state", 1,
        std::bind(&CombatRobotOperationSystem::onPanTiltState, this, _1));

    rclcpp::QoS qos_profile(10);
    qos_profile.reliability(rclcpp::ReliabilityPolicy::BestEffort);
    qos_profile.durability(rclcpp::DurabilityPolicy::Volatile);
    m_sub_detected_target_ = this->create_subscription<TargetPoint>(
        "/human_detector/human/target_point", qos_profile,
        std::bind(&CombatRobotOperationSystem::onTargetPoint, this, _1));

    // [per-robot 보드 모델] FSM 은 자기 네임스페이스(/sN) 안의 path_executor 만 게이트한다.
    // 토픽을 상대경로로 두어 /sN 에서 실행되면 /sN/... 로, 네임스페이스 없으면(단일로봇)
    // 기존 /... 로 해석된다(무회귀).
    m_sub_mission_control_command_ = this->create_subscription<MissionControlCommand>(
        "mission_control_command", 10,
        std::bind(&CombatRobotOperationSystem::onMissionControlCommand, this, _1));

    // ★ TRANSIENT_LOCAL: 미션 sender(리더 보드 로컬)가 발행할 때, 팔로워 FSM 이 mesh
    //   discovery 로 늦게 매칭돼도 latch 된 마지막 명령(LOAD 등)을 받도록 한다. VOLATILE
    //   이면 매칭 전 발행된 LOAD 를 놓쳐(팔로워 form-up 불가) → durable 구독으로 근본 해결.
    m_sub_swarm_path_command_ = this->create_subscription<SwarmPathCommand>(
        "swarm/path_command",
        rclcpp::QoS(rclcpp::KeepLast(10)).transient_local().reliable(),
        std::bind(&CombatRobotOperationSystem::onSwarmPathCommand, this, _1));

    // FSM 게이트 후 자기 robot 의 path_executor 로 전달되는 경로 명령.
    m_pub_mission_path_command_ =
        this->create_publisher<SwarmPathCommand>("mission/path_command", 10);

    // 대형(formation) 게이트: command_server → FSM → executor. 경로와 동일 패턴.
    //   ★ path_command 와 동일하게 TRANSIENT_LOCAL — 팔로워 늦은 매칭에도 마지막 대형
    //   명령을 놓치지 않도록 durable 구독.
    m_sub_swarm_control_command_ = this->create_subscription<SwarmControlCommand>(
        "swarm/control_command",
        rclcpp::QoS(rclcpp::KeepLast(10)).transient_local().reliable(),
        std::bind(&CombatRobotOperationSystem::onSwarmControlCommand, this, _1));
    m_pub_mission_control_command_swarm_ =
        this->create_publisher<SwarmControlCommand>("mission/control_command", 10);

    // path_executor 의 실측 상태/GPS/nav 진행률 → /operation_state 로 통합.
    m_sub_swarm_mission_state_ = this->create_subscription<OperationState>(
        "swarm/mission_state", 10,
        std::bind(&CombatRobotOperationSystem::onSwarmMissionState, this, _1));

    m_sub_zoom_level_ = this->create_subscription<std_msgs::msg::Int32>(
        "/zoom_level", 10,
        std::bind(&CombatRobotOperationSystem::onZoomLevel, this, _1));

    m_sub_touch_command = this->create_subscription<combat_robot_msgs::msg::TouchTargetPoint>(
        "/touch_command", 10,
        std::bind(&CombatRobotOperationSystem::onTouchCommand, this, _1));

    m_pub_operation_state = this->create_publisher<OperationState>("/operation_state", 10);
    m_pub_center_object = this->create_publisher<CenterObject>("/center_object", 10);

    m_sub_laser_distance_ = this->create_subscription<std_msgs::msg::Float64>(
        "/sensor/distance", 10,
        std::bind(&CombatRobotOperationSystem::onLaserDistance, this, _1));

    m_client_change_state_ =
        this->create_client<lifecycle_msgs::srv::ChangeState>("/pan_tilt_controller/change_state");

    std::string gun_cmd_topic =
        this->declare_parameter<std::string>("topics.gun_trigger_cmd", "/gun_trigger/cmd");
    std::string gun_status_topic =
        this->declare_parameter<std::string>("topics.gun_trigger_status", "/gun_trigger/status");

    pub_gun_cmd_ = this->create_publisher<std_msgs::msg::Int8>(gun_cmd_topic, 10);
    sub_gun_status_ = this->create_subscription<std_msgs::msg::Int8>(
        gun_status_topic, 10,
        [this](const std_msgs::msg::Int8::SharedPtr msg) {
            if (!msg) {
                return;
            }
            gun_status_.store(static_cast<int>(msg->data), std::memory_order_relaxed);
            last_gun_status_update_time_ = this->now();
        });

    const double timer_rate_hz = static_cast<double>(std::max(1, fps_));
    timer_ = rclcpp::create_timer(
        this, get_clock(), rclcpp::Rate(timer_rate_hz).period(),
        std::bind(&CombatRobotOperationSystem::on_timer, this));

    combat_robot_system_control_cmd_time_ = this->now();
}

CombatRobotOperationSystem::~CombatRobotOperationSystem()
{
    RCLCPP_INFO(this->get_logger(), "Combat Robot Operation System Destructor");

    // Send stop to every actuator twice with a small gap, so a dropped
    // packet on the serial bus / Modbus / sysfs PWM doesn't leave anything
    // moving or firing after the node exits.
    std_msgs::msg::Int8 gun_off;
    gun_off.data = 0;

    PublishDriveCommand(0.0, 0.0);
    PublishControllerStop();
    if (pub_gun_cmd_) {
        pub_gun_cmd_->publish(gun_off);
    }
    rclcpp::sleep_for(std::chrono::milliseconds(50));
    PublishDriveCommand(0.0, 0.0);
    PublishControllerStop();
    if (pub_gun_cmd_) {
        pub_gun_cmd_->publish(gun_off);
    }

    RCLCPP_INFO(this->get_logger(), "Stop command sent (drive + pan/tilt brake + gun off, duplicated for safety) in destructor.");
}

}  // namespace combat_robot_system

#include <rclcpp_components/register_node_macro.hpp>
RCLCPP_COMPONENTS_REGISTER_NODE(combat_robot_system::CombatRobotOperationSystem)
