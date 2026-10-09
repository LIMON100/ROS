#ifndef COMBAT_ROBOT_OPERATION_SYSTEM_HPP_
#define COMBAT_ROBOT_OPERATION_SYSTEM_HPP_

// for ROS2 System
#include <rclcpp/rclcpp.hpp>

// for C++ Common
#include <iostream>
#include <mutex>
#include <string>
#include <vector>
#include <deque>
#include <unordered_set>
#include <cmath>
#include <cctype>
#include <algorithm>
#include <chrono>

// for ROS2 std msgs
#include "std_msgs/msg/u_int8.hpp"
#include "std_msgs/msg/int8.hpp"
#include "std_msgs/msg/int32.hpp"
#include "std_msgs/msg/float64.hpp"

// for Lifecycle
#include "lifecycle_msgs/srv/change_state.hpp"

// for ROS2 Msgs
#include "combat_robot_msgs/msg/target_point.hpp"
#include "combat_robot_msgs/msg/detected_object.hpp"
#include "combat_robot_msgs/msg/detected_objects.hpp"
#include "combat_robot_msgs/msg/mission_control_command.hpp"
#include "combat_robot_msgs/msg/operation_state.hpp"
#include "combat_robot_msgs/msg/swarm_path_command.hpp"
#include "combat_robot_msgs/msg/swarm_control_command.hpp"
#include "combat_robot_msgs/msg/touch_target_point.hpp"
#include "combat_robot_msgs/msg/drive_command.hpp"

#include "combat_robot_msgs/msg/pan_tilt_control_command.hpp"
#include "combat_robot_msgs/msg/pan_tilt_state.hpp"
#include "combat_robot_msgs/msg/center_object.hpp"

namespace combat_robot_system {

using std::placeholders::_1;

using combat_robot_msgs::msg::DetectedObject;
using combat_robot_msgs::msg::DetectedObjects;
using combat_robot_msgs::msg::TargetPoint;
using combat_robot_msgs::msg::MissionControlCommand;
using combat_robot_msgs::msg::OperationState;
using combat_robot_msgs::msg::SwarmPathCommand;
using combat_robot_msgs::msg::SwarmControlCommand;
using combat_robot_msgs::msg::TouchTargetPoint;
using combat_robot_msgs::msg::DriveCommand;

using combat_robot_msgs::msg::PanTiltState;
using combat_robot_msgs::msg::PanTiltControlCommand;

using combat_robot_msgs::msg::CenterObject;

typedef enum{
    RUN_IDLE = 0,
    RUN_MOVING = 1,
    RUN_SURVEILLANCE = 2,
    RUN_DRONE_SURVEILLANCE = 3,
    RUN_ATTACKING = 4,
    RUN_MANUAL_ATTACK = 5,
    RUN_ASSAULT = 6,
    RUN_EMERGENCY_STOP = 7,
    RUN_DEMO = 8,
} e_run_mode_;

typedef enum{
    INIT_STATE,
    IDLE,
    MOVE_STATE,
    SURVEILLANCE_STATE,
    DRONE_SURVEILLANCE_STATE,
    ATTACKING_STATE,
    ASSAULT_STATE,
    TRACKING_STATE,
    RTH_STATE,
    EMERGENCY_STOP_STATE,
    ERROR_STATE,
} e_operation_state;

typedef enum{
    NONE,
    DETECTOR_ERROR,
    PANTILT_ERROR,
    GUNTRIGGER_ERROR,
    COMBATROBOT_ERROR,
} e_error_state_;

typedef enum{
    TILT_UP = 1,
    TILT_DOWN = 2,
} e_tilt_dir_;

typedef enum{
    PAN_LEFT = 1,
    PAN_RIGHT = 2,
} e_pan_dir_;

typedef struct{
    float x;
    float y;
} point_t;

typedef enum{
    SCAN_FIRST_ROW,
    SCAN_LEFT,
    SCAN_RIGHT,
    SCAN_NEXT_ROW
} e_scanning_action_;

typedef enum{
    DEMO_PHASE_IDLE,
    DEMO_PHASE_FORWARD,
    DEMO_PHASE_SCAN,
    DEMO_PHASE_ENGAGE,
    DEMO_PHASE_REVERSE,
    DEMO_PHASE_COMPLETE
} e_demo_phase_;

typedef struct TargetObject {
    float x = 0.0;
    float y = 0.0;
    float height = 0.0;
    int32_t track_id = -1;
    int32_t class_id = -1;
    combat_robot_msgs::msg::BoundingBox2d bounding_box;
} TargetObject;

typedef struct DummyLeaderState {
    bool enabled = false;
    double gps_lat = 37.402152;
    double gps_lon = 127.108517;
    float gps_heading = 78.0f;
    float current_speed_mps = 1.6f;
    uint16_t current_waypoint_index = 3;
    uint16_t total_waypoints = 8;
    float progress_ratio = 0.38f;
    float distance_to_next_wp_m = 12.5f;
    float distance_to_goal_m = 94.0f;
} DummyLeaderState;

typedef struct MissionExecutionState {
    uint8_t status = OperationState::MISSION_NONE;
    uint16_t current_waypoint_index = 0;
    uint16_t total_waypoints = 0;
    float current_speed_mps = 0.0f;
    bool path_loaded = false;
} MissionExecutionState;

// 차량 mission_control_node(/swarm/mission_state)가 보고하는 실측 nav 텔레메트리.
// FSM 이 자체 생산하지 않는 값(실 GPS/nav 진행률)을 /operation_state 에 통합해
// command_server 로 단일 전달하기 위해 캐싱한다.
typedef struct VehicleNavTelemetry {
    bool valid = false;
    uint8_t mission_status = OperationState::MISSION_NONE;
    double gps_lat = 0.0;
    double gps_lon = 0.0;
    float gps_heading = 0.0f;
    float current_speed_mps = 0.0f;
    uint16_t current_waypoint_index = 0;
    uint16_t total_waypoints = 0;
    float progress_ratio = 0.0f;
    float distance_to_next_wp_m = 0.0f;
    float distance_to_goal_m = 0.0f;
    uint8_t error_code = 0;
} VehicleNavTelemetry;

class CombatRobotOperationSystem : public rclcpp::Node{
public:
    explicit CombatRobotOperationSystem(const rclcpp::NodeOptions & options);
    ~CombatRobotOperationSystem();

