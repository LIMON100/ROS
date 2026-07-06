cmake_minimum_required(VERSION 3.16)
project(san_rth)

# SkyHunter v1.5.3 — DCN-2026-017 Return-to-Home action server.

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_action REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(nav2_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(tf2 REQUIRED)
find_package(tf2_geometry_msgs REQUIRED)
find_package(combat_robot_msgs REQUIRED)
# yaml-cpp ships as plain CMake config in /usr/lib/.../cmake/yaml-cpp.
find_package(yaml-cpp REQUIRED)

# rth_action_node executable.
add_executable(rth_action_node src/rth_action_node.cpp)
target_include_directories(rth_action_node PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include)
ament_target_dependencies(rth_action_node
  rclcpp rclcpp_action
  nav_msgs nav2_msgs sensor_msgs geometry_msgs
  tf2 tf2_geometry_msgs
  combat_robot_msgs)
target_link_libraries(rth_action_node yaml-cpp)

install(TARGETS rth_action_node
  DESTINATION lib/${PROJECT_NAME})

install(DIRECTORY include/
  DESTINATION include)

install(DIRECTORY launch
  DESTINATION share/${PROJECT_NAME})

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  # ament_copyright cannot validate the proprietary 'Proprietary - SkyAutoNet'
  # license (it only knows OSS templates); skip it. Copyright headers are
  # still present and enforced by cpplint's [legal/copyright] check.
  set(ament_cmake_copyright_FOUND TRUE)
  find_package(ament_cmake_gtest REQUIRED)
  ament_lint_auto_find_test_dependencies()

  # Pure-logic helpers gtest — no rclcpp/DDS/action runtime needed.
  ament_add_gtest(test_rth_helpers test/test_rth_helpers.cpp)
  target_include_directories(test_rth_helpers PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
  ament_target_dependencies(test_rth_helpers
    geometry_msgs tf2 tf2_geometry_msgs)
  target_link_libraries(test_rth_helpers yaml-cpp)
endif()

ament_package()
