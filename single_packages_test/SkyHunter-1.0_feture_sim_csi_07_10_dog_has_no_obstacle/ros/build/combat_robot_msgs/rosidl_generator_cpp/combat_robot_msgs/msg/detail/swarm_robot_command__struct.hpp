// generated from rosidl_generator_cpp/resource/idl__struct.hpp.em
// with input from combat_robot_msgs:msg/SwarmRobotCommand.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/swarm_robot_command.hpp"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__SWARM_ROBOT_COMMAND__STRUCT_HPP_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__SWARM_ROBOT_COMMAND__STRUCT_HPP_

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
# define DEPRECATED__combat_robot_msgs__msg__SwarmRobotCommand __attribute__((deprecated))
#else
# define DEPRECATED__combat_robot_msgs__msg__SwarmRobotCommand __declspec(deprecated)
#endif

namespace combat_robot_msgs
{

namespace msg
{

// message struct
template<class ContainerAllocator>
struct SwarmRobotCommand_
{
  using Type = SwarmRobotCommand_<ContainerAllocator>;

  explicit SwarmRobotCommand_(rosidl_runtime_cpp::MessageInitialization _init = rosidl_runtime_cpp::MessageInitialization::ALL)
  : header(_init)
  {
    if (rosidl_runtime_cpp::MessageInitialization::ALL == _init ||
      rosidl_runtime_cpp::MessageInitialization::ZERO == _init)
    {
      this->sequence = 0ul;
      this->command_type = 0;
      this->leader_robot_id = 0ul;
      this->target_robot_id = 0ul;
      this->operation_mode = 0;
      this->estop_requested = false;
      this->path_command = 0;
      this->num_waypoints = 0;
      this->path_id = "";
      this->path_json = "";
      this->formation_type = 0;
      this->formation_number = 0;
      this->grouping_index = 0;
      this->slot_index = 0;
      this->selected_robot_count = 0;
      std::fill<typename std::array<uint32_t, 8>::iterator, uint32_t>(this->selected_robot_ids.begin(), this->selected_robot_ids.end(), 0ul);
    }
  }

  explicit SwarmRobotCommand_(const ContainerAllocator & _alloc, rosidl_runtime_cpp::MessageInitialization _init = rosidl_runtime_cpp::MessageInitialization::ALL)
  : header(_alloc, _init),
    path_id(_alloc),
    path_json(_alloc),
    selected_robot_ids(_alloc)
  {
    if (rosidl_runtime_cpp::MessageInitialization::ALL == _init ||
      rosidl_runtime_cpp::MessageInitialization::ZERO == _init)
    {
      this->sequence = 0ul;
      this->command_type = 0;
      this->leader_robot_id = 0ul;
      this->target_robot_id = 0ul;
      this->operation_mode = 0;
      this->estop_requested = false;
      this->path_command = 0;
      this->num_waypoints = 0;
      this->path_id = "";
      this->path_json = "";
      this->formation_type = 0;
      this->formation_number = 0;
      this->grouping_index = 0;
      this->slot_index = 0;
      this->selected_robot_count = 0;
      std::fill<typename std::array<uint32_t, 8>::iterator, uint32_t>(this->selected_robot_ids.begin(), this->selected_robot_ids.end(), 0ul);
    }
  }

  // field types and members
  using _header_type =
    std_msgs::msg::Header_<ContainerAllocator>;
  _header_type header;
  using _sequence_type =
    uint32_t;
  _sequence_type sequence;
  using _command_type_type =
    uint8_t;
  _command_type_type command_type;
  using _leader_robot_id_type =
    uint32_t;
  _leader_robot_id_type leader_robot_id;
  using _target_robot_id_type =
    uint32_t;
  _target_robot_id_type target_robot_id;
  using _operation_mode_type =
    uint8_t;
  _operation_mode_type operation_mode;
  using _estop_requested_type =
    bool;
  _estop_requested_type estop_requested;
  using _path_command_type =
    uint8_t;
  _path_command_type path_command;
  using _num_waypoints_type =
    uint16_t;
  _num_waypoints_type num_waypoints;
  using _path_id_type =
    std::basic_string<char, std::char_traits<char>, typename std::allocator_traits<ContainerAllocator>::template rebind_alloc<char>>;
  _path_id_type path_id;
  using _path_json_type =
    std::basic_string<char, std::char_traits<char>, typename std::allocator_traits<ContainerAllocator>::template rebind_alloc<char>>;
  _path_json_type path_json;
  using _formation_type_type =
    uint8_t;
  _formation_type_type formation_type;
  using _formation_number_type =
    uint8_t;
  _formation_number_type formation_number;
  using _grouping_index_type =
    uint8_t;
  _grouping_index_type grouping_index;
  using _slot_index_type =
    uint8_t;
  _slot_index_type slot_index;
  using _selected_robot_count_type =
    uint8_t;
  _selected_robot_count_type selected_robot_count;
  using _selected_robot_ids_type =
    std::array<uint32_t, 8>;
  _selected_robot_ids_type selected_robot_ids;

