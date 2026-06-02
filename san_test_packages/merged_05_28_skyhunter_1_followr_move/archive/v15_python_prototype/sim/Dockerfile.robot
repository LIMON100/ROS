# Robot stack image -- runs main.py + all P1+P2 stories.
# Base: ROS 2 Humble + Python 3.10 + (optional) RKNN runtime libs.

FROM ros:humble-ros-base-jammy

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    python3-pip \
    python3-yaml \
    python3-numpy \
    python3-scipy \
    python3-opencv \
    linuxptp \
    chrony \
    iperf3 \
    osmium-tool \
    curl \
    && rm -rf /var/lib/apt/lists/*

# RKNN runtime is shipped with the RK3588J vendor BSP; in sim we run
# without it. The vendor wheel is dropped under sim/rknn_toolkit_lite/
# at deploy time. The COPY is intentionally absent from the source tree
# so `docker build` doesn't fail on a missing directory — if the wheel
# is present, mount it via volume:
#   docker run -v /path/to/rknn_toolkit_lite:/opt/rknn_toolkit_lite ...
# and the install_rknn.sh entrypoint will pick it up.

WORKDIR /opt/patrol
COPY requirements.txt /opt/patrol/requirements.txt
RUN pip install --break-system-packages -r /opt/patrol/requirements.txt

COPY . /opt/patrol/

HEALTHCHECK --interval=30s --timeout=5s \
    CMD curl -f http://localhost:5001/health || exit 1

CMD ["python3", "main.py"]
