"""Two-robot swarm sim: one shared gz world + N namespaced robot stacks.

  s1  robot_id=1  role=leader    spawn (0, 0)
  s2  robot_id=2  role=follower  spawn (0, -spacing)   [follower drives a lateral
                                                         formation offset]

Usage:
  ros2 launch combat_robot_nav2 swarm_sim.launch.py              # 2 robots
  ros2 launch combat_robot_nav2 swarm_sim.launch.py num_robots:=1  # leader only

Bring-up is staggered (gz -> s1 -> s2) so gz/nav2 lifecycles settle in order.
"""
import math
import os
import random as _random
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription


def _scatter(n, box=8.0, min_sep=2.5):
    """n non-overlapping random (x,y) in [-box,box]^2, each >= min_sep apart."""
    pts = []
    tries = 0
    while len(pts) < n and tries < 3000:
        tries += 1
        c = (_random.uniform(-box, box), _random.uniform(-box, box))
        if all(math.hypot(c[0] - p[0], c[1] - p[1]) >= min_sep for p in pts):
            pts.append(c)
    while len(pts) < n:                       # fallback grid if packing failed
        pts.append((min_sep * len(pts), box + min_sep))
    return pts
from launch.actions import (DeclareLaunchArgument, IncludeLaunchDescription,
                            TimerAction, OpaqueFunction)
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def _tf_unify(ns):
    """Make a namespaced robot visible to a single rviz: relay /<ns>/tf(_static) to
    the global /tf(_static), and add an identity map -> <ns>/map (robots share a
    GNSS datum, so their map origins coincide) so all trees hang under one 'map'."""
    return [
        Node(package='topic_tools', executable='relay', name=f'{ns}_tf_relay',
             arguments=[f'/{ns}/tf', '/tf'], output='log'),
        Node(package='topic_tools', executable='relay', name=f'{ns}_tf_static_relay',
             arguments=[f'/{ns}/tf_static', '/tf_static'], output='log'),
        Node(package='tf2_ros', executable='static_transform_publisher',
             name=f'{ns}_map_link',
             arguments=['0', '0', '0', '0', '0', '0', 'map', f'{ns}/map'],
             output='log'),
    ]


def _robot(launch_dir, ns, robot_id, role, x, y, spacing, formation_mode,
           followers='', yaw=0.0):
    return IncludeLaunchDescription(
        PythonLaunchDescriptionSource(os.path.join(launch_dir, 'robot_bringup_sim.launch.py')),
        launch_arguments={
            'robot_ns': ns, 'robot_id': str(robot_id), 'leader_robot_id': '1',
            'role': role, 'x': str(x), 'y': str(y), 'yaw': str(yaw),
            'formation_lateral_spacing_m': str(spacing),
            'formation_mode': formation_mode,
            'formation_followers': followers,
        }.items())


