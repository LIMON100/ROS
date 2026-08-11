cmake_minimum_required(VERSION 3.16)
project(gun_trigger)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)

add_executable(gun_trigger_node
  src/gun_trigger.cpp
)

# ✅ include 경로/정의/링크 등 의존성 설정을 한 번에 처리
ament_target_dependencies(gun_trigger_node
  rclcpp
  std_msgs
)

# lgpio 제거: gun_trigger는 sysfs PWM(/sys/class/pwm/)을 직접 파일 I/O로 제어하므로 불필요
# 코드에서 <lgpio.h> include 없음, open_gpio_()는 std::ofstream으로 구현됨
# target_link_libraries(gun_trigger_node lgpio)

target_include_directories(gun_trigger_node PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)

install(TARGETS gun_trigger_node
  RUNTIME DESTINATION lib/${PROJECT_NAME})

install(DIRECTORY include/
  DESTINATION include)

# Install launch files so colcon places them into install/<pkg>/share/<pkg>/launch
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/launch")
  install(DIRECTORY launch
    DESTINATION share/${PROJECT_NAME}/
  )
endif()

ament_package()
