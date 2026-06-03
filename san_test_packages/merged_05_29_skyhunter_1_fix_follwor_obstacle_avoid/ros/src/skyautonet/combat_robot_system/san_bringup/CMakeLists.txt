cmake_minimum_required(VERSION 3.16)
project(san_bringup)

# SAN v1.5 Phase 2-E — squadron bringup.
# DCN-2026-002 D-007 (3-Tier) + D-008 (IPC unification).
# DCN-2026-014 v2 — adds the standalone network_bringup C++ executable
# (called from systemd ExecStartPre to set mesh0 static IP).

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)

# DCN-2026-014 Item 8 — non-ROS standalone executable. No rclcpp link
# (runs before ROS init via systemd ExecStartPre). Header lives under
# include/ so the gtest can include without depending on the binary.
add_executable(network_bringup src/network_bringup.cpp)
target_include_directories(network_bringup PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include)

install(TARGETS network_bringup
  DESTINATION lib/${PROJECT_NAME})

install(DIRECTORY include/
  DESTINATION include)

install(DIRECTORY launch config
  DESTINATION share/${PROJECT_NAME}
)

# The systemd unit is installed under a separate share/ subdir so
# packagers can `cp` it to /lib/systemd/system/ during system provisioning.
# We do NOT install into /lib/systemd/system/ from colcon — that's a
# system-level path, not a ROS install path.
install(DIRECTORY systemd
  DESTINATION share/${PROJECT_NAME}
)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  # ament_copyright cannot validate the proprietary 'Proprietary - SkyAutoNet'
  # license (it only knows OSS templates); skip it. Copyright headers are
  # still present and enforced by cpplint's [legal/copyright] check.
  set(ament_cmake_copyright_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()

  # [DCN-2026-014 Item 11] 10-case gtest for FastDDS profile selection
  # (T1..T10 — production+sim+discovery XMLs, systemd env, mesh0
  # whitelist contract, initial peers list, MTU + TTL bounds).
  # Replaces the prior test_launch_wrappers.py (now under
  # test/deprecated/) — pytest retired by the C++ / gtest-only policy.
  find_package(ament_cmake_gtest REQUIRED)
  find_package(ament_index_cpp REQUIRED)
  # tinyxml2_vendor adds its Modules dir to CMAKE_MODULE_PATH so the
  # subsequent FindTinyXML2.cmake (capital T) resolves; the imported
  # target tinyxml2::tinyxml2 is provided by the vendor extras.
  find_package(tinyxml2_vendor REQUIRED)
  find_package(TinyXML2 REQUIRED)

  ament_add_gtest(test_fastrtps_profile_selection
    test/test_fastrtps_profile_selection.cpp)
  ament_target_dependencies(test_fastrtps_profile_selection
    ament_index_cpp)
  target_link_libraries(test_fastrtps_profile_selection tinyxml2::tinyxml2)
  # T5 walks the repo tree for infra/systemd/; pass the absolute path
  # from CMake configuration to avoid relative-path fragility.
  set_tests_properties(test_fastrtps_profile_selection PROPERTIES
    ENVIRONMENT "SAN_INFRA_SYSTEMD_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../../../../../infra/systemd")

  # [DCN-2026-014 Item 8] gtest for network_bringup pure-logic helpers
  # (lookupIp + parseInt + trim).
  ament_add_gtest(test_network_bringup
    test/test_network_bringup.cpp)
  target_include_directories(test_network_bringup PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)

  # [DCN-2026-012 v2] 6-case gtest for multi-robot sim hardening
  # (T1 static_tf, T2 rosbridge port arithmetic, T3 lifecycle_manager
  # bond_timeout + node_names, T4 TimerAction sim startup stagger,
  # T5 wrapper passthrough, T6 real-hw mode unchanged). Pure-XML
  # structural verification over the installed share/launch dir.
  ament_add_gtest(test_multi_robot_sim
    test/test_multi_robot_sim.cpp)
  ament_target_dependencies(test_multi_robot_sim
    ament_index_cpp)
  target_link_libraries(test_multi_robot_sim tinyxml2::tinyxml2)
endif()

ament_package()
