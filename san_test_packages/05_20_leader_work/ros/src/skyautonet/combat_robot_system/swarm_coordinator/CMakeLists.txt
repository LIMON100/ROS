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

find_package(tf2 REQUIRED)
find_package(tf2_ros REQUIRED)
find_package(geometry_msgs REQUIRED)

add_library(swarm_coordinator_core SHARED
  src/swarm_coordinator.cpp
  src/hub_health_monitor.cpp
)
target_include_directories(swarm_coordinator_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
ament_target_dependencies(swarm_coordinator_core rclcpp combat_robot_msgs)

add_executable(swarm_coordinator src/swarm_coordinator_main.cpp)
target_link_libraries(swarm_coordinator swarm_coordinator_core)
ament_target_dependencies(swarm_coordinator rclcpp combat_robot_msgs)

add_executable(swarm_monitor_node src/swarm_monitor_node.cpp)
target_include_directories(swarm_monitor_node PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
ament_target_dependencies(swarm_monitor_node
  rclcpp tf2 tf2_ros geometry_msgs)
install(TARGETS swarm_monitor_node DESTINATION lib/${PROJECT_NAME})


install(TARGETS swarm_coordinator RUNTIME DESTINATION lib/${PROJECT_NAME})
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
endif()

ament_export_include_directories(include)
ament_export_libraries(swarm_coordinator_core)
ament_export_dependencies(rclcpp combat_robot_msgs tf2 tf2_ros geometry_msgs)
ament_package()
