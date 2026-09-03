# Dependency-free unit tests (no gtest requirement for the POC bring-up).
# Also drives the OffboardController FSM: FcuLink.cpp is deliberately NOT
# linked — the test defines a link-seam FcuLink double with scripted telemetry.
add_executable(test_safety_fsm test_safety_fsm.cpp
  ${CMAKE_SOURCE_DIR}/obc/src/SafetyMonitor.cpp
  ${CMAKE_SOURCE_DIR}/obc/src/Geofence.cpp
  ${CMAKE_SOURCE_DIR}/obc/src/OffboardController.cpp
  ${CMAKE_SOURCE_DIR}/obc/src/FcuLink.cpp)
target_include_directories(test_safety_fsm PRIVATE ${CMAKE_SOURCE_DIR}/obc/src)
target_link_libraries(test_safety_fsm PRIVATE riposte_common)
add_test(NAME safety_fsm COMMAND test_safety_fsm)

add_executable(test_seqslot test_seqslot.cpp)
target_link_libraries(test_seqslot PRIVATE riposte_common)
add_test(NAME seqslot COMMAND test_seqslot)

# Seeker perception logic (Tracker gating/alpha-beta + TargetEstimator geometry)
# driven through a mock IDetector — HAL-mock host test, no camera/NPU (G17.13).
add_executable(test_seeker test_seeker.cpp
  ${CMAKE_SOURCE_DIR}/seeker/src/Tracker.cpp
  ${CMAKE_SOURCE_DIR}/seeker/src/TargetEstimator.cpp
  ${CMAKE_SOURCE_DIR}/seeker/src/AssocCost.cpp)
target_include_directories(test_seeker PRIVATE ${CMAKE_SOURCE_DIR}/seeker/src)
target_link_libraries(test_seeker PRIVATE riposte_common)
add_test(NAME seeker COMMAND test_seeker)

# Search policy: 3x3 tile sweep (R-5), 4-of-5 confirmation window (R-6), periodic
# wide re-detect (R-7), plus the NV12 ROI crop + coordinate remap. Pure logic.
add_executable(test_search test_search.cpp
  ${CMAKE_SOURCE_DIR}/seeker/src/SearchScheduler.cpp)
target_include_directories(test_search PRIVATE ${CMAKE_SOURCE_DIR}/seeker/src)
target_link_libraries(test_search PRIVATE riposte_common)
add_test(NAME search COMMAND test_search)

# Model I/O: letterbox transform (aspect preservation + padding rejection), NMS,
# YOLOv8 head decode. The detector maths that lives OUTSIDE HailoRT, so it is
# host-testable without the accelerator.
add_executable(test_modelio test_modelio.cpp
  ${CMAKE_SOURCE_DIR}/seeker/src/ModelIo.cpp)
target_include_directories(test_modelio PRIVATE ${CMAKE_SOURCE_DIR}/seeker/src)
target_link_libraries(test_modelio PRIVATE riposte_common)
add_test(NAME modelio COMMAND test_modelio)

# NMS-by-class output parser (S-7 primary path): the device buffer is untrusted
# data, so the framing/validation logic is what these tests pin down. Links
# ModelIo for the parse -> letterbox_undo round trip.
add_executable(test_hailoparse test_hailoparse.cpp
  ${CMAKE_SOURCE_DIR}/seeker/src/ModelIo.cpp)
target_include_directories(test_hailoparse PRIVATE ${CMAKE_SOURCE_DIR}/seeker/src)
target_link_libraries(test_hailoparse PRIVATE riposte_common)
add_test(NAME hailoparse COMMAND test_hailoparse)

# CPU letterbox preprocessing (NV12 -> RGB888): the accuracy REFERENCE the RGA
# production path is compared against at bring-up (SDD §4.4.3/S-9).
add_executable(test_preproc test_preproc.cpp
  ${CMAKE_SOURCE_DIR}/seeker/src/Preproc.cpp
  ${CMAKE_SOURCE_DIR}/seeker/src/ModelIo.cpp)
