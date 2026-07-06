cmake_minimum_required(VERSION 3.16)
project(san_nav2)
find_package(ament_cmake REQUIRED)

install(
  DIRECTORY config launch rviz behavior_trees
  DESTINATION share/${PROJECT_NAME}
)

ament_package()
