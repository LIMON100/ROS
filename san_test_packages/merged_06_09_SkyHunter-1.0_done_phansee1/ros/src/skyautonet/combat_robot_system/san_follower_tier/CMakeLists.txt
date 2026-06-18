cmake_minimum_required(VERSION 3.16)
project(san_follower_tier)

# SAN v1.5 — Follower Tier FSM (PDR-2 / PDR-3).
#
# PATCH 2026-05-13 (Tier deep-dive review):
#   * Added comm_health library (pure-logic comm/breadcrumb tracker)
#   * Patched tier_fsm.cpp (dt-aware step + bidirectional hysteresis
#     + anti-flap on all tiers + KPP-2 latency probe)
#   * Patched tier_node.cpp (real comm_link/breadcrumb subscriptions
#     + real elapsed dt + FSM call inside lock + KPP-2 latency)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(combat_robot_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(tf2_ros REQUIRED)
# ─── Pure-logic libraries ────────────────────────────────────────────────

add_library(${PROJECT_NAME}_fsm
  src/tier_fsm.cpp
)
target_include_directories(${PROJECT_NAME}_fsm PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

# ★ PATCH 2026-05-13: new comm health tracker library
add_library(${PROJECT_NAME}_comm_health
  src/comm_health.cpp
)
target_include_directories(${PROJECT_NAME}_comm_health PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

# ─── rclcpp Node library ────────────────────────────────────────────────

add_library(${PROJECT_NAME}_node
  src/tier_node.cpp
)
target_include_directories(${PROJECT_NAME}_node PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(${PROJECT_NAME}_node
  ${PROJECT_NAME}_fsm
  ${PROJECT_NAME}_comm_health
)
ament_target_dependencies(${PROJECT_NAME}_node
  rclcpp
  std_msgs
  combat_robot_msgs
)

# ─── Executable ─────────────────────────────────────────────────────────

add_executable(tier_node
  src/tier_main.cpp
)
target_link_libraries(tier_node
  ${PROJECT_NAME}_node
)
ament_target_dependencies(tier_node rclcpp)

add_executable(follower_pursuit_node
  src/follower_pursuit_node.cpp
)
target_include_directories(follower_pursuit_node PRIVATE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
ament_target_dependencies(follower_pursuit_node
  rclcpp
  nav_msgs
  geometry_msgs
  sensor_msgs
  combat_robot_msgs
  tf2_ros
)

# ─── Install ────────────────────────────────────────────────────────────

install(DIRECTORY include/ DESTINATION include)
install(TARGETS
    ${PROJECT_NAME}_fsm
    ${PROJECT_NAME}_comm_health
    ${PROJECT_NAME}_node
  EXPORT export_${PROJECT_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
install(TARGETS tier_node
  DESTINATION lib/${PROJECT_NAME}
)

install(TARGETS tier_node follower_pursuit_node
  DESTINATION lib/${PROJECT_NAME}
)

install(DIRECTORY config launch
  DESTINATION share/${PROJECT_NAME}
  OPTIONAL
)
ament_export_include_directories(include)
ament_export_libraries(
  ${PROJECT_NAME}_fsm
  ${PROJECT_NAME}_comm_health
)
ament_export_dependencies(rclcpp std_msgs combat_robot_msgs)

# ─── Tests ──────────────────────────────────────────────────────────────

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  # ament_copyright cannot validate the proprietary 'Proprietary - SkyAutoNet'
  # license (it only knows OSS templates); skip it. Copyright headers are
  # still present and enforced by cpplint's [legal/copyright] check.
  set(ament_cmake_copyright_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
  find_package(ament_cmake_gtest REQUIRED)

  ament_add_gtest(test_tier_fsm
    test/test_tier_fsm.cpp
  )
  target_link_libraries(test_tier_fsm
    ${PROJECT_NAME}_fsm
    ${PROJECT_NAME}_comm_health
  )

  # Pure-logic cmd_vel smoother (header-only, ROS-free).
  ament_add_gtest(test_cmd_smoother
    test/test_cmd_smoother.cpp
  )
  target_include_directories(test_cmd_smoother PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  )

  ament_add_gtest(test_slot_pursuit test/test_slot_pursuit.cpp)
  target_include_directories(test_slot_pursuit PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)

endif()

ament_package()
