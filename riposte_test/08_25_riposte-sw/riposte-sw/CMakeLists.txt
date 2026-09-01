cmake_minimum_required(VERSION 3.16)
project(riposte_sw LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-Wall -Wextra -Werror)

# ---- Sanitizers (G11.3): run before hardware, gate multithreaded changes ----
# -DRIPOSTE_SANITIZE=address,undefined  (ASan+UBSan; the common dev pass)
# -DRIPOSTE_SANITIZE=thread             (TSan; required clean for MT changes)
# ASan and TSan are mutually exclusive; keep them in separate build dirs.
set(RIPOSTE_SANITIZE "" CACHE STRING
    "Comma-separated -fsanitize list (e.g. address,undefined or thread)")
if(RIPOSTE_SANITIZE)
  add_compile_options(-fsanitize=${RIPOSTE_SANITIZE} -fno-omit-frame-pointer -g)
  add_link_options(-fsanitize=${RIPOSTE_SANITIZE})
  if(RIPOSTE_SANITIZE MATCHES "thread")
    # TSan cannot instrument std::atomic_thread_fence; g++ warns and -Werror
    # would block the build. The fences are the SeqSlot seqlock's memory-order
    # barriers (G6.3); demote just this warning. The payload itself is atomic
    # words (COMMON-SDD C-5), so TSan runs with NO suppressions — any race it
    # reports anywhere, SeqSlot included, is a real defect.
    add_compile_options(-Wno-error=tsan)
  endif()
endif()

# ---- Hardware-dependent components (OFF => SIL build on a dev PC) ----
option(RIPOSTE_WITH_HAILO   "Build HailoRT detector"        OFF)
option(RIPOSTE_WITH_RGA     "RK3588 RGA model-input preprocessing" OFF)
option(RIPOSTE_WITH_RKNN    "RK3588 NPU ReID embedder (T1)" OFF)
option(RIPOSTE_WITH_MAVSDK  "Build MAVSDK FCU link"         OFF)
option(RIPOSTE_WITH_V4L2    "Build V4L2 camera"             OFF)
option(RIPOSTE_WITH_SYSTEMD "Use sd_notify watchdog"        OFF)
option(RIPOSTE_WITH_GZ      "Build Gazebo SITL track bridge (test)" OFF)

# ---- common (header-only interface library) ----
add_library(riposte_common INTERFACE)
target_include_directories(riposte_common INTERFACE common/include)
target_link_libraries(riposte_common INTERFACE pthread rt)

find_package(Threads REQUIRED)

# =========================== riposte-seeker ===========================
add_executable(riposte-seeker
  seeker/src/main.cpp
  seeker/src/CameraIngest.cpp
  seeker/src/SearchScheduler.cpp
  seeker/src/ModelIo.cpp
  seeker/src/Preproc.cpp
  seeker/src/ChannelMap.cpp
  seeker/src/MotionCandidates.cpp
  seeker/src/SeekerConfig.cpp
  seeker/src/AssocCost.cpp
  seeker/src/TrackFusion.cpp
  seeker/src/Tracker.cpp
  seeker/src/TargetEstimator.cpp
  seeker/src/VideoRecorder.cpp
  seeker/src/RecordWorker.cpp
  seeker/src/EmbedWorker.cpp)
target_include_directories(riposte-seeker PRIVATE seeker/src)
target_link_libraries(riposte-seeker PRIVATE riposte_common Threads::Threads)

if(RIPOSTE_WITH_HAILO)
  target_sources(riposte-seeker PRIVATE seeker/src/HailoDetector.cpp)
  target_compile_definitions(riposte-seeker PRIVATE RIPOSTE_WITH_HAILO)
  # HailoRT ships as a plain .so + headers (no CMake package on the target BSP).
  find_path(HAILORT_INCLUDE_DIR hailo/hailort.hpp REQUIRED)
  find_library(HAILORT_LIBRARY hailort REQUIRED)
  target_include_directories(riposte-seeker PRIVATE ${HAILORT_INCLUDE_DIR})
  target_link_libraries(riposte-seeker PRIVATE ${HAILORT_LIBRARY})
