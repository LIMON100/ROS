"""SAN v1.5.2 — DCN-2026-011 D-034 launch wrapper smoke tests.

Loads each of the five robot profile wrappers and verifies the launch
description constructs without exception. This catches the most
common provisioning regressions (Python import errors, missing
DeclareLaunchArgument, typos in the wrapper-to-squadron parameter
plumbing) without spinning the full ROS 2 graph — which would require
hardware adapters that don't exist on CI hosts.

A full end-to-end smoke (5-second launch + node graph assertion) is
the responsibility of the integration-tests workflow on a HIL bench.
"""

import importlib.util
import unittest
from pathlib import Path


WRAPPER_NAMES = [
    "leader_go2",
    "hub_sbc1",
    "hub_sbc2",
    "deputy",
    "follower",
]

LAUNCH_DIR = Path(__file__).resolve().parent.parent / "launch"


def _load_wrapper(name):
    """Import a wrapper module from its file path without colcon install."""
    path = LAUNCH_DIR / f"{name}.launch.py"
    spec = importlib.util.spec_from_file_location(
        f"san_bringup_wrapper_{name}", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class LaunchWrapperSmoke(unittest.TestCase):

    def test_all_wrappers_present(self):
        """Each wrapper file exists on disk (catches accidental deletion)."""
        for name in WRAPPER_NAMES:
            self.assertTrue(
                (LAUNCH_DIR / f"{name}.launch.py").is_file(),
                f"Missing wrapper: {name}.launch.py")

    def test_all_wrappers_construct_a_launch_description(self):
        """generate_launch_description() runs without exception for each."""
        for name in WRAPPER_NAMES:
            with self.subTest(wrapper=name):
                module = _load_wrapper(name)
                self.assertTrue(
                    hasattr(module, "generate_launch_description"),
                    f"{name}: missing generate_launch_description()")
                ld = module.generate_launch_description()
                self.assertIsNotNone(ld)
                # The LaunchDescription should contain at least the
                # DeclareLaunchArguments + LogInfo + IncludeLaunchDescription
                # actions — i.e. non-trivial action count.
                actions = ld.entities
                self.assertGreaterEqual(
                    len(actions), 3,
                    f"{name}: launch description has only "
                    f"{len(actions)} actions (expected >= 3)")

    def test_wrappers_log_profile_name_on_startup(self):
        """Sanity-check each wrapper emits a LogInfo identifying itself —
        operations staff use the journalctl log to confirm the right
        profile booted."""
        from launch.actions import LogInfo
        for name in WRAPPER_NAMES:
            with self.subTest(wrapper=name):
                module = _load_wrapper(name)
                ld = module.generate_launch_description()
                log_msgs = [
                    a for a in ld.entities if isinstance(a, LogInfo)]
                self.assertTrue(
                    log_msgs,
                    f"{name}: no LogInfo action present — operators "
                    f"rely on journal output to verify provisioning")


if __name__ == "__main__":
    unittest.main()
