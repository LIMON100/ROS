cmake_minimum_required(VERSION 3.16)
project(san_slam)

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
find_package(nav_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(OpenCV REQUIRED COMPONENTS core imgcodecs)

add_library(san_slam_core SHARED
  src/delta_encoder.cpp
  src/local_slam_node.cpp
)
target_include_directories(san_slam_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
  ${OpenCV_INCLUDE_DIRS}
)
target_link_libraries(san_slam_core ${OpenCV_LIBS})
ament_target_dependencies(san_slam_core
  rclcpp combat_robot_msgs nav_msgs geometry_msgs)

add_executable(local_slam_node src/local_slam_main.cpp)
target_link_libraries(local_slam_node san_slam_core)
ament_target_dependencies(local_slam_node
  rclcpp combat_robot_msgs nav_msgs geometry_msgs)

install(TARGETS local_slam_node RUNTIME DESTINATION lib/${PROJECT_NAME})
install(TARGETS san_slam_core LIBRARY DESTINATION lib)
install(DIRECTORY include/ DESTINATION include)
install(DIRECTORY launch config DESTINATION share/${PROJECT_NAME})

if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)
  ament_add_gtest(test_local_slam test/test_local_slam.cpp)
  target_link_libraries(test_local_slam san_slam_core)
  ament_target_dependencies(test_local_slam
    rclcpp combat_robot_msgs nav_msgs)
endif()

ament_export_include_directories(include)
ament_export_libraries(san_slam_core)
ament_export_dependencies(
  rclcpp combat_robot_msgs nav_msgs geometry_msgs)
ament_package()
