"""Helpers for per-robot namespaced sim bringup.

Multi-robot nav2 in a single DDS domain needs every frame and absolute topic in
the nav2 / ekf configs prefixed per robot (sN/base_footprint, /sN/rslidar_points,
…). nav2_common.launch.RewrittenYaml rewrites *by leaf key name* across the whole
file, which cannot express "global_frame is map here but odom there". So we load
the YAMLs, walk them node-by-node, apply a frame map + topic prefix, and dump a
per-robot temp file. Frames use the bare prefix "sN/"; absolute topics use "/sN".
"""
import os
import tempfile

import yaml


def _prefix_frame(value, ns):
    # "map" -> "s1/map", "base_footprint" -> "s1/base_footprint"
    if not isinstance(value, str) or value == "" or "/" in value:
        return value
    return f"{ns}/{value}"


def _prefix_topic(value, ns):
    # absolute topic "/rslidar_points" -> "/s1/rslidar_points"; leave relative as-is
    if isinstance(value, str) and value.startswith("/"):
        return f"/{ns}{value}"
    return value


# Parameter leaf keys that hold a TF frame id.
_FRAME_KEYS = {
    "global_frame", "robot_base_frame", "base_frame", "fixed_frame",
    "base_frame_id", "odom_frame_id", "map_frame", "odom_frame",
    "base_link_frame", "world_frame", "robot_base_frame_id",
}
# Parameter leaf keys that hold an (absolute) topic name.
_TOPIC_KEYS = {
    "topic", "pointcloud_topic", "scan_topic", "odom_topic",
    "input_topic", "output_topic", "odom0", "imu0", "imu1",
    "footprint_topic",
}


def _walk(node, ns):
    if isinstance(node, dict):
        for k, v in list(node.items()):
            if isinstance(v, (dict, list)):
                _walk(v, ns)
            elif k in _FRAME_KEYS:
                node[k] = _prefix_frame(v, ns)
            elif k in _TOPIC_KEYS:
                node[k] = _prefix_topic(v, ns)
    elif isinstance(node, list):
        for item in node:
            _walk(item, ns)


def rewrite_yaml_for_namespace(src_path, ns, extra_overrides=None, nest_under_ns=False):
    """Return a temp YAML path with frames/topics prefixed for namespace `ns`.

    extra_overrides: optional {leaf_key: value} forced on every matching node.
    nest_under_ns: wrap the whole file under a top-level `ns:` key so that nodes
        launched inside PushRosNamespace(ns) match the params. Use for configs we
        launch directly (ekf); leave False for nav2 (navigation_launch nests it).
    """
    with open(src_path, "r") as f:
        data = yaml.safe_load(f)

    _walk(data, ns)

    if extra_overrides:
        def _apply(node):
            if isinstance(node, dict):
                for k, v in list(node.items()):
                    if isinstance(v, (dict, list)):
                        _apply(v)
                    elif k in extra_overrides:
                        node[k] = extra_overrides[k]
            elif isinstance(node, list):
                for item in node:
                    _apply(item)
        _apply(data)

    if nest_under_ns:
        data = {ns: data}

    fd, out_path = tempfile.mkstemp(prefix=f"{ns}_", suffix=os.path.basename(src_path))
    with os.fdopen(fd, "w") as f:
        yaml.safe_dump(data, f, default_flow_style=False)
    return out_path
