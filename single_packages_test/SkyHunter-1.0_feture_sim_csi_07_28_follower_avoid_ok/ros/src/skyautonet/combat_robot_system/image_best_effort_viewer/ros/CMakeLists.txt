cmake_minimum_required(VERSION 3.5)
project(image_best_effort_viewer)

# 필수 패키지
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(cv_bridge REQUIRED)
find_package(OpenCV REQUIRED)
find_package(image_transport REQUIRED)

# include 경로 설정
include_directories(
  ${OpenCV_INCLUDE_DIRS}
)

# 노드 빌드
add_executable(image_best_effort_viewer src/image_best_effort_viewer.cpp)
target_link_libraries(image_best_effort_viewer PUBLIC
  ${sensor_msgs_TARGETS}
  cv_bridge::cv_bridge
  image_transport::image_transport
  rclcpp::rclcpp
  sensor_msgs::sensor_msgs_library
  ${OpenCV_LIBS}
)

# 실행 파일 및 launch 파일 설치
install(TARGETS
  image_best_effort_viewer
  DESTINATION lib/${PROJECT_NAME}
)

install(DIRECTORY launch
  DESTINATION share/${PROJECT_NAME}/
)

ament_package()
