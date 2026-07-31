// generated from rosidl_generator_cpp/resource/idl__struct.hpp.em
// with input from combat_robot_msgs:msg/LidarStatus.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/lidar_status.hpp"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__LIDAR_STATUS__STRUCT_HPP_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__LIDAR_STATUS__STRUCT_HPP_

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
# define DEPRECATED__combat_robot_msgs__msg__LidarStatus __attribute__((deprecated))
#else
# define DEPRECATED__combat_robot_msgs__msg__LidarStatus __declspec(deprecated)
#endif

namespace combat_robot_msgs
{

namespace msg
{

// message struct
template<class ContainerAllocator>
struct LidarStatus_
{
  using Type = LidarStatus_<ContainerAllocator>;

  explicit LidarStatus_(rosidl_runtime_cpp::MessageInitialization _init = rosidl_runtime_cpp::MessageInitialization::ALL)
  : header(_init)
  {
    if (rosidl_runtime_cpp::MessageInitialization::ALL == _init ||
      rosidl_runtime_cpp::MessageInitialization::ZERO == _init)
    {
      this->status = 0;
      this->last_scan_point_count = 0ul;
      this->scan_rate_hz = 0.0f;
      this->obstacle_detected = false;
      this->min_obstacle_distance_m = 0.0f;
    }
  }

  explicit LidarStatus_(const ContainerAllocator & _alloc, rosidl_runtime_cpp::MessageInitialization _init = rosidl_runtime_cpp::MessageInitialization::ALL)
  : header(_alloc, _init)
  {
    if (rosidl_runtime_cpp::MessageInitialization::ALL == _init ||
      rosidl_runtime_cpp::MessageInitialization::ZERO == _init)
    {
      this->status = 0;
      this->last_scan_point_count = 0ul;
      this->scan_rate_hz = 0.0f;
      this->obstacle_detected = false;
      this->min_obstacle_distance_m = 0.0f;
    }
  }

  // field types and members
  using _header_type =
    std_msgs::msg::Header_<ContainerAllocator>;
  _header_type header;
  using _status_type =
    uint8_t;
  _status_type status;
  using _last_scan_point_count_type =
    uint32_t;
  _last_scan_point_count_type last_scan_point_count;
  using _scan_rate_hz_type =
    float;
  _scan_rate_hz_type scan_rate_hz;
  using _obstacle_detected_type =
    bool;
  _obstacle_detected_type obstacle_detected;
  using _min_obstacle_distance_m_type =
    float;
  _min_obstacle_distance_m_type min_obstacle_distance_m;

  // setters for named parameter idiom
  Type & set__header(
    const std_msgs::msg::Header_<ContainerAllocator> & _arg)
  {
    this->header = _arg;
    return *this;
  }
  Type & set__status(
    const uint8_t & _arg)
  {
    this->status = _arg;
    return *this;
  }
  Type & set__last_scan_point_count(
    const uint32_t & _arg)
  {
    this->last_scan_point_count = _arg;
    return *this;
  }
  Type & set__scan_rate_hz(
    const float & _arg)
  {
    this->scan_rate_hz = _arg;
    return *this;
  }
  Type & set__obstacle_detected(
    const bool & _arg)
  {
    this->obstacle_detected = _arg;
    return *this;
  }
  Type & set__min_obstacle_distance_m(
    const float & _arg)
  {
    this->min_obstacle_distance_m = _arg;
    return *this;
  }

  // constant declarations
  static constexpr uint8_t LIDAR_OK =
    0u;
  static constexpr uint8_t LIDAR_DEGRADED =
    1u;
  static constexpr uint8_t LIDAR_FAULT =
    2u;

  // pointer types
  using RawPtr =
    combat_robot_msgs::msg::LidarStatus_<ContainerAllocator> *;
  using ConstRawPtr =
    const combat_robot_msgs::msg::LidarStatus_<ContainerAllocator> *;
  using SharedPtr =
    std::shared_ptr<combat_robot_msgs::msg::LidarStatus_<ContainerAllocator>>;
  using ConstSharedPtr =
    std::shared_ptr<combat_robot_msgs::msg::LidarStatus_<ContainerAllocator> const>;

  template<typename Deleter = std::default_delete<
      combat_robot_msgs::msg::LidarStatus_<ContainerAllocator>>>
  using UniquePtrWithDeleter =
    std::unique_ptr<combat_robot_msgs::msg::LidarStatus_<ContainerAllocator>, Deleter>;

  using UniquePtr = UniquePtrWithDeleter<>;

  template<typename Deleter = std::default_delete<
      combat_robot_msgs::msg::LidarStatus_<ContainerAllocator>>>
  using ConstUniquePtrWithDeleter =
    std::unique_ptr<combat_robot_msgs::msg::LidarStatus_<ContainerAllocator> const, Deleter>;
  using ConstUniquePtr = ConstUniquePtrWithDeleter<>;

  using WeakPtr =
    std::weak_ptr<combat_robot_msgs::msg::LidarStatus_<ContainerAllocator>>;
  using ConstWeakPtr =
    std::weak_ptr<combat_robot_msgs::msg::LidarStatus_<ContainerAllocator> const>;

  // pointer types similar to ROS 1, use SharedPtr / ConstSharedPtr instead
  // NOTE: Can't use 'using' here because GNU C++ can't parse attributes properly
  typedef DEPRECATED__combat_robot_msgs__msg__LidarStatus
    std::shared_ptr<combat_robot_msgs::msg::LidarStatus_<ContainerAllocator>>
    Ptr;
  typedef DEPRECATED__combat_robot_msgs__msg__LidarStatus
    std::shared_ptr<combat_robot_msgs::msg::LidarStatus_<ContainerAllocator> const>
    ConstPtr;

  // comparison operators
  bool operator==(const LidarStatus_ & other) const
  {
    if (this->header != other.header) {
      return false;
    }
    if (this->status != other.status) {
      return false;
    }
    if (this->last_scan_point_count != other.last_scan_point_count) {
      return false;
    }
    if (this->scan_rate_hz != other.scan_rate_hz) {
      return false;
    }
    if (this->obstacle_detected != other.obstacle_detected) {
      return false;
    }
    if (this->min_obstacle_distance_m != other.min_obstacle_distance_m) {
      return false;
    }
    return true;
  }
  bool operator!=(const LidarStatus_ & other) const
  {
    return !this->operator==(other);
  }
};  // struct LidarStatus_

// alias to use template instance with default allocator
using LidarStatus =
  combat_robot_msgs::msg::LidarStatus_<std::allocator<void>>;

// constant definitions
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t LidarStatus_<ContainerAllocator>::LIDAR_OK;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t LidarStatus_<ContainerAllocator>::LIDAR_DEGRADED;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t LidarStatus_<ContainerAllocator>::LIDAR_FAULT;
#endif  // __cplusplus < 201703L

}  // namespace msg

}  // namespace combat_robot_msgs

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__LIDAR_STATUS__STRUCT_HPP_
