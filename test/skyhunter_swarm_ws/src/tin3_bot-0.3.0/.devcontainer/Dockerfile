# ROS 2 Humble + Gazebo Harmonic Development Container
FROM osrf/ros:humble-desktop-full

# Avoid prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Set up locale
RUN apt-get update && apt-get install -y locales \
    && locale-gen en_US en_US.UTF-8 \
    && update-locale LC_ALL=en_US.UTF-8 LANG=en_US.UTF-8
ENV LANG=en_US.UTF-8

# Install common development tools
RUN apt-get update && apt-get install -y \
    git \
    curl \
    wget \
    vim \
    nano \
    htop \
    tmux \
    python3-pip \
    python3-colcon-common-extensions \
    python3-rosdep \
    python3-vcstool \
    && rm -rf /var/lib/apt/lists/*

# Install Gazebo Harmonic
RUN curl -sSL https://packages.osrfoundation.org/gazebo.gpg -o /usr/share/keyrings/pkgs-osrf-archive-keyring.gpg \
    && echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/pkgs-osrf-archive-keyring.gpg] http://packages.osrfoundation.org/gazebo/ubuntu-stable $(lsb_release -cs) main" | tee /etc/apt/sources.list.d/gazebo-stable.list > /dev/null \
    && apt-get update && apt-get install -y \
    gz-harmonic \
    && rm -rf /var/lib/apt/lists/*

# Install ROS-Gazebo bridge packages for Humble + Harmonic
RUN apt-get update && apt-get install -y \
    ros-humble-ros-gz \
    ros-humble-ros-gz-bridge \
    ros-humble-ros-gz-sim \
    ros-humble-ros-gz-image \
    && rm -rf /var/lib/apt/lists/*

# Install robot dependencies (common packages)
RUN apt-get update && apt-get install -y \
    ros-humble-robot-state-publisher \
    ros-humble-joint-state-publisher \
    ros-humble-joint-state-publisher-gui \
    ros-humble-xacro \
    ros-humble-robot-localization \
    ros-humble-navigation2 \
    ros-humble-nav2-bringup \
    ros-humble-slam-toolbox \
    ros-humble-tf2-ros \
    ros-humble-tf2-tools \
    ros-humble-rviz2 \
    ros-humble-rqt* \
    ros-humble-plotjuggler-ros \
    ros-humble-gps-msgs \
    && rm -rf /var/lib/apt/lists/*

# Create workspace directory (generic path)
RUN mkdir -p /ros2_ws/src

# Set up ROS environment
RUN echo "source /opt/ros/humble/setup.bash" >> /etc/bash.bashrc \
    && echo "export GZ_VERSION=harmonic" >> /etc/bash.bashrc

# Set working directory
WORKDIR /ros2_ws

# Set environment variables
ENV ROS_DISTRO=humble
ENV GZ_VERSION=harmonic

CMD ["/bin/bash"]
