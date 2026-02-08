cmake_minimum_required(VERSION 3.8)
project(my_bot_nav2)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()

find_package(ament_cmake REQUIRED)

install(DIRECTORY config launch urdf worlds
  DESTINATION share/${PROJECT_NAME}
)

ament_package()