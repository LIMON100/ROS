cmake_minimum_required(VERSION 3.16)
project(san_lidar)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(diagnostic_msgs REQUIRED)
find_package(combat_robot_msgs REQUIRED)
find_package(pcl_conversions REQUIRED)
find_package(PCL REQUIRED COMPONENTS common io segmentation filters)

add_library(san_lidar_core SHARED
  src/ground_segmenter.cpp
  src/robosense_e1_driver.cpp
  # Phase 2-E Turn 7 — Single-point LRF (DCN-2026-002 D-008)
  src/lrf_parser.cpp
  src/lrf_node.cpp
)
target_include_directories(san_lidar_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
  ${PCL_INCLUDE_DIRS}
)
target_link_libraries(san_lidar_core ${PCL_LIBRARIES})
ament_target_dependencies(san_lidar_core
  rclcpp sensor_msgs std_msgs diagnostic_msgs pcl_conversions combat_robot_msgs)

add_executable(robosense_e1_driver src/lidar_node.cpp)
target_link_libraries(robosense_e1_driver san_lidar_core)
ament_target_dependencies(robosense_e1_driver
  rclcpp sensor_msgs std_msgs diagnostic_msgs pcl_conversions combat_robot_msgs)

# Phase 2-E Turn 7 — LRF executable
add_executable(lrf_node src/lrf_main.cpp)
target_link_libraries(lrf_node san_lidar_core)
ament_target_dependencies(lrf_node rclcpp combat_robot_msgs std_msgs)

install(TARGETS robosense_e1_driver lrf_node
  RUNTIME DESTINATION lib/${PROJECT_NAME})
install(TARGETS san_lidar_core LIBRARY DESTINATION lib)
install(DIRECTORY include/ DESTINATION include)
install(DIRECTORY launch config DESTINATION share/${PROJECT_NAME})

if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)
  ament_add_gtest(test_ground_segmenter test/test_ground_segmenter.cpp)
  target_link_libraries(test_ground_segmenter san_lidar_core)
  target_include_directories(test_ground_segmenter PRIVATE ${PCL_INCLUDE_DIRS})

  # DCN-2026-006 EXT D-017 regression: fail_reason + counters
  ament_add_gtest(test_ground_segmenter_d017
    test/test_ground_segmenter_d017.cpp)
  target_link_libraries(test_ground_segmenter_d017 san_lidar_core)
  target_include_directories(test_ground_segmenter_d017
    PRIVATE ${PCL_INCLUDE_DIRS})

  # Phase 2-E Turn 7 — LRF parser standalone gtest
  ament_add_gtest(test_lrf_parser
    test/test_lrf_parser.cpp
    src/lrf_parser.cpp)
  target_include_directories(test_lrf_parser PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
endif()

ament_export_include_directories(include)
ament_export_libraries(san_lidar_core)
ament_export_dependencies(rclcpp sensor_msgs std_msgs diagnostic_msgs pcl_conversions combat_robot_msgs)
ament_package()