    static constexpr auto in_offset_range = [](auto a, auto b, auto offset) {
        return a >= (b - offset) && a <= (b + offset);
    };

private:
    // global variable state
    // Removed combat_robot_system_control_cmd_time_ and combat_robot_system_control_
    rclcpp::Time combat_robot_system_control_cmd_time_{0, 0, RCL_ROS_TIME};//
    bool combat_robot_system_control_ = false;

    // operation state

    e_run_mode_ run_mode_{RUN_IDLE}; // Current run mode
    e_operation_state state_{INIT_STATE};
    uint8_t last_non_estop_active_mode_id_{OperationState::ACTIVE_MODE_IDLE};
    
    e_error_state_ sensor_error_state_{NONE}; // Sensor error state
    e_error_state_ error_state_{NONE};
    rclcpp::Time last_error_recovery_attempt_time_{0, 0, RCL_ROS_TIME}; // Last error recovery attempt time
    bool isInitialized_{false}; // Flag to check if the system is initialized
    bool init_command_sent_{false}; // Flag to check if the init command has been sent
    bool last_cmd_stop_=false;
    std::string deployment_mode_{"production"};
    bool demo_deployment_enabled_{false};
    uint8_t demo_reported_active_mode_id_{OperationState::ACTIVE_MODE_RECON};

    std::mutex mtx_cmd_;
    std::mutex mtx_touch_cmd_;
    mutable std::mutex mtx_target_;
    mutable std::mutex mtx_actuator_;
    mutable std::mutex mtx_mission_;
    
    // from Human Detector
    rclcpp::Time last_detector_update_time_{0, 0, RCL_ROS_TIME};
    double last_Target_time_{0.0};
    std_msgs::msg::Header last_target_header_;
    std::atomic<bool> isTargetLocked_{false}; // Flag to check if the target is locked
    TargetObject target_object_;

    // from Pan/Tilt Mount
    rclcpp::Time last_pantilt_state_update_time_{0, 0, RCL_ROS_TIME}; // Last update time for the pan-tilt state (0 means not updated yet)
    float current_actuator_horizontal_angle_{-1.0}; // Horizontal angle (pan)
    float current_actuator_vertical_angle_{-1.0};   // Vertical angle (tilt)
    float prev_actuator_horizontal_angle_{-1.0}; // Previous horizontal angle
    float prev_actuator_vertical_angle_{-1.0};   // Previous vertical angle
    bool is_pan_tilt_moving_{false}; // Flag to check if the pan-tilt mount is moving

