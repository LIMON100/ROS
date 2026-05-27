cmake_minimum_required(VERSION 3.16)
project(san_hub_slam)

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
find_package(OpenCV REQUIRED COMPONENTS core imgcodecs)
# [DCN-2026-006 EXT D-026] hub_slam audit publisher.
find_package(diagnostic_msgs REQUIRED)

# Optional g2o - production builds wire it via apt `ros-humble-libg2o`.
find_path(G2O_INCLUDE_DIR g2o/core/sparse_optimizer.h
  HINTS /usr/include /usr/local/include)
find_library(G2O_CORE NAMES g2o_core
  HINTS /usr/lib /usr/local/lib /usr/lib/x86_64-linux-gnu)
if(G2O_INCLUDE_DIR AND G2O_CORE)
  set(HAVE_G2O ON)
  message(STATUS "san_hub_slam: g2o FOUND - pose-graph enabled")
else()
  set(HAVE_G2O OFF)
  message(STATUS "san_hub_slam: g2o not found - pose-graph stubbed")
endif()

add_library(san_hub_slam_core SHARED
  src/aggregator.cpp
  src/pose_graph_optimizer.cpp
  src/hub_slam_node.cpp
)
target_include_directories(san_hub_slam_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
  ${OpenCV_INCLUDE_DIRS}
)
target_link_libraries(san_hub_slam_core ${OpenCV_LIBS})
if(HAVE_G2O)
  target_compile_definitions(san_hub_slam_core PRIVATE HAVE_G2O)
  target_include_directories(san_hub_slam_core PRIVATE ${G2O_INCLUDE_DIR})
  target_link_libraries(san_hub_slam_core ${G2O_CORE})
endif()
ament_target_dependencies(san_hub_slam_core
  rclcpp combat_robot_msgs diagnostic_msgs)

add_executable(hub_slam_node src/hub_slam_main.cpp)
target_link_libraries(hub_slam_node san_hub_slam_core)
ament_target_dependencies(hub_slam_node rclcpp combat_robot_msgs diagnostic_msgs)

install(TARGETS hub_slam_node RUNTIME DESTINATION lib/${PROJECT_NAME})
install(TARGETS san_hub_slam_core LIBRARY DESTINATION lib)
install(DIRECTORY include/ DESTINATION include)
install(DIRECTORY launch config DESTINATION share/${PROJECT_NAME})

if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)
  ament_add_gtest(test_aggregation_5s test/test_aggregation_5s.cpp)
  target_link_libraries(test_aggregation_5s san_hub_slam_core)
  ament_target_dependencies(test_aggregation_5s
    rclcpp combat_robot_msgs)

  # [DCN-2026-006 EXT D-021/D-026] Bayesian voting + mismatch tracker.
  ament_add_gtest(test_aggregator_voting test/test_aggregator_voting.cpp)
  target_link_libraries(test_aggregator_voting san_hub_slam_core)
endif()

ament_export_include_directories(include)
ament_export_libraries(san_hub_slam_core)
ament_export_dependencies(rclcpp combat_robot_msgs)
ament_package()
