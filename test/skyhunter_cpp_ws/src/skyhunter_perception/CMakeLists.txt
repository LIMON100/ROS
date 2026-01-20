cmake_minimum_required(VERSION 3.8)
project(skyhunter_perception)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Find necessary packages
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(pcl_conversions REQUIRED)

# --- Find PCL (Point Cloud Library) ---
find_package(PCL 1.10 REQUIRED)

# --- Build the C++ Node ---
add_executable(
  lidar_processor_node
  src/lidar_processor_node.cpp
)
ament_target_dependencies(
  lidar_processor_node
  rclcpp
  sensor_msgs
  std_msgs
  pcl_conversions
)
# --- Link against PCL libraries ---
target_link_libraries(
  lidar_processor_node
  ${PCL_LIBRARIES}
)

# --- Installation ---
install(
  TARGETS lidar_processor_node
  DESTINATION lib/${PROJECT_NAME}
)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()