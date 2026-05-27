// SAN v1.3 PHASE 2 v2 - mwan3 hotplug + status via libubus C API.
//
// Replaces the v1 shell pipe `mwan3 status | grep wan_lte`. Subscribes
// to the mwan3 ubus object so the wan_lte interface transition surfaces
// as an event in the uloop thread; the role manager reacts within the
// same heartbeat tick instead of waiting on a 1 Hz poll.
//
// Service control (mwan3 reload) goes through procd via
// `ubus call service event {name:mwan3, action:reload}` — no
// /etc/init.d invocation.

#pragma once

#include <rclcpp/logger.hpp>
#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

extern "C" {
#include <libubox/blobmsg.h>
#include <libubox/uloop.h>
#include <libubus.h>
}

namespace san_lte_redundancy {

class Mwan3UbusMonitor {
public:
    using StatusCallback =
        std::function<void(const std::string& interface, bool is_up)>;

    explicit Mwan3UbusMonitor(rclcpp::Logger logger);
    ~Mwan3UbusMonitor();

    Mwan3UbusMonitor(const Mwan3UbusMonitor&) = delete;
    Mwan3UbusMonitor& operator=(const Mwan3UbusMonitor&) = delete;

    // Register a callback fired from the uloop thread on every
    // wan_lte transition. The callback must be reentrancy-safe (it
    // runs while our internal mutex is held).
    void onLteStatusChange(StatusCallback cb);

    // Synchronous status query via `ubus call mwan3 status`. Returns
    // the latched flag updated by the most recent invoke OR hotplug
    // event. Caller can also invoke refreshLteStatus() to force a
    // fresh ubus_invoke roundtrip.
    bool isLteUp();
    void refreshLteStatus();

    // procd service event: equivalent of `/etc/init.d/mwan3 reload`
    // but routed through ubus, so there is no shell exec involved.
    bool reloadMwan3Service();

    // For unit tests: inject a synthetic interface_status event.
    void injectStatusEventForTest(const std::string& interface, bool is_up);

private:
    rclcpp::Logger logger_;
    struct ubus_context* ubus_ctx_;
    struct ubus_subscriber subscriber_;
    bool subscribed_;

    std::mutex callback_mutex_;
    StatusCallback status_callback_;

    std::atomic<bool> wan_lte_up_;
    std::atomic<bool> running_;
    std::thread uloop_thread_;

    void uloopThreadMain();

    // Dispatch a single hotplug event to the registered callback.
    void dispatchStatusEvent(const std::string& interface, bool is_up);

    // libubus C callback shim — extracts the Mwan3UbusMonitor* from
    // the subscriber, then calls dispatchStatusEvent.
    static int onHotplugEventTrampoline(struct ubus_context* ctx,
                                        struct ubus_object* obj,
                                        struct ubus_request_data* req,
                                        const char* method,
                                        struct blob_attr* msg);
};

}  // namespace san_lte_redundancy
