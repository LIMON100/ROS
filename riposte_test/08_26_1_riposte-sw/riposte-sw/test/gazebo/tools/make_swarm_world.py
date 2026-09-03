 #!/usr/bin/env python3
"""Generate riposte_swarm.sdf -- N independently moving target drones.

The shipped worlds carry either ONE constant-velocity target (riposte_closure)
or six STATIC balloons (riposte_balloon).  Neither exercises multi-target
tracking against MOVING targets, so this generator emits a world in which every
target is a separate model with its own start pose and its own commanded
velocity, named with a common prefix so tools/gz_track_bridge can pick them all
up in multi-target mode:

    gz_track_bridge riposte_swarm x500_0 'drone_*'

Speeds and headings live in TARGETS below -- that table IS the configuration.
Edit it and re-run; no XML is edited by hand.

    python3 make_swarm_world.py            # writes riposte_swarm.sdf here

World systems are copied from riposte_closure.sdf deliberately: a world that
lists its own plugins does NOT inherit the server defaults, so every system the
x500 needs (imu / magnetometer / air pressure / navsat) must be named here or
preflight fails ("Found 0 compass").
"""
import os
WORLD = "riposte_swarm"
SCALE = float(os.environ.get("SPEED_SCALE", "1.0"))

# name, start pose (ENU m), commanded linear velocity (m/s).
# Bearings are spread so the ownship must yaw to find them all; altitudes are
# 4-7 m so the elevation angle stays inside a body-fixed camera's vertical FOV
# on approach (the -28 deg loss recorded in GAZEBO-TEST-001 S-G4).
TARGETS = [
    # name       x     y     z    vx     vy   vz
    ("drone_1",  30.0,  -6.0, 5.0, -0.40,  0.15, 0.0),
    ("drone_2",  32.0,   0.0, 5.5, -0.45,  0.00, 0.0),
    ("drone_3",  28.0,   6.0, 4.5, -0.40, -0.15, 0.0),
    ("drone_4",  36.0,  -3.0, 6.0, -0.50,  0.10, 0.0),
    ("drone_5",  34.0,   4.0, 4.5, -0.45, -0.10, 0.0),
]   
LEAD = float(os.environ.get("START_LEAD", "0"))
TARGETS = [(n, x - vx*SCALE*LEAD, y - vy*SCALE*LEAD, z, vx*SCALE, vy*SCALE, vz*SCALE)
           for (n, x, y, z, vx, vy, vz) in TARGETS]

   

HDR = """<?xml version="1.0" ?>
<!--
Riposte MULTI-TARGET swarm world (Gazebo Harmonic / gz-sim 8).

GENERATED FILE - edit make_swarm_world.py and re-run, not this file.

{n} target drones, each an independent model with its own start pose and its
own VelocityControl command, sharing the prefix "drone_" so the bridge can
track them all at once:
    PX4_GZ_WORLD={world} \\
      GZ_SIM_RESOURCE_PATH=<repo>/riposte-sw/test/gazebo/worlds:<...> \\
        make px4_sitl gz_x500

      gz_track_bridge {world} x500_0 'drone_*'

    Truth poses go out on /world/{world}/pose/info.  The bridge applies its range
    + field-of-view model (GZ_BRIDGE_MAX_RANGE_M / HFOV_DEG / VFOV_DEG), counts
    the visible targets into num_targets, and publishes the nearest as primary
    (REQ-001 R-8).
  -->
  <sdf version="1.9">
    <world name="{world}">
      <physics name="1ms" type="ignored">
        <max_step_size>0.004</max_step_size>
        <real_time_factor>1.0</real_time_factor>
      </physics>

      <plugin filename="gz-sim-physics-system" name="gz::sim::systems::Physics"/>
      <plugin filename="gz-sim-user-commands-system"
              name="gz::sim::systems::UserCommands"/>
      <plugin filename="gz-sim-scene-broadcaster-system"
              name="gz::sim::systems::SceneBroadcaster"/>
      <plugin filename="gz-sim-sensors-system" name="gz::sim::systems::Sensors">
        <render_engine>ogre2</render_engine>
      </plugin> 
      <!-- x500 sensor feeds. A world listing its own plugins does not inherit the
           server defaults, so each system the vehicle needs must appear here. -->
      <plugin filename="gz-sim-imu-system" name="gz::sim::systems::Imu"/>
      <plugin filename="gz-sim-magnetometer-system" name="gz::sim::systems::Magnetometer"/>
      <plugin filename="gz-sim-air-pressure-system"
              name="gz::sim::systems::AirPressure"/>
      <plugin filename="gz-sim-navsat-system" name="gz::sim::systems::NavSat"/>
  
      <!-- PX4 GPS origin (matches PX4 default SITL home). -->
      <spherical_coordinates>
        <surface_model>EARTH_WGS84</surface_model>
        <world_frame_orientation>ENU</world_frame_orientation>
        <latitude_deg>47.397971057728974</latitude_deg>
        <longitude_deg>8.546163739800146</longitude_deg>
        <elevation>0</elevation>
      </spherical_coordinates>

      <light type="directional" name="sun">
        <cast_shadows>true</cast_shadows>
        <pose>0 0 10 0 0 0</pose>
        <diffuse>0.8 0.8 0.8 1</diffuse>
        <specular>0.2 0.2 0.2 1</specular>
        <direction>-0.5 0.1 -0.9</direction>
      </light>
      <model name="ground_plane">
        <static>true</static>
        <link name="link">
          <collision name="collision">
            <geometry><plane><normal>0 0 1</normal><size>200 200</size></plane></geometry>
          </collision>
          <visual name="visual">
            <geometry><plane><normal>0 0 1</normal><size>200 200</size></plane></geometry>
            <material>
              <ambient>0.3 0.35 0.3 1</ambient>
              <diffuse>0.3 0.4 0.3 1</diffuse>
            </material>
          </visual>
        </link>
      </model>
  """
