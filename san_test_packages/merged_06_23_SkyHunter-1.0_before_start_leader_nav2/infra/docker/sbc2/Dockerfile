# SAN v1.3 PHASE 4 — Hub UGV SBC #2 base image.
#
# Layered build: ROS2 Jazzy base -> san_hub_comm + san_lte_redundancy
# + san_operation_control + san_role_management.

ARG ROS_DISTRO=jazzy
FROM ros:${ROS_DISTRO}-ros-base AS builder

# ---- System deps ----
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        git \
        pkg-config \
        libuci-dev \
        libubox-dev \
        libubus-dev \
        libgstreamer1.0-dev \
        libgstreamer-plugins-base1.0-dev \
        gstreamer1.0-plugins-base \
        gstreamer1.0-plugins-good \
        gstreamer1.0-plugins-bad \
        gstreamer1.0-plugins-ugly \
        gstreamer1.0-libav \
        ros-${ROS_DISTRO}-rmw-cyclonedds-cpp \
    && rm -rf /var/lib/apt/lists/*

# ---- SAN sources ----
WORKDIR /workspace
COPY ros/src/skyautonet ./src/skyautonet

# ---- Build ----
SHELL ["/bin/bash", "-lc"]
RUN source /opt/ros/${ROS_DISTRO}/setup.bash \
    && colcon build --symlink-install \
        --packages-select \
            combat_robot_msgs \
            san_hub_comm \
            san_video_sender \
            san_lte_redundancy \
            san_operation_control \
            san_role_management \
            swarm_coordinator

# ============================================================
# Runtime stage
# ============================================================
FROM ros:${ROS_DISTRO}-ros-base AS runtime

RUN apt-get update && apt-get install -y --no-install-recommends \
        libuci20 \
        libubox20 \
        libubus20 \
        gstreamer1.0-plugins-base \
        gstreamer1.0-plugins-good \
        gstreamer1.0-plugins-bad \
        gstreamer1.0-plugins-ugly \
        gstreamer1.0-libav \
        ros-${ROS_DISTRO}-rmw-cyclonedds-cpp \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /workspace/install /workspace/install
WORKDIR /workspace

ENV RMW_IMPLEMENTATION=rmw_cyclonedds_cpp
ENV ROS_DOMAIN_ID=42

CMD ["bash", "-lc", "source /opt/ros/jazzy/setup.bash && source /workspace/install/setup.bash && ros2 launch san_hub_comm gstreamer_relay.launch.xml"]
