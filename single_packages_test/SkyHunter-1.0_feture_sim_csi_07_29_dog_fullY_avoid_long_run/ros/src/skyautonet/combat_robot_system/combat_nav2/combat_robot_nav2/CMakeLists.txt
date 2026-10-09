cmake_minimum_required(VERSION 3.8)
project(combat_robot_nav2)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# 1. 기본 ament 빌드 툴
find_package(ament_cmake REQUIRED)

# 2. C++ 미션 컨트롤 노드용 추가 의존성
find_package(rclcpp REQUIRED)
find_package(rclcpp_action REQUIRED)
find_package(nav2_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(combat_robot_msgs REQUIRED)
find_package(robot_localization REQUIRED)
find_package(geographic_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(tf2 REQUIRED)
find_package(tf2_geometry_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(visualization_msgs REQUIRED)

# 3. C++ 소스 코드 컴파일 (src/swarm_path_executor.cpp를 빌드)
add_executable(swarm_path_executor src/swarm_path_executor.cpp)
target_include_directories(swarm_path_executor PRIVATE include)

# 4. 컴파일된 노드에 의존성 연결
ament_target_dependencies(swarm_path_executor
  rclcpp
  rclcpp_action
  nav2_msgs
  geometry_msgs
  combat_robot_msgs
  robot_localization
  geographic_msgs
  sensor_msgs
  std_msgs
  nav_msgs
  visualization_msgs
  tf2
  tf2_geometry_msgs
)

# 3b. GNSS heading provider (C++ 포팅, gnss_heading.py 대체)
add_executable(gnss_heading_node src/gnss_heading_node.cpp)
ament_target_dependencies(gnss_heading_node
  rclcpp
  sensor_msgs
  std_msgs
  geometry_msgs
)

# 3c. GPS→map ENU 변환 (C++ 포팅, gps_to_map.py 대체)
add_executable(gps_to_map_node src/gps_to_map_node.cpp)
ament_target_dependencies(gps_to_map_node
  rclcpp
  sensor_msgs
  nav_msgs
  robot_localization
  geographic_msgs
)

# 3d. odometry/gps frame_id 재설정 (C++ 포팅, frame_fixer.py 대체)
add_executable(frame_fixer_node src/frame_fixer_node.cpp)
ament_target_dependencies(frame_fixer_node
  rclcpp
  nav_msgs
)

# 3e. 편대 장애물 공유 (C++ 포팅, swarm_obstacle_share.py 대체)
add_executable(swarm_obstacle_share_node src/swarm_obstacle_share_node.cpp)
ament_target_dependencies(swarm_obstacle_share_node
  rclcpp
  nav_msgs
  sensor_msgs
  tf2
  tf2_ros
  geometry_msgs
)

# 5. 빌드된 실행 파일 설치 (ros2 run으로 실행할 수 있도록 lib 경로에 설치)
install(TARGETS
  swarm_path_executor
  gnss_heading_node
  gps_to_map_node
  frame_fixer_node
  swarm_obstacle_share_node
  DESTINATION lib/${PROJECT_NAME}
)

# 6. 기존 설정 파일 및 디렉토리 설치 (src는 컴파일되므로 제외)
install(DIRECTORY
  launch
  config
  map
  world
  include
  rviz
  DESTINATION share/${PROJECT_NAME}
)

install(PROGRAMS
  script/sensor_time_sync.py
  script/can_reader.py
  script/gnss_status_bridge.py
  script/gyro_bias_comp.py
  script/swarm_lidar_filter.py
  script/swarm_datum_sync.py
  script/mock_board.py
  DESTINATION lib/${PROJECT_NAME}
)

# 보드 없이 s1/s2 데이터를 흉내내는 시뮬레이터 헬퍼 + mesh 운영 아티팩트
install(PROGRAMS script/run_sim.sh DESTINATION lib/${PROJECT_NAME})
install(DIRECTORY mesh DESTINATION share/${PROJECT_NAME})

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  # the following line skips the linter which checks for copyrights
  # comment the line when a copyright and license is added to all source files
  set(ament_cmake_copyright_FOUND TRUE)
  # the following line skips cpplint (only works in a git repo)
  # comment the line when this package is in a git repo and when
  # a copyright and license is added to all source files
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()

  find_package(ament_cmake_gtest REQUIRED)
  ament_add_gtest(test_convoy_geometry test/test_convoy_geometry.cpp)
  target_include_directories(test_convoy_geometry PRIVATE include)

  ament_add_gtest(test_early_column test/test_early_column.cpp)
  target_include_directories(test_early_column PRIVATE include)
endif()

ament_package()