  // setters for named parameter idiom
  Type & set__header(
    const std_msgs::msg::Header_<ContainerAllocator> & _arg)
  {
    this->header = _arg;
    return *this;
  }
  Type & set__sequence(
    const uint32_t & _arg)
  {
    this->sequence = _arg;
    return *this;
  }
  Type & set__command_type(
    const uint8_t & _arg)
  {
    this->command_type = _arg;
    return *this;
  }
  Type & set__leader_robot_id(
    const uint32_t & _arg)
  {
    this->leader_robot_id = _arg;
    return *this;
  }
  Type & set__target_robot_id(
    const uint32_t & _arg)
  {
    this->target_robot_id = _arg;
    return *this;
  }
  Type & set__operation_mode(
    const uint8_t & _arg)
  {
    this->operation_mode = _arg;
    return *this;
  }
  Type & set__estop_requested(
    const bool & _arg)
  {
    this->estop_requested = _arg;
    return *this;
  }
  Type & set__path_command(
    const uint8_t & _arg)
  {
    this->path_command = _arg;
    return *this;
  }
  Type & set__num_waypoints(
    const uint16_t & _arg)
  {
    this->num_waypoints = _arg;
    return *this;
  }
  Type & set__path_id(
    const std::basic_string<char, std::char_traits<char>, typename std::allocator_traits<ContainerAllocator>::template rebind_alloc<char>> & _arg)
  {
    this->path_id = _arg;
    return *this;
  }
  Type & set__path_json(
    const std::basic_string<char, std::char_traits<char>, typename std::allocator_traits<ContainerAllocator>::template rebind_alloc<char>> & _arg)
  {
    this->path_json = _arg;
    return *this;
  }
  Type & set__formation_type(
    const uint8_t & _arg)
  {
    this->formation_type = _arg;
    return *this;
  }
  Type & set__formation_number(
    const uint8_t & _arg)
  {
    this->formation_number = _arg;
    return *this;
  }
  Type & set__grouping_index(
    const uint8_t & _arg)
  {
    this->grouping_index = _arg;
    return *this;
  }
  Type & set__slot_index(
    const uint8_t & _arg)
  {
    this->slot_index = _arg;
    return *this;
  }
  Type & set__selected_robot_count(
    const uint8_t & _arg)
  {
    this->selected_robot_count = _arg;
    return *this;
  }
  Type & set__selected_robot_ids(
    const std::array<uint32_t, 8> & _arg)
  {
    this->selected_robot_ids = _arg;
    return *this;
  }

  // constant declarations
  static constexpr uint8_t COMMAND_NONE =
    0u;
  static constexpr uint8_t COMMAND_MODE =
    1u;
  static constexpr uint8_t COMMAND_PATH =
    2u;
  static constexpr uint8_t COMMAND_FORMATION =
    3u;
  static constexpr uint8_t COMMAND_SYNC =
    4u;
  static constexpr uint8_t PATH_CMD_NONE =
    0u;
  static constexpr uint8_t PATH_CMD_START =
    1u;
  static constexpr uint8_t PATH_CMD_STOP =
    2u;
  static constexpr uint8_t PATH_CMD_PAUSE =
    3u;
  static constexpr uint8_t PATH_CMD_RESUME =
    4u;
  static constexpr uint8_t PATH_CMD_LOAD_PATH =
    5u;

  // pointer types
  using RawPtr =
    combat_robot_msgs::msg::SwarmRobotCommand_<ContainerAllocator> *;
  using ConstRawPtr =
    const combat_robot_msgs::msg::SwarmRobotCommand_<ContainerAllocator> *;
  using SharedPtr =
    std::shared_ptr<combat_robot_msgs::msg::SwarmRobotCommand_<ContainerAllocator>>;
  using ConstSharedPtr =
    std::shared_ptr<combat_robot_msgs::msg::SwarmRobotCommand_<ContainerAllocator> const>;