endif()
if(RIPOSTE_WITH_RGA)
  target_sources(riposte-seeker PRIVATE seeker/src/PreprocRga.cpp)
  target_compile_definitions(riposte-seeker PRIVATE RIPOSTE_WITH_RGA)
  find_path(RGA_INCLUDE_DIR rga/im2d.h REQUIRED)
  find_library(RGA_LIBRARY rga REQUIRED)
  target_include_directories(riposte-seeker PRIVATE ${RGA_INCLUDE_DIR})
  target_link_libraries(riposte-seeker PRIVATE ${RGA_LIBRARY})
endif()
if(RIPOSTE_WITH_RKNN)
  target_sources(riposte-seeker PRIVATE seeker/src/RknnEmbedder.cpp)
  target_compile_definitions(riposte-seeker PRIVATE RIPOSTE_WITH_RKNN)
  # librknnrt ships as a plain .so + rknn_api.h on the RK3588 BSP.
  find_path(RKNN_INCLUDE_DIR rknn_api.h REQUIRED)
  find_library(RKNN_LIBRARY rknnrt REQUIRED)
  target_include_directories(riposte-seeker PRIVATE ${RKNN_INCLUDE_DIR})
  target_link_libraries(riposte-seeker PRIVATE ${RKNN_LIBRARY})
endif()
if(RIPOSTE_WITH_V4L2)
  target_compile_definitions(riposte-seeker PRIVATE RIPOSTE_WITH_V4L2)
endif()

# ============================= riposte-obc ============================
add_executable(riposte-obc
  obc/src/main.cpp
  obc/src/OffboardController.cpp
  obc/src/SetpointStreamer.cpp
  obc/src/SafetyMonitor.cpp
  obc/src/Geofence.cpp
  obc/src/Rendezvous.cpp
  obc/src/FcuLink.cpp
  obc/src/TargetEkf.cpp
  obc/src/TargetImm.cpp
  obc/src/sources/TestPatternSource.cpp
  obc/src/sources/GuidanceSource.cpp
  obc/src/sources/MissionSource.cpp
  obc/src/sources/BalloonPatrolSource.cpp
  obc/src/sources/AttitudeTrackingSource.cpp)
target_include_directories(riposte-obc PRIVATE obc/src)
target_link_libraries(riposte-obc PRIVATE riposte_common Threads::Threads)

if(RIPOSTE_WITH_MAVSDK)
  # v3.17.1 기준선 고정. v2 -> v3 에서 Telemetry::Battery::remaining_percent 의
  # 단위가 0~1 비율에서 0~100 퍼센트로 바뀌는 등 조용한 의미 변화가 있으므로,
  # 버전을 바꾸면 FcuLink 의 정규화 경계와 SITL 통합시험을 다시 검증해야 한다.
  find_package(MAVSDK 3.17.1 REQUIRED)
  target_compile_definitions(riposte-obc PRIVATE RIPOSTE_WITH_MAVSDK)
  target_link_libraries(riposte-obc PRIVATE MAVSDK::mavsdk)
endif()

# ========================= riposte-supervisor ========================
add_executable(riposte-supervisor supervisor/src/main.cpp)
target_link_libraries(riposte-supervisor PRIVATE riposte_common Threads::Threads)
if(RIPOSTE_WITH_SYSTEMD)
  target_compile_definitions(riposte-supervisor PRIVATE RIPOSTE_WITH_SYSTEMD)
  target_link_libraries(riposte-supervisor PRIVATE systemd)
endif()

# ============ Gazebo SITL track bridge (test harness) ===============
# Stands in for riposte-seeker: feeds Gazebo target truth into the TrackBus so
# the guidance/safety loop can chase a physically-simulated target. Needs the
# gz-transport/gz-msgs/gz-math dev packages; off by default so the gate is clean.
if(RIPOSTE_WITH_GZ)
  find_package(gz-transport13 REQUIRED)
  find_package(gz-msgs10 REQUIRED)
  find_package(gz-math7 REQUIRED)
  add_executable(gz_track_bridge tools/gz_track_bridge.cpp)
  target_link_libraries(gz_track_bridge PRIVATE riposte_common
    gz-transport13::gz-transport13 gz-msgs10::gz-msgs10 gz-math7::gz-math7)
endif()

# ============================== tests ================================
enable_testing()
add_subdirectory(test)
