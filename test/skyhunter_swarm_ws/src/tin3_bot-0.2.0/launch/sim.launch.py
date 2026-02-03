# # Workable 02-02
# import os
# from ament_index_python.packages import get_package_share_directory
# from launch import LaunchDescription
# from launch.actions import (
#     DeclareLaunchArgument, IncludeLaunchDescription, SetEnvironmentVariable, 
#     TimerAction, OpaqueFunction
# )
# from launch.launch_description_sources import PythonLaunchDescriptionSource
# from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
# from launch_ros.actions import Node

# # ========================================================================
# # 1. ROBOT SPAWNER (The Body)
# # This function calls 'spawn_robot.launch.py' for each robot.
# # It handles the Gazebo model, the Bridge, and the Robot State Publisher.
# # ========================================================================
# def spawn_robots(context, num_robots, use_ekf, lidar_mode):
#     pkg_tin3_bot = get_package_share_directory("tin3_bot")
#     num = int(context.perform_substitution(num_robots))
#     spawn_actions = []
    
#     for i in range(num):
#         # Namespace: robot_01, robot_02... (Standard ROS 2 convention)
#         ns = f"robot_{i + 1:02d}"
        
#         # Initial V-Shape Positions
#         x = 0.0 if i == 0 else -2.0 * i
#         y = 0.0 if i == 0 else (2.0 if i % 2 != 0 else -2.0)
        
#         # Stagger spawning to prevent Gazebo freeze
#         spawn = TimerAction(
#             period=float(i) * 2.0,
#             actions=[
#                 IncludeLaunchDescription(
#                     PythonLaunchDescriptionSource(
#                         os.path.join(pkg_tin3_bot, "launch", "spawn_robot.launch.py")
#                     ),
#                     launch_arguments={
#                         "robot_ns": ns,
#                         "x": str(x), "y": str(y), "z": "0.5",
#                         "use_ekf": "true", # EKF provides the /odom -> /base_footprint TF
#                         "lidar_mode": "half",
#                     }.items(),
#                 )
#             ],
#         )
#         spawn_actions.append(spawn)
#     return spawn_actions

# # ========================================================================
# # 2. SWARM INTELLIGENCE (The Brain)
# # This function starts your C++ Leader and Follower nodes.
# # ========================================================================
# def launch_swarm_logic(context, num_robots):
#     num = int(context.perform_substitution(num_robots))
#     pkg_tin3_bot = get_package_share_directory("tin3_bot")
#     nodes = []

#     # --- ROBOT 01: LEADER ---
#     leader_ns = "robot_01"
    
#     # 1. Perception Node (Elevation Mapper)
#     leader_perception = Node(
#         package='skyhunter_perception', 
#         executable='elevation_mapper_node',
#         namespace=leader_ns,
#         name='elevation_mapper',
#         parameters=[{
#             'use_sim_time': True,
#             'base_frame': f'{leader_ns}/base_footprint', # Matches tin3_bot URDF
#             'map_frame': f'{leader_ns}/odom',
#             'cloud_topic': 'scan/points', # tin3_bot bridge topic
#             'map_topic': 'elevation_map'
#         }],
#         output='screen'
#     )
#     nodes.append(leader_perception)

#     # 2. Leader Logic Node
#     leader_node = Node(
#         package='skyhunter_formation', 
#         executable='leader_node',
#         namespace=leader_ns,
#         name='leader_node',
#         parameters=[{
#             'use_sim_time': True,
#             'odom_topic': 'odom',             # Reads /robot_01/odom
#             'leader_state_topic': '/leader_state' # Writes to GLOBAL /leader_state
#         }],
#         output='screen'
#     )
#     nodes.append(leader_node)

#     # 3. Nav2 Stack (Path Planning)
#     nav2_launch = IncludeLaunchDescription(
#         PythonLaunchDescriptionSource(
#             os.path.join(pkg_tin3_bot, "launch", "nav2_test.launch.py")
#         ),
#         launch_arguments={'robot_ns': leader_ns}.items()
#     )
#     nodes.append(nav2_launch)

#     # --- ROBOTS 02...N: FOLLOWERS ---
#     # Offsets for V-Formation (x, y) relative to leader
#     offsets = [(-2.0, 2.0), (-2.0, -2.0), (-4.0, 4.0), (-4.0, -4.0), (-6.0, 6.0), (-6.0, -6.0)]

#     for i in range(1, num):
#         robot_ns = f"robot_{i + 1:02d}"
        
#         # Assign offset cyclically
#         off_x, off_y = offsets[(i-1) % len(offsets)]

