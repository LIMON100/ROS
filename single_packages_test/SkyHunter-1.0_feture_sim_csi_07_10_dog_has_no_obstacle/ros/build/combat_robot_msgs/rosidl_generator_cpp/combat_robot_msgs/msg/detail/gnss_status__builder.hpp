// generated from rosidl_generator_cpp/resource/idl__builder.hpp.em
// with input from combat_robot_msgs:msg/GnssStatus.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/gnss_status.hpp"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__GNSS_STATUS__BUILDER_HPP_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__GNSS_STATUS__BUILDER_HPP_

#include <algorithm>
#include <utility>

#include "combat_robot_msgs/msg/detail/gnss_status__struct.hpp"
#include "rosidl_runtime_cpp/message_initialization.hpp"


namespace combat_robot_msgs
{

namespace msg
{

namespace builder
{

class Init_GnssStatus_vertical_accuracy_m
{
public:
  explicit Init_GnssStatus_vertical_accuracy_m(::combat_robot_msgs::msg::GnssStatus & msg)
  : msg_(msg)
  {}
  ::combat_robot_msgs::msg::GnssStatus vertical_accuracy_m(::combat_robot_msgs::msg::GnssStatus::_vertical_accuracy_m_type arg)
  {
    msg_.vertical_accuracy_m = std::move(arg);
    return std::move(msg_);
  }

private:
  ::combat_robot_msgs::msg::GnssStatus msg_;
};

class Init_GnssStatus_horizontal_accuracy_m
{
public:
  explicit Init_GnssStatus_horizontal_accuracy_m(::combat_robot_msgs::msg::GnssStatus & msg)
  : msg_(msg)
  {}
  Init_GnssStatus_vertical_accuracy_m horizontal_accuracy_m(::combat_robot_msgs::msg::GnssStatus::_horizontal_accuracy_m_type arg)
  {
    msg_.horizontal_accuracy_m = std::move(arg);
    return Init_GnssStatus_vertical_accuracy_m(msg_);
  }

private:
  ::combat_robot_msgs::msg::GnssStatus msg_;
};

class Init_GnssStatus_ground_speed_mps
{
public:
  explicit Init_GnssStatus_ground_speed_mps(::combat_robot_msgs::msg::GnssStatus & msg)
  : msg_(msg)
  {}
  Init_GnssStatus_horizontal_accuracy_m ground_speed_mps(::combat_robot_msgs::msg::GnssStatus::_ground_speed_mps_type arg)
  {
    msg_.ground_speed_mps = std::move(arg);
    return Init_GnssStatus_horizontal_accuracy_m(msg_);
  }

private:
  ::combat_robot_msgs::msg::GnssStatus msg_;
};

class Init_GnssStatus_heading_deg
{
public:
  explicit Init_GnssStatus_heading_deg(::combat_robot_msgs::msg::GnssStatus & msg)
  : msg_(msg)
  {}
  Init_GnssStatus_ground_speed_mps heading_deg(::combat_robot_msgs::msg::GnssStatus::_heading_deg_type arg)
  {
    msg_.heading_deg = std::move(arg);
    return Init_GnssStatus_ground_speed_mps(msg_);
  }

private:
  ::combat_robot_msgs::msg::GnssStatus msg_;
};

class Init_GnssStatus_altitude_m
{
public:
  explicit Init_GnssStatus_altitude_m(::combat_robot_msgs::msg::GnssStatus & msg)
  : msg_(msg)
  {}
  Init_GnssStatus_heading_deg altitude_m(::combat_robot_msgs::msg::GnssStatus::_altitude_m_type arg)
  {
    msg_.altitude_m = std::move(arg);
    return Init_GnssStatus_heading_deg(msg_);
  }

private:
  ::combat_robot_msgs::msg::GnssStatus msg_;
};

class Init_GnssStatus_longitude
{
public:
  explicit Init_GnssStatus_longitude(::combat_robot_msgs::msg::GnssStatus & msg)
  : msg_(msg)
  {}
  Init_GnssStatus_altitude_m longitude(::combat_robot_msgs::msg::GnssStatus::_longitude_type arg)
  {
    msg_.longitude = std::move(arg);
    return Init_GnssStatus_altitude_m(msg_);
  }

private:
  ::combat_robot_msgs::msg::GnssStatus msg_;
};

class Init_GnssStatus_latitude
{
public:
  explicit Init_GnssStatus_latitude(::combat_robot_msgs::msg::GnssStatus & msg)
  : msg_(msg)
  {}
  Init_GnssStatus_longitude latitude(::combat_robot_msgs::msg::GnssStatus::_latitude_type arg)
  {
    msg_.latitude = std::move(arg);
    return Init_GnssStatus_longitude(msg_);
  }

private:
  ::combat_robot_msgs::msg::GnssStatus msg_;
};

class Init_GnssStatus_num_satellites
{
public:
  explicit Init_GnssStatus_num_satellites(::combat_robot_msgs::msg::GnssStatus & msg)
  : msg_(msg)
  {}
  Init_GnssStatus_latitude num_satellites(::combat_robot_msgs::msg::GnssStatus::_num_satellites_type arg)
  {
    msg_.num_satellites = std::move(arg);
    return Init_GnssStatus_latitude(msg_);
  }

private:
  ::combat_robot_msgs::msg::GnssStatus msg_;
};

class Init_GnssStatus_fix_status
{
public:
  explicit Init_GnssStatus_fix_status(::combat_robot_msgs::msg::GnssStatus & msg)
  : msg_(msg)
  {}
  Init_GnssStatus_num_satellites fix_status(::combat_robot_msgs::msg::GnssStatus::_fix_status_type arg)
  {
    msg_.fix_status = std::move(arg);
    return Init_GnssStatus_num_satellites(msg_);
  }

private:
  ::combat_robot_msgs::msg::GnssStatus msg_;
};

class Init_GnssStatus_header
{
public:
  Init_GnssStatus_header()
  : msg_(::rosidl_runtime_cpp::MessageInitialization::SKIP)
  {}
  Init_GnssStatus_fix_status header(::combat_robot_msgs::msg::GnssStatus::_header_type arg)
  {
    msg_.header = std::move(arg);
    return Init_GnssStatus_fix_status(msg_);
  }

private:
  ::combat_robot_msgs::msg::GnssStatus msg_;
};

}  // namespace builder

}  // namespace msg

template<typename MessageType>
auto build();

template<>
inline
auto build<::combat_robot_msgs::msg::GnssStatus>()
{
  return combat_robot_msgs::msg::builder::Init_GnssStatus_header();
}

}  // namespace combat_robot_msgs

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__GNSS_STATUS__BUILDER_HPP_
