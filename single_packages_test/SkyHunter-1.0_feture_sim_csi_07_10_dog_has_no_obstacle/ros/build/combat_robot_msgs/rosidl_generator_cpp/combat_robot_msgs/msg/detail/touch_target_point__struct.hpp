// generated from rosidl_generator_cpp/resource/idl__struct.hpp.em
// with input from combat_robot_msgs:msg/TouchTargetPoint.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/touch_target_point.hpp"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__TOUCH_TARGET_POINT__STRUCT_HPP_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__TOUCH_TARGET_POINT__STRUCT_HPP_

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "rosidl_runtime_cpp/bounded_vector.hpp"
#include "rosidl_runtime_cpp/message_initialization.hpp"


#ifndef _WIN32
# define DEPRECATED__combat_robot_msgs__msg__TouchTargetPoint __attribute__((deprecated))
#else
# define DEPRECATED__combat_robot_msgs__msg__TouchTargetPoint __declspec(deprecated)
#endif

namespace combat_robot_msgs
{

namespace msg
{

// message struct
template<class ContainerAllocator>
struct TouchTargetPoint_
{
  using Type = TouchTargetPoint_<ContainerAllocator>;

  explicit TouchTargetPoint_(rosidl_runtime_cpp::MessageInitialization _init = rosidl_runtime_cpp::MessageInitialization::ALL)
  {
    if (rosidl_runtime_cpp::MessageInitialization::ALL == _init ||
      rosidl_runtime_cpp::MessageInitialization::ZERO == _init)
    {
      this->touch_x = 0.0f;
      this->touch_y = 0.0f;
    }
  }

  explicit TouchTargetPoint_(const ContainerAllocator & _alloc, rosidl_runtime_cpp::MessageInitialization _init = rosidl_runtime_cpp::MessageInitialization::ALL)
  {
    (void)_alloc;
    if (rosidl_runtime_cpp::MessageInitialization::ALL == _init ||
      rosidl_runtime_cpp::MessageInitialization::ZERO == _init)
    {
      this->touch_x = 0.0f;
      this->touch_y = 0.0f;
    }
  }

  // field types and members
  using _touch_x_type =
    float;
  _touch_x_type touch_x;
  using _touch_y_type =
    float;
  _touch_y_type touch_y;

  // setters for named parameter idiom
  Type & set__touch_x(
    const float & _arg)
  {
    this->touch_x = _arg;
    return *this;
  }
  Type & set__touch_y(
    const float & _arg)
  {
    this->touch_y = _arg;
    return *this;
  }

  // constant declarations

  // pointer types
  using RawPtr =
    combat_robot_msgs::msg::TouchTargetPoint_<ContainerAllocator> *;
  using ConstRawPtr =
    const combat_robot_msgs::msg::TouchTargetPoint_<ContainerAllocator> *;
  using SharedPtr =
    std::shared_ptr<combat_robot_msgs::msg::TouchTargetPoint_<ContainerAllocator>>;
  using ConstSharedPtr =
    std::shared_ptr<combat_robot_msgs::msg::TouchTargetPoint_<ContainerAllocator> const>;

  template<typename Deleter = std::default_delete<
      combat_robot_msgs::msg::TouchTargetPoint_<ContainerAllocator>>>
  using UniquePtrWithDeleter =
    std::unique_ptr<combat_robot_msgs::msg::TouchTargetPoint_<ContainerAllocator>, Deleter>;

  using UniquePtr = UniquePtrWithDeleter<>;

  template<typename Deleter = std::default_delete<
      combat_robot_msgs::msg::TouchTargetPoint_<ContainerAllocator>>>
  using ConstUniquePtrWithDeleter =
    std::unique_ptr<combat_robot_msgs::msg::TouchTargetPoint_<ContainerAllocator> const, Deleter>;
  using ConstUniquePtr = ConstUniquePtrWithDeleter<>;

  using WeakPtr =
    std::weak_ptr<combat_robot_msgs::msg::TouchTargetPoint_<ContainerAllocator>>;
  using ConstWeakPtr =
    std::weak_ptr<combat_robot_msgs::msg::TouchTargetPoint_<ContainerAllocator> const>;

  // pointer types similar to ROS 1, use SharedPtr / ConstSharedPtr instead
  // NOTE: Can't use 'using' here because GNU C++ can't parse attributes properly
  typedef DEPRECATED__combat_robot_msgs__msg__TouchTargetPoint
    std::shared_ptr<combat_robot_msgs::msg::TouchTargetPoint_<ContainerAllocator>>
    Ptr;
  typedef DEPRECATED__combat_robot_msgs__msg__TouchTargetPoint
    std::shared_ptr<combat_robot_msgs::msg::TouchTargetPoint_<ContainerAllocator> const>
    ConstPtr;

  // comparison operators
  bool operator==(const TouchTargetPoint_ & other) const
  {
    if (this->touch_x != other.touch_x) {
      return false;
    }
    if (this->touch_y != other.touch_y) {
      return false;
    }
    return true;
  }
  bool operator!=(const TouchTargetPoint_ & other) const
  {
    return !this->operator==(other);
  }
};  // struct TouchTargetPoint_

// alias to use template instance with default allocator
using TouchTargetPoint =
  combat_robot_msgs::msg::TouchTargetPoint_<std::allocator<void>>;

// constant definitions

}  // namespace msg

}  // namespace combat_robot_msgs

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__TOUCH_TARGET_POINT__STRUCT_HPP_
