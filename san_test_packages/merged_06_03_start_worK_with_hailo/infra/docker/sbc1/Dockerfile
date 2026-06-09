# SAN v1.3 PHASE 4 — Hub UGV SBC #1 base image.
#
# Layered build: ROS2 Humble base -> san_lidar + san_costmap + san_slam
# + san_hub_slam. Built as multi-stage so the final image ships only
# the install/ tree.

ARG ROS_DISTRO=humble
FROM ros:${ROS_DISTRO}-ros-base AS builder

# ---- System deps ----
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        git \
        pkg-config \
        libpcl-dev \
        libopencv-dev \
        ros-${ROS_DISTRO}-pcl-conversions \
        ros-${ROS_DISTRO}-nav-msgs \
        ros-${ROS_DISTRO}-sensor-msgs \
        ros-${ROS_DISTRO}-rmw-cyclonedds-cpp \
        ros-${ROS_DISTRO}-slam-toolbox \
    && rm -rf /var/lib/apt/lists/*

# Optional g2o for pose-graph optimization.
RUN apt-get update && apt-get install -y --no-install-recommends \
        ros-${ROS_DISTRO}-libg2o \
    && rm -rf /var/lib/apt/lists/* \
    || echo "libg2o optional - continuing without"

# ---- SAN sources ----
WORKDIR /workspace
COPY ros/src/skyautonet ./src/skyautonet

# ---- Build ----
SHELL ["/bin/bash", "-lc"]
RUN source /opt/ros/${ROS_DISTRO}/setup.bash \
    && colcon build --symlink-install \
        --packages-select \
            combat_robot_msgs \
            san_lidar \
            san_costmap \
            san_slam \
            san_hub_slam \
            swarm_coordinator

# ============================================================
# Runtime stage
# ============================================================
FROM ros:${ROS_DISTRO}-ros-base AS runtime

RUN apt-get update && apt-get install -y --no-install-recommends \
        libpcl-dev \
        libopencv-dev \
        ros-${ROS_DISTRO}-pcl-conversions \
        ros-${ROS_DISTRO}-nav-msgs \
        ros-${ROS_DISTRO}-sensor-msgs \
        ros-${ROS_DISTRO}-rmw-cyclonedds-cpp \
        ros-${ROS_DISTRO}-slam-toolbox \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /workspace/install /workspace/install
WORKDIR /workspace

ENV RMW_IMPLEMENTATION=rmw_cyclonedds_cpp
ENV ROS_DOMAIN_ID=42

# Default command can be overridden by docker-compose per service.
CMD ["bash", "-lc", "source /opt/ros/humble/setup.bash && source /workspace/install/setup.bash && ros2 launch san_hub_slam san_hub_slam.launch.xml"]
