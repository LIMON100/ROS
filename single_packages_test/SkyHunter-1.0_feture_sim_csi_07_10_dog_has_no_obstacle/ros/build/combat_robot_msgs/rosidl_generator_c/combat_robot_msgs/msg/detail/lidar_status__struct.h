// NOLINT: This file starts with a BOM since it contain non-ASCII characters
// generated from rosidl_generator_c/resource/idl__struct.h.em
// with input from combat_robot_msgs:msg/LidarStatus.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/lidar_status.h"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__LIDAR_STATUS__STRUCT_H_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__LIDAR_STATUS__STRUCT_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Constants defined in the message

/// Constant 'LIDAR_OK'.
/**
  * Driver / hardware 상태
  * 정상 스캔 중
 */
enum
{
  combat_robot_msgs__msg__LidarStatus__LIDAR_OK = 0
};

/// Constant 'LIDAR_DEGRADED'.
/**
  * 스캔되지만 품질 저하 (rate↓, noise↑)
 */
enum
{
  combat_robot_msgs__msg__LidarStatus__LIDAR_DEGRADED = 1
};

/// Constant 'LIDAR_FAULT'.
/**
  * 통신 / hardware 장애
 */
enum
{
  combat_robot_msgs__msg__LidarStatus__LIDAR_FAULT = 2
};

// Include directives for member types
// Member 'header'
#include "std_msgs/msg/detail/header__struct.h"

/// Struct defined in msg/LidarStatus in the package combat_robot_msgs.
/**
  * LiDAR 드라이버가 publish 하는 status.
  * robot_server 가 /lidar/status 토픽으로 subscribe 하여 LiDAR 헬스 / 장애물 정보를
  * 추후 status 패킷에 반영하거나 안전 분기 입력으로 활용함.
  *
  * 가능한 한 모든 필드를 매 주기 채워 보내주세요.
 */
typedef struct combat_robot_msgs__msg__LidarStatus
{
  std_msgs__msg__Header header;
  uint8_t status;
  /// 마지막 스캔 통계
  uint32_t last_scan_point_count;
  /// 실측 Hz (목표값과 차이 나면 LIDAR_DEGRADED 권장)
  float scan_rate_hz;
  /// 장애물 감지 — 안전 정지 입력으로 활용 가능.
  /// 가까이 있는 장애물 없으면 obstacle_detected=false, min_obstacle_distance_m=NaN.
  bool obstacle_detected;
  float min_obstacle_distance_m;
} combat_robot_msgs__msg__LidarStatus;

// Struct for a sequence of combat_robot_msgs__msg__LidarStatus.
typedef struct combat_robot_msgs__msg__LidarStatus__Sequence
{
  combat_robot_msgs__msg__LidarStatus * data;
  /// The number of valid items in data
  size_t size;
  /// The number of allocated items in data
  size_t capacity;
} combat_robot_msgs__msg__LidarStatus__Sequence;

#ifdef __cplusplus
}
#endif

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__LIDAR_STATUS__STRUCT_H_