target_include_directories(test_preproc PRIVATE ${CMAKE_SOURCE_DIR}/seeker/src)
target_link_libraries(test_preproc PRIVATE riposte_common)
add_test(NAME preproc COMMAND test_preproc)

# T1 ReID association cost fusion (TRACKER-REQ TR-B, host half): motion gate +
# appearance distance + greedy matching. Pure maths, no NPU — the RKNN embedder
# lands with TR-A and only ever feeds this the same plain float vectors.
add_executable(test_assoc test_assoc.cpp
  ${CMAKE_SOURCE_DIR}/seeker/src/AssocCost.cpp)
target_include_directories(test_assoc PRIVATE ${CMAKE_SOURCE_DIR}/seeker/src)
target_link_libraries(test_assoc PRIVATE riposte_common)
add_test(NAME assoc COMMAND test_assoc)

# T2 template fusion policy (TRACKER-REQ TR-C, host half): detection-authoritative
# fusion, visual coast, drift cross-check, motion-coast fallback. Driven through
# the SyntheticTemplateTracker — pure policy, no NPU.
add_executable(test_fusion test_fusion.cpp
  ${CMAKE_SOURCE_DIR}/seeker/src/TrackFusion.cpp
  ${CMAKE_SOURCE_DIR}/seeker/src/Tracker.cpp
  ${CMAKE_SOURCE_DIR}/seeker/src/AssocCost.cpp)
target_include_directories(test_fusion PRIVATE ${CMAKE_SOURCE_DIR}/seeker/src)
target_link_libraries(test_fusion PRIVATE riposte_common)
add_test(NAME fusion COMMAND test_fusion)

# Moving-target EKF (ESTIMATION-REQ EST-P1): constant-velocity absolute-NED
# filter with anisotropic range covariance and own-motion separation. It runs in
# the OBC (only the OBC holds own-vehicle telemetry — the seeker never touches
# the FC, I2), so it lives in obc/src. Pure maths, no camera/FC in the test.
add_executable(test_ekf test_ekf.cpp
  ${CMAKE_SOURCE_DIR}/obc/src/TargetEkf.cpp)
target_include_directories(test_ekf PRIVATE ${CMAKE_SOURCE_DIR}/obc/src)
target_link_libraries(test_ekf PRIVATE riposte_common)
add_test(NAME ekf COMMAND test_ekf)

# IMM target estimator (ESTIMATION-REQ EST-P3): two constant-acceleration
# models blended by likelihood — smooth on straight legs, agile in maneuvers.
# Pure maths driven by synthetic trajectories, no camera/FC.
add_executable(test_imm test_imm.cpp
  ${CMAKE_SOURCE_DIR}/obc/src/TargetImm.cpp
  ${CMAKE_SOURCE_DIR}/obc/src/TargetEkf.cpp)
target_include_directories(test_imm PRIVATE ${CMAKE_SOURCE_DIR}/obc/src)
target_link_libraries(test_imm PRIVATE riposte_common)
add_test(NAME imm COMMAND test_imm)

# Augmented-state size/range EKF (ESTIMATION-REQ §5.5 / EST-P7a): joint
# size-range estimation, observability under own-stationary vs own-weaving.
# Pure maths driven by synthetic bearing+size observations, no camera/FC.
add_executable(test_sizeekf test_sizeekf.cpp
  ${CMAKE_SOURCE_DIR}/obc/src/SizeRangeEkf.cpp)
target_include_directories(test_sizeekf PRIVATE ${CMAKE_SOURCE_DIR}/obc/src)
target_link_libraries(test_sizeekf PRIVATE riposte_common)
add_test(NAME sizeekf COMMAND test_sizeekf)

# Seeker config range validation (AGENTS §7.9): reject bad config at startup.
# Pure logic, no INI/shm.
add_executable(test_seekerconfig test_seekerconfig.cpp
  ${CMAKE_SOURCE_DIR}/seeker/src/SeekerConfig.cpp)
target_include_directories(test_seekerconfig PRIVATE ${CMAKE_SOURCE_DIR}/seeker/src)
target_link_libraries(test_seekerconfig PRIVATE riposte_common)
add_test(NAME seekerconfig COMMAND test_seekerconfig)

