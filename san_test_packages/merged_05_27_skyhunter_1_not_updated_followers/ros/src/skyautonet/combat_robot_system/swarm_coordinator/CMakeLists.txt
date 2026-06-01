cmake_minimum_required(VERSION 3.16)
project(swarm_coordinator)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(combat_robot_msgs REQUIRED)
# DCN-2026-013: swarm_monitor_node — PoseArray on /swarm/poses + TF queries
find_package(geometry_msgs REQUIRED)
find_package(tf2 REQUIRED)
find_package(tf2_ros REQUIRED)

add_library(swarm_coordinator_core SHARED
  src/swarm_coordinator.cpp
  src/hub_health_monitor.cpp
  # DCN-2026-013 — swarm_monitor_node (Hub-only /swarm/poses publisher)
  src/swarm_monitor_node.cpp
)
target_include_directories(swarm_coordinator_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
ament_target_dependencies(swarm_coordinator_core
  rclcpp combat_robot_msgs
  # DCN-2026-013 transitive deps
  geometry_msgs tf2 tf2_ros)

add_executable(swarm_coordinator src/swarm_coordinator_main.cpp)
target_link_libraries(swarm_coordinator swarm_coordinator_core)
ament_target_dependencies(swarm_coordinator rclcpp combat_robot_msgs)

# DCN-2026-013: standalone executable for the monitor node so squadron
# launch / systemd can manage it as its own process.
add_executable(swarm_monitor_node src/swarm_monitor_node_main.cpp)
target_link_libraries(swarm_monitor_node swarm_coordinator_core)
ament_target_dependencies(swarm_monitor_node
  rclcpp geometry_msgs tf2 tf2_ros)

install(TARGETS swarm_coordinator swarm_monitor_node
  RUNTIME DESTINATION lib/${PROJECT_NAME})
install(TARGETS swarm_coordinator_core LIBRARY DESTINATION lib)
install(DIRECTORY include/ DESTINATION include)
install(DIRECTORY launch config DESTINATION share/${PROJECT_NAME})

if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)
  ament_add_gtest(test_swarm_constants test/test_swarm_constants.cpp)
  target_link_libraries(test_swarm_constants swarm_coordinator_core)

  ament_add_gtest(test_hub_health test/test_hub_health.cpp)
  target_link_libraries(test_hub_health swarm_coordinator_core)
  ament_target_dependencies(test_hub_health combat_robot_msgs)

  # DCN-2026-013: 4-case gtest for the Hub-only publisher gate.
  # Constructs SwarmMonitorNode with robot_role overrides and asserts
  # publisherExistsForTest() — Hub/Leader create, Follower/Deputy skip.
  ament_add_gtest(test_swarm_monitor_gate test/test_swarm_monitor_gate.cpp)
  target_link_libraries(test_swarm_monitor_gate swarm_coordinator_core)
  ament_target_dependencies(test_swarm_monitor_gate
    rclcpp geometry_msgs tf2 tf2_ros)
endif()

ament_export_include_directories(include)
ament_export_libraries(swarm_coordinator_core)
ament_export_dependencies(rclcpp combat_robot_msgs
  geometry_msgs tf2 tf2_ros)
ament_package()
