#!/usr/bin/env python3
# 태블릿 STATE 명령 에뮬레이터: command_server COMMAND_PORT(65432)로 StateCommand(raw 72B).
# 사용법: state_tcp_send.py <host> <command_id>  (2=PROTECT_G, 4=ASSAULT, 1=RECON)
import sys, socket, struct
host = sys.argv[1]; cid = int(sys.argv[2])
# <3B I 3b f B I 4B 8I 2d B> = 72B (pragma pack 1)
pkt = struct.pack('<3BI3bfBI4B8I2dB',
    cid,0,0,        # command_id, e_stop, attack_permission
    0,              # approval_request_id
    0,0,0,          # pan,tilt,zoom
    0.0,            # lateral_wind
    0,              # stream_command
    0,              # stream_target_robot_id
    0,0,0,0,        # formation_type,number,grouping,selected_count
    0,0,0,0,0,0,0,0,# selected_robot_ids[8]
    0.0,0.0,        # drone lat,lon
    0)              # drone_target_valid
assert len(pkt)==72, len(pkt)
s = socket.create_connection((host,65432),timeout=5); s.sendall(pkt)
print(f"sent StateCommand command_id={cid} ({len(pkt)}B) to {host}:65432"); s.close()