#         # 1. Follower Perception
#         follower_perception = Node(
#             package='skyhunter_perception', 
#             executable='elevation_mapper_node',
#             namespace=robot_ns,
#             name='elevation_mapper',
#             parameters=[{
#                 'use_sim_time': True,
#                 'base_frame': f'{robot_ns}/base_footprint',
#                 'map_frame': f'{robot_ns}/odom',
#                 'cloud_topic': 'scan/points',
#                 'map_topic': 'elevation_map'
#             }],
#             output='screen'
#         )
#         nodes.append(follower_perception)

#         # 2. Follower Logic
#         follower_node = Node(
#             package='skyhunter_formation', 
#             executable='follower_node',
#             namespace=robot_ns,
#             name='follower_node',
#             parameters=[{
#                 'use_sim_time': True,
#                 'offset_x': off_x,
#                 'offset_y': off_y,
#                 'leader_topic': '/leader_state', # Must match Leader's output
#                 'map_topic': 'elevation_map',    # Must match Perception output
#                 'kp_linear': 1.0, 
#                 'kp_angular': 2.0,
#                 'min_safe_dist': 1.0
#             }],
#             output='screen'
#         )
#         nodes.append(follower_node)

#     return nodes

# def generate_launch_description():
#     pkg_ros_gz_sim = get_package_share_directory("ros_gz_sim")
#     pkg_tin3_bot = get_package_share_directory("tin3_bot")

#     # Set Resource Path for Gazebo to find meshes
#     gz_resource_path = SetEnvironmentVariable(
#         name="GZ_SIM_RESOURCE_PATH",
#         value=[os.environ.get("GZ_SIM_RESOURCE_PATH", ""), ":", os.path.dirname(pkg_tin3_bot)]
#     )

#     return LaunchDescription([
#         gz_resource_path,
        
#         # Args
#         DeclareLaunchArgument("world", default_value="rough_terrain.sdf"),
#         DeclareLaunchArgument("num_robots", default_value="2"),
#         DeclareLaunchArgument("use_ekf", default_value="true"),
#         DeclareLaunchArgument("lidar_mode", default_value="half"),
        
#         # Start Gazebo
#         IncludeLaunchDescription(
#             PythonLaunchDescriptionSource(os.path.join(pkg_ros_gz_sim, "launch", "gz_sim.launch.py")),
#             launch_arguments={"gz_args": [PathJoinSubstitution([pkg_tin3_bot, "worlds", LaunchConfiguration("world")]), " -r"]}.items(),
#         ),
        
#         # Start Clock Bridge (Crucial for sim time)
#         Node(package="ros_gz_bridge", executable="parameter_bridge",
#              name="clock_bridge", arguments=["/clock@rosgraph_msgs/msg/Clock[gz.msgs.Clock"], output="screen"),
             
#         # Spawn Robots
#         OpaqueFunction(function=spawn_robots, args=[LaunchConfiguration("num_robots"), LaunchConfiguration("use_ekf"), LaunchConfiguration("lidar_mode")]),
        
#         # Start Logic (Delayed to ensure robots are ready)
#         TimerAction(period=8.0, actions=[OpaqueFunction(function=launch_swarm_logic, args=[LaunchConfiguration("num_robots")])])
#     ])



















import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import (
    DeclareLaunchArgument, IncludeLaunchDescription, SetEnvironmentVariable, 
    TimerAction, OpaqueFunction
)
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node

def spawn_robots(context, num_robots, use_ekf, lidar_mode):
    pkg_tin3_bot = get_package_share_directory("tin3_bot")
    num = int(context.perform_substitution(num_robots))
    spawn_actions = []
    
    for i in range(num):
        ns = f"robot_{i + 1:02d}"
        x = 0.0 if i == 0 else -2.0 * i
        y = 0.0 if i == 0 else (2.0 if i % 2 != 0 else -2.0)
        
        spawn = TimerAction(
            period=float(i) * 2.0,
            actions=[
                IncludeLaunchDescription(
                    PythonLaunchDescriptionSource(
                        os.path.join(pkg_tin3_bot, "launch", "spawn_robot.launch.py")
                    ),
                    launch_arguments={
                        "robot_ns": ns,
                        "x": str(x), "y": str(y), "z": "0.5",
                        "use_ekf": "true",
                        "lidar_mode": "half",
                    }.items(),
                )
            ],
        )
        spawn_actions.append(spawn)
    return spawn_actions

