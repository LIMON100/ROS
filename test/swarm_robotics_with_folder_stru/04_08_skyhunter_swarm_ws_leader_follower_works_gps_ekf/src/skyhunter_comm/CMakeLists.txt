cmake_minimum_required(VERSION 3.8)
project(skyhunter_comm)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Find dependencies
find_package(ament_cmake REQUIRED)
find_package(ament_cmake_python REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclpy REQUIRED)
find_package(std_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(tf2 REQUIRED)
find_package(tf2_ros REQUIRED)
find_package(skyhunter_msgs REQUIRED)

# Include directories for C++
include_directories(include)

# =============================================================================
# C++ Nodes (add when implementing)
# =============================================================================
# Example:
# add_executable(wifi6_mesh_sim_node src/wifi6_mesh_sim_node.cpp)
# ament_target_dependencies(wifi6_mesh_sim_node rclcpp tf2 tf2_ros skyhunter_msgs)


# =============================================================================
# Python Nodes
# =============================================================================
ament_python_install_package(${PROJECT_NAME})

# Install Python scripts as executables
install(PROGRAMS
  scripts/wifi6_mesh_simulator
  scripts/lora_simulator
  scripts/lte_simulator
  scripts/jammer_service
  scripts/swarm_comm_manager
  scripts/comm_traffic_filter
  scripts/mesh_visualizer
  scripts/rssi_plotter
  DESTINATION lib/${PROJECT_NAME}
)
# Install test scripts
install(PROGRAMS
  test/test_failover.py
  test/test_full_chain.py
  test/test_bridge_relay.py
  test/test_bridge_stress.py
  test/test_packet_loss.py
  test/test_jammer.py
  test/test_3tier_failover.py
  test/test_lora_estop.py      
  test/test_latency.py         
  DESTINATION lib/${PROJECT_NAME}
)

# =============================================================================
# Install shared files
# =============================================================================
install(DIRECTORY
  launch
  config
  docs
  DESTINATION share/${PROJECT_NAME}
)

# =============================================================================
# Testing
# =============================================================================
if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
