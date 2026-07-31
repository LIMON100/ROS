// generated from rosidl_generator_cpp/resource/idl__struct.hpp.em
// with input from combat_robot_msgs:msg/ChassisStatus.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/chassis_status.hpp"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__CHASSIS_STATUS__STRUCT_HPP_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__CHASSIS_STATUS__STRUCT_HPP_

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
# define DEPRECATED__combat_robot_msgs__msg__ChassisStatus __attribute__((deprecated))
#else
# define DEPRECATED__combat_robot_msgs__msg__ChassisStatus __declspec(deprecated)
#endif

namespace combat_robot_msgs
{

namespace msg
{

// message struct
template<class ContainerAllocator>
struct ChassisStatus_
{
  using Type = ChassisStatus_<ContainerAllocator>;

  explicit ChassisStatus_(rosidl_runtime_cpp::MessageInitialization _init = rosidl_runtime_cpp::MessageInitialization::ALL)
  : header(_init)
  {
    if (rosidl_runtime_cpp::MessageInitialization::ALL == _init ||
      rosidl_runtime_cpp::MessageInitialization::ZERO == _init)
    {
      this->drive_state = 0;
      this->battery_pct = 0;
      this->battery_voltage_v = 0.0f;
      this->battery_current_a = 0.0f;
      this->linear_velocity_mps = 0.0f;
      this->angular_velocity_rps = 0.0f;
      this->fault_flags = 0ul;
      this->motor_temp_c = 0.0f;
    }
  }

  explicit ChassisStatus_(const ContainerAllocator & _alloc, rosidl_runtime_cpp::MessageInitialization _init = rosidl_runtime_cpp::MessageInitialization::ALL)
  : header(_alloc, _init)
  {
    if (rosidl_runtime_cpp::MessageInitialization::ALL == _init ||
      rosidl_runtime_cpp::MessageInitialization::ZERO == _init)
    {
      this->drive_state = 0;
      this->battery_pct = 0;
      this->battery_voltage_v = 0.0f;
      this->battery_current_a = 0.0f;
      this->linear_velocity_mps = 0.0f;
      this->angular_velocity_rps = 0.0f;
      this->fault_flags = 0ul;
      this->motor_temp_c = 0.0f;
    }
  }

  // field types and members
  using _header_type =
    std_msgs::msg::Header_<ContainerAllocator>;
  _header_type header;
  using _drive_state_type =
    uint8_t;
  _drive_state_type drive_state;
  using _battery_pct_type =
    uint8_t;
  _battery_pct_type battery_pct;
  using _battery_voltage_v_type =
    float;
  _battery_voltage_v_type battery_voltage_v;
  using _battery_current_a_type =
    float;
  _battery_current_a_type battery_current_a;
  using _linear_velocity_mps_type =
    float;
  _linear_velocity_mps_type linear_velocity_mps;
  using _angular_velocity_rps_type =
    float;
  _angular_velocity_rps_type angular_velocity_rps;
  using _fault_flags_type =
    uint32_t;
  _fault_flags_type fault_flags;
  using _motor_temp_c_type =
    float;
  _motor_temp_c_type motor_temp_c;

  // setters for named parameter idiom
  Type & set__header(
    const std_msgs::msg::Header_<ContainerAllocator> & _arg)
  {
    this->header = _arg;
    return *this;
  }
  Type & set__drive_state(
    const uint8_t & _arg)
  {
    this->drive_state = _arg;
    return *this;
  }
  Type & set__battery_pct(
    const uint8_t & _arg)
  {
    this->battery_pct = _arg;
    return *this;
  }
  Type & set__battery_voltage_v(
    const float & _arg)
  {
    this->battery_voltage_v = _arg;
    return *this;
  }
  Type & set__battery_current_a(
    const float & _arg)
  {
    this->battery_current_a = _arg;
    return *this;
  }
  Type & set__linear_velocity_mps(
    const float & _arg)
  {
    this->linear_velocity_mps = _arg;
    return *this;
  }
  Type & set__angular_velocity_rps(
    const float & _arg)
  {
    this->angular_velocity_rps = _arg;
    return *this;
  }
  Type & set__fault_flags(
    const uint32_t & _arg)
  {
    this->fault_flags = _arg;
    return *this;
  }
  Type & set__motor_temp_c(
    const float & _arg)
  {
    this->motor_temp_c = _arg;
    return *this;
  }

