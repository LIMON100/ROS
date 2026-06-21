cmake_minimum_required(VERSION 3.16)
project(san_test)

# SkyHunter v1.5.3 — DCN-2026-022 Gate-1 regression test automation.
# Tier 3 per ADR-008.

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_action REQUIRED)
find_package(combat_robot_msgs REQUIRED)
find_package(nav2_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(diagnostic_msgs REQUIRED)
find_package(tf2 REQUIRED)
find_package(tf2_ros REQUIRED)
# Phase-7 P2-2 — best-practice find_package for san_bringup. Not
# strictly required to BUILD san_test (no C++ deps on it), but
# declared so colcon's build order matches the launch-time include
# graph (gate1_regression.launch.xml includes deputy.launch.xml).
find_package(san_bringup REQUIRED)

# gtest_vendor provides the gtest sources + headers ament_cmake_gtest
# wraps. The regression runner links to them directly because it is a
# production executable (not a ament_add_gtest test), but still needs
# GoogleTest as its driver framework.
find_package(ament_cmake_gtest REQUIRED)

# OBJECT library: 8 L5 scenario definitions. Object-library form is
# REQUIRED here because the scenarios register their TEST(...) cases
# via gtest's static-initializer constructors — with a regular shared
# or static library, the linker strips those constructors (no caller
# from the consuming binary references them, so the linker thinks
# they're unused). OBJECT library forces the .o files to be linked
# into every consumer's final binary, preserving the constructors.
# Symptom of getting this wrong: "0 tests from 0 test suites ran."
add_library(san_test_scenarios OBJECT
  src/scenarios/l5_scenarios.cpp
)
target_include_directories(san_test_scenarios PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
  ${GTEST_INCLUDE_DIRS}
)
ament_target_dependencies(san_test_scenarios
  rclcpp rclcpp_action combat_robot_msgs
  nav2_msgs nav_msgs sensor_msgs geometry_msgs diagnostic_msgs
  tf2 tf2_ros)

# Standalone regression runner — for ros2 run / launch invocation +
# JUnit XML output (CI consumable). Distinct from the ament_add_gtest
# target so it can be launched as a normal ROS executable inside a
# live ROS graph (the gtest target runs as a unit test outside any
# graph).
add_executable(gate1_regression_runner
  src/gate1_regression_runner.cpp
  $<TARGET_OBJECTS:san_test_scenarios>)
target_link_libraries(gate1_regression_runner gtest gtest_main)
ament_target_dependencies(gate1_regression_runner
  rclcpp rclcpp_action combat_robot_msgs
  nav2_msgs nav_msgs sensor_msgs geometry_msgs diagnostic_msgs
  tf2 tf2_ros)

install(TARGETS gate1_regression_runner
  DESTINATION lib/${PROJECT_NAME})
install(DIRECTORY include/ DESTINATION include)
install(DIRECTORY launch DESTINATION share/${PROJECT_NAME})

if(BUILD_TESTING)
  # Drives the SAME scenarios through colcon test (and CTest).
  # CI parity: same suite runs in both the user-invoked runner and
  # the build-time gtest harness. Scenarios are pulled in as object
  # files so their static gtest registrars survive into the test
  # binary (same mechanism as the runner above).
  ament_add_gtest(test_l5_scenarios
    src/test_l5_main.cpp
    $<TARGET_OBJECTS:san_test_scenarios>
    TIMEOUT 60)
  ament_target_dependencies(test_l5_scenarios
    rclcpp rclcpp_action combat_robot_msgs
    nav2_msgs nav_msgs sensor_msgs geometry_msgs diagnostic_msgs
    tf2 tf2_ros)
endif()

ament_export_include_directories(include)
# Note: san_test_scenarios is an OBJECT library (no installable
# artifact), so it is intentionally NOT exported. Downstream packages
# that want the same scenario set should depend on san_test as a
# build_depend and include the .cpp directly.
ament_export_dependencies(rclcpp rclcpp_action combat_robot_msgs
  nav2_msgs nav_msgs sensor_msgs geometry_msgs diagnostic_msgs
  tf2 tf2_ros)
ament_package()