  template<typename Deleter = std::default_delete<
      combat_robot_msgs::msg::SwarmRobotCommand_<ContainerAllocator>>>
  using UniquePtrWithDeleter =
    std::unique_ptr<combat_robot_msgs::msg::SwarmRobotCommand_<ContainerAllocator>, Deleter>;

  using UniquePtr = UniquePtrWithDeleter<>;

  template<typename Deleter = std::default_delete<
      combat_robot_msgs::msg::SwarmRobotCommand_<ContainerAllocator>>>
  using ConstUniquePtrWithDeleter =
    std::unique_ptr<combat_robot_msgs::msg::SwarmRobotCommand_<ContainerAllocator> const, Deleter>;
  using ConstUniquePtr = ConstUniquePtrWithDeleter<>;

  using WeakPtr =
    std::weak_ptr<combat_robot_msgs::msg::SwarmRobotCommand_<ContainerAllocator>>;
  using ConstWeakPtr =
    std::weak_ptr<combat_robot_msgs::msg::SwarmRobotCommand_<ContainerAllocator> const>;

  // pointer types similar to ROS 1, use SharedPtr / ConstSharedPtr instead
  // NOTE: Can't use 'using' here because GNU C++ can't parse attributes properly
  typedef DEPRECATED__combat_robot_msgs__msg__SwarmRobotCommand
    std::shared_ptr<combat_robot_msgs::msg::SwarmRobotCommand_<ContainerAllocator>>
    Ptr;
  typedef DEPRECATED__combat_robot_msgs__msg__SwarmRobotCommand
    std::shared_ptr<combat_robot_msgs::msg::SwarmRobotCommand_<ContainerAllocator> const>
    ConstPtr;

  // comparison operators
  bool operator==(const SwarmRobotCommand_ & other) const
  {
    if (this->header != other.header) {
      return false;
    }
    if (this->sequence != other.sequence) {
      return false;
    }
    if (this->command_type != other.command_type) {
      return false;
    }
    if (this->leader_robot_id != other.leader_robot_id) {
      return false;
    }
    if (this->target_robot_id != other.target_robot_id) {
      return false;
    }
    if (this->operation_mode != other.operation_mode) {
      return false;
    }
    if (this->estop_requested != other.estop_requested) {
      return false;
    }
    if (this->path_command != other.path_command) {
      return false;
    }
    if (this->num_waypoints != other.num_waypoints) {
      return false;
    }
    if (this->path_id != other.path_id) {
      return false;
    }
    if (this->path_json != other.path_json) {
      return false;
    }
    if (this->formation_type != other.formation_type) {
      return false;
    }
    if (this->formation_number != other.formation_number) {
      return false;
    }
    if (this->grouping_index != other.grouping_index) {
      return false;
    }
    if (this->slot_index != other.slot_index) {
      return false;
    }
    if (this->selected_robot_count != other.selected_robot_count) {
      return false;
    }
    if (this->selected_robot_ids != other.selected_robot_ids) {
      return false;
    }
    return true;
  }
  bool operator!=(const SwarmRobotCommand_ & other) const
  {
    return !this->operator==(other);
  }
};  // struct SwarmRobotCommand_

// alias to use template instance with default allocator
using SwarmRobotCommand =
  combat_robot_msgs::msg::SwarmRobotCommand_<std::allocator<void>>;

// constant definitions
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t SwarmRobotCommand_<ContainerAllocator>::COMMAND_NONE;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t SwarmRobotCommand_<ContainerAllocator>::COMMAND_MODE;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t SwarmRobotCommand_<ContainerAllocator>::COMMAND_PATH;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t SwarmRobotCommand_<ContainerAllocator>::COMMAND_FORMATION;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t SwarmRobotCommand_<ContainerAllocator>::COMMAND_SYNC;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t SwarmRobotCommand_<ContainerAllocator>::PATH_CMD_NONE;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t SwarmRobotCommand_<ContainerAllocator>::PATH_CMD_START;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t SwarmRobotCommand_<ContainerAllocator>::PATH_CMD_STOP;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t SwarmRobotCommand_<ContainerAllocator>::PATH_CMD_PAUSE;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t SwarmRobotCommand_<ContainerAllocator>::PATH_CMD_RESUME;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t SwarmRobotCommand_<ContainerAllocator>::PATH_CMD_LOAD_PATH;
#endif  // __cplusplus < 201703L

}  // namespace msg

}  // namespace combat_robot_msgs

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__SWARM_ROBOT_COMMAND__STRUCT_HPP_