    // Pan/Tilt Control
    float prev_target_pan_deg_{0.0};
    float prev_target_tilt_deg_{0.0};

    int pan_dir_{0};
    int tilt_dir_{0};
    int tilt_scan_cnt_{0};

    float min_pan_deg_{-180.0f};
    float max_pan_deg_{180.0f};
    float min_tilt_deg_{-75.0f};
    float max_tilt_deg_{45.0f};

    float pan_speed_divider_{10.0f};   // pan_coeff_speed = range_deg_ / pan_speed_divider_
    float tilt_speed_divider_{5.0f};   // tilt_coeff_speed = range_deg_ / tilt_speed_divider_
    float tilt_aspect_factor_{0.75f};  // tilt_diff scaling (was hardcoded *3/4)
    float pan_tilt_min_speed_{3.0f};   // speed floor/offset: speed=(|err_deg|+min_speed)*coeff; raise to speed up final approach

    float range_deg_{65.4f}; // Camera FOV (deg)
    int aspect_ratio_w_{16};
    int aspect_ratio_h_{9};
    int fps_{30};
    const double pi = 3.14159265358979323846;
    float h_trans_{0.0f}; // computed from range_deg_ and aspect ratio in initParameters()
    float v_trans_{0.0f};

    std::atomic<bool> attack_mode_init_{false};
    std::atomic<bool> request_attack_permission_{false};

    // from Mission Control Command
    rclcpp::Time last_mission_control_command_time_{0, 0, RCL_ROS_TIME};
    MissionControlCommand mission_control_command_;
    MissionExecutionState mission_execution_state_;
    // mtx_mission_ 로 보호. 차량 mission_control 실측 nav 텔레메트리 캐시.
    VehicleNavTelemetry vehicle_nav_telemetry_;
    rclcpp::Time last_swarm_mission_state_time_{0, 0, RCL_ROS_TIME};
    TouchTargetPoint touch_command_point_;
    std::atomic<bool> new_touch_command_{false};
    std::atomic<bool> new_pan_tilt_command_{false};

    // Tracking parameters
    int tracking_horizontal_angle_{0};
    int tracking_vertical_angle_{0};
    int tracking_pan_speed_{0};
    int tracking_tilt_speed_{0};

    // Ballistic correction parameters
    double m_gravity_correction_{0.0};
    double m_wind_correction_x_{0.0};
    double m_wind_correction_y_{0.0};

    // Pan-tilt delay compensation
    bool m_enable_prediction_{false};
    double m_pan_tilt_delay_{0.2}; // seconds
    TargetObject m_prev_target_object_;
    double m_prev_target_time_{0.0};
    double m_velocity_x_{0.0};
    double m_velocity_y_{0.0};

    // Feedforward gain: scales target velocity (img coord/s → deg) added directly to target angle.
    // Set to 0.0 to disable. Start low (0.05) and increase while watching for overshoot.
    double m_ff_gain_{0.0};

    // Low-pass filter alpha for velocity smoothing (0 < alpha <= 1).
    // Higher = faster response, more noise. Lower = smoother, more lag.
    double m_velocity_lpf_alpha_{0.5};

    const float crosshair_x_{0.5}; // Crosshair position (normalized 0-1)
    const float crosshair_y_{0.5};

    // Scanning parameters
    std::atomic<bool> isScanning_{false};
    e_scanning_action_ prev_scanning_action_{SCAN_FIRST_ROW};
    rclcpp::Time last_scanning_stable_time_{0, 0, RCL_ROS_TIME}; // Timer for dwell time after reaching scan limit
    rclcpp::Time last_scanning_move_start_time_{0, 0, RCL_ROS_TIME}; // Timer for checking movement timeout
    double scanning_timeout_duration_{5.0}; // Dynamic timeout duration (recomputed at run-time)
    double scan_base_timeout_sec_{3.0};
    double scan_timeout_speed_divider_{2.0};
    float scan_pan_range_deg_{40.0f};   // total pan sweep width centered on default_pan_angle_
    float scan_tilt_range_deg_{0.0f};   // total tilt sweep height (0 = pan-only sweep)
    float scan_tilt_step_deg_{15.0f};   // row step when tilt_range_deg > 0
    float scan_general_tilt_deg_{0.0f}; // PROTECT_GENERAL scan tilt baseline (horizontal)
    float scan_drone_tilt_deg_{-30.0f}; // PROTECT_DRONE scan tilt baseline (angled upward)
    bool waiting_for_stabilization_{false}; // Flag to indicate waiting for stabilization dwell time

