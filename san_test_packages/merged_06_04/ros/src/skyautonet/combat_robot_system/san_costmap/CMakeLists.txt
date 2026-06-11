cmake_minimum_required(VERSION 3.16)
project(san_costmap)

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
find_package(sensor_msgs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(pcl_conversions REQUIRED)
find_package(PCL REQUIRED COMPONENTS common io)
find_package(OpenCV REQUIRED COMPONENTS core imgcodecs)

add_library(san_costmap_core SHARED
  src/obstacle_layer_v13.cpp
  src/traversability_layer.cpp
  src/inflation_layer_v13.cpp
  src/cost_map_publisher.cpp
  src/cost_map_node.cpp
)
target_include_directories(san_costmap_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
  ${PCL_INCLUDE_DIRS}
  ${OpenCV_INCLUDE_DIRS}
)
target_link_libraries(san_costmap_core
  ${PCL_LIBRARIES} ${OpenCV_LIBS})
ament_target_dependencies(san_costmap_core
  rclcpp combat_robot_msgs sensor_msgs std_msgs pcl_conversions)

add_executable(cost_map_node src/cost_map_main.cpp)
target_link_libraries(cost_map_node san_costmap_core)
ament_target_dependencies(cost_map_node
  rclcpp combat_robot_msgs sensor_msgs std_msgs pcl_conversions)

install(TARGETS cost_map_node RUNTIME DESTINATION lib/${PROJECT_NAME})
install(TARGETS san_costmap_core LIBRARY DESTINATION lib)
install(DIRECTORY include/ DESTINATION include)
install(DIRECTORY launch config DESTINATION share/${PROJECT_NAME})

if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)

  ament_add_gtest(test_cost_thresholds test/test_cost_thresholds.cpp)
  target_link_libraries(test_cost_thresholds san_costmap_core)

  ament_add_gtest(test_kpp_latency test/test_kpp_latency.cpp)
  target_link_libraries(test_kpp_latency san_costmap_core)
  ament_target_dependencies(test_kpp_latency rclcpp combat_robot_msgs)

  ament_add_gtest(test_kpp_coverage test/test_kpp_coverage.cpp)
  target_link_libraries(test_kpp_coverage san_costmap_core)
  ament_target_dependencies(test_kpp_coverage rclcpp combat_robot_msgs)

  # DCN-2026-006 EXT D-015 + D-016 regression
  ament_add_gtest(test_traversability_d015_d016
    test/test_traversability_d015_d016.cpp)
  target_link_libraries(test_traversability_d015_d016 san_costmap_core)

  # Phase-7 safety-critical audit P0: InflationLayerV13.
  # Pins the exponential-decay invariants (monotonicity, no-leakage
  # outside radius, max-pool semantics, lethal preservation, clamp)
  # — Nav2 planning relies on these and a 1-line off-by-one would
  # silently route through inflated zones.
  ament_add_gtest(test_inflation_layer test/test_inflation_layer.cpp)
  target_link_libraries(test_inflation_layer san_costmap_core)
endif()

ament_export_include_directories(include)
ament_export_libraries(san_costmap_core)
ament_export_dependencies(
  rclcpp combat_robot_msgs sensor_msgs std_msgs pcl_conversions)
ament_package()
