// generated from rosidl_generator_cpp/resource/idl__struct.hpp.em
// with input from combat_robot_msgs:msg/UserCommand.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/user_command.hpp"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__USER_COMMAND__STRUCT_HPP_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__USER_COMMAND__STRUCT_HPP_

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
# define DEPRECATED__combat_robot_msgs__msg__UserCommand __attribute__((deprecated))
#else
# define DEPRECATED__combat_robot_msgs__msg__UserCommand __declspec(deprecated)
#endif

namespace combat_robot_msgs
{

namespace msg
{

// message struct
template<class ContainerAllocator>
struct UserCommand_
{
  using Type = UserCommand_<ContainerAllocator>;

  explicit UserCommand_(rosidl_runtime_cpp::MessageInitialization _init = rosidl_runtime_cpp::MessageInitialization::ALL)
  : header(_init)
  {
    if (rosidl_runtime_cpp::MessageInitialization::ALL == _init ||
      rosidl_runtime_cpp::MessageInitialization::ZERO == _init)
    {
      this->command_from = 0;
      this->command_id = 0;
      this->target_x = 0.0f;
      this->target_y = 0.0f;
      this->drone_target_lat = 0.0;
      this->drone_target_lon = 0.0;
      this->drone_target_valid = false;
      this->gun_trigger = false;
      this->gun_trigger_permission = false;
      this->pan_speed = 0;
      this->tilt_speed = 0;
      this->zoom_command = 0;
      this->stream_command = 0;
    }
  }

  explicit UserCommand_(const ContainerAllocator & _alloc, rosidl_runtime_cpp::MessageInitialization _init = rosidl_runtime_cpp::MessageInitialization::ALL)
  : header(_alloc, _init)
  {
    if (rosidl_runtime_cpp::MessageInitialization::ALL == _init ||
      rosidl_runtime_cpp::MessageInitialization::ZERO == _init)
    {
      this->command_from = 0;
      this->command_id = 0;
      this->target_x = 0.0f;
      this->target_y = 0.0f;
      this->drone_target_lat = 0.0;
      this->drone_target_lon = 0.0;
      this->drone_target_valid = false;
      this->gun_trigger = false;
      this->gun_trigger_permission = false;
      this->pan_speed = 0;
      this->tilt_speed = 0;
      this->zoom_command = 0;
      this->stream_command = 0;
    }
  }

  // field types and members
  using _header_type =
    std_msgs::msg::Header_<ContainerAllocator>;
  _header_type header;
  using _command_from_type =
    uint8_t;
  _command_from_type command_from;
  using _command_id_type =
    uint8_t;
  _command_id_type command_id;
  using _target_x_type =
    float;
  _target_x_type target_x;
  using _target_y_type =
    float;
  _target_y_type target_y;
  using _drone_target_lat_type =
    double;
  _drone_target_lat_type drone_target_lat;
  using _drone_target_lon_type =
    double;
  _drone_target_lon_type drone_target_lon;
  using _drone_target_valid_type =
    bool;
  _drone_target_valid_type drone_target_valid;
  using _gun_trigger_type =
    bool;
  _gun_trigger_type gun_trigger;
  using _gun_trigger_permission_type =
    bool;
  _gun_trigger_permission_type gun_trigger_permission;
  using _pan_speed_type =
    int8_t;
  _pan_speed_type pan_speed;
  using _tilt_speed_type =
    int8_t;
  _tilt_speed_type tilt_speed;
  using _zoom_command_type =
    int8_t;
  _zoom_command_type zoom_command;
  using _stream_command_type =
    uint8_t;
  _stream_command_type stream_command;

  // setters for named parameter idiom
  Type & set__header(
    const std_msgs::msg::Header_<ContainerAllocator> & _arg)
  {
    this->header = _arg;
    return *this;
  }
  Type & set__command_from(
    const uint8_t & _arg)
  {
    this->command_from = _arg;
    return *this;
  }
  Type & set__command_id(
    const uint8_t & _arg)
  {
    this->command_id = _arg;
    return *this;
  }
  Type & set__target_x(
    const float & _arg)
  {
    this->target_x = _arg;
    return *this;
  }
  Type & set__target_y(
    const float & _arg)
  {
    this->target_y = _arg;
    return *this;
  }
  Type & set__drone_target_lat(
    const double & _arg)
  {
    this->drone_target_lat = _arg;
    return *this;
  }
  Type & set__drone_target_lon(
    const double & _arg)
  {
    this->drone_target_lon = _arg;
    return *this;
  }
  Type & set__drone_target_valid(
    const bool & _arg)
  {
    this->drone_target_valid = _arg;
    return *this;
  }
  Type & set__gun_trigger(
    const bool & _arg)
  {
    this->gun_trigger = _arg;
    return *this;
  }
  Type & set__gun_trigger_permission(
    const bool & _arg)
  {
    this->gun_trigger_permission = _arg;
    return *this;
  }
  Type & set__pan_speed(
    const int8_t & _arg)
  {
    this->pan_speed = _arg;
    return *this;
  }
  Type & set__tilt_speed(
    const int8_t & _arg)
  {
    this->tilt_speed = _arg;
    return *this;
  }
  Type & set__zoom_command(
    const int8_t & _arg)
  {
    this->zoom_command = _arg;
    return *this;
  }
  Type & set__stream_command(
    const uint8_t & _arg)
  {
    this->stream_command = _arg;
    return *this;
  }

