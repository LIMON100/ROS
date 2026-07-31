// NOLINT: This file starts with a BOM since it contain non-ASCII characters
// generated from rosidl_generator_c/resource/idl__struct.h.em
// with input from combat_robot_msgs:msg/GnssStatus.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/gnss_status.h"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__GNSS_STATUS__STRUCT_H_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__GNSS_STATUS__STRUCT_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Constants defined in the message

/// Constant 'FIX_NONE'.
/**
  * Fix 품질 (NMEA / RTK 기준)
  * No fix / 수신기 끊김
 */
enum
{
  combat_robot_msgs__msg__GnssStatus__FIX_NONE = 0
};

/// Constant 'FIX_2D'.
/**
  * 2D 위성 fix
 */
enum
{
  combat_robot_msgs__msg__GnssStatus__FIX_2D = 1
};

/// Constant 'FIX_3D'.
/**
  * 3D 위성 fix
 */
enum
{
  combat_robot_msgs__msg__GnssStatus__FIX_3D = 2
};

/// Constant 'FIX_DGPS'.
/**
  * Differential GPS
 */
enum
{
  combat_robot_msgs__msg__GnssStatus__FIX_DGPS = 3
};

/// Constant 'FIX_RTK_FLOAT'.
/**
  * RTK Float (cm~m 수준)
 */
enum
{
  combat_robot_msgs__msg__GnssStatus__FIX_RTK_FLOAT = 4
};

/// Constant 'FIX_RTK_FIXED'.
/**
  * RTK Fixed (cm 수준)
 */
enum
{
  combat_robot_msgs__msg__GnssStatus__FIX_RTK_FIXED = 5
};

// Include directives for member types
// Member 'header'
#include "std_msgs/msg/detail/header__struct.h"

/// Struct defined in msg/GnssStatus in the package combat_robot_msgs.
/**
  * GNSS / RTK 드라이버가 publish 하는 status.
  * robot_server 가 /gnss/status 토픽으로 subscribe 하여 leader 위치/heading/speed 와
  * 앱 상태 패킷의 fix 품질 표시에 사용함.
  *
  * 모든 필드를 매 주기 채워 보내주세요. 값을 알 수 없으면 아래 정의된 INVALID 값을 사용.
 */
typedef struct combat_robot_msgs__msg__GnssStatus
{
  std_msgs__msg__Header header;
  uint8_t fix_status;
  /// 추적 위성 수 (0~32 typical)
  uint8_t num_satellites;
  /// 위치 — WGS-84
  /// 유효하지 않으면 latitude=longitude=NaN
  /// 도
  double latitude;
  /// 도
  double longitude;
  /// MSL m
  double altitude_m;
  /// 방위각 / 속도
  /// 유효하지 않으면 -1.0
  /// 0~360, true north 기준
  float heading_deg;
  /// >= 0
  float ground_speed_mps;
  /// 정확도 (1-sigma CEP)
  /// 알 수 없으면 -1.0
  float horizontal_accuracy_m;
  float vertical_accuracy_m;
} combat_robot_msgs__msg__GnssStatus;

// Struct for a sequence of combat_robot_msgs__msg__GnssStatus.
typedef struct combat_robot_msgs__msg__GnssStatus__Sequence
{
  combat_robot_msgs__msg__GnssStatus * data;
  /// The number of valid items in data
  size_t size;
  /// The number of allocated items in data
  size_t capacity;
} combat_robot_msgs__msg__GnssStatus__Sequence;

#ifdef __cplusplus
}
#endif

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__GNSS_STATUS__STRUCT_H_
