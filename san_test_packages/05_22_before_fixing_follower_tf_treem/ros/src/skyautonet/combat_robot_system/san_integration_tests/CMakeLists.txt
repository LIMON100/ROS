cmake_minimum_required(VERSION 3.16)
# CMake needs a language enabled so launch_testing_ament_cmake's
# transitive find_package(PythonLibs) (via FindPythonExtra) can detect
# the Python toolchain. With `LANGUAGES NONE` it silently fails with
# "Could NOT find PythonLibs (missing: PYTHON_LIBRARIES
# PYTHON_INCLUDE_DIRS)" regardless of whether libpython3-dev is
# installed — see #142 / #145 retrospective. Default `C CXX` is fine;
# there are no compile targets in this package so the extra language
# costs nothing.
project(san_integration_tests)

find_package(ament_cmake REQUIRED)

if(BUILD_TESTING)
  find_package(ament_cmake_pytest REQUIRED)
  find_package(launch_testing_ament_cmake REQUIRED)

  # Each TST S20 scenario as a separate launch_test target so CI
  # reports them independently (helpful for PDR matrix).
  add_launch_test(test/test_s20_1_squadron_stability.py
    TIMEOUT 120)
  add_launch_test(test/test_s20_2_leader_failover.py
    TIMEOUT 60)
  add_launch_test(test/test_s20_3_rtcm_chain.py
    TIMEOUT 60)
  add_launch_test(test/test_s20_4_fire_auth.py
    TIMEOUT 30)
  add_launch_test(test/test_s20_5_comm_link_failover.py
    TIMEOUT 30)
  add_launch_test(test/test_s20_6_e2e_latency.py
    TIMEOUT 120)

  # PDR-6 additions — SDD §6.1 BT Fallback root + KPP-2 E2E + T4 recovery.
  add_launch_test(test/test_s20_7_mission_bt_boot.py
    TIMEOUT 30)
  # PDR-7a — actual BT priority injection (B1 wiring enables this).
  add_launch_test(test/test_s20_7b_bt_priority_injection.py
    TIMEOUT 30)
  add_launch_test(test/test_s20_8_kpp2_e2e.py
    TIMEOUT 60)
  add_launch_test(test/test_s20_9_t4_recovery.py
    TIMEOUT 30)
endif()

install(DIRECTORY test scenarios DESTINATION share/${PROJECT_NAME})

ament_package()
