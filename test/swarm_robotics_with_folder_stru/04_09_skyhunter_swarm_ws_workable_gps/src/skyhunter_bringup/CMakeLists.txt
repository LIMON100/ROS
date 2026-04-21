cmake_minimum_required(VERSION 3.8)
project(skyhunter_bringup)

find_package(ament_cmake REQUIRED)

# Install config files
install(DIRECTORY config/
  DESTINATION share/${PROJECT_NAME}/config
)

# Install launch files
install(DIRECTORY launch/
  DESTINATION share/${PROJECT_NAME}/launch
)

ament_package()
