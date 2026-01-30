# cmake_minimum_required(VERSION 3.8)
# project(skyhunter_perception)

# if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
#   add_compile_options(-Wall -Wextra -Wpedantic)
# endif()

# # Find Packages
# find_package(ament_cmake REQUIRED)
# find_package(rclcpp REQUIRED)
# find_package(sensor_msgs REQUIRED)
# find_package(std_msgs REQUIRED)
# find_package(geometry_msgs REQUIRED)
# find_package(pcl_conversions REQUIRED)
# find_package(PCL REQUIRED)
# find_package(grid_map_ros REQUIRED)
# find_package(grid_map_msgs REQUIRED)
# find_package(Eigen3 REQUIRED)

# # TF2 Packages
# find_package(tf2 REQUIRED)
# find_package(tf2_ros REQUIRED)
# find_package(tf2_eigen REQUIRED)
# find_package(tf2_sensor_msgs REQUIRED)
# find_package(tf2_geometry_msgs REQUIRED)

# find_package(nav2_costmap_2d REQUIRED)
# find_package(pluginlib REQUIRED)

# # =======================================================
# # === Build E1 Processor Node
# # =======================================================
# add_executable(
#   e1_preprocessor_node
#   src/e1_preprocessor_node.cpp
# )

# # Specify dependencies ONLY for the LiDAR node
# ament_target_dependencies(
#   e1_preprocessor_node
#   rclcpp
#   sensor_msgs
#   std_msgs
#   pcl_conversions
#   grid_map_ros
# )

# # Link the LiDAR node against the PCL libraries
# target_link_libraries(
#   e1_preprocessor_node
#   ${PCL_LIBRARIES}
# )

# # =======================================================
# # === Build Elevation Mapper Node 
# # =======================================================
# add_executable(
#   elevation_mapper_node
#   src/elevation_mapper_node.cpp
# )
# ament_target_dependencies(
#   elevation_mapper_node
#   rclcpp
#   sensor_msgs
#   geometry_msgs
#   pcl_conversions
#   tf2
#   tf2_ros
#   tf2_eigen
#   tf2_sensor_msgs
#   tf2_geometry_msgs
#   grid_map_ros
#   grid_map_msgs
# )
# # PCL is needed for the PointCloud type
# target_link_libraries(
#   elevation_mapper_node
#   ${PCL_LIBRARIES}
#   Eigen3::Eigen
# )

# # =======================================================
# # === Build IMU Processor Node
# # =======================================================
# add_executable(
#   imu_processor_node
#   src/imu_processor_node.cpp
# )

# # Specify dependencies ONLY for the IMU node
# ament_target_dependencies(
#   imu_processor_node
#   rclcpp
#   sensor_msgs
#   tf2_ros
# )

# # =======================================================
# # === Installation
# # =======================================================
# # Install both executables so ros2 run can find them
# install(
#   TARGETS
#     e1_preprocessor_node
#     elevation_mapper_node
#     imu_processor_node
#   DESTINATION lib/${PROJECT_NAME}
# )

# # =======================================================
# # === Finalization & Testing
# # =======================================================
# if(BUILD_TESTING)
#   find_package(ament_lint_auto REQUIRED)
#   ament_lint_auto_find_test_dependencies()
# endif()

# ament_package()


cmake_minimum_required(VERSION 3.8)
project(skyhunter_perception)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Find Packages
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(pcl_conversions REQUIRED)
find_package(PCL REQUIRED)
find_package(grid_map_ros REQUIRED)
find_package(grid_map_msgs REQUIRED)
find_package(Eigen3 REQUIRED)

# TF2 Packages
find_package(tf2 REQUIRED)
find_package(tf2_ros REQUIRED)
find_package(tf2_eigen REQUIRED)
find_package(tf2_sensor_msgs REQUIRED)
find_package(tf2_geometry_msgs REQUIRED)

find_package(nav2_costmap_2d REQUIRED)
find_package(pluginlib REQUIRED)

# =======================================================
# === Build E1 Processor Node
# =======================================================
add_executable(
  e1_preprocessor_node
  src/e1_preprocessor_node.cpp
)

ament_target_dependencies(
  e1_preprocessor_node
  rclcpp
  sensor_msgs
  std_msgs
  pcl_conversions
  grid_map_ros
)

target_link_libraries(
  e1_preprocessor_node
  ${PCL_LIBRARIES}
)

# =======================================================
# === Build Elevation Mapper Node 
# =======================================================
add_executable(
  elevation_mapper_node
  src/elevation_mapper_node.cpp
)
ament_target_dependencies(
  elevation_mapper_node
  rclcpp
  sensor_msgs
  geometry_msgs
  pcl_conversions
  tf2
  tf2_ros
  tf2_eigen
  tf2_sensor_msgs
  tf2_geometry_msgs
  grid_map_ros
  grid_map_msgs
)
target_link_libraries(
  elevation_mapper_node
  ${PCL_LIBRARIES}
  Eigen3::Eigen
)

# =======================================================
# === Build IMU Processor Node
# =======================================================
add_executable(
  imu_processor_node
  src/imu_processor_node.cpp
)

ament_target_dependencies(
  imu_processor_node
  rclcpp
  sensor_msgs
  tf2_ros
)

# =======================================================
# === Build Terrain Costmap Plugin 
# =======================================================
add_library(terrain_layer_plugin SHARED src/terrain_costmap_layer.cpp)
ament_target_dependencies(
  terrain_layer_plugin
  rclcpp
  nav2_costmap_2d
  grid_map_ros
  grid_map_msgs
  pluginlib
)
# Register the plugin definition file
pluginlib_export_plugin_description_file(nav2_costmap_2d plugins.xml)

# =======================================================
# === Installation
# =======================================================
install(
  TARGETS
    terrain_layer_plugin
    e1_preprocessor_node
    elevation_mapper_node
    imu_processor_node
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION lib/${PROJECT_NAME}
)

install(FILES plugins.xml
  DESTINATION share/${PROJECT_NAME}
)

# =======================================================
# === EXPORT (CRITICAL FIX)
# =======================================================
# This allows other packages to find your plugin library
ament_export_libraries(terrain_layer_plugin)
ament_export_dependencies(
  nav2_costmap_2d 
  grid_map_ros 
  grid_map_msgs
  rclcpp
)

# =======================================================
# === Finalization & Testing
# =======================================================
if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()