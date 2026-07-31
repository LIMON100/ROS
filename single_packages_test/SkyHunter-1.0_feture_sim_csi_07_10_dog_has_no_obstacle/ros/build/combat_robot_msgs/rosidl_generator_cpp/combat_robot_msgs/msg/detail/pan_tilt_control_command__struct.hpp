// generated from rosidl_generator_cpp/resource/idl__struct.hpp.em
// with input from combat_robot_msgs:msg/PanTiltControlCommand.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/pan_tilt_control_command.hpp"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__PAN_TILT_CONTROL_COMMAND__STRUCT_HPP_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__PAN_TILT_CONTROL_COMMAND__STRUCT_HPP_

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "rosidl_runtime_cpp/bounded_vector.hpp"
#include "rosidl_runtime_cpp/message_initialization.hpp"


// Include directives for member types
// Member 'header'
#include "std_msgs/msg/detail/header__struct.hpp"

#ifndef _WIN32
# define DEPRECATED__combat_robot_msgs__msg__PanTiltControlCommand __attribute__((deprecated))
#else
# define DEPRECATED__combat_robot_msgs__msg__PanTiltControlCommand __declspec(deprecated)
#endif

namespace combat_robot_msgs
{

namespace msg
{

// message struct
template<class ContainerAllocator>
struct PanTiltControlCommand_
{
  using Type = PanTiltControlCommand_<ContainerAllocator>;

  explicit PanTiltControlCommand_(rosidl_runtime_cpp::MessageInitialization _init = rosidl_runtime_cpp::MessageInitialization::ALL)
  : header(_init)
  {
    if (rosidl_runtime_cpp::MessageInitialization::ALL == _init ||
      rosidl_runtime_cpp::MessageInitialization::ZERO == _init)
    {
      this->control_mode = 0;
      this->horizontal_angle = 0.0f;
      this->vertical_angle = 0.0f;
      this->pan_speed = 0;
      this->tilt_speed = 0;
      this->pan_dir = 0;
      this->tilt_dir = 0;
    }
  }

  explicit PanTiltControlCommand_(const ContainerAllocator & _alloc, rosidl_runtime_cpp::MessageInitialization _init = rosidl_runtime_cpp::MessageInitialization::ALL)
  : header(_alloc, _init)
  {
    if (rosidl_runtime_cpp::MessageInitialization::ALL == _init ||
      rosidl_runtime_cpp::MessageInitialization::ZERO == _init)
    {
      this->control_mode = 0;
      this->horizontal_angle = 0.0f;
      this->vertical_angle = 0.0f;
      this->pan_speed = 0;
      this->tilt_speed = 0;
      this->pan_dir = 0;
      this->tilt_dir = 0;
    }
  }

  // field types and members
  using _header_type =
    std_msgs::msg::Header_<ContainerAllocator>;
  _header_type header;
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
    uint8_t;
  _pan_speed_type pan_speed;
  using _tilt_speed_type =
    uint8_t;
  _tilt_speed_type tilt_speed;
  using _pan_dir_type =
    uint8_t;
  _pan_dir_type pan_dir;
  using _tilt_dir_type =
    uint8_t;
  _tilt_dir_type tilt_dir;

  // setters for named parameter idiom
  Type & set__header(
    const std_msgs::msg::Header_<ContainerAllocator> & _arg)
  {
    this->header = _arg;
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
    const uint8_t & _arg)
  {
    this->pan_speed = _arg;
    return *this;
  }
  Type & set__tilt_speed(
    const uint8_t & _arg)
  {
    this->tilt_speed = _arg;
    return *this;
  }
  Type & set__pan_dir(
    const uint8_t & _arg)
  {
    this->pan_dir = _arg;
    return *this;
  }
  Type & set__tilt_dir(
    const uint8_t & _arg)
  {
    this->tilt_dir = _arg;
    return *this;
  }

  // constant declarations
  static constexpr uint8_t CONTROL_BRAKE =
    0u;
  static constexpr uint8_t CONTROL_HOR_POS =
    1u;
  static constexpr uint8_t CONTROL_VER_POS =
    2u;
  static constexpr uint8_t CONTROL_DIR =
    3u;

  // pointer types
  using RawPtr =
    combat_robot_msgs::msg::PanTiltControlCommand_<ContainerAllocator> *;
  using ConstRawPtr =
    const combat_robot_msgs::msg::PanTiltControlCommand_<ContainerAllocator> *;
  using SharedPtr =
    std::shared_ptr<combat_robot_msgs::msg::PanTiltControlCommand_<ContainerAllocator>>;
  using ConstSharedPtr =
    std::shared_ptr<combat_robot_msgs::msg::PanTiltControlCommand_<ContainerAllocator> const>;

  template<typename Deleter = std::default_delete<
      combat_robot_msgs::msg::PanTiltControlCommand_<ContainerAllocator>>>
  using UniquePtrWithDeleter =
    std::unique_ptr<combat_robot_msgs::msg::PanTiltControlCommand_<ContainerAllocator>, Deleter>;

  using UniquePtr = UniquePtrWithDeleter<>;

  template<typename Deleter = std::default_delete<
      combat_robot_msgs::msg::PanTiltControlCommand_<ContainerAllocator>>>
  using ConstUniquePtrWithDeleter =
    std::unique_ptr<combat_robot_msgs::msg::PanTiltControlCommand_<ContainerAllocator> const, Deleter>;
  using ConstUniquePtr = ConstUniquePtrWithDeleter<>;

  using WeakPtr =
    std::weak_ptr<combat_robot_msgs::msg::PanTiltControlCommand_<ContainerAllocator>>;
  using ConstWeakPtr =
    std::weak_ptr<combat_robot_msgs::msg::PanTiltControlCommand_<ContainerAllocator> const>;

  // pointer types similar to ROS 1, use SharedPtr / ConstSharedPtr instead
  // NOTE: Can't use 'using' here because GNU C++ can't parse attributes properly
  typedef DEPRECATED__combat_robot_msgs__msg__PanTiltControlCommand
    std::shared_ptr<combat_robot_msgs::msg::PanTiltControlCommand_<ContainerAllocator>>
    Ptr;
  typedef DEPRECATED__combat_robot_msgs__msg__PanTiltControlCommand
    std::shared_ptr<combat_robot_msgs::msg::PanTiltControlCommand_<ContainerAllocator> const>
    ConstPtr;

  // comparison operators
  bool operator==(const PanTiltControlCommand_ & other) const
  {
    if (this->header != other.header) {
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
    if (this->pan_dir != other.pan_dir) {
      return false;
    }
    if (this->tilt_dir != other.tilt_dir) {
      return false;
    }
    return true;
  }
  bool operator!=(const PanTiltControlCommand_ & other) const
  {
    return !this->operator==(other);
  }
};  // struct PanTiltControlCommand_

// alias to use template instance with default allocator
using PanTiltControlCommand =
  combat_robot_msgs::msg::PanTiltControlCommand_<std::allocator<void>>;

// constant definitions
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t PanTiltControlCommand_<ContainerAllocator>::CONTROL_BRAKE;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t PanTiltControlCommand_<ContainerAllocator>::CONTROL_HOR_POS;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t PanTiltControlCommand_<ContainerAllocator>::CONTROL_VER_POS;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t PanTiltControlCommand_<ContainerAllocator>::CONTROL_DIR;
#endif  // __cplusplus < 201703L

}  // namespace msg

}  // namespace combat_robot_msgs

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__PAN_TILT_CONTROL_COMMAND__STRUCT_HPP_
