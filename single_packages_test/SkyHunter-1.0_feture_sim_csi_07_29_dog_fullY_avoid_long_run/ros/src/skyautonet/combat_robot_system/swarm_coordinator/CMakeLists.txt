cmake_minimum_required(VERSION 3.8)
project(swarm_coordinator)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_components REQUIRED)
find_package(combat_robot_msgs REQUIRED)
find_package(std_msgs REQUIRED)

add_library(swarm_coordinator SHARED
  src/swarm_coordinator.cpp
)

target_include_directories(swarm_coordinator PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

target_compile_features(swarm_coordinator PUBLIC cxx_std_17)

ament_target_dependencies(swarm_coordinator
  rclcpp
  rclcpp_components
  combat_robot_msgs
  std_msgs
)

rclcpp_components_register_node(swarm_coordinator
  PLUGIN "swarm_coordinator::SwarmCoordinatorNode"
  EXECUTABLE swarm_coordinator_node
)

install(TARGETS
  swarm_coordinator
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

install(
  DIRECTORY config launch
  DESTINATION share/${PROJECT_NAME}
)

install(
  DIRECTORY include/
  DESTINATION include
)

ament_package()