# Wide-channel motion candidate generation (DUALEO R-13): ego-motion estimation
# (FAST corners + block matching + RANSAC similarity), residual extraction and
# small-blob candidates. Pure logic, no camera/NPU/OpenCV — the scenes are
# synthesized with a KNOWN background transform so the estimate can be pinned.
add_executable(test_motion test_motion.cpp
  ${CMAKE_SOURCE_DIR}/seeker/src/MotionCandidates.cpp)
target_include_directories(test_motion PRIVATE ${CMAKE_SOURCE_DIR}/seeker/src)
target_link_libraries(test_motion PRIVATE riposte_common)
add_test(NAME motion COMMAND test_motion)

# Wide<->narrow EO channel coordinate mapping (DUALEO-REQ §3.6 / P4): the
# nested-coaxial dual-EO handoff geometry. Pure maths, no camera.
add_executable(test_channelmap test_channelmap.cpp
  ${CMAKE_SOURCE_DIR}/seeker/src/ChannelMap.cpp)
target_include_directories(test_channelmap PRIVATE ${CMAKE_SOURCE_DIR}/seeker/src)
target_link_libraries(test_channelmap PRIVATE riposte_common)
add_test(NAME channelmap COMMAND test_channelmap)

# Lead-rendezvous geometry for the GCS target cue (R-2): aim where the target
# will be, not where it was. Pure geometry, no FC or buses.
add_executable(test_rendezvous test_rendezvous.cpp
  ${CMAKE_SOURCE_DIR}/obc/src/Rendezvous.cpp)
target_include_directories(test_rendezvous PRIVATE ${CMAKE_SOURCE_DIR}/obc/src)
target_link_libraries(test_rendezvous PRIVATE riposte_common)
add_test(NAME rendezvous COMMAND test_rendezvous)

# GuidanceSource PN math + freshness gate, driven through the real compute() with
# TrackState injected into the TrackBus shm (no seeker/FC). Needs rt for shm_*.
add_executable(test_guidance test_guidance.cpp
  ${CMAKE_SOURCE_DIR}/obc/src/sources/GuidanceSource.cpp
  ${CMAKE_SOURCE_DIR}/obc/src/TargetImm.cpp
  ${CMAKE_SOURCE_DIR}/obc/src/TargetEkf.cpp)
target_include_directories(test_guidance PRIVATE ${CMAKE_SOURCE_DIR}/obc/src)
target_link_libraries(test_guidance PRIVATE riposte_common)
add_test(NAME guidance COMMAND test_guidance)

# OperatorAuthorization engage-token gate (D-2/SI-2: no autonomous engage path,
# default-deny when unconfigured). Header-only logic; no FC/socket.
add_executable(test_authz test_authz.cpp)
target_include_directories(test_authz PRIVATE ${CMAKE_SOURCE_DIR}/obc/src)
target_link_libraries(test_authz PRIVATE riposte_common)
add_test(NAME authz COMMAND test_authz)

# Balloon-trial behaviour: Geofence model + SM-10 hard limit + the patrol source
# (search / approach / slow-down / turn-away), driven through the real compute()
# with TrackState injected into the TrackBus shm. Needs rt for shm_*.
# Also hosts the patrol/mission source PARAMETER validation matrices (startup
# gates), which pulls in MissionSource (+ its Rendezvous dependency).
add_executable(test_patrol test_patrol.cpp
  ${CMAKE_SOURCE_DIR}/obc/src/Geofence.cpp
  ${CMAKE_SOURCE_DIR}/obc/src/SafetyMonitor.cpp
  ${CMAKE_SOURCE_DIR}/obc/src/sources/GuidanceSource.cpp
  ${CMAKE_SOURCE_DIR}/obc/src/sources/BalloonPatrolSource.cpp
  ${CMAKE_SOURCE_DIR}/obc/src/sources/MissionSource.cpp
  ${CMAKE_SOURCE_DIR}/obc/src/Rendezvous.cpp
  ${CMAKE_SOURCE_DIR}/obc/src/TargetImm.cpp
  ${CMAKE_SOURCE_DIR}/obc/src/TargetEkf.cpp)