    // Tracking lock-lost debounce: hold TRACKING_STATE for this many seconds
    // after isTargetLocked_ goes false before reverting to SURVEILLANCE.
    rclcpp::Time tracking_lock_lost_start_time_{0, 0, RCL_ROS_TIME};
    double tracking_lock_lost_grace_sec_{1.0};
    // Lock-flicker rejection: a single lock=true frame must be followed by
    // continuously-held lock for `min_lock_held_sec` before it counts as a
    // genuine re-acquisition and resets the lost timer. Brief blips of
    // lock=true (e.g. spurious detector frames) no longer keep TRACKING
    // pinned open indefinitely.
    rclcpp::Time tracking_lock_held_start_time_{0, 0, RCL_ROS_TIME};
    double tracking_min_lock_held_sec_{0.2};

    // PROTECT_GENERAL / PROTECT_DRONE multi-target engage queue.
    // SURVEILLANCE accumulates each newly-locked track_id into the queue
    // (up to protect.target_count). TRACKING dequeues one, fires
    // continuously while the target stays locked, then rotates to the
    // next queued id. Per-id approval state lives in the two sets below:
    //   approved_track_ids_ — operator returned APPROVE, auto-fire
    //   denied_track_ids_   — operator returned DENY, skip on adopt
    // Both reset on IDLE entry.
    std::deque<int32_t> protect_target_queue_;
    std::unordered_set<int32_t> protect_approved_track_ids_;
    std::unordered_set<int32_t> protect_denied_track_ids_;
    int32_t protect_active_track_id_{-1};
    rclcpp::Time protect_scan_start_time_{0, 0, RCL_ROS_TIME};
    int protect_target_count_{3};
    // Default 0 = transition to TRACKING on the first lock, so pan/tilt
    // stops sweeping and starts aiming immediately. Set >0 to accumulate
    // multiple track_ids before engagement (wall-demo style).
    double protect_scan_duration_sec_{0.0};

    // Aim point inside the bounding box. For class_id=0 (person), bbox
    // center is mid-torso — aim higher by treating the aim point as
    // `human_height_ratio` of the way from bbox TOP toward the bottom.
    // 0.0 ≈ top of head; 0.3 ≈ head/face; 0.5 = center (bbox center,
    // disables the offset); 0.7 ≈ lower torso.
    double aim_human_height_ratio_{0.3};

    e_demo_phase_ demo_phase_{DEMO_PHASE_IDLE};
    rclcpp::Time demo_phase_start_time_{0, 0, RCL_ROS_TIME};
    rclcpp::Time demo_fire_start_time_{0, 0, RCL_ROS_TIME};
    rclcpp::Time demo_total_start_time_{0, 0, RCL_ROS_TIME};
    bool demo_initialized_{false};
    bool demo_completed_{false};
    bool demo_fire_started_{false};
    int32_t demo_active_track_id_{-1};
    std::vector<int32_t> demo_completed_track_ids_;
    std::deque<int32_t> demo_target_queue_;
    rclcpp::Time demo_engage_wait_start_time_{0, 0, RCL_ROS_TIME};
    double demo_forward_distance_m_{0.3};
    double demo_reverse_distance_m_{0.3};
    double demo_drive_speed_mps_{0.2};
    double demo_fire_duration_sec_{3.0};
    double demo_fire_duration_multi_sec_{3.0};
    double demo_active_fire_duration_sec_{3.0};
    double demo_scan_duration_sec_{3.0};
    double demo_engage_target_timeout_sec_{5.0};
    double demo_total_duration_sec_{0.0};  // 0 = single pass (forward→scan→engage→reverse); >0 = loop scan/engage until elapsed
    int demo_target_count_{3};
    float demo_scan_default_pan_deg_{0.0f};
    float demo_scan_default_tilt_deg_{0.0f};
    
