// SAN v1.4 PHASE 8 - Limp Mode manager.
//
// ★ v1.4 REVISED definition (per the updated SDD-SWARM §5.7):
//   When BOTH Hub UGV and Deputy UGV fail, the Android app reaches
//   the surviving swarm directly over the Wi-Fi 6 mesh:
//     * Fire / strike commands ENABLED via mesh-direct HMAC-SHA256 auth
//     * Video pipelines redirect from Hub SRT relay to Android IP
//     * Only complex missions (formation reshuffle, AI inference) pause
//     * Simple missions (recon, movement, fire) keep operating
//
// The manager watches HubRoleAnnouncement + RobotStatus to detect
// the dual-failure case, enters/exits Limp Mode on a 7 s grace, and
// drives the necessary parameter reconfigures.

#pragma once

#include <rclcpp/rclcpp.hpp>
#include <combat_robot_msgs/msg/hub_role_announcement.hpp>
#include <combat_robot_msgs/msg/robot_status.hpp>
#include <std_msgs/msg/string.hpp>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>

#include "san_role_management/role_types.hpp"

namespace san_role_management {

struct VideoSenderMode {
    std::string stream_target = "hub_relay";
    std::string android_app_ip;
    std::string transport_mode = "srt_via_hub";
};

class LimpModeManager : public rclcpp::Node {
public:
    LimpModeManager();
    explicit LimpModeManager(const rclcpp::NodeOptions& options);

    bool isInLimpMode() const { return in_limp_mode_.load(); }
    VideoSenderMode videoSenderMode() const;
    bool fireAuthMeshDirect() const { return fire_auth_mesh_direct_; }
    bool isComplexMissionPaused() const { return complex_paused_; }
    bool isSimpleMissionPaused() const { return false; }

    // Test entry points.
    void injectHubAnnouncementForTest(
        const combat_robot_msgs::msg::HubRoleAnnouncement& msg);
    void injectRobotStatusForTest(
        const combat_robot_msgs::msg::RobotStatus& msg);
    void setAndroidEndpointForTest(const std::string& ip) {
        std::lock_guard<std::mutex> lock(endpoint_mutex_);
        android_app_ip_ = ip;
    }
    void simulateHubLossForTest();
    void simulateDeputyLossForTest();
    void simulateHubRecoveryForTest();
    void simulateDeputyRecoveryForTest();
    // Drive a watchdog tick without waiting for the timer.
    void tickForTest() { watchdogTick(); }

private:
    uint32_t hub_robot_id_ = 2;
    uint32_t deputy_robot_id_ = 3;
    int hub_timeout_ms_ = HUB_HEARTBEAT_TIMEOUT_MS;
    int deputy_timeout_ms_ = HUB_HEARTBEAT_TIMEOUT_MS;
    int limp_guard_ms_ = LIMP_MODE_ENTRY_GUARD_MS;
    int watchdog_period_ms_ = 500;

    std::atomic<bool> in_limp_mode_;
    std::optional<rclcpp::Time> last_hub_alive_;
    std::optional<rclcpp::Time> last_deputy_alive_;

    // Reconfiguration state.
    bool fire_auth_mesh_direct_ = false;
    bool complex_paused_ = false;

    mutable std::mutex endpoint_mutex_;
    std::string android_app_ip_;

    mutable std::mutex video_mode_mutex_;
    VideoSenderMode video_mode_;

    using HubAnn = combat_robot_msgs::msg::HubRoleAnnouncement;
    using Status = combat_robot_msgs::msg::RobotStatus;

    rclcpp::Subscription<HubAnn>::SharedPtr hub_sub_;
    rclcpp::Subscription<Status>::SharedPtr status_sub_;
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr alert_pub_;
    rclcpp::TimerBase::SharedPtr watchdog_timer_;

    // [C-1 fix v1.5.1 — DCN-2026-003 D-005] Mutually-exclusive callback
    // group binds subs + watchdog so they cannot race on
    // last_hub_alive_, last_deputy_alive_, limp_engaged_ under MTE.
    rclcpp::CallbackGroup::SharedPtr cb_group_;

    void declareParameters();
    void readParameters();
    void wireInterfaces();

    void onHubAnnouncement(HubAnn::SharedPtr msg);
    void onRobotStatus(Status::SharedPtr msg);
    void watchdogTick();
    void enterLimpMode();
    void exitLimpMode();

    // ★ v1.4 revised: operational features.
    void enableMeshAuthForFire();
    void redirectVideoToAndroidDirect();
    void notifyAndroidMeshDirectMode();
    void pauseComplexMissions();
    void resumeComplexMissions();
    void publishLimpModeAlert(const std::string& text);

    std::string discoverAndroidAppIp() const;

    // Returns true when (now - last_*_alive_) is below the respective
    // timeout. A missing optional means "never seen".
    bool isHubAlive(const rclcpp::Time& now_t) const;
    bool isDeputyAlive(const rclcpp::Time& now_t) const;

    void noteHubAlive();
    void noteDeputyAlive();
};

}  // namespace san_role_management
