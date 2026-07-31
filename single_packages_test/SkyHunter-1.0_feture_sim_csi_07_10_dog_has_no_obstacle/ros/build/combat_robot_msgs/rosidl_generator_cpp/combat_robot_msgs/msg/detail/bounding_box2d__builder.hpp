// generated from rosidl_generator_cpp/resource/idl__builder.hpp.em
// with input from combat_robot_msgs:msg/BoundingBox2d.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/bounding_box2d.hpp"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__BOUNDING_BOX2D__BUILDER_HPP_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__BOUNDING_BOX2D__BUILDER_HPP_

#include <algorithm>
#include <utility>

#include "combat_robot_msgs/msg/detail/bounding_box2d__struct.hpp"
#include "rosidl_runtime_cpp/message_initialization.hpp"


namespace combat_robot_msgs
{

namespace msg
{

namespace builder
{

class Init_BoundingBox2d_height
{
public:
  explicit Init_BoundingBox2d_height(::combat_robot_msgs::msg::BoundingBox2d & msg)
  : msg_(msg)
  {}
  ::combat_robot_msgs::msg::BoundingBox2d height(::combat_robot_msgs::msg::BoundingBox2d::_height_type arg)
  {
    msg_.height = std::move(arg);
    return std::move(msg_);
  }

private:
  ::combat_robot_msgs::msg::BoundingBox2d msg_;
};

class Init_BoundingBox2d_width
{
public:
  explicit Init_BoundingBox2d_width(::combat_robot_msgs::msg::BoundingBox2d & msg)
  : msg_(msg)
  {}
  Init_BoundingBox2d_height width(::combat_robot_msgs::msg::BoundingBox2d::_width_type arg)
  {
    msg_.width = std::move(arg);
    return Init_BoundingBox2d_height(msg_);
  }

private:
  ::combat_robot_msgs::msg::BoundingBox2d msg_;
};

class Init_BoundingBox2d_y
{
public:
  explicit Init_BoundingBox2d_y(::combat_robot_msgs::msg::BoundingBox2d & msg)
  : msg_(msg)
  {}
  Init_BoundingBox2d_width y(::combat_robot_msgs::msg::BoundingBox2d::_y_type arg)
  {
    msg_.y = std::move(arg);
    return Init_BoundingBox2d_width(msg_);
  }

private:
  ::combat_robot_msgs::msg::BoundingBox2d msg_;
};

class Init_BoundingBox2d_x
{
public:
  Init_BoundingBox2d_x()
  : msg_(::rosidl_runtime_cpp::MessageInitialization::SKIP)
  {}
  Init_BoundingBox2d_y x(::combat_robot_msgs::msg::BoundingBox2d::_x_type arg)
  {
    msg_.x = std::move(arg);
    return Init_BoundingBox2d_y(msg_);
  }

private:
  ::combat_robot_msgs::msg::BoundingBox2d msg_;
};

}  // namespace builder

}  // namespace msg

template<typename MessageType>
auto build();

template<>
inline
auto build<::combat_robot_msgs::msg::BoundingBox2d>()
{
  return combat_robot_msgs::msg::builder::Init_BoundingBox2d_x();
}

}  // namespace combat_robot_msgs

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__BOUNDING_BOX2D__BUILDER_HPP_
