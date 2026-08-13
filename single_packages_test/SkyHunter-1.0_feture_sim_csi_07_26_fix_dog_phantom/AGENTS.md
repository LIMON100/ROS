# Repository Guidelines

## Project Structure & Module Organization
This repository is organized around a ROS2 workspace in `ros/`.
- `ros/src/skyautonet/combat_robot_launch`: top-level launch files (system entry points).
- `ros/src/skyautonet/combat_robot_system`: core runtime packages (`human_detector`, `pan_tilt_controller`, `robot_server`, `combat_robot_operation_system`, etc.).
- `ros/src/skyautonet/combat_robot_visualization/display`: ImGui-based visualization and assets.
- `scripts/`: system setup and operations scripts (drivers, CAN, trigger, RTSP, environment setup).
- `test/`: utility and hardware-oriented test scripts (`*.py`, `*.sh`).
- `docs/coding_standards/`: mandatory coding and safety guidance.

## Repository Boundaries
- `d:\repo\combatrobot_1` is the main git repository for the ROS2 workspace and related docs/scripts.
- `combatrobotcontroller/` is a separate nested git repository with its own `.git` directory and history.
- When checking `git status`, `git diff`, or `git log`, run the command in the correct repository context. Do not assume changes under `combatrobotcontroller/` belong to the top-level repository history.

## Environment Constraints
- This folder is edited from a Windows PC while modifying Linux-targeted code.
- Linux builds, ROS runtime validation, and hardware-dependent execution are not available in this Windows environment.
- Do not claim to have built or run Linux-target code locally from this machine unless that was explicitly done on a proper Linux target.
- Use Windows commands and tools for inspection and editing in this workspace, primarily PowerShell-based commands.

## Build, Test, and Development Commands
Run from repository root unless noted.
- `cd ros && rosdep install --from-paths src --ignore-src -r -y`: install ROS package dependencies.
- `cd ros && colcon build --symlink-install --cmake-args -DCMAKE_BUILD_TYPE=Release`: build all ROS packages.
- `source ros/install/setup.bash && ros2 launch combat_robot_launch combat_robot.launch.xml`: launch the integrated system.
- `cd ros && colcon test && colcon test-result --verbose`: run package tests and print detailed results.
- `./scripts/install_hailo_driver.sh`: install Hailo runtime/driver prerequisites on target hardware.

## Coding Style & Naming Conventions
Follow `docs/coding_standards/codingstandards-style.md` and `codingstandards-performance-safety.md`.
- C++: use `.cpp`/`.hpp`, 2-space indentation, no tabs.
- Naming: types `PascalCase`, functions/variables `lowerCamelCase`, constants `ALL_CAPS`.
- Prefixes used in this codebase: private members `m_`, parameters `t_`.
- Prefer `nullptr`, `const`, RAII/smart pointers, scoped variables, and explicit braces `{}` for control blocks.
- Avoid `using` in header files and avoid global mutable state.

## Testing Guidelines
- Unit tests are present in package-level `test/` directories (for example, `combat_robot_operation_system/test/test_fsm_transitions.cpp` with GoogleTest).
- Python packages may declare `pytest` dependencies (for example, `imu_publisher`).
- Name tests by behavior (`test_<feature>_<scenario>.cpp` or `.py`) and keep fixtures deterministic.
- For hardware-dependent checks, place scripts in root `test/` and document required devices in script headers.

## Commit & Pull Request Guidelines
- Follow Conventional Commit style seen in history: `feat:`, `fix:`, `test:`, `build:`, `docs:`.
- Keep commits focused by package or subsystem (for example, only `pan_tilt_controller` changes in one commit).
- PRs should include:
  - purpose and impacted ROS packages,
  - linked issue/ticket,
  - test evidence (`colcon test` output or hardware validation notes),
  - screenshots/log snippets for visualization, RTSP, or UI-related changes.