target_include_directories(test_patrol PRIVATE ${CMAKE_SOURCE_DIR}/obc/src)
target_link_libraries(test_patrol PRIVATE riposte_common)
add_test(NAME patrol COMMAND test_patrol)

# V4L2 dequeue boundary predicates (P1-05): pure metadata validation the
# device-facing dqbuf()/fill() consult before touching any buffer. Header-only
# logic — no device, no V4L2 headers needed.
add_executable(test_camera test_camera.cpp)
target_include_directories(test_camera PRIVATE ${CMAKE_SOURCE_DIR}/seeker/src)
target_link_libraries(test_camera PRIVATE riposte_common)
add_test(NAME camera COMMAND test_camera)

# INI Config loader: robust fallback to defaults on missing/malformed values (G3).
add_executable(test_config test_config.cpp)
target_link_libraries(test_config PRIVATE riposte_common)
add_test(NAME config COMMAND test_config)

# VideoRecorder disk-reclaim policy (plan_eviction): oldest-first eviction at the
# 80%/10% thresholds. Pure logic; links VideoRecorder.cpp, no camera/ffmpeg.
add_executable(test_recorder test_recorder.cpp
  ${CMAKE_SOURCE_DIR}/seeker/src/VideoRecorder.cpp)
target_include_directories(test_recorder PRIVATE ${CMAKE_SOURCE_DIR}/seeker/src)
target_link_libraries(test_recorder PRIVATE riposte_common)
add_test(NAME recorder COMMAND test_recorder)

# Attitude (pitch/yaw) control: SM-4 attitude clamp + AttitudeTrackingSource
# steering law (TrackBus shm injected). Links SafetyMonitor + the tracking source.
add_executable(test_attitude test_attitude.cpp
  ${CMAKE_SOURCE_DIR}/obc/src/SafetyMonitor.cpp
  ${CMAKE_SOURCE_DIR}/obc/src/Geofence.cpp
  ${CMAKE_SOURCE_DIR}/obc/src/sources/AttitudeTrackingSource.cpp)
target_include_directories(test_attitude PRIVATE ${CMAKE_SOURCE_DIR}/obc/src)
target_link_libraries(test_attitude PRIVATE riposte_common)
add_test(NAME attitude COMMAND test_attitude)

# Blackbox retention: size-bounded rotation of the supervisor's JSONL log.
add_executable(test_blackbox test_blackbox.cpp)
target_include_directories(test_blackbox PRIVATE ${CMAKE_SOURCE_DIR}/supervisor/src)
add_test(NAME blackbox COMMAND test_blackbox)

# Engage helper CLI (operator console stand-in).
add_executable(riposte-engage ${CMAKE_SOURCE_DIR}/tools/engage_cli.cpp)
target_link_libraries(riposte-engage PRIVATE riposte_common)

# PX4 SITL integration scripts live in test/sitl/ (not ctest targets — they
# require a PX4 build + MAVSDK ON build; see README "PX4 SITL 통합시험"):
#   run_sitl_test.sh        engage lifecycle
#   run_sm_injection.sh     SM-1~8 fault injection
#   run_guidance_tracking.sh perception -> PN tracking + SM-4 clamp

# EmbedWorker deadline isolation (TRACKER-REQ TR-4, SEEKER-SDD S-12/§4.5). Runs
# a real worker thread against embedder doubles that are slow, wedged or
# failing: the perception thread must stay bounded by the deadline, never
# consume a stale result, degrade to motion-only under sustained misses, and
# shut down even with the embedder hung inside the driver call.
add_executable(test_embedworker test_embedworker.cpp
  ${CMAKE_SOURCE_DIR}/seeker/src/EmbedWorker.cpp
  ${CMAKE_SOURCE_DIR}/seeker/src/AssocCost.cpp)
target_include_directories(test_embedworker PRIVATE ${CMAKE_SOURCE_DIR}/seeker/src)
target_link_libraries(test_embedworker PRIVATE riposte_common Threads::Threads)
add_test(NAME embedworker COMMAND test_embedworker)