def launch_setup(context, *args, **kwargs):
    launch_dir = os.path.join(get_package_share_directory('combat_robot_nav2'), 'launch')
    num = int(LaunchConfiguration('num_robots').perform(context))
    spacing = float(LaunchConfiguration('formation_lateral_spacing_m').perform(context))
    fmode = LaunchConfiguration('formation_mode').perform(context)
    random_spawn = LaunchConfiguration('random_spawn').perform(context).lower() in ('true', '1')

    # random_spawn: scatter all robots at random non-overlapping poses (+random yaw).
    # On a mission each robot then navigates from its random spot to its formation slot
    # (offset-path start) and the leader speed-governor waits for stragglers → form-up.
    rand_xy = _scatter(num) if random_spawn else None
    # Random POSITION only — keep spawn yaw=0. A random spawn yaw breaks the navsat/IMU
    # heading alignment (ekf heading comes out ~180deg off for some robots), so nav2
    # drives them the wrong way. yaw=0 matches the working fixed-spawn heading alignment.
    rand_yaw = [0.0 for _ in range(num)] if random_spawn else None

    world = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(os.path.join(launch_dir, 'gz_world_sim.launch.py')))

    actions = [world]
    # Leader waits for followers (ids 2..num) to form up before start (dynamic + FollowPath static).
    leader_followers = ','.join(str(i) for i in range(2, num + 1))
    lx, ly, lyaw = (rand_xy[0][0], rand_xy[0][1], rand_yaw[0]) if random_spawn else (0.0, 0.0, 0.0)
    actions.append(TimerAction(
        period=8.0,
        actions=[_robot(launch_dir, 's1', 1, 'leader', lx, ly, spacing, fmode,
                        leader_followers, yaw=lyaw)]))

    # Symmetric slot (must match executor formationSlotOffset): rank=id-1 →
    # 1:+1(left) 2:-1(right) 3:+2 4:-2 ... For a north path, +slot = west (-x).
    def slot_of(rid):
        rank = rid - 1
        return (rank + 1) // 2 if rank % 2 == 1 else -(rank // 2)

    # Followers s2..sN, staggered. Spawn:
    #  - static  : at their lateral slot (start in formation).
    #  - dynamic : SCATTERED off-slot (behind, spread) so the form-up phase is visible.
    for k, rid in enumerate(range(2, num + 1)):
        slot = slot_of(rid)
        if random_spawn:
            x, y, yaw = rand_xy[rid - 1][0], rand_xy[rid - 1][1], rand_yaw[rid - 1]
        elif fmode == 'dynamic':
            x, y, yaw = -slot * 4.0, -5.0, 0.0   # behind + opposite-spread → drives into slot
        else:
            x, y, yaw = 0.0, slot * spacing, 0.0  # east path: lateral along y (drive = +x)
        actions.append(TimerAction(
            period=20.0 + k * 8.0,
            actions=[_robot(launch_dir, f's{rid}', rid, 'follower', x, y, spacing, fmode,
                            yaw=yaw)]))

    # TF unification + rviz so one rviz shows all robots (TF is per-robot namespaced).
    # use_rviz:=false 면 rviz 와 TF 통합 relay 를 모두 끈다 — rviz 렌더링 + relay×N 이
    # 단일호스트 부하의 큰 축이라, N대 군집을 실시간으로 돌릴 때 부하를 크게 줄인다.
    # (로봇 거동은 gz 창으로 계속 관찰 가능; rviz 의 costmap/플랜 뷰만 빠짐.)
    use_rviz = LaunchConfiguration('use_rviz').perform(context).lower() in ('true', '1')
    if use_rviz:
        tf_nodes = []
        for i in range(1, num + 1):
            tf_nodes += _tf_unify(f's{i}')
        rviz_cfg = os.path.join(get_package_share_directory('combat_robot_nav2'),
                                'rviz', 'swarm_sim.rviz')
        rviz = Node(package='rviz2', executable='rviz2', name='rviz2',
                    arguments=['-d', rviz_cfg],
                    parameters=[{'use_sim_time': True}], output='screen')
        actions.append(TimerAction(period=20.0 + num * 8.0, actions=tf_nodes + [rviz]))
    return actions


def generate_launch_description():
    return LaunchDescription([
        DeclareLaunchArgument('num_robots', default_value='2'),
        DeclareLaunchArgument('formation_lateral_spacing_m', default_value='2.0'),
        DeclareLaunchArgument('formation_mode', default_value='static',
                              description='static (offset-path, symmetric independent nav2 + leader-paced speed sync) | dynamic (legacy leader-pose chasing)'),
        DeclareLaunchArgument('random_spawn', default_value='false',
                              description='true: scatter robots at random poses; they form up into the formation on a mission'),
        DeclareLaunchArgument('use_rviz', default_value='true',
                              description='false: rviz + TF통합 relay 끔 (단일호스트 N대 부하 절감; gz 창으로 관찰)'),
        OpaqueFunction(function=launch_setup),
    ])