MODEL = """
      <!-- {name}: start ({x} {y} {z}) ENU, velocity ({vx} {vy} {vz}) m/s, speed {spd:.2f} m/s -->
      <model name="{name}">
        <pose>{x} {y} {z} 0 0 0</pose>
        <link name="body">
        <gravity>false</gravity>
          <inertial>
            <mass>0.5</mass>
            <inertia><ixx>0.01</ixx><iyy>0.01</iyy><izz>0.02</izz>
                     <ixy>0</ixy><ixz>0</ixz><iyz>0</iyz></inertia>
          </inertial>
          <visual name="core">
            <geometry><box><size>0.30 0.30 0.10</size></box></geometry>
            <material>
              <ambient>{r} {g} {b} 1</ambient>
              <diffuse>{r} {g} {b} 1</diffuse>
            </material>
          </visual>
          <visual name="arm">
            <geometry><box><size>0.5 0.05 0.03</size></box></geometry>
            <material><diffuse>0.2 0.2 0.2 1</diffuse></material>
          </visual>
        </link>
        <plugin filename="gz-sim-velocity-control-system"
                name="gz::sim::systems::VelocityControl">
          <initial_linear>{vx} {vy} {vz}</initial_linear>
          <initial_angular>0 0 0</initial_angular>
        </plugin>
        </model>
  """

# Distinct colours so the GUI shows which track id belongs to which model.
COLOURS = [(0.9, 0.1, 0.1), (0.1, 0.5, 0.9), (0.9, 0.7, 0.1),
            (0.2, 0.8, 0.2), (0.8, 0.2, 0.8), (0.1, 0.8, 0.8)]
            
            
def main():
    out = [HDR.format(n=len(TARGETS), world=WORLD)]
    for i, (name, x, y, z, vx, vy, vz) in enumerate(TARGETS):
        r, g, b = COLOURS[i % len(COLOURS)]
        spd = (vx * vx + vy * vy + vz * vz) ** 0.5
        out.append(MODEL.format(name=name, x=x, y=y, z=z, vx=vx, vy=vy, vz=vz,
                                spd=spd, r=r, g=g, b=b))
    out.append("  </world>\n</sdf>\n")
    
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "worlds",
                        WORLD + ".sdf")
    with open(path, "w") as f:
        f.write("".join(out))
        
    print("wrote " + path)
    print("targets: %d" % len(TARGETS))

    for name, x, y, z, vx, vy, vz in TARGETS:
        rng = (x * x + y * y + z * z) ** 0.5
        spd = (vx * vx + vy * vy + vz * vz) ** 0.5
        print("  %-8s start %6.1f %6.1f %4.1f  range %5.1f m  speed %.2f m/s"
            % (name, x, y, z, rng, spd))
            
            
if __name__ == "__main__":
    main()
