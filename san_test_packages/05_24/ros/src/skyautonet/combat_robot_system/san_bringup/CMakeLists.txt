cmake_minimum_required(VERSION 3.16)
project(san_bringup)

# SAN v1.5 Phase 2-E — squadron bringup.
# DCN-2026-002 D-007 (3-Tier) + D-008 (IPC unification).
#
# No source code — this package only installs launch files, config
# overlays, and the systemd service unit.

find_package(ament_cmake REQUIRED)

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
  ament_lint_auto_find_test_dependencies()

  # [DCN-2026-011 D-034] Smoke test for the 5 robot profile launch
  # wrappers — verifies generate_launch_description() runs without
  # exception. End-to-end node-graph assertions are covered by the
  # integration-tests workflow on a HIL bench.
  find_package(ament_cmake_pytest REQUIRED)
  ament_add_pytest_test(test_launch_wrappers
    test/test_launch_wrappers.py)
endif()

ament_package()