  // constant declarations
  static constexpr uint8_t TABLET =
    0u;
  static constexpr uint8_t IDLE =
    0u;
  static constexpr uint8_t RECON =
    1u;
  static constexpr uint8_t PROTECT_GENERAL =
    2u;
  static constexpr uint8_t PROTECT_DRONE =
    3u;
  static constexpr uint8_t DEBUG_ATTACK =
    4u;
  static constexpr uint8_t DEBUG_TRACKING =
    5u;
  static constexpr uint8_t ASSAULT =
    6u;
  static constexpr uint8_t RETURN_TO_HOME =
    7u;
  static constexpr uint8_t ESTOP =
    8u;
  static constexpr uint8_t STREAM_NONE =
    0u;
  static constexpr uint8_t STREAM_START =
    1u;
  static constexpr uint8_t STREAM_STOP =
    2u;

  // pointer types
  using RawPtr =
    combat_robot_msgs::msg::UserCommand_<ContainerAllocator> *;
  using ConstRawPtr =
    const combat_robot_msgs::msg::UserCommand_<ContainerAllocator> *;
  using SharedPtr =
    std::shared_ptr<combat_robot_msgs::msg::UserCommand_<ContainerAllocator>>;
  using ConstSharedPtr =
    std::shared_ptr<combat_robot_msgs::msg::UserCommand_<ContainerAllocator> const>;

  template<typename Deleter = std::default_delete<
      combat_robot_msgs::msg::UserCommand_<ContainerAllocator>>>
  using UniquePtrWithDeleter =
    std::unique_ptr<combat_robot_msgs::msg::UserCommand_<ContainerAllocator>, Deleter>;

  using UniquePtr = UniquePtrWithDeleter<>;

  template<typename Deleter = std::default_delete<
      combat_robot_msgs::msg::UserCommand_<ContainerAllocator>>>
  using ConstUniquePtrWithDeleter =
    std::unique_ptr<combat_robot_msgs::msg::UserCommand_<ContainerAllocator> const, Deleter>;
  using ConstUniquePtr = ConstUniquePtrWithDeleter<>;

  using WeakPtr =
    std::weak_ptr<combat_robot_msgs::msg::UserCommand_<ContainerAllocator>>;
  using ConstWeakPtr =
    std::weak_ptr<combat_robot_msgs::msg::UserCommand_<ContainerAllocator> const>;

  // pointer types similar to ROS 1, use SharedPtr / ConstSharedPtr instead
  // NOTE: Can't use 'using' here because GNU C++ can't parse attributes properly
  typedef DEPRECATED__combat_robot_msgs__msg__UserCommand
    std::shared_ptr<combat_robot_msgs::msg::UserCommand_<ContainerAllocator>>
    Ptr;
  typedef DEPRECATED__combat_robot_msgs__msg__UserCommand
    std::shared_ptr<combat_robot_msgs::msg::UserCommand_<ContainerAllocator> const>
    ConstPtr;

  // comparison operators
  bool operator==(const UserCommand_ & other) const
  {
    if (this->header != other.header) {
      return false;
    }
    if (this->command_from != other.command_from) {
      return false;
    }
    if (this->command_id != other.command_id) {
      return false;
    }
    if (this->target_x != other.target_x) {
      return false;
    }
    if (this->target_y != other.target_y) {
      return false;
    }
    if (this->drone_target_lat != other.drone_target_lat) {
      return false;
    }
    if (this->drone_target_lon != other.drone_target_lon) {
      return false;
    }
    if (this->drone_target_valid != other.drone_target_valid) {
      return false;
    }
    if (this->gun_trigger != other.gun_trigger) {
      return false;
    }
    if (this->gun_trigger_permission != other.gun_trigger_permission) {
      return false;
    }
    if (this->pan_speed != other.pan_speed) {
      return false;
    }
    if (this->tilt_speed != other.tilt_speed) {
      return false;
    }
    if (this->zoom_command != other.zoom_command) {
      return false;
    }
    if (this->stream_command != other.stream_command) {
      return false;
    }
    return true;
  }
  bool operator!=(const UserCommand_ & other) const
  {
    return !this->operator==(other);
  }
};  // struct UserCommand_

// alias to use template instance with default allocator
using UserCommand =
  combat_robot_msgs::msg::UserCommand_<std::allocator<void>>;

// constant definitions
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t UserCommand_<ContainerAllocator>::TABLET;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t UserCommand_<ContainerAllocator>::IDLE;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t UserCommand_<ContainerAllocator>::RECON;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t UserCommand_<ContainerAllocator>::PROTECT_GENERAL;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t UserCommand_<ContainerAllocator>::PROTECT_DRONE;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t UserCommand_<ContainerAllocator>::DEBUG_ATTACK;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t UserCommand_<ContainerAllocator>::DEBUG_TRACKING;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t UserCommand_<ContainerAllocator>::ASSAULT;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t UserCommand_<ContainerAllocator>::RETURN_TO_HOME;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t UserCommand_<ContainerAllocator>::ESTOP;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t UserCommand_<ContainerAllocator>::STREAM_NONE;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t UserCommand_<ContainerAllocator>::STREAM_START;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t UserCommand_<ContainerAllocator>::STREAM_STOP;
#endif  // __cplusplus < 201703L

}  // namespace msg

}  // namespace combat_robot_msgs

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__USER_COMMAND__STRUCT_HPP_
