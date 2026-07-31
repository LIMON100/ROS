// generated from rosidl_generator_cpp/resource/idl__struct.hpp.em
// with input from combat_robot_msgs:msg/PanTiltState.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/pan_tilt_state.hpp"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__PAN_TILT_STATE__STRUCT_HPP_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__PAN_TILT_STATE__STRUCT_HPP_

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "rosidl_runtime_cpp/bounded_vector.hpp"
#include "rosidl_runtime_cpp/message_initialization.hpp"


// Include directives for member types
// Member 'stamp'
#include "builtin_interfaces/msg/detail/time__struct.hpp"

#ifndef _WIN32
# define DEPRECATED__combat_robot_msgs__msg__PanTiltState __attribute__((deprecated))
#else
# define DEPRECATED__combat_robot_msgs__msg__PanTiltState __declspec(deprecated)
#endif

namespace combat_robot_msgs
{

namespace msg
{

// message struct
template<class ContainerAllocator>
struct PanTiltState_
{
  using Type = PanTiltState_<ContainerAllocator>;

  explicit PanTiltState_(rosidl_runtime_cpp::MessageInitialization _init = rosidl_runtime_cpp::MessageInitialization::ALL)
  : stamp(_init)
  {
    if (rosidl_runtime_cpp::MessageInitialization::ALL == _init ||
      rosidl_runtime_cpp::MessageInitialization::ZERO == _init)
    {
      this->control_mode = 0;
      this->horizontal_angle = 0.0f;
      this->vertical_angle = 0.0f;
      this->pan_speed = 0l;
      this->tilt_speed = 0l;
    }
  }

  explicit PanTiltState_(const ContainerAllocator & _alloc, rosidl_runtime_cpp::MessageInitialization _init = rosidl_runtime_cpp::MessageInitialization::ALL)
  : stamp(_alloc, _init)
  {
    if (rosidl_runtime_cpp::MessageInitialization::ALL == _init ||
      rosidl_runtime_cpp::MessageInitialization::ZERO == _init)
    {
      this->control_mode = 0;
      this->horizontal_angle = 0.0f;
      this->vertical_angle = 0.0f;
      this->pan_speed = 0l;
      this->tilt_speed = 0l;
    }
  }

  // field types and members
  using _stamp_type =
    builtin_interfaces::msg::Time_<ContainerAllocator>;
  _stamp_type stamp;
  using _control_mode_type =
    uint8_t;
  _control_mode_type control_mode;
  using _horizontal_angle_type =
    float;
  _horizontal_angle_type horizontal_angle;
  using _vertical_angle_type =
    float;
  _vertical_angle_type vertical_angle;
  using _pan_speed_type =
    int32_t;
  _pan_speed_type pan_speed;
  using _tilt_speed_type =
    int32_t;
  _tilt_speed_type tilt_speed;

  // setters for named parameter idiom
  Type & set__stamp(
    const builtin_interfaces::msg::Time_<ContainerAllocator> & _arg)
  {
    this->stamp = _arg;
    return *this;
  }
  Type & set__control_mode(
    const uint8_t & _arg)
  {
    this->control_mode = _arg;
    return *this;
  }
  Type & set__horizontal_angle(
    const float & _arg)
  {
    this->horizontal_angle = _arg;
    return *this;
  }
  Type & set__vertical_angle(
    const float & _arg)
  {
    this->vertical_angle = _arg;
    return *this;
  }
  Type & set__pan_speed(
    const int32_t & _arg)
  {
    this->pan_speed = _arg;
    return *this;
  }
  Type & set__tilt_speed(
    const int32_t & _arg)
  {
    this->tilt_speed = _arg;
    return *this;
  }

  // constant declarations

  // pointer types
  using RawPtr =
    combat_robot_msgs::msg::PanTiltState_<ContainerAllocator> *;
  using ConstRawPtr =
    const combat_robot_msgs::msg::PanTiltState_<ContainerAllocator> *;
  using SharedPtr =
    std::shared_ptr<combat_robot_msgs::msg::PanTiltState_<ContainerAllocator>>;
  using ConstSharedPtr =
    std::shared_ptr<combat_robot_msgs::msg::PanTiltState_<ContainerAllocator> const>;

  template<typename Deleter = std::default_delete<
      combat_robot_msgs::msg::PanTiltState_<ContainerAllocator>>>
  using UniquePtrWithDeleter =
    std::unique_ptr<combat_robot_msgs::msg::PanTiltState_<ContainerAllocator>, Deleter>;

  using UniquePtr = UniquePtrWithDeleter<>;

  template<typename Deleter = std::default_delete<
      combat_robot_msgs::msg::PanTiltState_<ContainerAllocator>>>
  using ConstUniquePtrWithDeleter =
    std::unique_ptr<combat_robot_msgs::msg::PanTiltState_<ContainerAllocator> const, Deleter>;
  using ConstUniquePtr = ConstUniquePtrWithDeleter<>;

  using WeakPtr =
    std::weak_ptr<combat_robot_msgs::msg::PanTiltState_<ContainerAllocator>>;
  using ConstWeakPtr =
    std::weak_ptr<combat_robot_msgs::msg::PanTiltState_<ContainerAllocator> const>;

  // pointer types similar to ROS 1, use SharedPtr / ConstSharedPtr instead
  // NOTE: Can't use 'using' here because GNU C++ can't parse attributes properly
  typedef DEPRECATED__combat_robot_msgs__msg__PanTiltState
    std::shared_ptr<combat_robot_msgs::msg::PanTiltState_<ContainerAllocator>>
    Ptr;
  typedef DEPRECATED__combat_robot_msgs__msg__PanTiltState
    std::shared_ptr<combat_robot_msgs::msg::PanTiltState_<ContainerAllocator> const>
    ConstPtr;

  // comparison operators
  bool operator==(const PanTiltState_ & other) const
  {
    if (this->stamp != other.stamp) {
      return false;
    }
    if (this->control_mode != other.control_mode) {
      return false;
    }
    if (this->horizontal_angle != other.horizontal_angle) {
      return false;
    }
    if (this->vertical_angle != other.vertical_angle) {
      return false;
    }
    if (this->pan_speed != other.pan_speed) {
      return false;
    }
    if (this->tilt_speed != other.tilt_speed) {
      return false;
    }
    return true;
  }
  bool operator!=(const PanTiltState_ & other) const
  {
    return !this->operator==(other);
  }
};  // struct PanTiltState_

// alias to use template instance with default allocator
using PanTiltState =
  combat_robot_msgs::msg::PanTiltState_<std::allocator<void>>;

// constant definitions

}  // namespace msg

}  // namespace combat_robot_msgs

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__PAN_TILT_STATE__STRUCT_HPP_
