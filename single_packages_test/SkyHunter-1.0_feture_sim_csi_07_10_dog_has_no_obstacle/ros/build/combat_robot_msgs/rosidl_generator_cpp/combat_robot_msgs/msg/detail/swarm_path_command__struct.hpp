// generated from rosidl_generator_cpp/resource/idl__struct.hpp.em
// with input from combat_robot_msgs:msg/SwarmPathCommand.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/swarm_path_command.hpp"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__SWARM_PATH_COMMAND__STRUCT_HPP_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__SWARM_PATH_COMMAND__STRUCT_HPP_

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
# define DEPRECATED__combat_robot_msgs__msg__SwarmPathCommand __attribute__((deprecated))
#else
# define DEPRECATED__combat_robot_msgs__msg__SwarmPathCommand __declspec(deprecated)
#endif

namespace combat_robot_msgs
{

namespace msg
{

// message struct
template<class ContainerAllocator>
struct SwarmPathCommand_
{
  using Type = SwarmPathCommand_<ContainerAllocator>;

  explicit SwarmPathCommand_(rosidl_runtime_cpp::MessageInitialization _init = rosidl_runtime_cpp::MessageInitialization::ALL)
  : header(_init)
  {
    if (rosidl_runtime_cpp::MessageInitialization::ALL == _init ||
      rosidl_runtime_cpp::MessageInitialization::ZERO == _init)
    {
      this->command = 0;
      this->num_waypoints = 0;
      this->path_json = "";
    }
  }

  explicit SwarmPathCommand_(const ContainerAllocator & _alloc, rosidl_runtime_cpp::MessageInitialization _init = rosidl_runtime_cpp::MessageInitialization::ALL)
  : header(_alloc, _init),
    path_json(_alloc)
  {
    if (rosidl_runtime_cpp::MessageInitialization::ALL == _init ||
      rosidl_runtime_cpp::MessageInitialization::ZERO == _init)
    {
      this->command = 0;
      this->num_waypoints = 0;
      this->path_json = "";
    }
  }

  // field types and members
  using _header_type =
    std_msgs::msg::Header_<ContainerAllocator>;
  _header_type header;
  using _command_type =
    uint8_t;
  _command_type command;
  using _num_waypoints_type =
    uint16_t;
  _num_waypoints_type num_waypoints;
  using _path_json_type =
    std::basic_string<char, std::char_traits<char>, typename std::allocator_traits<ContainerAllocator>::template rebind_alloc<char>>;
  _path_json_type path_json;

  // setters for named parameter idiom
  Type & set__header(
    const std_msgs::msg::Header_<ContainerAllocator> & _arg)
  {
    this->header = _arg;
    return *this;
  }
  Type & set__command(
    const uint8_t & _arg)
  {
    this->command = _arg;
    return *this;
  }
  Type & set__num_waypoints(
    const uint16_t & _arg)
  {
    this->num_waypoints = _arg;
    return *this;
  }
  Type & set__path_json(
    const std::basic_string<char, std::char_traits<char>, typename std::allocator_traits<ContainerAllocator>::template rebind_alloc<char>> & _arg)
  {
    this->path_json = _arg;
    return *this;
  }

  // constant declarations
  static constexpr uint8_t CMD_NONE =
    0u;
  static constexpr uint8_t CMD_START =
    1u;
  static constexpr uint8_t CMD_STOP =
    2u;
  static constexpr uint8_t CMD_PAUSE =
    3u;
  static constexpr uint8_t CMD_RESUME =
    4u;
  static constexpr uint8_t CMD_LOAD_PATH =
    5u;
  static constexpr uint8_t CMD_COMPLETE =
    6u;

  // pointer types
  using RawPtr =
    combat_robot_msgs::msg::SwarmPathCommand_<ContainerAllocator> *;
  using ConstRawPtr =
    const combat_robot_msgs::msg::SwarmPathCommand_<ContainerAllocator> *;
  using SharedPtr =
    std::shared_ptr<combat_robot_msgs::msg::SwarmPathCommand_<ContainerAllocator>>;
  using ConstSharedPtr =
    std::shared_ptr<combat_robot_msgs::msg::SwarmPathCommand_<ContainerAllocator> const>;

  template<typename Deleter = std::default_delete<
      combat_robot_msgs::msg::SwarmPathCommand_<ContainerAllocator>>>
  using UniquePtrWithDeleter =
    std::unique_ptr<combat_robot_msgs::msg::SwarmPathCommand_<ContainerAllocator>, Deleter>;

  using UniquePtr = UniquePtrWithDeleter<>;

  template<typename Deleter = std::default_delete<
      combat_robot_msgs::msg::SwarmPathCommand_<ContainerAllocator>>>
  using ConstUniquePtrWithDeleter =
    std::unique_ptr<combat_robot_msgs::msg::SwarmPathCommand_<ContainerAllocator> const, Deleter>;
  using ConstUniquePtr = ConstUniquePtrWithDeleter<>;

  using WeakPtr =
    std::weak_ptr<combat_robot_msgs::msg::SwarmPathCommand_<ContainerAllocator>>;
  using ConstWeakPtr =
    std::weak_ptr<combat_robot_msgs::msg::SwarmPathCommand_<ContainerAllocator> const>;

  // pointer types similar to ROS 1, use SharedPtr / ConstSharedPtr instead
  // NOTE: Can't use 'using' here because GNU C++ can't parse attributes properly
  typedef DEPRECATED__combat_robot_msgs__msg__SwarmPathCommand
    std::shared_ptr<combat_robot_msgs::msg::SwarmPathCommand_<ContainerAllocator>>
    Ptr;
  typedef DEPRECATED__combat_robot_msgs__msg__SwarmPathCommand
    std::shared_ptr<combat_robot_msgs::msg::SwarmPathCommand_<ContainerAllocator> const>
    ConstPtr;

  // comparison operators
  bool operator==(const SwarmPathCommand_ & other) const
  {
    if (this->header != other.header) {
      return false;
    }
    if (this->command != other.command) {
      return false;
    }
    if (this->num_waypoints != other.num_waypoints) {
      return false;
    }
    if (this->path_json != other.path_json) {
      return false;
    }
    return true;
  }
  bool operator!=(const SwarmPathCommand_ & other) const
  {
    return !this->operator==(other);
  }
};  // struct SwarmPathCommand_

// alias to use template instance with default allocator
using SwarmPathCommand =
  combat_robot_msgs::msg::SwarmPathCommand_<std::allocator<void>>;

// constant definitions
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t SwarmPathCommand_<ContainerAllocator>::CMD_NONE;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t SwarmPathCommand_<ContainerAllocator>::CMD_START;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t SwarmPathCommand_<ContainerAllocator>::CMD_STOP;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t SwarmPathCommand_<ContainerAllocator>::CMD_PAUSE;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t SwarmPathCommand_<ContainerAllocator>::CMD_RESUME;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t SwarmPathCommand_<ContainerAllocator>::CMD_LOAD_PATH;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t SwarmPathCommand_<ContainerAllocator>::CMD_COMPLETE;
#endif  // __cplusplus < 201703L

}  // namespace msg

}  // namespace combat_robot_msgs

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__SWARM_PATH_COMMAND__STRUCT_HPP_
