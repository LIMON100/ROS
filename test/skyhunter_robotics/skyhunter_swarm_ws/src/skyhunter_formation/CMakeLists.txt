cmake_minimum_required(VERSION 3.8)
project(skyhunter_formation)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(skyhunter_msgs REQUIRED)
find_package(tf2 REQUIRED)
find_package(tf2_ros REQUIRED)
find_package(tf2_geometry_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(std_srvs REQUIRED)
find_package(grid_map_ros REQUIRED)
find_package(grid_map_msgs REQUIRED)
find_package(visualization_msgs REQUIRED)

# PCL Dependencies
find_package(pcl_conversions REQUIRED)
find_package(PCL REQUIRED)

find_package(grid_map_core REQUIRED)
find_package(OpenMP REQUIRED)
find_package(std_msgs REQUIRED)

# Add PCL Include Directories (Crucial for PCL)
include_directories(${PCL_INCLUDE_DIRS})

# ==========================
# 1. Leader Node
# ==========================
add_executable(leader_node src/leader_node.cpp)
ament_target_dependencies(leader_node
  rclcpp
  geometry_msgs
  nav_msgs
  skyhunter_msgs
  visualization_msgs
  sensor_msgs
  pcl_conversions
  grid_map_core   
  grid_map_ros    
  std_msgs
)

# Link PCL and Grid Map libraries
target_link_libraries(leader_node 
  ${PCL_LIBRARIES}
)

# Ensure Grid Map include directories are visible
target_include_directories(leader_node PUBLIC 
  ${grid_map_core_INCLUDE_DIRS}
  ${grid_map_ros_INCLUDE_DIRS}
)

# ==========================
# 2. Follower Node
# ==========================
add_executable(follower_node src/follower_node.cpp)
ament_target_dependencies(
  follower_node
  rclcpp
  geometry_msgs
  nav_msgs
  skyhunter_msgs
  tf2               
  tf2_ros
  tf2_geometry_msgs
  sensor_msgs
  std_srvs
  grid_map_ros  
  grid_map_msgs 
  visualization_msgs
  pcl_conversions
  std_msgs
)

# CRITICAL FIX: Link the actual PCL and TF2 libraries to the Follower Node
# This solves the "undefined reference" error
target_link_libraries(follower_node 
  ${PCL_LIBRARIES}
  OpenMP::OpenMP_CXX
  tf2::tf2
  tf2_ros::tf2_ros
  tf2_geometry_msgs::tf2_geometry_msgs
)

# ==========================
# 3. Swarm Monitor Node
# ==========================
add_executable(swarm_monitor_node src/swarm_monitor_node.cpp)
ament_target_dependencies(swarm_monitor_node
  rclcpp
  geometry_msgs
  nav_msgs
)


add_executable(lidar_test_node src/lidar_test_node.cpp)

# 2. Add dependencies
ament_target_dependencies(lidar_test_node
  rclcpp
  sensor_msgs
  pcl_conversions
  grid_map_core 
  grid_map_ros  
)
target_include_directories(lidar_test_node PUBLIC ${grid_map_core_INCLUDE_DIRS})
# 3. Link PCL libraries (Crucial)
target_link_libraries(lidar_test_node ${PCL_LIBRARIES})


add_executable(location_tracker src/location_tracker.cpp)
ament_target_dependencies(location_tracker 
  rclcpp 
  tf2_ros 
  geometry_msgs 
  nav_msgs     
  tf2_geometry_msgs
)
target_link_libraries(location_tracker
  tf2::tf2
  tf2_ros::tf2_ros
  tf2_geometry_msgs::tf2_geometry_msgs
)
install(TARGETS location_tracker DESTINATION lib/${PROJECT_NAME})


## direct_mover
add_executable(direct_mover src/direct_mover.cpp)
ament_target_dependencies(direct_mover 
  rclcpp 
  geometry_msgs 
  nav_msgs 
  tf2
  tf2_ros             
  tf2_geometry_msgs 
)

# 2. Explicitly link the libraries to the binary
target_link_libraries(direct_mover 
  tf2::tf2
  tf2_ros::tf2_ros
  tf2_geometry_msgs::tf2_geometry_msgs
)

## leadership_manager
add_executable(leadership_manager src/leadership_manager.cpp)
ament_target_dependencies(leadership_manager
  rclcpp
  std_msgs
  skyhunter_msgs
)

# Ensure it is installed so the launch file can find it
install(TARGETS
  leadership_manager
  DESTINATION lib/${PROJECT_NAME}
)

# ==========================
# Installation
# ==========================
install(TARGETS
  leader_node
  follower_node
  swarm_monitor_node
  lidar_test_node
  # direct_mover
  leadership_manager
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