    // Pub & Sub
    // Pan/Tilt Mount
    rclcpp::Publisher<PanTiltControlCommand>::SharedPtr m_pub_pan_tilt_control_command;
    rclcpp::Publisher<DriveCommand>::SharedPtr m_pub_drive_command_;
    rclcpp::Subscription<PanTiltState>::SharedPtr m_sub_controller_state;

    // Detector
    rclcpp::Subscription<TargetPoint>::SharedPtr m_sub_detected_target_;
    rclcpp::Subscription<DetectedObjects>::SharedPtr m_sub_detected_objects_;

    // Gun Trigger
    // Removed m_pub_gun_trigger
    rclcpp::Publisher<std_msgs::msg::Int8>::SharedPtr pub_gun_cmd_;
    rclcpp::Subscription<std_msgs::msg::Int8>::SharedPtr sub_gun_status_;
    std::atomic<int> gun_status_{0}; // 0=IDLE, 1=FIRING, -1=ERROR
    rclcpp::Time last_gun_status_update_time_{0, 0, RCL_ROS_TIME};
    // Moving
    // Removed m_pub_combat_robot_system_control

    // Mission Control Command
    rclcpp::Subscription<MissionControlCommand>::SharedPtr m_sub_mission_control_command_;
    rclcpp::Subscription<SwarmPathCommand>::SharedPtr m_sub_swarm_path_command_;
    // 태블릿 경로 명령을 게이트 후 path_executor 로 전달하는 내부 토픽 publisher.
    rclcpp::Publisher<SwarmPathCommand>::SharedPtr m_pub_mission_path_command_;
    // 대형(formation) 게이트: command_server → FSM → executor.
    rclcpp::Subscription<SwarmControlCommand>::SharedPtr m_sub_swarm_control_command_;
    rclcpp::Publisher<SwarmControlCommand>::SharedPtr m_pub_mission_control_command_swarm_;
    // path_executor 의 swarm/mission_state 를 받아 /operation_state 에 통합.
    rclcpp::Subscription<OperationState>::SharedPtr m_sub_swarm_mission_state_;
    rclcpp::Subscription<TouchTargetPoint>::SharedPtr m_sub_touch_command;

    // Zoom Controller
    rclcpp::Subscription<std_msgs::msg::Int32>::SharedPtr m_sub_zoom_level_;
    std::atomic<int> current_zoom_level_{0};

    rclcpp::TimerBase::SharedPtr timer_;
    // Operation state publisher
    rclcpp::Publisher<OperationState>::SharedPtr m_pub_operation_state;
    rclcpp::Publisher<CenterObject>::SharedPtr m_pub_center_object;

    // Laser Distance
    rclcpp::Subscription<std_msgs::msg::Float64>::SharedPtr m_sub_laser_distance_;
    std::atomic<double> current_laser_distance_{0.0};

    // Lifecycle Client
    rclcpp::Client<lifecycle_msgs::srv::ChangeState>::SharedPtr m_client_change_state_;
    int recovery_step_{0};

    // Parameters for hardware checks
    bool check_detector_status_{true};
    bool check_gun_status_{true};
    bool check_pantilt_status_{true};
    double detector_status_timeout_sec_{3.0};
    double gun_status_timeout_sec_{5.0};
    double pantilt_status_timeout_sec_{3.0};
    DummyLeaderState dummy_leader_state_;

    /* Operation Functions */
    bool Init_statefunc();
    void Idle_statefunc();

    void Move_statefunc();
    void Demo_statefunc();
    void Surveillance_statefunc();
    void SurveillanceDrone_statefunc();
    // Shared PROTECT surveillance tick: scan centered at t_default_tilt_deg and
    // accumulate newly-locked track_ids of class t_wanted_class_id into the engage
    // queue. PROTECT_GENERAL uses (general tilt, person=0); PROTECT_DRONE uses
    // (drone tilt, drone=1) — otherwise identical.
    void protectSurveillanceTick(float t_default_tilt_deg, int32_t t_wanted_class_id);
    // Class this engagement mode targets: drone (1) for PROTECT_DRONE, person (0)
    // for PROTECT_GENERAL and ASSAULT. Single source of truth for the
    // run_mode -> class mapping.
    int32_t wantedClassForRunMode() const {
        return (run_mode_ == RUN_DRONE_SURVEILLANCE) ? 1 : 0;
    }
    // Run modes that perform the shared scan -> track -> operator-approval ->
    // fire engagement flow with the target queue: the two PROTECT modes plus
    // ASSAULT (hybrid). RECON / manual-attack / demo are excluded. Single
    // source of truth for the "is this an engagement run" checks.
    bool isEngagementRunMode() const {
        return run_mode_ == RUN_SURVEILLANCE ||
               run_mode_ == RUN_DRONE_SURVEILLANCE ||
               run_mode_ == RUN_ASSAULT;
    }
    // True iff a target of the wanted class is currently locked (reads class
    // under mtx_target_). Used so PROTECT only enters/holds TRACKING for its
    // own class and ignores a wrong-class lock.
    bool wantedClassTargetLocked();
    void Attacking_statefunc();
    void Assault_statefunc();