def launch_swarm_logic(context, num_robots):
    num = int(context.perform_substitution(num_robots))
    pkg_tin3_bot = get_package_share_directory("tin3_bot")
    nodes = []

    for i in range(num):
        robot_ns = f"robot_{i + 1:02d}"

        # 1. Perception Node (Elevation Mapper) - for ALL robots
        perception_node = Node(
            package='skyhunter_perception', 
            executable='elevation_mapper_node',
            namespace=robot_ns,
            name='elevation_mapper',
            parameters=[{
                'use_sim_time': True,
                'base_frame': f'{robot_ns}/base_footprint',
                'map_frame': f'{robot_ns}/odom',
                'cloud_topic': 'scan/points',
                'map_topic': 'elevation_map'
            }],
            output='screen'
        )
        nodes.append(perception_node)

        # 2. Logic Splitting
        if i == 0:
            # ==========================
            # LEADER (Robot 01)
            # ==========================
            leader_node = Node(
                package='skyhunter_formation', 
                executable='leader_node',
                namespace=robot_ns,
                name='leader_node',
                parameters=[{
                    'use_sim_time': True,
                    'odom_topic': 'odom',
                    'leader_state_topic': '/leader_state' 
                }],
                output='screen'
            )
            nodes.append(leader_node)

            # Launch Nav2 Stack (This handles the map->odom TF for the leader)
            nav2_launch = IncludeLaunchDescription(
                PythonLaunchDescriptionSource(
                    os.path.join(pkg_tin3_bot, "launch", "nav2_test.launch.py")
                ),
                launch_arguments={'robot_ns': robot_ns}.items()
            )
            nodes.append(nav2_launch)

        else:
            # ==========================
            # FOLLOWERS (Robot 02+)
            # ==========================
            
            # *** FIX: Only spawn Static TF for followers ***
            # The Leader gets its TF from nav2_test.launch.py
            static_tf_publisher = Node(
                package='tf2_ros',
                executable='static_transform_publisher',
                name=f'map_to_{robot_ns}_odom_tf',
                arguments=['0', '0', '0', '0', '0', '0', 'map', f'{robot_ns}/odom'],
                parameters=[{'use_sim_time': True}],
            )
            nodes.append(static_tf_publisher)

            # Follower Logic
            offsets = [(-2.0, 2.0), (-2.0, -2.0), (-4.0, 4.0), (-4.0, -4.0), (-6.0, 6.0), (-6.0, -6.0)]
            off_x, off_y = offsets[(i-1) % len(offsets)]
            
            follower_node = Node(
                package='skyhunter_formation', 
                executable='follower_node',
                namespace=robot_ns,
                name='follower_node',
                parameters=[{
                    'use_sim_time': True,
                    'offset_x': off_x,
                    'offset_y': off_y,
                    'leader_topic': '/leader_state',
                    'map_topic': 'elevation_map',
                    'k_x': 1.5, 
                    'k_theta': 4.0 
                }],
                output='screen'
            )
            nodes.append(follower_node)

    return nodes

def generate_launch_description():
    pkg_ros_gz_sim = get_package_share_directory("ros_gz_sim")
    pkg_tin3_bot = get_package_share_directory("tin3_bot")

    gz_resource_path = SetEnvironmentVariable(
        name="GZ_SIM_RESOURCE_PATH",
        value=[os.environ.get("GZ_SIM_RESOURCE_PATH", ""), ":", os.path.dirname(pkg_tin3_bot)]
    )

    return LaunchDescription([
        gz_resource_path,
        DeclareLaunchArgument("world", default_value="obstacle_world.sdf"),
        DeclareLaunchArgument("num_robots", default_value="2"),
        DeclareLaunchArgument("use_ekf", default_value="true"),
        DeclareLaunchArgument("lidar_mode", default_value="half"),
        
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(os.path.join(pkg_ros_gz_sim, "launch", "gz_sim.launch.py")),
            launch_arguments={"gz_args": [PathJoinSubstitution([pkg_tin3_bot, "worlds", LaunchConfiguration("world")]), " -r"]}.items(),
        ),
        
        Node(package="ros_gz_bridge", executable="parameter_bridge",
             name="clock_bridge", arguments=["/clock@rosgraph_msgs/msg/Clock[gz.msgs.Clock"], output="screen"),
             
        OpaqueFunction(function=spawn_robots, args=[LaunchConfiguration("num_robots"), LaunchConfiguration("use_ekf"), LaunchConfiguration("lidar_mode")]),
        TimerAction(period=8.0, actions=[OpaqueFunction(function=launch_swarm_logic, args=[LaunchConfiguration("num_robots")])])
    ])