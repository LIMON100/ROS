cmake_minimum_required(VERSION 3.8)
project(tin3_navigation)

find_package(ament_cmake REQUIRED)

# Install config files
install(DIRECTORY
  config
  launch
  maps
  rviz
  DESTINATION share/${PROJECT_NAME}/
)

ament_package()