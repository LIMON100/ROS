// generated from rosidl_generator_cpp/resource/idl__builder.hpp.em
// with input from combat_robot_msgs:msg/LidarStatus.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/lidar_status.hpp"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__LIDAR_STATUS__BUILDER_HPP_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__LIDAR_STATUS__BUILDER_HPP_

#include <algorithm>
#include <utility>

#include "combat_robot_msgs/msg/detail/lidar_status__struct.hpp"
#include "rosidl_runtime_cpp/message_initialization.hpp"


namespace combat_robot_msgs
{

namespace msg
{

namespace builder
{

class Init_LidarStatus_min_obstacle_distance_m
{
public:
  explicit Init_LidarStatus_min_obstacle_distance_m(::combat_robot_msgs::msg::LidarStatus & msg)
  : msg_(msg)
  {}
  ::combat_robot_msgs::msg::LidarStatus min_obstacle_distance_m(::combat_robot_msgs::msg::LidarStatus::_min_obstacle_distance_m_type arg)
  {
    msg_.min_obstacle_distance_m = std::move(arg);
    return std::move(msg_);
  }

private:
  ::combat_robot_msgs::msg::LidarStatus msg_;
};

class Init_LidarStatus_obstacle_detected
{
public:
  explicit Init_LidarStatus_obstacle_detected(::combat_robot_msgs::msg::LidarStatus & msg)
  : msg_(msg)
  {}
  Init_LidarStatus_min_obstacle_distance_m obstacle_detected(::combat_robot_msgs::msg::LidarStatus::_obstacle_detected_type arg)
  {
    msg_.obstacle_detected = std::move(arg);
    return Init_LidarStatus_min_obstacle_distance_m(msg_);
  }

private:
  ::combat_robot_msgs::msg::LidarStatus msg_;
};

class Init_LidarStatus_scan_rate_hz
{
public:
  explicit Init_LidarStatus_scan_rate_hz(::combat_robot_msgs::msg::LidarStatus & msg)
  : msg_(msg)
  {}
  Init_LidarStatus_obstacle_detected scan_rate_hz(::combat_robot_msgs::msg::LidarStatus::_scan_rate_hz_type arg)
  {
    msg_.scan_rate_hz = std::move(arg);
    return Init_LidarStatus_obstacle_detected(msg_);
  }

private:
  ::combat_robot_msgs::msg::LidarStatus msg_;
};

class Init_LidarStatus_last_scan_point_count
{
public:
  explicit Init_LidarStatus_last_scan_point_count(::combat_robot_msgs::msg::LidarStatus & msg)
  : msg_(msg)
  {}
  Init_LidarStatus_scan_rate_hz last_scan_point_count(::combat_robot_msgs::msg::LidarStatus::_last_scan_point_count_type arg)
  {
    msg_.last_scan_point_count = std::move(arg);
    return Init_LidarStatus_scan_rate_hz(msg_);
  }

private:
  ::combat_robot_msgs::msg::LidarStatus msg_;
};

class Init_LidarStatus_status
{
public:
  explicit Init_LidarStatus_status(::combat_robot_msgs::msg::LidarStatus & msg)
  : msg_(msg)
  {}
  Init_LidarStatus_last_scan_point_count status(::combat_robot_msgs::msg::LidarStatus::_status_type arg)
  {
    msg_.status = std::move(arg);
    return Init_LidarStatus_last_scan_point_count(msg_);
  }

private:
  ::combat_robot_msgs::msg::LidarStatus msg_;
};

class Init_LidarStatus_header
{
public:
  Init_LidarStatus_header()
  : msg_(::rosidl_runtime_cpp::MessageInitialization::SKIP)
  {}
  Init_LidarStatus_status header(::combat_robot_msgs::msg::LidarStatus::_header_type arg)
  {
    msg_.header = std::move(arg);
    return Init_LidarStatus_status(msg_);
  }

private:
  ::combat_robot_msgs::msg::LidarStatus msg_;
};

}  // namespace builder

}  // namespace msg

template<typename MessageType>
auto build();

template<>
inline
auto build<::combat_robot_msgs::msg::LidarStatus>()
{
  return combat_robot_msgs::msg::builder::Init_LidarStatus_header();
}

}  // namespace combat_robot_msgs

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__LIDAR_STATUS__BUILDER_HPP_
