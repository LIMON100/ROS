#!/usr/bin/env python3
# 태블릿 에뮬레이터: 리더 command_server PATH_PORT(65436)로 경로명령 TCP 전송.
# AssaultCommandHeader(packed): uint8 command, uint16 num_waypoints, uint32 data_length (LE) + json payload.
# 사용법: path_tcp_send.py <host> <cmd:load|start|stop> [path_json]
import sys, socket, struct
CMD = {"none":0,"start":1,"stop":2,"pause":3,"resume":4,"load":5}

def main():
    host = sys.argv[1]; cmd = CMD[sys.argv[2].lower()]
    pj = sys.argv[3] if len(sys.argv) > 3 else ""
    nw = pj.count("{") if "{" in pj else 0
    payload = pj.encode() if cmd == 5 else b""
    header = struct.pack("<BHI", cmd, nw, len(payload))
    s = socket.create_connection((host, 65436), timeout=5)
    s.sendall(header + payload)
    print(f"sent cmd={cmd} nw={nw} paylen={len(payload)} to {host}:65436")
    s.close()

if __name__ == "__main__":
    main()
