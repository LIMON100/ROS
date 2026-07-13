// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 PHASE 2-E - mwan3 hotplug + status via libubus C API.
//
// Replaces the v1 shell pipe `mwan3 status | grep wan_lte`. Subscribes
// to the mwan3 ubus object so the wan_lte interface transition surfaces
// as an event in the uloop thread; the role manager reacts within the
// same heartbeat tick instead of waiting on a 1 Hz poll.
//
// Service control (mwan3 reload) goes through procd via
// `ubus call service event {name:mwan3, action:reload}` — no
// /etc/init.d invocation.
//
// PATCH 2026-05-13 (LR5): the previous implementation used
// offsetof() on Mwan3UbusMonitor — a non-standard-layout class with
// std::mutex / std::thread members. offsetof on a non-standard-layout
// class is conditionally-supported in C++17 (most compilers emit a
// warning), and UB in earlier standards. The PATCH switches to a
// dedicated SubscriberHolder struct (standard-layout) that owns the
// ubus_subscriber and points back at the Mwan3UbusMonitor, so
// trampoline recovery never relies on offsetof of a non-POD parent.
//
// PATCH 2026-05-13 (LR6): destructor now stops the uloop callback
// path BEFORE freeing the ubus context, so a hotplug event arriving
// mid-shutdown cannot dereference a freed pointer.

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

namespace san_lte_redundancy
{

class Mwan3UbusMonitor;

// ★ PATCH 2026-05-13 (LR5): standard-layout holder so the C
// trampoline can recover the owner via container_of without
// invoking UB on Mwan3UbusMonitor itself.
struct SubscriberHolder
{
  struct ubus_subscriber sub;
  Mwan3UbusMonitor * owner;
};

class Mwan3UbusMonitor
{
public:
  using StatusCallback =
    std::function<void (const std::string & interface, bool is_up)>;

  explicit Mwan3UbusMonitor(rclcpp::Logger logger);
  ~Mwan3UbusMonitor();

  Mwan3UbusMonitor(const Mwan3UbusMonitor &) = delete;
  Mwan3UbusMonitor & operator=(const Mwan3UbusMonitor &) = delete;

  // Register a callback fired from the uloop thread on every
  // wan_lte transition. The callback must be reentrancy-safe (it
  // runs while our internal mutex is held).
  void onLteStatusChange(StatusCallback cb);

  bool isLteUp();
  void refreshLteStatus();
  bool reloadMwan3Service();
  void injectStatusEventForTest(const std::string & interface, bool is_up);

private:
  rclcpp::Logger logger_;
  struct ubus_context * ubus_ctx_;
  // ★ PATCH 2026-05-13 (LR5): subscriber wrapped with back-pointer.
  SubscriberHolder holder_;
  bool subscribed_;

  std::mutex callback_mutex_;
  StatusCallback status_callback_;

  std::atomic<bool> wan_lte_up_;
  std::atomic<bool> running_;
  std::thread uloop_thread_;

  // ★ PATCH 2026-05-13 (LR6): protects ubus_ctx_ teardown vs the
  // refreshLteStatus / reloadMwan3Service callers + trampoline.
  std::mutex ctx_mutex_;

  void uloopThreadMain();
  void dispatchStatusEvent(const std::string & interface, bool is_up);

  static int onHotplugEventTrampoline(
    struct ubus_context * ctx,
    struct ubus_object * obj,
    struct ubus_request_data * req,
    const char * method,
    struct blob_attr * msg);
};

}  // namespace san_lte_redundancy
