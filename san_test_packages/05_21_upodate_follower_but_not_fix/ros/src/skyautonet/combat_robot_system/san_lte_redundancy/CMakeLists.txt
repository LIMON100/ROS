cmake_minimum_required(VERSION 3.16)
project(san_lte_redundancy)

# ─── compile options ──────────────────────────────────────────────
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ─── deps ─────────────────────────────────────────────────────────
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(combat_robot_msgs REQUIRED)
find_package(PkgConfig REQUIRED)

# OpenWRT libraries (libuci/libubox/libubus). Only present on the Limon
# LTE router build environment — on Ubuntu CI / dev boxes they are
# absent, in which case we degrade this package to a no-op so the rest
# of the workspace still builds. The runtime artifacts only matter on
# OpenWRT anyway.
pkg_check_modules(UCI  QUIET libuci)
pkg_check_modules(UBOX QUIET libubox)
pkg_check_modules(UBUS QUIET libubus)

if(NOT UCI_FOUND OR NOT UBOX_FOUND OR NOT UBUS_FOUND)
  message(STATUS
    "san_lte_redundancy: OpenWRT deps (libuci/libubox/libubus) missing — "
    "building STUB package. Full build requires the Limon SDK / "
    "OpenWRT cross-toolchain. Stub provides no-op Mwan3UciController "
    "so downstream packages (san_role_management) still compile + link.")

  # Stub library: provides the same Mwan3UciController symbols that
  # downstream packages link against, but with no-op bodies. Without
  # this on non-OpenWRT environments, every `colcon build` of
  # san_role_management fails — which has been the second root cause
  # of the long-standing "colcon build + test (Humble)" + "TST S20
  # series" CI breakage on every PR.
  add_library(san_lte_redundancy_core SHARED
    src/stub_mwan3_uci_controller.cpp
  )
  target_include_directories(san_lte_redundancy_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  )
  ament_target_dependencies(san_lte_redundancy_core rclcpp)

  # Stub executables. squadron.launch.py references three node names
  # (lte_node, lte_link_quality_node, lte_modem_node); without an
  # installed binary at lib/<package>/<name>, launch_ros raises
  # "libexec directory ... does not exist" and the TST S20-1/2/4
  # launch tests fail before any assertions run. Each stub is the
  # same no-op rclcpp::spin() — the name is what matters for launch
  # resolution.
  foreach(_stub_name lte_node lte_link_quality_node lte_modem_node)
    add_executable(${_stub_name} src/stub_node_main.cpp)
    target_link_libraries(${_stub_name} san_lte_redundancy_core)
    ament_target_dependencies(${_stub_name} rclcpp)
  endforeach()

  install(TARGETS
    san_lte_redundancy_core
    LIBRARY DESTINATION lib
  )
  install(TARGETS
    lte_node lte_link_quality_node lte_modem_node
    RUNTIME DESTINATION lib/${PROJECT_NAME}
  )
  install(DIRECTORY include/ DESTINATION include)
  # Match the non-stub install so launch / config artifacts ship too.
  install(DIRECTORY launch config DESTINATION share/${PROJECT_NAME}
    OPTIONAL)

  ament_export_include_directories(include)
  ament_export_libraries(san_lte_redundancy_core)
  ament_export_dependencies(rclcpp)
  ament_package()
  return()
endif()

# ─── shared library: san_lte_redundancy_core ─────────────────────
add_library(san_lte_redundancy_core SHARED
  src/mwan3_uci_controller.cpp
  src/mwan3_ubus_monitor.cpp
  src/lte_role_manager.cpp
  src/mwan3_init_node.cpp
  src/lte_link_quality_node.cpp
  # Phase 2-E Turn 3 (DCN-2026-002 D-008) — AT-command modem driver
  src/at_response_parser.cpp
  src/lte_modem_node.cpp
  src/stub_at_command.cpp
)
target_include_directories(san_lte_redundancy_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
  ${UCI_INCLUDE_DIRS}
  ${UBOX_INCLUDE_DIRS}
  ${UBUS_INCLUDE_DIRS}
)
target_link_libraries(san_lte_redundancy_core
  ${UCI_LIBRARIES} ${UBOX_LIBRARIES} ${UBUS_LIBRARIES}
)
ament_target_dependencies(san_lte_redundancy_core rclcpp combat_robot_msgs)