    void Tracking_statefunc();

    void EmergencyStop_statefunc();

    void Error_statefunc();

    void on_timer();
    e_error_state_ checkSensorState() const;

    void initParameters();

   /* State transition Functions */ 
    bool transitState(e_operation_state state);
    e_operation_state  updateState();


    /**  Publish **/
    /* Pan/Tilt Mount */
    void PublishPanDegControl(float deg, uint8_t speed = 50);
    void PublishTiltDegControl(float deg, uint8_t speed = 100);
    void PublishDegControl(float pan_deg, float tilt_deg, uint8_t pan_speed = 50, uint8_t tilt_speed = 100);
    void PublishDirControl(float pan_deg, float tilt_deg, uint8_t pan_speed = 50, uint8_t tilt_speed = 100);
    void PublishDriveCommand(double linear_velocity, double angular_velocity = 0.0);
    void PublishControllerStop();
    void pantiltManualControl();
    void PublishCenterObject(const TargetObject& target, const std_msgs::msg::Header& header);

    /* Weapon fire */
    void PublishFireWeapon();
    void PublishStopSound();
    void PublishSound_setVol();

    /** subscriptions **/
    // Removed onCombatRobotSystemControl
    void onTargetPoint(const TargetPoint::ConstSharedPtr msg);
    void onDetectedObjects(const DetectedObjects::ConstSharedPtr msg);
    void onPanTiltState(const PanTiltState::ConstSharedPtr msg);
    void onMissionControlCommand(const MissionControlCommand::ConstSharedPtr msg);
    void onSwarmPathCommand(const SwarmPathCommand::ConstSharedPtr msg);
    void onSwarmControlCommand(const SwarmControlCommand::ConstSharedPtr msg);
    void onSwarmMissionState(const OperationState::ConstSharedPtr msg);
    void onZoomLevel(const std_msgs::msg::Int32::ConstSharedPtr msg);
    void onTouchCommand(const TouchTargetPoint::ConstSharedPtr msg);
    void onLaserDistance(const std_msgs::msg::Float64::ConstSharedPtr msg);


    /**  SubFunctions **/
    bool isInitialized() const { return isInitialized_; }

    /* Suveillance functions*/
    void initScanning(const float default_pan_angle_ = 0.0, const float default_tilt_angle_ = 0.0);
    void Scanning(const float default_pan_angle_ = 0.0, const float default_tilt_angle_ = 0.0);

    /* Tracking and fire Subfunctions */
    bool keepTarget(const TargetObject& target_object);
    bool isDemoTrackCompleted(int32_t track_id) const;
    void handleFireControl();
    void resetDemoSequence(bool stop_drive = true);
    bool shouldRunDemoForCommand(uint8_t command_id) const;
    uint8_t activeModeIdForCommand(uint8_t command_id) const;

    /* Pan/Tilt Control */
    bool isOutofMaximumAngle();
    std::pair<uint8_t, uint8_t> CalculatePanTiltSpeed(float target_pan_deg, float target_tilt_deg, float curr_pan_deg, float curr_tilt_deg);
    void resetMissionExecution();
    void fillMissionExecutionStatus(OperationState& msg) const;

    void OperateCombatRobotSystem(e_operation_state state);
    bool isCenter(point_t target_point);

    bool isInitPose();

    /* Initialize */
    bool InitPanTiltModule();

    /* Lifecycle Recovery */
    void changePanTiltState(uint8_t transition_id);

};

} // namespace combat_robot_system

#endif 
