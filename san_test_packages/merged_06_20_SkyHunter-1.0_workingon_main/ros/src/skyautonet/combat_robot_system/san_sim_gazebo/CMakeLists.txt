cmake_minimum_required(VERSION 3.16)
project(san_sim_gazebo)
find_package(ament_cmake REQUIRED)

install(
  DIRECTORY worlds config launch models rviz
  DESTINATION share/${PROJECT_NAME}
)

ament_package()