# ─── executables ─────────────────────────────────────────────────
add_executable(lte_node src/lte_node.cpp)
target_link_libraries(lte_node san_lte_redundancy_core)
ament_target_dependencies(lte_node rclcpp combat_robot_msgs)

add_executable(mwan3_init src/lte_init_main.cpp)
target_link_libraries(mwan3_init san_lte_redundancy_core)
ament_target_dependencies(mwan3_init rclcpp)

add_executable(lte_link_quality_node src/lte_link_quality_main.cpp)
target_link_libraries(lte_link_quality_node san_lte_redundancy_core)
ament_target_dependencies(lte_link_quality_node rclcpp combat_robot_msgs)

# Phase 2-E Turn 3 — AT-command modem driver (DCN-2026-002 D-008)
add_executable(lte_modem_node src/lte_modem_main.cpp)
target_link_libraries(lte_modem_node san_lte_redundancy_core)
ament_target_dependencies(lte_modem_node rclcpp combat_robot_msgs)

# ─── install ─────────────────────────────────────────────────────
install(TARGETS lte_node mwan3_init lte_link_quality_node lte_modem_node
  RUNTIME DESTINATION lib/${PROJECT_NAME}
)
install(TARGETS san_lte_redundancy_core
  LIBRARY DESTINATION lib
)
install(DIRECTORY include/ DESTINATION include)
install(DIRECTORY launch config DESTINATION share/${PROJECT_NAME})

# ─── tests ───────────────────────────────────────────────────────
if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)
  find_package(ament_lint_auto REQUIRED)

  ament_add_gtest(test_uci_controller test/test_uci_controller.cpp)
  target_link_libraries(test_uci_controller san_lte_redundancy_core)
  ament_target_dependencies(test_uci_controller rclcpp)

  ament_add_gtest(test_ubus_monitor test/test_ubus_monitor.cpp)
  target_link_libraries(test_ubus_monitor san_lte_redundancy_core)
  ament_target_dependencies(test_ubus_monitor rclcpp)

  ament_add_gtest(test_lte_failover test/test_lte_failover.cpp)
  target_link_libraries(test_lte_failover san_lte_redundancy_core)
  ament_target_dependencies(test_lte_failover rclcpp combat_robot_msgs)

  ament_add_gtest(test_split_brain test/test_split_brain.cpp)
  target_link_libraries(test_split_brain san_lte_redundancy_core)
  ament_target_dependencies(test_split_brain rclcpp combat_robot_msgs)

  ament_add_gtest(test_chain_promotion test/test_chain_promotion.cpp)
  target_link_libraries(test_chain_promotion san_lte_redundancy_core)
  ament_target_dependencies(test_chain_promotion rclcpp combat_robot_msgs)

  ament_add_gtest(test_link_quality_grader test/test_link_quality_grader.cpp)
  target_link_libraries(test_link_quality_grader san_lte_redundancy_core)
  ament_target_dependencies(test_link_quality_grader rclcpp combat_robot_msgs)

  # [DCN-2026-006 EXT D-020] hysteresis grader (StatefulLteLinkQualityGrader).
  ament_add_gtest(test_link_quality_grader_hysteresis
                  test/test_link_quality_grader_hysteresis.cpp)
  target_link_libraries(test_link_quality_grader_hysteresis
                        san_lte_redundancy_core)
  ament_target_dependencies(test_link_quality_grader_hysteresis
                            rclcpp combat_robot_msgs)

  # Phase 2-E Turn 3 — AT response parser (no rclcpp dep, pure logic)
  ament_add_gtest(test_at_response_parser test/test_at_response_parser.cpp)
  target_link_libraries(test_at_response_parser san_lte_redundancy_core)
endif()

ament_export_include_directories(include)
ament_export_libraries(san_lte_redundancy_core)
ament_export_dependencies(rclcpp combat_robot_msgs)

ament_package()
