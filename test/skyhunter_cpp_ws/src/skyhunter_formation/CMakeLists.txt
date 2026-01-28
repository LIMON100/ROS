cmake_minimum_required(VERSION 3.8)
project(skyhunter_formation)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(skyhunter_msgs REQUIRED)
# --- ADD THESE LINES ---
find_package(tf2 REQUIRED)
find_package(tf2_geometry_msgs REQUIRED)
# ---------------------
find_package(sensor_msgs REQUIRED)
find_package(tf2_ros REQUIRED)
find_package(std_srvs REQUIRED)

# Leader Node
add_executable(leader_node src/leader_node.cpp)
ament_target_dependencies(leader_node
  rclcpp
  geometry_msgs
  nav_msgs
  skyhunter_msgs
)

# Follower Node
add_executable(follower_node src/follower_node.cpp)
ament_target_dependencies(follower_node
  rclcpp
  geometry_msgs
  nav_msgs
  skyhunter_msgs
  tf2               # Now this will work because we found it above
  tf2_geometry_msgs
  sensor_msgs
  tf2_ros
  std_srvs
)

# Swarm Monitor Node
add_executable(swarm_monitor_node src/swarm_monitor_node.cpp)
ament_target_dependencies(swarm_monitor_node
  rclcpp
  geometry_msgs
  nav_msgs
)

install(TARGETS
  leader_node
  follower_node
  swarm_monitor_node
  DESTINATION lib/${PROJECT_NAME}
)

install(
  DIRECTORY launch config
  DESTINATION share/${PROJECT_NAME}
)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()