cmake_minimum_required(VERSION 3.8)
project(swarm_path_test)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(combat_robot_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(tf2 REQUIRED)
find_package(tf2_ros REQUIRED)

add_executable(send_path src/send_path.cpp)
ament_target_dependencies(send_path rclcpp combat_robot_msgs)

add_executable(jog_tune src/jog_tune.cpp)
ament_target_dependencies(jog_tune rclcpp geometry_msgs nav_msgs tf2 tf2_ros)

install(TARGETS send_path jog_tune
  DESTINATION lib/${PROJECT_NAME})

ament_package()
