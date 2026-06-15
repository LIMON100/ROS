cmake_minimum_required(VERSION 3.16)
project(san_comm_msgs)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()

find_package(ament_cmake REQUIRED)
find_package(std_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(rosidl_default_generators REQUIRED)

rosidl_generate_interfaces(${PROJECT_NAME}
  "msg/CommState.msg"
  "msg/MeshLink.msg"
  "msg/MeshLinkStates.msg"
  "msg/MeshMetrics.msg"
  "msg/LteStatus.msg"
  "msg/LoraStatus.msg"
  "msg/LoraHeartbeat.msg"
  "msg/LoraEstop.msg"
  "msg/SwarmHeartbeat.msg"
  "srv/InjectFailure.srv"
  "srv/SendEstop.srv"
  DEPENDENCIES std_msgs geometry_msgs
)

ament_export_dependencies(rosidl_default_runtime std_msgs geometry_msgs)
ament_package()
