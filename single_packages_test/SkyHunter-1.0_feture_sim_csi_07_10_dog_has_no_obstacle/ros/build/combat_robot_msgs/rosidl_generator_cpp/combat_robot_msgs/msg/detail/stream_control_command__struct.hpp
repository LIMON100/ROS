// generated from rosidl_generator_cpp/resource/idl__struct.hpp.em
// with input from combat_robot_msgs:msg/StreamControlCommand.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/stream_control_command.hpp"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__STREAM_CONTROL_COMMAND__STRUCT_HPP_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__STREAM_CONTROL_COMMAND__STRUCT_HPP_

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
# define DEPRECATED__combat_robot_msgs__msg__StreamControlCommand __attribute__((deprecated))
#else
# define DEPRECATED__combat_robot_msgs__msg__StreamControlCommand __declspec(deprecated)
#endif

namespace combat_robot_msgs
{

namespace msg
{

// message struct
template<class ContainerAllocator>
struct StreamControlCommand_
{
  using Type = StreamControlCommand_<ContainerAllocator>;

  explicit StreamControlCommand_(rosidl_runtime_cpp::MessageInitialization _init = rosidl_runtime_cpp::MessageInitialization::ALL)
  : header(_init)
  {
    if (rosidl_runtime_cpp::MessageInitialization::ALL == _init ||
      rosidl_runtime_cpp::MessageInitialization::ZERO == _init)
    {
      this->stream_command = 0;
      this->stream_target_robot_id = 0ul;
    }
  }

  explicit StreamControlCommand_(const ContainerAllocator & _alloc, rosidl_runtime_cpp::MessageInitialization _init = rosidl_runtime_cpp::MessageInitialization::ALL)
  : header(_alloc, _init)
  {
    if (rosidl_runtime_cpp::MessageInitialization::ALL == _init ||
      rosidl_runtime_cpp::MessageInitialization::ZERO == _init)
    {
      this->stream_command = 0;
      this->stream_target_robot_id = 0ul;
    }
  }

  // field types and members
  using _header_type =
    std_msgs::msg::Header_<ContainerAllocator>;
  _header_type header;
  using _stream_command_type =
    uint8_t;
  _stream_command_type stream_command;
  using _stream_target_robot_id_type =
    uint32_t;
  _stream_target_robot_id_type stream_target_robot_id;

  // setters for named parameter idiom
  Type & set__header(
    const std_msgs::msg::Header_<ContainerAllocator> & _arg)
  {
    this->header = _arg;
    return *this;
  }
  Type & set__stream_command(
    const uint8_t & _arg)
  {
    this->stream_command = _arg;
    return *this;
  }
  Type & set__stream_target_robot_id(
    const uint32_t & _arg)
  {
    this->stream_target_robot_id = _arg;
    return *this;
  }

  // constant declarations
  static constexpr uint8_t STREAM_NONE =
    0u;
  static constexpr uint8_t STREAM_START =
    1u;
  static constexpr uint8_t STREAM_STOP =
    2u;

  // pointer types
  using RawPtr =
    combat_robot_msgs::msg::StreamControlCommand_<ContainerAllocator> *;
  using ConstRawPtr =
    const combat_robot_msgs::msg::StreamControlCommand_<ContainerAllocator> *;
  using SharedPtr =
    std::shared_ptr<combat_robot_msgs::msg::StreamControlCommand_<ContainerAllocator>>;
  using ConstSharedPtr =
    std::shared_ptr<combat_robot_msgs::msg::StreamControlCommand_<ContainerAllocator> const>;

  template<typename Deleter = std::default_delete<
      combat_robot_msgs::msg::StreamControlCommand_<ContainerAllocator>>>
  using UniquePtrWithDeleter =
    std::unique_ptr<combat_robot_msgs::msg::StreamControlCommand_<ContainerAllocator>, Deleter>;

  using UniquePtr = UniquePtrWithDeleter<>;

  template<typename Deleter = std::default_delete<
      combat_robot_msgs::msg::StreamControlCommand_<ContainerAllocator>>>
  using ConstUniquePtrWithDeleter =
    std::unique_ptr<combat_robot_msgs::msg::StreamControlCommand_<ContainerAllocator> const, Deleter>;
  using ConstUniquePtr = ConstUniquePtrWithDeleter<>;

  using WeakPtr =
    std::weak_ptr<combat_robot_msgs::msg::StreamControlCommand_<ContainerAllocator>>;
  using ConstWeakPtr =
    std::weak_ptr<combat_robot_msgs::msg::StreamControlCommand_<ContainerAllocator> const>;

  // pointer types similar to ROS 1, use SharedPtr / ConstSharedPtr instead
  // NOTE: Can't use 'using' here because GNU C++ can't parse attributes properly
  typedef DEPRECATED__combat_robot_msgs__msg__StreamControlCommand
    std::shared_ptr<combat_robot_msgs::msg::StreamControlCommand_<ContainerAllocator>>
    Ptr;
  typedef DEPRECATED__combat_robot_msgs__msg__StreamControlCommand
    std::shared_ptr<combat_robot_msgs::msg::StreamControlCommand_<ContainerAllocator> const>
    ConstPtr;

  // comparison operators
  bool operator==(const StreamControlCommand_ & other) const
  {
    if (this->header != other.header) {
      return false;
    }
    if (this->stream_command != other.stream_command) {
      return false;
    }
    if (this->stream_target_robot_id != other.stream_target_robot_id) {
      return false;
    }
    return true;
  }
  bool operator!=(const StreamControlCommand_ & other) const
  {
    return !this->operator==(other);
  }
};  // struct StreamControlCommand_

// alias to use template instance with default allocator
using StreamControlCommand =
  combat_robot_msgs::msg::StreamControlCommand_<std::allocator<void>>;

// constant definitions
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t StreamControlCommand_<ContainerAllocator>::STREAM_NONE;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t StreamControlCommand_<ContainerAllocator>::STREAM_START;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t StreamControlCommand_<ContainerAllocator>::STREAM_STOP;
#endif  // __cplusplus < 201703L

}  // namespace msg

}  // namespace combat_robot_msgs

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__STREAM_CONTROL_COMMAND__STRUCT_HPP_