  // constant declarations
  static constexpr uint8_t DRIVE_OK =
    0u;
  static constexpr uint8_t DRIVE_FAULT =
    1u;
  static constexpr uint8_t DRIVE_ESTOP =
    2u;
  static constexpr uint32_t FAULT_NONE =
    0u;
  static constexpr uint32_t FAULT_LEFT_WHEEL =
    1u;
  static constexpr uint32_t FAULT_RIGHT_WHEEL =
    2u;
  static constexpr uint32_t FAULT_LOW_BATTERY =
    4u;
  static constexpr uint32_t FAULT_OVERTEMP =
    8u;
  static constexpr uint32_t FAULT_COMM =
    16u;

  // pointer types
  using RawPtr =
    combat_robot_msgs::msg::ChassisStatus_<ContainerAllocator> *;
  using ConstRawPtr =
    const combat_robot_msgs::msg::ChassisStatus_<ContainerAllocator> *;
  using SharedPtr =
    std::shared_ptr<combat_robot_msgs::msg::ChassisStatus_<ContainerAllocator>>;
  using ConstSharedPtr =
    std::shared_ptr<combat_robot_msgs::msg::ChassisStatus_<ContainerAllocator> const>;

  template<typename Deleter = std::default_delete<
      combat_robot_msgs::msg::ChassisStatus_<ContainerAllocator>>>
  using UniquePtrWithDeleter =
    std::unique_ptr<combat_robot_msgs::msg::ChassisStatus_<ContainerAllocator>, Deleter>;

  using UniquePtr = UniquePtrWithDeleter<>;

  template<typename Deleter = std::default_delete<
      combat_robot_msgs::msg::ChassisStatus_<ContainerAllocator>>>
  using ConstUniquePtrWithDeleter =
    std::unique_ptr<combat_robot_msgs::msg::ChassisStatus_<ContainerAllocator> const, Deleter>;
  using ConstUniquePtr = ConstUniquePtrWithDeleter<>;

  using WeakPtr =
    std::weak_ptr<combat_robot_msgs::msg::ChassisStatus_<ContainerAllocator>>;
  using ConstWeakPtr =
    std::weak_ptr<combat_robot_msgs::msg::ChassisStatus_<ContainerAllocator> const>;

  // pointer types similar to ROS 1, use SharedPtr / ConstSharedPtr instead
  // NOTE: Can't use 'using' here because GNU C++ can't parse attributes properly
  typedef DEPRECATED__combat_robot_msgs__msg__ChassisStatus
    std::shared_ptr<combat_robot_msgs::msg::ChassisStatus_<ContainerAllocator>>
    Ptr;
  typedef DEPRECATED__combat_robot_msgs__msg__ChassisStatus
    std::shared_ptr<combat_robot_msgs::msg::ChassisStatus_<ContainerAllocator> const>
    ConstPtr;

  // comparison operators
  bool operator==(const ChassisStatus_ & other) const
  {
    if (this->header != other.header) {
      return false;
    }
    if (this->drive_state != other.drive_state) {
      return false;
    }
    if (this->battery_pct != other.battery_pct) {
      return false;
    }
    if (this->battery_voltage_v != other.battery_voltage_v) {
      return false;
    }
    if (this->battery_current_a != other.battery_current_a) {
      return false;
    }
    if (this->linear_velocity_mps != other.linear_velocity_mps) {
      return false;
    }
    if (this->angular_velocity_rps != other.angular_velocity_rps) {
      return false;
    }
    if (this->fault_flags != other.fault_flags) {
      return false;
    }
    if (this->motor_temp_c != other.motor_temp_c) {
      return false;
    }
    return true;
  }
  bool operator!=(const ChassisStatus_ & other) const
  {
    return !this->operator==(other);
  }
};  // struct ChassisStatus_

// alias to use template instance with default allocator
using ChassisStatus =
  combat_robot_msgs::msg::ChassisStatus_<std::allocator<void>>;

// constant definitions
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t ChassisStatus_<ContainerAllocator>::DRIVE_OK;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t ChassisStatus_<ContainerAllocator>::DRIVE_FAULT;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t ChassisStatus_<ContainerAllocator>::DRIVE_ESTOP;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint32_t ChassisStatus_<ContainerAllocator>::FAULT_NONE;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint32_t ChassisStatus_<ContainerAllocator>::FAULT_LEFT_WHEEL;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint32_t ChassisStatus_<ContainerAllocator>::FAULT_RIGHT_WHEEL;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint32_t ChassisStatus_<ContainerAllocator>::FAULT_LOW_BATTERY;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint32_t ChassisStatus_<ContainerAllocator>::FAULT_OVERTEMP;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint32_t ChassisStatus_<ContainerAllocator>::FAULT_COMM;
#endif  // __cplusplus < 201703L

}  // namespace msg

}  // namespace combat_robot_msgs

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__CHASSIS_STATUS__STRUCT_HPP_
