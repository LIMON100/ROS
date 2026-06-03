// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.4 L5 regression - templated topic predicate waiter.
//
// Subscribes to a topic, holds the latest sample, and lets the runner
// block until a caller-supplied predicate (`std::function<bool(const T&)>`)
// returns true or a deadline elapses. Returns the elapsed milliseconds
// so the report can record the transition latency.

#pragma once

#include <rclcpp/rclcpp.hpp>

#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

namespace san_l5_regression
{

template<typename MsgT>
class TopicWatcher
{
public:
  using Predicate = std::function<bool (const MsgT &)>;

  TopicWatcher(
    rclcpp::Node * node,
    const std::string & topic,
    const rclcpp::QoS & qos = rclcpp::QoS(20).reliable())
  {
    sub_ = node->create_subscription<MsgT>(
      topic, qos,
      [this](typename MsgT::SharedPtr msg) {onMessage(msg);});
  }

  // Wait for `pred(msg)` to return true. Returns elapsed ms since the
  // call started, or std::nullopt if the deadline elapsed first. Already
  // satisfied predicates return 0 ms immediately.
  std::optional<int> waitFor(
    Predicate pred,
    std::chrono::milliseconds deadline)
  {
    const auto start = std::chrono::steady_clock::now();
    std::unique_lock<std::mutex> lock(mu_);
    if (latest_ && pred(*latest_)) {
      return 0;
    }
    pending_predicate_ = pred;
    const bool ok = cv_.wait_for(
      lock, deadline, [this] {
        return predicate_satisfied_;
      });
    const auto elapsed_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - start).count();
    pending_predicate_ = nullptr;
    const bool was_satisfied = predicate_satisfied_;
    predicate_satisfied_ = false;
    if (!ok || !was_satisfied) {return std::nullopt;}
    return static_cast<int>(elapsed_ms);
  }

  std::optional<MsgT> latest() const
  {
    std::lock_guard<std::mutex> lock(mu_);
    if (!latest_) {return std::nullopt;}
    return *latest_;
  }

  std::size_t messageCount() const
  {
    std::lock_guard<std::mutex> lock(mu_);
    return count_;
  }

  void reset()
  {
    std::lock_guard<std::mutex> lock(mu_);
    latest_.reset();
    count_ = 0;
    predicate_satisfied_ = false;
    pending_predicate_ = nullptr;
  }

private:
  typename rclcpp::Subscription<MsgT>::SharedPtr sub_;
  mutable std::mutex mu_;
  std::condition_variable cv_;
  std::unique_ptr<MsgT> latest_;
  std::size_t count_ = 0;
  Predicate pending_predicate_;
  bool predicate_satisfied_ = false;

  void onMessage(typename MsgT::SharedPtr msg)
  {
    if (msg == nullptr) {return;}
    std::lock_guard<std::mutex> lock(mu_);
    latest_ = std::make_unique<MsgT>(*msg);
    ++count_;
    if (pending_predicate_ && pending_predicate_(*latest_)) {
      predicate_satisfied_ = true;
      cv_.notify_all();
    }
  }
};

}  // namespace san_l5_regression
