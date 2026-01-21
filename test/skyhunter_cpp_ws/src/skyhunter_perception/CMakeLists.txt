cmake_minimum_required(VERSION 3.8)
project(skyhunter_perception)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Find all necessary packages for both nodes
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(pcl_conversions REQUIRED)
find_package(tf2_ros REQUIRED)
find_package(PCL 1.12 REQUIRED) # It's safer to use the version installed by Humble, which is 1.12

# =======================================================
# === Build LiDAR Processor Node
# =======================================================
add_executable(
  e1_preprocessor_node
  src/e1_preprocessor_node.cpp
)

# Specify dependencies ONLY for the LiDAR node
ament_target_dependencies(
  e1_preprocessor_node
  rclcpp
  sensor_msgs
  std_msgs
  pcl_conversions
)

# Link the LiDAR node against the PCL libraries
target_link_libraries(
  e1_preprocessor_node
  ${PCL_LIBRARIES}
)

# =======================================================
# === Build IMU Processor Node
# =======================================================
add_executable(
  imu_processor_node
  src/imu_processor_node.cpp
)

# Specify dependencies ONLY for the IMU node
ament_target_dependencies(
  imu_processor_node
  rclcpp
  sensor_msgs
  tf2_ros
)

# =======================================================
# === Installation
# =======================================================
# Install both executables so ros2 run can find them
install(
  TARGETS
    e1_preprocessor_node
    imu_processor_node
  DESTINATION lib/${PROJECT_NAME}
)

# =======================================================
# === Finalization & Testing
# =======================================================
if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()