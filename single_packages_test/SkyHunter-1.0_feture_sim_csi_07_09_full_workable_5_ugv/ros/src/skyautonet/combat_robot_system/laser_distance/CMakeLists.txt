cmake_minimum_required(VERSION 3.8)
project(laser_distance)

# C++17
set(CMAKE_CXX_STANDARD 17)

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)

include_directories(
  include
)

add_library(laser_distance
  src/laser_distance.cpp
)

add_executable(laser_distance_node node/laser_distance_node.cpp)
target_link_libraries(laser_distance_node PUBLIC
  ${std_msgs_TARGETS}
  rclcpp::rclcpp
  laser_distance
)

install(
  TARGETS laser_distance laser_distance_node
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION lib/${PROJECT_NAME}
)

install(
  DIRECTORY include/
  DESTINATION include/
)

install(
  DIRECTORY launch
  DESTINATION share/${PROJECT_NAME}/
)

ament_package()
